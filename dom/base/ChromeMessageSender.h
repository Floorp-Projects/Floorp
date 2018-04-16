/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChromeMessageSender_h
#define mozilla_dom_ChromeMessageSender_h

#include "mozilla/dom/MessageSender.h"

namespace mozilla {
namespace dom {

class MessageBroadcaster;

class ChromeMessageSender final : public MessageSender
{
public:
  explicit ChromeMessageSender(MessageBroadcaster* aParentManager)
    : MessageSender(nullptr, aParentManager, MessageManagerFlags::MM_CHROME)
  {
    // This is a bit hackish, we wait until the child process is running before attaching
    // to the parent manager (see MessageSender::InitWithCallback).
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // FrameScriptLoader
  void LoadFrameScript(const nsAString& aUrl, bool aAllowDelayedLoad,
                       bool aRunInGlobalScope, mozilla::ErrorResult& aError)
  {
    LoadScript(aUrl, aAllowDelayedLoad, aRunInGlobalScope, aError);
  }
  void RemoveDelayedFrameScript(const nsAString& aURL)
  {
    RemoveDelayedScript(aURL);
  }
  void GetDelayedFrameScripts(JSContext* aCx,
                              nsTArray<nsTArray<JS::Value>>& aScripts,
                              mozilla::ErrorResult& aError)
  {
    GetDelayedScripts(aCx, aScripts, aError);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ChromeMessageSender_h
