/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsAutoPtr.h"
#include "nsISynthVoiceRegistry.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class RemoteVoice;
class SpeechSynthesisUtterance;
class nsSpeechTask;
class VoiceData;

class nsSynthVoiceRegistry : public nsISynthVoiceRegistry
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYNTHVOICEREGISTRY

  nsSynthVoiceRegistry();
  virtual ~nsSynthVoiceRegistry();

  static nsSynthVoiceRegistry* GetInstance();

  static already_AddRefed<nsSynthVoiceRegistry> GetInstanceForService();

  static void Shutdown();

private:
  nsresult AddVoiceImpl(nsISupports* aService,
                        const nsAString& aUri,
                        const nsAString& aName,
                        const nsAString& aLang,
                        bool aLocalService);

  nsTArray<nsRefPtr<VoiceData> > mVoices;

  nsTArray<nsRefPtr<VoiceData> > mDefaultVoices;

  nsRefPtrHashtable<nsStringHashKey, VoiceData> mUriVoiceMap;
};

} // namespace dom
} // namespace mozilla
