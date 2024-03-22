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
#include "mozilla/RandomNum.h"
#include "nsFrameMessageManager.h"
#include "prenv.h"

namespace mozilla::dom {

LazyLogModule MMPrinter::sMMLog("MessageManager");

// You can use
// https://gist.github.com/tomrittervg/adb8688426a9a5340da96004e2c8af79 to parse
// the output of the logs into logs more friendly to reading.

/* static */
Maybe<uint64_t> MMPrinter::PrintHeader(char const* aLocation,
                                       const nsAString& aMsg) {
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
    return Nothing();
  }

  uint64_t aMsgId = RandomUint64OrDie();

  MOZ_LOG(MMPrinter::sMMLog, LogLevel::Debug,
          ("%" PRIu64 " %s Message: %s in process type: %s", aMsgId, aLocation,
           charMsg.get(), XRE_GetProcessTypeString()));

  return Some(aMsgId);
}

/* static */
void MMPrinter::PrintNoData(uint64_t aMsgId) {
  if (!MOZ_LOG_TEST(sMMLog, LogLevel::Verbose)) {
    return;
  }
  MOZ_LOG(MMPrinter::sMMLog, LogLevel::Verbose,
          ("%" PRIu64 " (No Data)", aMsgId));
}

/* static */
void MMPrinter::PrintData(uint64_t aMsgId, ClonedMessageData const& aData) {
  if (!MOZ_LOG_TEST(sMMLog, LogLevel::Verbose)) {
    return;
  }

  ErrorResult rv;

  AutoJSAPI jsapi;
  // We're using this context to deserialize, stringify, and print a message
  // manager message here. Since the messages are always sent from and to system
  // scopes, we need to do this in a system scope, or attempting to deserialize
  // certain privileged objects will fail.
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();

  ipc::StructuredCloneData data;
  ipc::UnpackClonedMessageData(aData, data);

  /* Read original StructuredCloneData. */
  JS::Rooted<JS::Value> scdContent(cx);
  data.Read(cx, &scdContent, rv);
  if (rv.Failed()) {
    // In testing, the only reason this would fail was if there was no data in
    // the message; so it seems like this is safe-ish.
    MMPrinter::PrintNoData(aMsgId);
    rv.SuppressException();
    return;
  }

  JS::Rooted<JSString*> unevalObj(cx, JS_ValueToSource(cx, scdContent));
  nsAutoJSString srcString;
  if (!srcString.init(cx, unevalObj)) return;

  MOZ_LOG(MMPrinter::sMMLog, LogLevel::Verbose,
          ("%" PRIu64 " %s", aMsgId, NS_ConvertUTF16toUTF8(srcString).get()));
}

}  // namespace mozilla::dom
