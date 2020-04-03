/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#ifndef mozilla_dom_ScriptLoadHandler_h
#define mozilla_dom_ScriptLoadHandler_h

#include "nsIIncrementalStreamLoader.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace dom {

class ScriptLoadRequest;
class ScriptLoader;
class SRICheckDataVerifier;

class ScriptLoadHandler final : public nsIIncrementalStreamLoaderObserver {
 public:
  explicit ScriptLoadHandler(
      ScriptLoader* aScriptLoader, ScriptLoadRequest* aRequest,
      UniquePtr<SRICheckDataVerifier>&& aSRIDataVerifier);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINCREMENTALSTREAMLOADEROBSERVER

 private:
  virtual ~ScriptLoadHandler();

  /*
   * Decode the given data into the already-allocated internal
   * |ScriptTextBuffer<Unit>|.
   *
   * This function is intended to be called only by |DecodeRawData| after
   * determining which sort of |ScriptTextBuffer<Unit>| has been allocated.
   */
  template <typename Unit>
  nsresult DecodeRawDataHelper(const uint8_t* aData, uint32_t aDataLength,
                               bool aEndOfStream);

  /*
   * Once the charset is found by the EnsureDecoder function, we can
   * incrementally convert the charset to the one expected by the JS Parser.
   */
  nsresult DecodeRawData(const uint8_t* aData, uint32_t aDataLength,
                         bool aEndOfStream);

  /*
   * Discover the charset by looking at the stream data, the script tag, and
   * other indicators.  Returns true if charset has been discovered.
   */
  bool EnsureDecoder(nsIIncrementalStreamLoader* aLoader, const uint8_t* aData,
                     uint32_t aDataLength, bool aEndOfStream) {
    // Check if the decoder has already been created.
    if (mDecoder) {
      return true;
    }

    return TrySetDecoder(aLoader, aData, aDataLength, aEndOfStream);
  }

  /*
   * Attempt to determine how script data will be decoded, when such
   * determination hasn't already been made.  (If you don't know whether it's
   * been made yet, use |EnsureDecoder| above instead.)  Return false if there
   * isn't enough information yet to make the determination, or true if a
   * determination was made.
   */
  bool TrySetDecoder(nsIIncrementalStreamLoader* aLoader, const uint8_t* aData,
                     uint32_t aDataLength, bool aEndOfStream);

  /*
   * When streaming bytecode, we have the opportunity to fallback early if SRI
   * does not match the expectation of the document.
   */
  nsresult MaybeDecodeSRI();

  // Query the channel to find the data type associated with the input stream.
  nsresult EnsureKnownDataType(nsIIncrementalStreamLoader* aLoader);

  // ScriptLoader which will handle the parsed script.
  RefPtr<ScriptLoader> mScriptLoader;

  // The ScriptLoadRequest for this load. Decoded data are accumulated on it.
  RefPtr<ScriptLoadRequest> mRequest;

  // SRI data verifier.
  UniquePtr<SRICheckDataVerifier> mSRIDataVerifier;

  // Status of SRI data operations.
  nsresult mSRIStatus;

  // Unicode decoder for charset.
  mozilla::UniquePtr<mozilla::Decoder> mDecoder;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScriptLoadHandler_h
