/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChromeMessageBroadcaster_h
#define mozilla_dom_ChromeMessageBroadcaster_h

#include "mozilla/dom/MessageListenerManager.h"

namespace mozilla {
namespace dom {

class ChromeMessageBroadcaster final : public MessageListenerManager
{
public:
  explicit ChromeMessageBroadcaster(MessageManagerFlags aFlags)
    : ChromeMessageBroadcaster(nullptr, aFlags)
  {
    MOZ_ASSERT(!(aFlags & ~(MessageManagerFlags::MM_GLOBAL |
                            MessageManagerFlags::MM_PROCESSMANAGER |
                            MessageManagerFlags::MM_OWNSCALLBACK)));
  }
  explicit ChromeMessageBroadcaster(ChromeMessageBroadcaster* aParentManager)
    : ChromeMessageBroadcaster(aParentManager, MessageManagerFlags::MM_NONE)
  {}

  static ChromeMessageBroadcaster* From(nsFrameMessageManager* aManager)
  {
    if (aManager->IsBroadcaster() && aManager->IsChrome()) {
      return static_cast<ChromeMessageBroadcaster*>(aManager);
    }
    return nullptr;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void BroadcastAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                             JS::Handle<JS::Value> aObj,
                             JS::Handle<JSObject*> aObjects,
                             mozilla::ErrorResult& aError)
  {
    DispatchAsyncMessage(aCx, aMessageName, aObj, aObjects, nullptr,
                         JS::UndefinedHandleValue, aError);
  }
  uint32_t ChildCount()
  {
    return mChildManagers.Length();
  }
  MessageListenerManager* GetChildAt(uint32_t aIndex)
  {
    return mChildManagers.SafeElementAt(aIndex);
  }
  void ReleaseCachedProcesses();

  // ProcessScriptLoader
  void LoadProcessScript(const nsAString& aUrl, bool aAllowDelayedLoad,
                         mozilla::ErrorResult& aError)
  {
    LoadScript(aUrl, aAllowDelayedLoad, false, aError);
  }
  void RemoveDelayedProcessScript(const nsAString& aURL)
  {
    RemoveDelayedScript(aURL);
  }
  void GetDelayedProcessScripts(JSContext* aCx,
                                nsTArray<nsTArray<JS::Value>>& aScripts,
                                mozilla::ErrorResult& aError)
  {
    GetDelayedScripts(aCx, aScripts, aError);
  }

  // GlobalProcessScriptLoader
  using nsFrameMessageManager::GetInitialProcessData;

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

  void AddChildManager(MessageListenerManager* aManager);
  void RemoveChildManager(MessageListenerManager* aManager);

private:
  ChromeMessageBroadcaster(ChromeMessageBroadcaster* aParentManager,
                           MessageManagerFlags aFlags);
  virtual ~ChromeMessageBroadcaster();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ChromeMessageBroadcaster_h
