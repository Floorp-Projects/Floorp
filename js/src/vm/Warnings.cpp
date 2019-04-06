/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Warnings.h"

#include <stdarg.h>  // va_{list,start,end}

#include "jsapi.h"    // js::AssertHeapIsIdle
#include "jstypes.h"  // JS_PUBLIC_API

#include "js/ErrorReport.h"  // JSREPORT_WARNING
#include "vm/JSContext.h"  // js::ArgumentsAre{ASCII,Latin1,UTF8}, js::ReportErrorVA

using js::ArgumentsAreASCII;
using js::ArgumentsAreLatin1;
using js::ArgumentsAreUTF8;
using js::AssertHeapIsIdle;
using js::ReportErrorVA;

JS_PUBLIC_API bool JS::WarnASCII(JSContext* cx, const char* format, ...) {
  va_list ap;
  bool ok;

  AssertHeapIsIdle();
  va_start(ap, format);
  ok = ReportErrorVA(cx, JSREPORT_WARNING, format, ArgumentsAreASCII, ap);
  va_end(ap);
  return ok;
}

JS_PUBLIC_API bool JS::WarnLatin1(JSContext* cx, const char* format, ...) {
  va_list ap;
  bool ok;

  AssertHeapIsIdle();
  va_start(ap, format);
  ok = ReportErrorVA(cx, JSREPORT_WARNING, format, ArgumentsAreLatin1, ap);
  va_end(ap);
  return ok;
}

JS_PUBLIC_API bool JS::WarnUTF8(JSContext* cx, const char* format, ...) {
  va_list ap;
  bool ok;

  AssertHeapIsIdle();
  va_start(ap, format);
  ok = ReportErrorVA(cx, JSREPORT_WARNING, format, ArgumentsAreUTF8, ap);
  va_end(ap);
  return ok;
}

JS_PUBLIC_API JS::WarningReporter JS::GetWarningReporter(JSContext* cx) {
  return cx->runtime()->warningReporter;
}

JS_PUBLIC_API JS::WarningReporter JS::SetWarningReporter(
    JSContext* cx, WarningReporter reporter) {
  WarningReporter older = cx->runtime()->warningReporter;
  cx->runtime()->warningReporter = reporter;
  return older;
}
