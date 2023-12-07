/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"
#include "util/Text.h"

static const char16_t arr[] = u"hi, don't delete me";
static const size_t arrlen = js_strlen(arr);

static const char arr2[] = "hi, don't delete me";
static const size_t arrlen2 = js_strlen(arr2);

static int finalized1 = 0;
static int finalized2 = 0;
static int finalized3 = 0;
static int finalized4 = 0;

struct ExternalStringCallbacks : public JSExternalStringCallbacks {
  int* finalizedCount = nullptr;
  bool isTwoBytes = false;

  explicit ExternalStringCallbacks(int* finalizedCount, bool isTwoBytes)
      : finalizedCount(finalizedCount), isTwoBytes(isTwoBytes) {}

  void finalize(JS::Latin1Char* chars) const override {
    MOZ_ASSERT(!isTwoBytes);
    MOZ_ASSERT(chars == (JS::Latin1Char*)arr2);
    (*finalizedCount)++;
  }

  void finalize(char16_t* chars) const override {
    MOZ_ASSERT(isTwoBytes);
    MOZ_ASSERT(chars == arr);
    (*finalizedCount)++;
  }

  size_t sizeOfBuffer(const JS::Latin1Char* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    MOZ_CRASH("Unexpected call");
  }

  size_t sizeOfBuffer(const char16_t* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    MOZ_CRASH("Unexpected call");
  }
};

static const ExternalStringCallbacks callbacks1(&finalized1, true);
static const ExternalStringCallbacks callbacks2(&finalized2, true);
static const ExternalStringCallbacks callbacks3(&finalized3, false);
static const ExternalStringCallbacks callbacks4(&finalized4, false);

BEGIN_TEST(testExternalStrings) {
  const unsigned N = 1000;

  for (unsigned i = 0; i < N; ++i) {
    CHECK(JS_NewExternalUCString(cx, arr, arrlen, &callbacks1));
    CHECK(JS_NewExternalUCString(cx, arr, arrlen, &callbacks2));
    CHECK(JS_NewExternalStringLatin1(cx, (JS::Latin1Char*)arr2, arrlen2,
                                     &callbacks3));
    CHECK(JS_NewExternalStringLatin1(cx, (JS::Latin1Char*)arr2, arrlen2,
                                     &callbacks4));
  }

  JS_GC(cx);

  CHECK((N - finalized1) == 0);
  CHECK((N - finalized2) == 0);
  CHECK((N - finalized3) == 0);
  CHECK((N - finalized4) == 0);

  return true;
}
END_TEST(testExternalStrings)
