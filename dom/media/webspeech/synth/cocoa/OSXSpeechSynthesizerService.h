/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_OsxSpeechSynthesizerService_h
#define mozilla_dom_OsxSpeechSynthesizerService_h

#include "nsAutoPtr.h"
#include "nsISpeechService.h"
#include "nsIObserver.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {

class OSXSpeechSynthesizerService final : public nsISpeechService
                                        , public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISPEECHSERVICE
  NS_DECL_NSIOBSERVER

  bool Init();

  static OSXSpeechSynthesizerService* GetInstance();
  static already_AddRefed<OSXSpeechSynthesizerService> GetInstanceForService();
  static void Shutdown();

private:
  OSXSpeechSynthesizerService();
  virtual ~OSXSpeechSynthesizerService();

  bool RegisterVoices();

  bool mInitialized;
  static mozilla::StaticRefPtr<OSXSpeechSynthesizerService> sSingleton;
};

} // namespace dom
} // namespace mozilla

#endif
