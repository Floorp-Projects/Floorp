/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChromeMessageBroadcaster_h
#define mozilla_dom_ChromeMessageBroadcaster_h

#include "mozilla/dom/MessageBroadcaster.h"

namespace mozilla {
namespace dom {

/**
 * Implementation for the WebIDL ChromeMessageBroadcaster interface. Used for window and
 * group message managers.
 */
class ChromeMessageBroadcaster final : public MessageBroadcaster
{
public:
  explicit ChromeMessageBroadcaster(MessageManagerFlags aFlags)
    : ChromeMessageBroadcaster(nullptr, aFlags)
  {
    MOZ_ASSERT(!(aFlags & ~(MessageManagerFlags::MM_GLOBAL |
                            MessageManagerFlags::MM_OWNSCALLBACK)));
  }
  explicit ChromeMessageBroadcaster(MessageBroadcaster* aParentManager)
    : ChromeMessageBroadcaster(aParentManager, MessageManagerFlags::MM_NONE)
  {}

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

private:
  ChromeMessageBroadcaster(MessageBroadcaster* aParentManager,
                           MessageManagerFlags aFlags)
    : MessageBroadcaster(aParentManager,
                         aFlags |
                         MessageManagerFlags::MM_CHROME)
  {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ChromeMessageBroadcaster_h
