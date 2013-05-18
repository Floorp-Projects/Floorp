/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "SpeechRecognition.h"

#include "nsISpeechRecognitionService.h"

#define NS_FAKE_SPEECH_RECOGNITION_SERVICE_CID \
  {0x48c345e7, 0x9929, 0x4f9a, {0xa5, 0x63, 0xf4, 0x78, 0x22, 0x2d, 0xab, 0xcd}};

namespace mozilla {

class FakeSpeechRecognitionService : public nsISpeechRecognitionService,
                                     public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISPEECHRECOGNITIONSERVICE
  NS_DECL_NSIOBSERVER

  FakeSpeechRecognitionService();

private:
  virtual ~FakeSpeechRecognitionService();

  WeakPtr<dom::SpeechRecognition> mRecognition;
  dom::SpeechRecognitionResultList* BuildMockResultList();
};

} // namespace mozilla
