/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MMPrinter.h"

namespace mozilla {
namespace dom {

LazyLogModule MMPrinter::sMMLog("MessageManager");

/* static */
void MMPrinter::PrintImpl(char const* aLocation, const nsAString& aMsg,
                          ClonedMessageData const& aData) {
  NS_ConvertUTF16toUTF8 charMsg(aMsg);

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