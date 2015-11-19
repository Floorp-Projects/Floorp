/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SapiService_h
#define mozilla_dom_SapiService_h

#include "nsAutoPtr.h"
#include "nsISpeechService.h"
#include "nsIObserver.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "mozilla/StaticPtr.h"

#include <windows.h>
#include <sapi.h>

namespace mozilla {
namespace dom {

class SapiCallback;

class SapiService final : public nsISpeechService
                        , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISPEECHSERVICE
  NS_DECL_NSIOBSERVER

  SapiService();
  bool Init();

  static SapiService* GetInstance();
  static already_AddRefed<SapiService> GetInstanceForService();

  static void Shutdown();

  static void __stdcall SpeechEventCallback(WPARAM aWParam, LPARAM aLParam);

private:
  virtual ~SapiService();

  bool RegisterVoices();

  nsRefPtrHashtable<nsStringHashKey, ISpObjectToken> mVoices;
  nsTArray<RefPtr<SapiCallback>> mCallbacks;
  RefPtr<ISpVoice> mSapiClient;

  bool mInitialized;

  static StaticRefPtr<SapiService> sSingleton;
};

} // namespace dom
} // namespace mozilla

#endif
