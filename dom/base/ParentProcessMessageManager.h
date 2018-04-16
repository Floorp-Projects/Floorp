/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ParentProcessMessageManager_h
#define mozilla_dom_ParentProcessMessageManager_h

#include "mozilla/dom/MessageBroadcaster.h"

namespace mozilla {
namespace dom {

/**
 * Implementation for the WebIDL ParentProcessMessageManager interface.
 * ParentProcessMessageManager is used in a parent process to communicate with all the
 * child processes.
 */
class ParentProcessMessageManager final : public MessageBroadcaster
{
public:
  ParentProcessMessageManager();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

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

private:
  virtual ~ParentProcessMessageManager();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ParentProcessMessageManager_h
