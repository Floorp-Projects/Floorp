#include "gdb-tests.h"
#include "jsapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/RootingAPI.h"
#include "js/Value.h"

#include <string.h>

FRAGMENT(asmjs, segfault) {
    using namespace JS;

    int line0 = __LINE__;
    const char* bytes = "\n"
        "function f(glob, ffi, heap) {\n"
        "    \"use asm\";\n"
        "    var f32 = new glob.Float32Array(heap);\n"
        "    function g(n) {\n"
        "        n = n | 0;\n"
        "        return +f32[n>>2];\n"
        "    }\n"
        "    return g;\n"
        "}\n"
        "\n"
        "var func = f(this, null, new ArrayBuffer(0x10000));\n"
        "func(0x10000 << 2);\n"
        "'ok'\n";

    CompileOptions opts(cx);
    opts.setFileAndLine(__FILE__, line0 + 1);
    opts.asmJSOption = JS::AsmJSOption::Enabled;

    Rooted<Value> rval(cx);
    bool ok = JS::EvaluateUtf8(cx, opts, bytes, strlen(bytes), &rval);

    breakpoint();

    use(ok);
    use(rval);
}
