#include <csignal>
#include <iostream>
#include <memory>

#include "nlohmann/json.hpp"
#include "logging.hpp"
#include "mqtt_client.hpp"

using json = nlohmann::json;

const std::string SERVER_ADDRESS("tcp://localhost:1883");

using namespace iotea::core;

static const std::string FEATURE = "provider_talent";
static const std::string FUNC_MULTIPLY = "multiply";
static const std::string FUNC_SUM = "sum";
static const std::string FUNC_FIBONACCI = "fibonacci";

class MathFunctions : public FunctionTalent {
   private:
    Callee fib;

   public:
    explicit MathFunctions(std::shared_ptr<Publisher> publisher)
        : FunctionTalent(FEATURE, publisher) {
        RegisterFunction(FUNC_MULTIPLY,
                         [this](const json& args, const CallContext& context) { Multiply(args, context); });

        RegisterFunction(FUNC_FIBONACCI,
                [this](const json& args, const CallContext& context) { Fibonacci(args, context); });

        fib = CreateCallee(FEATURE, FUNC_FIBONACCI);

        SkipCycleCheck(true);
    }

    void Multiply(const json& args, const CallContext& context) {
        auto a = args[0].get<int>();
        auto b = args[1];
        auto val = std::to_string(a * b["factor"].get<int>()) + " " + b["unit"].get<std::string>();
        context.Reply(val);

        static int dingdings = 0;
        NewEventContext("my-subject").Emit<int>("dingdings", ++dingdings, "blob");
    }


    void Fibonacci(const json& args, CallContext context) {
        auto n = args[0].get<int>();

        if (n <= 1) {
            context.Reply(n);
            return;
        }

        auto t1 = fib.Call(n - 1, context);
        auto t2 = fib.Call(n - 2, context);

        context.GatherAndReply([](std::vector<std::pair<json, EventContext>> replies) {
                auto n1 = replies[0].first.get<int>();
                auto n2 = replies[1].first.get<int>();

                return n1 + n2;
                }, {t1, t2});
    }


    schema::rules_ptr OnGetRules() const override { return nullptr; }
};

static std::shared_ptr<MqttClient> client = std::make_shared<MqttClient>(SERVER_ADDRESS, "function_provider");

void signal_handler(int signal) { client->Stop(); }

int main(int argc, char* argv[]) {
    auto talent = std::make_shared<MathFunctions>(client);
    client->RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);

    client->Run();

    return 0;
}
