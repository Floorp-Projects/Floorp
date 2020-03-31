#include "gdb-tests.h"
#include "jsapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/Value.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Utf8.h"

#include <string.h>

FRAGMENT(asmjs, segfault) {
  constexpr unsigned line0 = __LINE__;
  static const char chars[] =
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

  JS::CompileOptions opts(cx);
  opts.setFileAndLine(__FILE__, line0 + 1);
  opts.asmJSOption = JS::AsmJSOption::Enabled;

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  JS::Rooted<JS::Value> rval(cx);

  bool ok = srcBuf.init(cx, chars, mozilla::ArrayLength(chars) - 1,
                        JS::SourceOwnership::Borrowed) &&
            JS::Evaluate(cx, opts, srcBuf, &rval);

  breakpoint();

  use(ok);
  use(rval);
}
