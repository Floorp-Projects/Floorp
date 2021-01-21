/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"
#include "util/Text.h"

static const char16_t arr[] = u"hi, don't delete me";
static const size_t arrlen = js_strlen(arr);

static int finalized1 = 0;
static int finalized2 = 0;

struct ExternalStringCallbacks : public JSExternalStringCallbacks {
  int* finalizedCount = nullptr;

  explicit ExternalStringCallbacks(int* finalizedCount)
      : finalizedCount(finalizedCount) {}

  void finalize(char16_t* chars) const override {
    MOZ_ASSERT(chars == arr);
    (*finalizedCount)++;
  }

  size_t sizeOfBuffer(const char16_t* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    MOZ_CRASH("Unexpected call");
  }
};

static const ExternalStringCallbacks callbacks1(&finalized1);
static const ExternalStringCallbacks callbacks2(&finalized2);

BEGIN_TEST(testExternalStrings) {
  const unsigned N = 1000;

  for (unsigned i = 0; i < N; ++i) {
    CHECK(JS_NewExternalString(cx, arr, arrlen, &callbacks1));
    CHECK(JS_NewExternalString(cx, arr, arrlen, &callbacks2));
  }

  JS_GC(cx);

  // a generous fudge factor to account for strings rooted by conservative gc
  const unsigned epsilon = 10;

  CHECK((N - finalized1) < epsilon);
  CHECK((N - finalized2) < epsilon);

  return true;
}
END_TEST(testExternalStrings)
