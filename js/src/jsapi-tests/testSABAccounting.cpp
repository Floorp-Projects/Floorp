#include "builtin/TestingFunctions.h"
#include "js/SharedArrayBuffer.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testSABAccounting) {
  // Purge what we can
  JS::PrepareForFullGC(cx);
  NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);

  // Self-hosting and chrome code should not use SABs, or the point of this
  // predicate is completely lost.
  CHECK(!JS::ContainsSharedArrayBuffer(cx));

  JS::RootedObject obj(cx), obj2(cx);
  CHECK(obj = JS::NewSharedArrayBuffer(cx, 4096));
  CHECK(JS::ContainsSharedArrayBuffer(cx));
  CHECK(obj2 = JS::NewSharedArrayBuffer(cx, 4096));
  CHECK(JS::ContainsSharedArrayBuffer(cx));

  // Discard those objects again.
  obj = nullptr;
  obj2 = nullptr;
  JS::PrepareForFullGC(cx);
  NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);

  // Should be back to base state.
  CHECK(!JS::ContainsSharedArrayBuffer(cx));

  return true;
}
END_TEST(testSABAccounting)
