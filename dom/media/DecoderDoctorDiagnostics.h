/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecoderDoctorDiagnostics_h_
#define DecoderDoctorDiagnostics_h_

#include "nsString.h"

class nsIDocument;

namespace mozilla {

// DecoderDoctorDiagnostics class, used to gather data from PDMs/DecoderTraits,
// and then notify the user about issues preventing (or worsening) playback.
//
// The expected usage is:
// 1. Instantiate a DecoderDoctorDiagnostics in a function (close to the point
//    where a webpage is trying to know whether some MIME types can be played,
//    or trying to play a media file).
// 2. Pass a pointer to the DecoderDoctorDiagnostics structure to one of the
//    CanPlayStatus/IsTypeSupported/(others?). During that call, some PDMs may
//    add relevant diagnostic information.
// 3. Analyze the collected diagnostics, and optionally dispatch an event to the
//    UX, to notify the user about potential playback issues and how to resolve
//    them.
//
// This class' methods must be called from the main thread.

class DecoderDoctorDiagnostics
{
public:
  // Store the diagnostic information collected so far on a document for a
  // given format. All diagnostics for a document will be analyzed together
  // within a short timeframe.
  // Should only be called once.
  void StoreFormatDiagnostics(nsIDocument* aDocument,
                              const nsAString& aFormat,
                              bool aCanPlay,
                              const char* aCallSite);

  void StoreMediaKeySystemAccess(nsIDocument* aDocument,
                                 const nsAString& aKeySystem,
                                 bool aIsSupported,
                                 const char* aCallSite);

  enum DiagnosticsType {
    eUnsaved,
    eFormatSupportCheck,
    eMediaKeySystemAccessRequest
  };
  DiagnosticsType Type() const { return mDiagnosticsType; }

  // Description string, for logging purposes; only call on stored diags.
  nsCString GetDescription() const;

  // Methods to record diagnostic information:

  const nsAString& Format() const { return mFormat; }
  bool CanPlay() const { return mCanPlay; }

  void SetWMFFailedToLoad() { mWMFFailedToLoad = true; }
  bool DidWMFFailToLoad() const { return mWMFFailedToLoad; }

  void SetFFmpegFailedToLoad() { mFFmpegFailedToLoad = true; }
  bool DidFFmpegFailToLoad() const { return mFFmpegFailedToLoad; }

  void SetGMPPDMFailedToStartup() { mGMPPDMFailedToStartup = true; }
  bool DidGMPPDMFailToStartup() const { return mGMPPDMFailedToStartup; }

  void SetVideoFormatNotSupport() { mVideoNotSupported = true; }
  void SetAudioFormatNotSupport() { mAudioNotSupported = true; }

  void SetGMP(const nsACString& aGMP) { mGMP = aGMP; }
  const nsACString& GMP() const { return mGMP; }

  const nsAString& KeySystem() const { return mKeySystem; }
  bool IsKeySystemSupported() const { return mIsKeySystemSupported; }
  enum KeySystemIssue {
    eUnset,
    eWidevineWithNoWMF
  };
  void SetKeySystemIssue(KeySystemIssue aKeySystemIssue)
  {
    mKeySystemIssue = aKeySystemIssue;
  }
  KeySystemIssue GetKeySystemIssue() const
  {
    return mKeySystemIssue;
  }

private:
  // Currently-known type of diagnostics. Set from one of the 'Store...' methods.
  // This helps ensure diagnostics are only stored once,
  // and makes it easy to know what information they contain.
  DiagnosticsType mDiagnosticsType = eUnsaved;

  nsString mFormat;
  // True if there is at least one decoder that can play that format.
  bool mCanPlay = false;

  bool mWMFFailedToLoad = false;
  bool mFFmpegFailedToLoad = false;
  bool mGMPPDMFailedToStartup = false;
  bool mVideoNotSupported = false;
  bool mAudioNotSupported = false;
  nsCString mGMP;

  nsString mKeySystem;
  bool mIsKeySystemSupported = false;
  KeySystemIssue mKeySystemIssue = eUnset;
};

} // namespace mozilla

#endif
