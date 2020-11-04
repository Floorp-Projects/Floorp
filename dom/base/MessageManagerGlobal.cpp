/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessageManagerGlobal.h"
#include "mozilla/IntentionalCrash.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"

#ifdef ANDROID
#  include <android/log.h>
#endif
#ifdef XP_WIN
#  include <windows.h>
#endif

namespace mozilla {
namespace dom {

void MessageManagerGlobal::Dump(const nsAString& aStr) {
  if (!nsJSUtils::DumpEnabled()) {
    return;
  }

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s",
                      NS_ConvertUTF16toUTF8(aStr).get());
#endif
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    OutputDebugStringW(PromiseFlatString(aStr).get());
  }
#endif
  fputs(NS_ConvertUTF16toUTF8(aStr).get(), stdout);
  fflush(stdout);
}

void MessageManagerGlobal::Atob(const nsAString& aAsciiString,
                                nsAString& aBase64Data, ErrorResult& aError) {
  aError = nsContentUtils::Atob(aAsciiString, aBase64Data);
}

void MessageManagerGlobal::Btoa(const nsAString& aBase64Data,
                                nsAString& aAsciiString, ErrorResult& aError) {
  aError = nsContentUtils::Btoa(aBase64Data, aAsciiString);
}

}  // namespace dom
}  // namespace mozilla
