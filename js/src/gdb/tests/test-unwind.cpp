#include "gdb-tests.h"
#include "jsapi.h"
#include "jit/JitOptions.h"

#include <string.h>

static bool
Something(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
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

    CompileOptions opts(cx);
    opts.setFileAndLine(__FILE__, line0 + 1);
    RootedValue rval(cx);
    Evaluate(cx, opts, bytes, strlen(bytes), &rval);

    js::jit::JitOptions.baselineWarmUpThreshold = saveThreshold;
}
