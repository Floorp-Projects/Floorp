#include "gdb-tests.h"
#include "jsapi.h" // sundry symbols not moved to more-specific headers yet

#include "jit/JitOptions.h" // js::jit::JitOptions
#include "js/CallArgs.h" // JS::CallArgs, JS::CallArgsFromVp
#include "js/CompilationAndEvaluation.h" // JS::Evaluate
#include "js/CompileOptions.h" // JS::CompileOptions
#include "js/RootingAPI.h" // JS::Rooted
#include "js/Value.h" // JS::Value

#include <stdint.h> // uint32_t
#include <string.h> // strlen

static bool
Something(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    args.rval().setInt32(23);
    breakpoint();
    return true;
}

static const JSFunctionSpecWithHelp unwind_functions[] = {
    JS_FN_HELP("something", Something, 0, 0,
"something()",
"  Test function for test-unwind."),
    JS_FS_HELP_END
};

FRAGMENT(unwind, simple) {
    using namespace JS;

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    if (!JS_DefineFunctionsWithHelp(cx, global, unwind_functions))
        return;

    // baseline-eager.
    uint32_t saveThreshold = js::jit::JitOptions.baselineWarmUpThreshold;
    js::jit::JitOptions.baselineWarmUpThreshold = 0;

    int line0 = __LINE__;
    const char* bytes = "\n"
        "function unwindFunctionInner() {\n"
        "    return something();\n"
        "}\n"
        "\n"
        "function unwindFunctionOuter() {\n"
        "    return unwindFunctionInner();\n"
        "}\n"
        "\n"
        "unwindFunctionOuter();\n";

    JS::CompileOptions opts(cx);
    opts.setFileAndLine(__FILE__, line0 + 1);
    JS::Rooted<JS::Value> rval(cx);
    JS::Evaluate(cx, opts, bytes, strlen(bytes), &rval);

    js::jit::JitOptions.baselineWarmUpThreshold = saveThreshold;
}
