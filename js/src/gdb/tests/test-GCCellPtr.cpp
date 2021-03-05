#include "gdb-tests.h"

#include "mozilla/Unused.h"

#include "jsapi.h"

#include "js/CompileOptions.h"
#include "js/CompilationAndEvaluation.h"
#include "js/HeapAPI.h"
#include "js/RegExpFlags.h"
#include "js/SourceText.h"
#include "js/Symbol.h"
#include "js/TypeDecls.h"
#include "vm/BigIntType.h"
#include "vm/JSObject.h"
#include "vm/ObjectGroup.h"
#include "vm/RegExpObject.h"
#include "vm/Shape.h"

#include "vm/JSObject-inl.h"

FRAGMENT(GCCellPtr, simple) {
  JS::Rooted<JSObject*> glob(cx, JS::CurrentGlobalOrNull(cx));
  JS::Rooted<JSString*> empty(cx, JS_NewStringCopyN(cx, nullptr, 0));
  JS::Rooted<JS::Symbol*> unique(cx, JS::NewSymbol(cx, nullptr));
  JS::Rooted<JS::BigInt*> zeroBigInt(cx, JS::BigInt::zero(cx));
  JS::Rooted<js::RegExpObject*> regExp(
      cx, js::RegExpObject::create(cx, u"", 0, JS::RegExpFlags{},
                                   js::GenericObject));
  JS::Rooted<js::RegExpShared*> rootedRegExpShared(
      cx, js::RegExpObject::getShared(cx, regExp));

  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);
  JS::SourceText<char16_t> srcBuf;
  mozilla::Unused << srcBuf.init(cx, nullptr, 0, JS::SourceOwnership::Borrowed);
  JS::RootedScript emptyScript(cx, JS::Compile(cx, options, srcBuf));

  // Inline TraceKinds.
  JS::GCCellPtr nulll(nullptr);
  JS::GCCellPtr object(glob.get());
  JS::GCCellPtr string(empty.get());
  JS::GCCellPtr symbol(unique.get());
  JS::GCCellPtr bigint(zeroBigInt.get());
  JS::GCCellPtr shape(glob->shape());

  // Out-of-line TraceKinds.
  JS::GCCellPtr baseShape(glob->shape()->base());
  // JitCode can't easily be tested here, so skip it.
  JS::GCCellPtr script(emptyScript.get());
  JS::GCCellPtr scope(emptyScript->bodyScope());
  JS::GCCellPtr regExpShared(rootedRegExpShared.get());

  breakpoint();

  use(nulll);
  use(object);
  use(string);
  use(symbol);
  use(bigint);
  use(shape);
  use(baseShape);
  use(script);
  use(scope);
  use(regExpShared);
}
