/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPicoService_h
#define nsPicoService_h

#include "mozilla/Mutex.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsISpeechService.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace dom {

class PicoVoice;
class PicoCallbackRunnable;

typedef void* pico_System;
typedef void* pico_Resource;
typedef void* pico_Engine;

class nsPicoService : public nsIObserver,
                      public nsISpeechService
{
  friend class PicoCallbackRunnable;
  friend class PicoInitRunnable;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISPEECHSERVICE
  NS_DECL_NSIOBSERVER

  nsPicoService();

  static nsPicoService* GetInstance();

  static already_AddRefed<nsPicoService> GetInstanceForService();

  static void Shutdown();

private:

  virtual ~nsPicoService();

  void Init();

  void RegisterVoices();

  bool GetVoiceFileLanguage(const nsACString& aFileName, nsAString& aLang);

  void LoadEngine(PicoVoice* aVoice);

  void UnloadEngine();

  PicoVoice* CurrentVoice();

  bool mInitialized;

  nsCOMPtr<nsIThread> mThread;

  nsRefPtrHashtable<nsStringHashKey, PicoVoice> mVoices;

  Monitor mVoicesMonitor;

  PicoVoice* mCurrentVoice;

  Atomic<nsISpeechTask*> mCurrentTask;

  pico_System mPicoSystem;

  pico_Engine mPicoEngine;

  pico_Resource mSgResource;

  pico_Resource mTaResource;

  mozilla::UniquePtr<uint8_t[]> mPicoMemArea;

  static StaticRefPtr<nsPicoService> sSingleton;
};

} // namespace dom
} // namespace mozilla

#endif
