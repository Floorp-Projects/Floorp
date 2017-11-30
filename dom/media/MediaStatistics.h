/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStatistics_h_
#define MediaStatistics_h_

namespace mozilla {

struct MediaStatistics {
  // Estimate of the current playback rate (bytes/second).
  double mPlaybackRate;
  // Estimate of the current download rate (bytes/second). This
  // ignores time that the channel was paused by Gecko.
  double mDownloadRate;
  // Total length of media stream in bytes; -1 if not known
  int64_t mTotalBytes;
  // Current position of the download, in bytes. This is the offset of
  // the first uncached byte after the decoder position.
  int64_t mDownloadPosition;
  // Current position of playback, in bytes
  int64_t mPlaybackPosition;
  // If false, then mDownloadRate cannot be considered a reliable
  // estimate (probably because the download has only been running
  // a short time).
  bool mDownloadRateReliable;
  // If false, then mPlaybackRate cannot be considered a reliable
  // estimate (probably because playback has only been running
  // a short time).
  bool mPlaybackRateReliable;

  bool CanPlayThrough()
  {
    // Number of estimated seconds worth of data we need to have buffered
    // ahead of the current playback position before we allow the media decoder
    // to report that it can play through the entire media without the decode
    // catching up with the download. Having this margin make the
    // CanPlayThrough() calculation more stable in the case of
    // fluctuating bitrates.
    static const int64_t CAN_PLAY_THROUGH_MARGIN = 1;

    if ((mTotalBytes < 0 && mDownloadRateReliable) ||
        (mTotalBytes >= 0 && mTotalBytes == mDownloadPosition)) {
      return true;
    }

    if (!mDownloadRateReliable || !mPlaybackRateReliable) {
      return false;
    }

    int64_t bytesToDownload = mTotalBytes - mDownloadPosition;
    int64_t bytesToPlayback = mTotalBytes - mPlaybackPosition;
    double timeToDownload = bytesToDownload / mDownloadRate;
    double timeToPlay = bytesToPlayback / mPlaybackRate;

    if (timeToDownload > timeToPlay) {
      // Estimated time to download is greater than the estimated time to play.
      // We probably can't play through without having to stop to buffer.
      return false;
    }

    // Estimated time to download is less than the estimated time to play.
    // We can probably play through without having to buffer, but ensure that
    // we've got a reasonable amount of data buffered after the current
    // playback position, so that if the bitrate of the media fluctuates, or if
    // our download rate or decode rate estimation is otherwise inaccurate,
    // we don't suddenly discover that we need to buffer. This is particularly
    // required near the start of the media, when not much data is downloaded.
    int64_t readAheadMargin =
      static_cast<int64_t>(mPlaybackRate * CAN_PLAY_THROUGH_MARGIN);
    return mDownloadPosition > mPlaybackPosition + readAheadMargin;
  }
};

} // namespace mozilla

#endif // MediaStatistics_h_
