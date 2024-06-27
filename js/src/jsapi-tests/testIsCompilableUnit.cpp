/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <string_view>

#include "js/CompilationAndEvaluation.h"  // JS_Utf8BufferIsCompilableUnit
#include "js/ErrorReport.h"               // JSErrorReport
#include "js/GlobalObject.h"              // JS::CurrentGlobalOrNull
#include "js/RootingAPI.h"                // JS::Rooted
#include "js/SourceText.h"                // JS::SourceText, JS::SourceOwnership
#include "js/Warnings.h"                  // JS::SetWarningReporter
#include "jsapi-tests/tests.h"

static bool gIsCompilableUnitWarned = false;

BEGIN_TEST(testIsCompilableUnit) {
  JS::SetWarningReporter(cx, WarningReporter);

  // Compilable case.
  {
    static constexpr std::string_view src = "1";
    CHECK(JS_Utf8BufferIsCompilableUnit(cx, global, src.data(), src.length()));
    CHECK(!JS_IsExceptionPending(cx));
    CHECK(!gIsCompilableUnitWarned);
  }

  // Not compilable cases.
  {
    static constexpr std::string_view src = "1 + ";
    CHECK(!JS_Utf8BufferIsCompilableUnit(cx, global, src.data(), src.length()));
    CHECK(!JS_IsExceptionPending(cx));
    CHECK(!gIsCompilableUnitWarned);
  }

  {
    static constexpr std::string_view src = "() =>";
    CHECK(!JS_Utf8BufferIsCompilableUnit(cx, global, src.data(), src.length()));
    CHECK(!JS_IsExceptionPending(cx));
    CHECK(!gIsCompilableUnitWarned);
  }

  // Compilable but warned case.
  {
    static constexpr std::string_view src = "() => { return; unreachable(); }";
    CHECK(JS_Utf8BufferIsCompilableUnit(cx, global, src.data(), src.length()));
    CHECK(!JS_IsExceptionPending(cx));
    CHECK(!gIsCompilableUnitWarned);
  }

  // Invalid UTF-8 case.
  // This should report error with returning *true*.
  {
    static constexpr std::string_view src = "\x80";
    CHECK(JS_Utf8BufferIsCompilableUnit(cx, global, src.data(), src.length()));
    CHECK(JS_IsExceptionPending(cx));
    CHECK(!gIsCompilableUnitWarned);
    JS_ClearPendingException(cx);
  }

  return true;
}

static void WarningReporter(JSContext* cx, JSErrorReport* report) {
  gIsCompilableUnitWarned = true;
}
END_TEST(testIsCompilableUnit);
