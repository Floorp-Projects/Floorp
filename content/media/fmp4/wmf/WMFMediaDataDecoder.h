/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFMediaDataDecoder_h_)
#define WMFMediaDataDecoder_h_


#include "WMF.h"
#include "MP4Reader.h"
#include "MFTDecoder.h"
#include "mozilla/RefPtr.h"

class mp4_demuxer::MP4Sample;

namespace mozilla {

// Encapsulates the initialization of the MFTDecoder appropriate for decoding
// a given stream, and the process of converting the IMFSample produced
// by the MFT into a MediaData object.
class WMFOutputSource {
public:
  virtual ~WMFOutputSource() {}

  // Creates an initializs the MFTDecoder.
  // Returns nullptr on failure.
  virtual TemporaryRef<MFTDecoder> Init() = 0;

  // Submit a compressed sample for decoding.
  // This should forward to the MFTDecoder after performing
  // any required sample formatting.
  virtual HRESULT Input(mp4_demuxer::MP4Sample* aSample) = 0;

  // Produces decoded output, if possible. Blocks until output can be produced,
  // or until no more is able to be produced.
  // Returns S_OK on success, or MF_E_TRANSFORM_NEED_MORE_INPUT if there's not
  // enough data to produce more output. If this returns a failure code other
  // than MF_E_TRANSFORM_NEED_MORE_INPUT, an error will be reported to the
  // MP4Reader.
  virtual HRESULT Output(int64_t aStreamOffset,
                         nsAutoPtr<MediaData>& aOutput) = 0;
};

// Decodes audio and video using Windows Media Foundation. Samples are decoded
// using the MFTDecoder created by the WMFOutputSource. This class implements
// the higher-level logic that drives mapping the MFT to the async
// MediaDataDecoder interface. The specifics of decoding the exact stream
// type are handled by WMFOutputSource and the MFTDecoder it creates.
class WMFMediaDataDecoder : public MediaDataDecoder {
public:
  WMFMediaDataDecoder(WMFOutputSource* aOutputSource,
                      MediaTaskQueue* aAudioTaskQueue,
                      MediaDataDecoderCallback* aCallback);
  ~WMFMediaDataDecoder();

  virtual nsresult Init() MOZ_OVERRIDE;

  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample);

  virtual nsresult Flush() MOZ_OVERRIDE;

  virtual nsresult Drain() MOZ_OVERRIDE;

  virtual nsresult Shutdown() MOZ_OVERRIDE;

private:

  // Called on the task queue. Inserts the sample into the decoder, and
  // extracts output if available.
  void ProcessDecode(mp4_demuxer::MP4Sample* aSample);

  // Called on the task queue. Extracts output if available, and delivers
  // it to the reader. Called after ProcessDecode() and ProcessDrain().
  void ProcessOutput();

  // Called on the task queue. Orders the MFT to drain, and then extracts
  // all available output.
  void ProcessDrain();

  RefPtr<MediaTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;

  RefPtr<MFTDecoder> mDecoder;
  nsAutoPtr<WMFOutputSource> mSource;

  // The last offset into the media resource that was passed into Input().
  // This is used to approximate the decoder's position in the media resource.
  int64_t mLastStreamOffset;
};

} // namespace mozilla

#endif // WMFMediaDataDecoder_h_
