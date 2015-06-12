/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PocketSphinxRecognitionService_h
#define mozilla_dom_PocketSphinxRecognitionService_h

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsISpeechRecognitionService.h"
#include "speex/speex_resampler.h"

extern "C" {
#include <pocketsphinx/pocketsphinx.h>
#include <sphinxbase/sphinx_config.h>
}

#define NS_POCKETSPHINX_SPEECH_RECOGNITION_SERVICE_CID                         \
  {                                                                            \
    0x0ff5ce56, 0x5b09, 0x4db8, {                                              \
      0xad, 0xc6, 0x82, 0x66, 0xaf, 0x95, 0xf8, 0x64                           \
    }                                                                          \
  };

namespace mozilla {

/**
 * Pocketsphix implementation of the nsISpeechRecognitionService interface
 */
class PocketSphinxSpeechRecognitionService : public nsISpeechRecognitionService,
                                             public nsIObserver
{
public:
  // Add XPCOM glue code
  NS_DECL_ISUPPORTS
  NS_DECL_NSISPEECHRECOGNITIONSERVICE

  // Add nsIObserver code
  NS_DECL_NSIOBSERVER

  /**
   * Default constructs a PocketSphinxSpeechRecognitionService loading default
   * files
   */
  PocketSphinxSpeechRecognitionService();

private:
  /**
   * Private destructor to prevent bypassing of reference counting
   */
  virtual ~PocketSphinxSpeechRecognitionService();

  /** The associated SpeechRecognition */
  WeakPtr<dom::SpeechRecognition> mRecognition;

  /**
   * Builds a mock SpeechRecognitionResultList
   */
  dom::SpeechRecognitionResultList* BuildMockResultList();

  /** Speex state */
  SpeexResamplerState* mSpeexState;

  /** Pocksphix decoder */
  ps_decoder_t* mPSHandle;

  /** Sphinxbase parsed command line arguments */
  cmd_ln_t* mPSConfig;

  /** Flag to verify if decoder was created */
  bool ISDecoderCreated;

  /** Flag to verify if grammar was compiled */
  bool ISGrammarCompiled;

  /** Audio data */
  nsTArray<int16_t> mAudioVector;
};

} // namespace mozilla

#endif
