#include "gdb-tests.h"
#include "jsapi.h"  // sundry symbols not moved to more-specific headers yet
#include "jsfriendapi.h"  // JSFunctionSpecWithHelp

#include "jit/JitOptions.h"               // js::jit::JitOptions
#include "js/CallArgs.h"                  // JS::CallArgs, JS::CallArgsFromVp
#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/CompileOptions.h"            // JS::CompileOptions
#include "js/GlobalObject.h"              // JS::CurrentGlobalOrNull
#include "js/PropertyAndElement.h"        // JS_DefineProperty
#include "js/RootingAPI.h"                // JS::Rooted
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "js/Value.h"                     // JS::Value

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <stdint.h>  // uint32_t
#include <string.h>  // strlen

static bool Something(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  args.rval().setInt32(23);
  breakpoint();
  return true;
}

// clang-format off
static const JSFunctionSpecWithHelp unwind_functions[] = {
    JS_FN_HELP("something", Something, 0, 0,
"something()",
"  Test function for test-unwind."),
    JS_FS_HELP_END
};
// clang-format on

FRAGMENT(unwind, simple) {
  using namespace JS;

  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  if (!JS_DefineFunctionsWithHelp(cx, global, unwind_functions)) {
    return;
  }

  // Define an itercount property and use it to ensure Baseline compilation.
  uint32_t threshold = js::jit::JitOptions.baselineJitWarmUpThreshold;
  RootedValue val(cx, Int32Value(threshold + 10));
  if (!JS_DefineProperty(cx, global, "itercount", val, 0)) {
    return;
  }

  int line0 = __LINE__;
  const char* bytes =
      "\n"
      "function unwindFunctionInner() {\n"
      "    for (var i = 0; i < itercount; i++) {}\n"
      "    return something();\n"
      "}\n"
      "\n"
      "function unwindFunctionOuter() {\n"
      "    for (var i = 0; i < itercount; i++) {}\n"
      "    return unwindFunctionInner();\n"
      "}\n"
      "\n"
      "unwindFunctionOuter();\n";

  JS::CompileOptions opts(cx);
  opts.setFileAndLine(__FILE__, line0 + 1);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, bytes, strlen(bytes), JS::SourceOwnership::Borrowed)) {
    return;
  }

  JS::Rooted<JS::Value> rval(cx);
  JS::Evaluate(cx, opts, srcBuf, &rval);
}
