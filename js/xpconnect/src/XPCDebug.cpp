/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "js/friend/DumpFunctions.h"  // JS::FormatStackDump
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

#include "mozilla/Sprintf.h"

#ifdef XP_WIN
#  include <windows.h>
#  include "nsPrintfCString.h"
#endif

#ifdef ANDROID
#  include <android/log.h>
#endif

static void DebugDump(const char* str) {
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    nsPrintfCString output("%s\n", str);
    OutputDebugStringA(output.get());
  }
#elif defined(ANDROID)
  __android_log_print(ANDROID_LOG_DEBUG, "Gecko", "%s\n", str);
#endif
  printf("%s\n", str);
}

bool xpc_DumpJSStack(bool showArgs, bool showLocals, bool showThisProps) {
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    printf("there is no JSContext on the stack!\n");
  } else if (JS::UniqueChars buf =
                 xpc_PrintJSStack(cx, showArgs, showLocals, showThisProps)) {
    DebugDump(buf.get());
  }
  return true;
}

JS::UniqueChars xpc_PrintJSStack(JSContext* cx, bool showArgs, bool showLocals,
                                 bool showThisProps) {
  JS::AutoSaveExceptionState state(cx);

  JS::UniqueChars buf =
      JS::FormatStackDump(cx, showArgs, showLocals, showThisProps);
  if (!buf) {
    DebugDump("Failed to format JavaScript stack for dump");
  }

  state.restore();
  return buf;
}
