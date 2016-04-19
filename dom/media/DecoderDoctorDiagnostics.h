/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecoderDoctorDiagnostics_h_
#define DecoderDoctorDiagnostics_h_

class nsIDocument;
class nsAString;

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
  void StoreDiagnostics(nsIDocument* aDocument,
                        const nsAString& aFormat,
                        const char* aCallSite);

  // Methods to record diagnostic information:

  void SetCanPlay() { mCanPlay = true; }
  bool CanPlay() const { return mCanPlay; }

  void SetFFmpegFailedToLoad() { mFFmpegFailedToLoad = true; }
  bool DidFFmpegFailToLoad() const { return mFFmpegFailedToLoad; }

private:
  // True if there is at least one decoder that can play the media.
  bool mCanPlay = false;

  bool mFFmpegFailedToLoad = false;
};

} // namespace mozilla

#endif
