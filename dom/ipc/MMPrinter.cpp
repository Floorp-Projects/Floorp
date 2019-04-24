/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MMPrinter.h"

#include "jsapi.h"
#include "nsJSUtils.h"
#include "Logging.h"
#include "mozilla/Bootstrap.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ErrorResult.h"
#include "nsFrameMessageManager.h"

namespace mozilla {
namespace dom {

LazyLogModule MMPrinter::sMMLog("MessageManager");

/* static */
void MMPrinter::PrintImpl(char const* aLocation, const nsAString& aMsg,
                          ClonedMessageData const& aData) {
  NS_ConvertUTF16toUTF8 charMsg(aMsg);

  /*
   * The topic will be skipped if the topic name appears anywhere as a substring
   * of the filter.
   *
   * Example:
   *   MOZ_LOG_MESSAGEMANAGER_SKIP="foobar|extension"
   *     Will  match the topics 'foobar', 'foo', 'bar', and 'ten' (even though
   * you may not have intended to match the latter three) and it will not match
   * the topics 'extensionresult' or 'Foo'.
   */
  char* mmSkipLog = PR_GetEnv("MOZ_LOG_MESSAGEMANAGER_SKIP");

  if (mmSkipLog && strstr(mmSkipLog, charMsg.get())) {
    return;
  }

  MOZ_LOG(MMPrinter::sMMLog, LogLevel::Debug,
          ("%s Message: %s in process type: %s", aLocation, charMsg.get(),
           XRE_ChildProcessTypeToString(XRE_GetProcessType())));

  if (!MOZ_LOG_TEST(sMMLog, LogLevel::Verbose)) {
    return;
  }

  ErrorResult rv;

  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::UnprivilegedJunkScope()));
  JSContext* cx = jsapi.cx();

  ipc::StructuredCloneData data;
  ipc::UnpackClonedMessageDataForChild(aData, data);

  /* Read original StructuredCloneData. */
  JS::RootedValue scdContent(cx);
  data.Read(cx, &scdContent, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return;
  }

  JS::RootedString unevalObj(cx, JS_ValueToSource(cx, scdContent));
  nsAutoJSString srcString;
  if (!srcString.init(cx, unevalObj)) return;

  MOZ_LOG(MMPrinter::sMMLog, LogLevel::Verbose,
          ("   %s", NS_ConvertUTF16toUTF8(srcString).get()));
  return;
}

}  // namespace dom
}  // namespace mozilla