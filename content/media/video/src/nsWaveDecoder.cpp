/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Matthew Gregan <kinetik@flim.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "limits"
#include "prlog.h"
#include "prmem.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISeekableStream.h"
#include "nsAudioStream.h"
#include "nsAutoLock.h"
#include "nsHTMLMediaElement.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsWaveDecoder.h"

// Maximum number of seconds to wait when buffering.
#define BUFFERING_TIMEOUT 3

// The number of seconds of buffer data before buffering happens
// based on current playback rate.
#define BUFFERING_SECONDS_LOW_WATER_MARK 1

// Magic values that identify RIFF chunks we're interested in.
#define RIFF_CHUNK_MAGIC 0x52494646
#define WAVE_CHUNK_MAGIC 0x57415645
#define FRMT_CHUNK_MAGIC 0x666d7420
#define DATA_CHUNK_MAGIC 0x64617461

// Size of RIFF chunk header.  4 byte chunk header type and 4 byte size field.
#define RIFF_CHUNK_HEADER_SIZE 8

// Size of RIFF header.  RIFF chunk and 4 byte RIFF type.
#define RIFF_INITIAL_SIZE (RIFF_CHUNK_HEADER_SIZE + 4)

// Size of required part of format chunk.  Actual format chunks may be
// extended (for non-PCM encodings), but we skip any extended data.
#define WAVE_FORMAT_CHUNK_SIZE 16

// Size of format chunk including RIFF chunk header.
#define WAVE_FORMAT_SIZE (RIFF_CHUNK_HEADER_SIZE + WAVE_FORMAT_CHUNK_SIZE)

// PCM encoding type from format chunk.  Linear PCM is the only encoding
// supported by nsAudioStream.
#define WAVE_FORMAT_ENCODING_PCM 1

/*
  A single nsWaveStateMachine instance is owned by the decoder, created
   on-demand at load time.  Upon creation, the decoder immediately
   dispatches the state machine event to the decode thread to begin
   execution.  Once running, metadata loading begins immediately.  If this
   completes successfully, the state machine will move into a paused state
   awaiting further commands.  The state machine provides a small set of
   threadsafe methods enabling the main thread to play, pause, seek, and
   query parameters.

   An weak (raw) pointer to the decoder's nsMediaStream is used by the state
   machine to read data, seek, and query stream information.  The decoder is
   responsible for creating and opening the stream, and may also cancel it.
   Every other stream operation is performed on the playback thread by the
   state machine.  A cancel from the main thread will force any in-flight
   stream operations to abort.
 */
class nsWaveStateMachine : public nsRunnable
{
public:
  enum State {
    STATE_LOADING_METADATA,
    STATE_BUFFERING,
    STATE_PLAYING,
    STATE_SEEKING,
    STATE_PAUSED,
    STATE_ENDED,
    STATE_ERROR,
    STATE_SHUTDOWN
  };

  nsWaveStateMachine(nsWaveDecoder* aDecoder, nsMediaStream* aStream,
                     PRUint32 aBufferWaitTime, float aInitialVolume);
  ~nsWaveStateMachine();

  // Return current audio volume from the audio backend.  Result in range
  // [0.0, 1.0].  Threadsafe.
  float GetVolume();

  // Set specified volume.  aVolume must be in range [0.0, 1.0].
  // Threadsafe.
  void SetVolume(float aVolume);

  /*
    The following four member functions initiate the appropriate state
    transition suggested by the function name.  Threadsafe.
   */
  void Play();
  void Pause();
  void Seek(float aTime);
  void Shutdown();

  // Returns the playback length of the audio data in seconds, calculated
  // from the length extracted from the metadata.  Returns NaN if called
  // before metadata validation has completed.  Threadsafe.
  float GetDuration();

  // Returns the current playback position in the audio stream in seconds.
  // Threadsafe.
  float GetCurrentTime();

  // Returns true if the state machine is seeking.  Threadsafe.
  PRBool IsSeeking();

  // Returns true if the state machine has reached the end of playback.  Threadsafe.
  PRBool IsEnded();

  // Called by the decoder to indicate that the media stream has closed.
  // aAtEnd is true if we read to the end of the file.
  void StreamEnded(PRBool aAtEnd);

  // Main state machine loop. Runs forever, until shutdown state is reached.
  NS_IMETHOD Run();

  // Called by the decoder when the SeekStarted event runs.  This ensures
  // the current time offset of the state machine is updated to the new seek
  // position at the appropriate time.
  void UpdateTimeOffset(float aTime);

  // Called by the decoder, on the main thread.
  nsMediaDecoder::Statistics GetStatistics();

  // Called on the main thread only
  void SetTotalBytes(PRInt64 aBytes);
  // Called on the main thread
  void NotifyBytesDownloaded(PRInt64 aBytes);
  // Called on the main thread
  void NotifyDownloadSeeked(PRInt64 aOffset);
  // Called on the main thread
  void NotifyDownloadEnded(nsresult aStatus);
  // Called on any thread
  void NotifyBytesConsumed(PRInt64 aBytes);

  // Called by the main thread
  PRBool HasPendingData();

private:
  // Change the current state and wake the playback thread if it is waiting
  // on mMonitor.  Used by public member functions called from both threads,
  // so must hold mMonitor.  Threadsafe.
  void ChangeState(State aState);

  // Create and initialize audio stream using current audio parameters.
  void OpenAudioStream();

  // Shut down and dispose audio stream.
  void CloseAudioStream();

  // Read RIFF_INITIAL_SIZE from the beginning of the stream and verify that
  // the stream data is a RIFF bitstream containing WAVE data.
  PRBool LoadRIFFChunk();

  // Scan forward in the stream looking for the WAVE format chunk.  If
  // found, parse and validate required metadata, then use it to set
  // mSampleRate, mChannels, mSampleSize, and mSampleFormat.
  PRBool LoadFormatChunk();

  // Scan forward in the stream looking for the start of the PCM data.  If
  // found, record the data length and offset in mWaveLength and
  // mWavePCMOffset.
  PRBool FindDataOffset();

  // Returns the number of seconds that aBytes represents based on the
  // current audio parameters.  e.g.  176400 bytes is 1 second at 16-bit
  // stereo 44.1kHz.
  float BytesToTime(PRUint32 aBytes) const
  {
    NS_ABORT_IF_FALSE(mMetadataValid, "Requires valid metadata");
    return float(aBytes) / mSampleRate / mSampleSize;
  }

  // Returns the number of bytes that aTime represents based on the current
  // audio parameters.  e.g.  1 second is 176400 bytes at 16-bit stereo
  // 44.1kHz.
  PRUint32 TimeToBytes(float aTime) const
  {
    NS_ABORT_IF_FALSE(mMetadataValid, "Requires valid metadata");
    return PRUint32(aTime * mSampleRate * mSampleSize);
  }

  // Rounds aBytes down to the nearest complete sample.  Assumes beginning
  // of byte range is already sample aligned by caller.
  PRUint32 RoundDownToSample(PRUint32 aBytes) const
  {
    NS_ABORT_IF_FALSE(mMetadataValid, "Requires valid metadata");
    return aBytes - (aBytes % mSampleSize);
  }

  // Weak (raw) pointer to our decoder instance.  The decoder manages the
  // lifetime of the state machine object, so it is guaranteed that the
  // state machine will not outlive the decoder.  The decoder is not
  // threadsafe, so this pointer must only be used to create runnable events
  // targeted at the main thread.
  nsWaveDecoder* mDecoder;

  // Weak (raw) pointer to a media stream.  The decoder manages the lifetime
  // of the stream, so it is guaranteed that the stream will live as long as
  // the state machine.  The stream is threadsafe, but is only used on the
  // playback thread except for create, open, and cancel, which are called
  // from the main thread.
  nsMediaStream* mStream;

  // Our audio stream.  Created on demand when entering playback state.  It
  // is destroyed when seeking begins and will not be reinitialized until
  // playback resumes, so it is possible for this to be null.
  nsAutoPtr<nsAudioStream> mAudioStream;

  // Maximum time in milliseconds to spend waiting for data during buffering.
  PRUint32 mBufferingWait;

  // Maximum number of bytes to wait for during buffering.
  PRUint32 mBufferingBytes;

  // Machine time that buffering began, used with mBufferingWait to time out
  // buffering.
  PRIntervalTime mBufferingStart;

  // Maximum number of bytes mAudioStream buffers internally.  Used to
  // calculate next wakeup time after refilling audio buffers.
  PRUint32 mAudioBufferSize;

  /*
    Metadata extracted from the WAVE header.  Used to initialize the audio
    stream, and for byte<->time domain conversions.
  */

  // Number of samples per second.  Limited to range [100, 96000] in LoadFormatChunk.
  PRUint32 mSampleRate;

  // Number of channels.  Limited to range [1, 2] in LoadFormatChunk.
  PRUint32 mChannels;

  // Size of a single sample segment, which includes a sample for each
  // channel (interleaved).
  PRUint32 mSampleSize;

  // The sample format of the PCM data.
  nsAudioStream::SampleFormat mSampleFormat;

  // Size of PCM data stored in the WAVE as reported by the data chunk in
  // the media.
  PRUint32 mWaveLength;

  // Start offset of the PCM data in the media stream.  Extends mWaveLength
  // bytes.
  PRUint32 mWavePCMOffset;

  /*
    All member variables following this comment are accessed by both
    threads and must be synchronized via mMonitor.
  */
  PRMonitor* mMonitor;

  // The state to enter when the state machine loop iterates next.
  State mState;

  // A queued state transition.  This is used to record the next state
  // transition when play or pause is requested during seeking or metadata
  // loading to ensure a completed metadata load or seek returns to the most
  // recently requested state on completion.
  State mNextState;

  // Length of the current resource, or -1 if not available.
  PRInt64 mTotalBytes;
  // Current download position in the stream.
  // NOTE: because we don't have to read when we seek, there is no need
  // to track a separate "progress position" which ignores download
  // position changes due to reads servicing seeks.
  PRInt64 mDownloadPosition;
  // Current playback position in the stream.
  PRInt64 mPlaybackPosition;
  // Data needed to estimate download data rate. The channel timeline is
  // wall-clock time.
  nsMediaDecoder::ChannelStatistics mDownloadStatistics;

  // Volume that the audio backend will be initialized with.
  float mInitialVolume;

  // Time position (in seconds) to offset current time from audio stream.
  // Set when the seek started event runs and when the stream is closed
  // during shutdown.
  float mTimeOffset;

  // Set when StreamEnded has fired to indicate that we should not expect
  // any more data from mStream than what is already buffered (i.e. what
  // Available() reports).
  PRPackedBool mExpectMoreData;

  // Time position (in seconds) to seek to.  Set by Seek(float).
  float mSeekTime;

  // True once metadata has been parsed and validated. Users of mSampleRate,
  // mChannels, mSampleSize, mSampleFormat, mWaveLength, mWavePCMOffset must
  // check this flag before assuming the values are valid.
  PRPackedBool mMetadataValid;
};

nsWaveStateMachine::nsWaveStateMachine(nsWaveDecoder* aDecoder, nsMediaStream* aStream,
                                       PRUint32 aBufferWaitTime, float aInitialVolume)
  : mDecoder(aDecoder),
    mStream(aStream),
    mBufferingWait(aBufferWaitTime),
    mBufferingBytes(0),
    mBufferingStart(0),
    mAudioBufferSize(0),
    mSampleRate(0),
    mChannels(0),
    mSampleSize(0),
    mSampleFormat(nsAudioStream::FORMAT_S16_LE),
    mWaveLength(0),
    mWavePCMOffset(0),
    mMonitor(nsnull),
    mState(STATE_LOADING_METADATA),
    mNextState(STATE_PAUSED),
    mTotalBytes(-1),
    mDownloadPosition(0),
    mPlaybackPosition(0),
    mInitialVolume(aInitialVolume),
    mTimeOffset(0.0),
    mExpectMoreData(PR_TRUE),
    mSeekTime(0.0),
    mMetadataValid(PR_FALSE)
{
  mMonitor = nsAutoMonitor::NewMonitor("nsWaveStateMachine");
  mDownloadStatistics.Start(PR_IntervalNow());
}

nsWaveStateMachine::~nsWaveStateMachine()
{
  nsAutoMonitor::DestroyMonitor(mMonitor);
}

void
nsWaveStateMachine::Shutdown()
{
  ChangeState(STATE_SHUTDOWN);
}

void
nsWaveStateMachine::Play()
{
  nsAutoMonitor monitor(mMonitor);
  if (mState == STATE_LOADING_METADATA || mState == STATE_SEEKING) {
    mNextState = STATE_PLAYING;
  } else {
    ChangeState(STATE_PLAYING);
  }
}

float
nsWaveStateMachine::GetVolume()
{
  float volume = mInitialVolume;
  if (mAudioStream) {
    volume = mAudioStream->GetVolume();
  }
  return volume;
}

void
nsWaveStateMachine::SetVolume(float aVolume)
{
  nsAutoMonitor monitor(mMonitor);
  mInitialVolume = aVolume;
  if (mAudioStream) {
    mAudioStream->SetVolume(aVolume);
  }
}

void
nsWaveStateMachine::Pause()
{
  nsAutoMonitor monitor(mMonitor);
  if (mState == STATE_LOADING_METADATA || mState == STATE_SEEKING) {
    mNextState = STATE_PAUSED;
  } else {
    ChangeState(STATE_PAUSED);
  }
}

void
nsWaveStateMachine::Seek(float aTime)
{
  nsAutoMonitor monitor(mMonitor);
  mSeekTime = aTime;
  if (mSeekTime < 0.0) {
    mSeekTime = 0.0;
  }
  if (mState == STATE_LOADING_METADATA) {
    mNextState = STATE_SEEKING;
  } else if (mState != STATE_SEEKING) {
    mNextState = mState;
    ChangeState(STATE_SEEKING);
  }
}

float
nsWaveStateMachine::GetDuration()
{
  nsAutoMonitor monitor(mMonitor);
  if (mMetadataValid) {
    PRUint32 length = mWaveLength;
    // If the decoder has a valid content length, and it's shorter than the
    // expected length of the PCM data, calculate the playback duration from
    // the content length rather than the expected PCM data length.
    if (mTotalBytes >= 0 && mTotalBytes - mWavePCMOffset < length) {
      length = mTotalBytes - mWavePCMOffset;
    }
    return BytesToTime(length);
  }
  return std::numeric_limits<float>::quiet_NaN();
}

float
nsWaveStateMachine::GetCurrentTime()
{
  nsAutoMonitor monitor(mMonitor);
  double time = 0.0;
  if (mAudioStream) {
    time = mAudioStream->GetTime();
  }
  return float(time + mTimeOffset);
}

PRBool
nsWaveStateMachine::IsSeeking()
{
  nsAutoMonitor monitor(mMonitor);
  return mState == STATE_SEEKING || mNextState == STATE_SEEKING;
}

PRBool
nsWaveStateMachine::IsEnded()
{
  nsAutoMonitor monitor(mMonitor);
  return mState == STATE_ENDED || mState == STATE_SHUTDOWN;
}

void
nsWaveStateMachine::StreamEnded(PRBool aAtEnd)
{
  nsAutoMonitor monitor(mMonitor);
  mExpectMoreData = PR_FALSE;

  // If we know the content length, set the bytes downloaded to this
  // so the final progress event gets the correct final value.
  if (mTotalBytes >= 0) {
    mDownloadPosition = mTotalBytes;
  }
}

PRBool
nsWaveStateMachine::HasPendingData()
{
  nsAutoMonitor monitor(mMonitor);
  return mPlaybackPosition < mDownloadPosition;
}

NS_IMETHODIMP
nsWaveStateMachine::Run()
{
  // Monitor is held by this thread almost permanently, but must be manually
  // dropped during long operations to prevent the main thread from blocking
  // when calling methods on the state machine object.
  nsAutoMonitor monitor(mMonitor);

  for (;;) {
    switch (mState) {
    case STATE_LOADING_METADATA:
      {
        monitor.Exit();
        PRBool loaded = LoadRIFFChunk() && LoadFormatChunk() && FindDataOffset();
        monitor.Enter();

        if (mState == STATE_LOADING_METADATA) {
          nsCOMPtr<nsIRunnable> event;
          State newState;

          if (loaded) {
            mMetadataValid = PR_TRUE;
            if (mNextState != STATE_SEEKING) {
              event = NS_NEW_RUNNABLE_METHOD(nsWaveDecoder, mDecoder, MetadataLoaded);
            }
            newState = mNextState;
          } else {
            event = NS_NEW_RUNNABLE_METHOD(nsWaveDecoder, mDecoder, MediaErrorDecode);
            newState = STATE_ERROR;
          }

          if (event) {
            NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
          }
          ChangeState(newState);
        }
      }
      break;

    case STATE_BUFFERING: {
      PRIntervalTime now = PR_IntervalNow();
      if ((PR_IntervalToMilliseconds(now - mBufferingStart) < mBufferingWait) &&
          mStream->Available() < mBufferingBytes) {
        LOG(PR_LOG_DEBUG, ("Buffering data until %d bytes or %d milliseconds\n",
                           mBufferingBytes - mStream->Available(),
                           mBufferingWait - (now - mBufferingStart)));
        monitor.Wait(PR_MillisecondsToInterval(1000));
      } else {
        ChangeState(mNextState);
        nsCOMPtr<nsIRunnable> event =
          NS_NEW_RUNNABLE_METHOD(nsWaveDecoder, mDecoder, BufferingStopped);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }

      break;
    }

    case STATE_PLAYING: {
      if (!mAudioStream) {
        OpenAudioStream();
      } else {
        mAudioStream->Resume();
      }

      if (mStream->Available() < mSampleSize) {
        if (mExpectMoreData) {
          // Buffer until mBufferingWait milliseconds of data is available.
          mBufferingBytes = TimeToBytes(float(mBufferingWait) / 1000.0);
          mBufferingStart = PR_IntervalNow();
          ChangeState(STATE_BUFFERING);
        } else {
          // Media stream has ended and there is less data available than a
          // single sample so end playback.
          ChangeState(STATE_ENDED);
        }
      } else {
        // Assuming enough data is available from the network, we aim to
        // completely fill the audio backend's buffers with data.  This
        // allows us plenty of time to wake up and refill the buffers
        // without an underrun occurring.
        PRUint32 sampleSize = mSampleFormat == nsAudioStream::FORMAT_U8 ? 1 : 2;
        PRUint32 len = RoundDownToSample(NS_MIN(mStream->Available(),
                                                PRUint32(mAudioStream->Available() * sampleSize)));
        if (len) {
          nsAutoArrayPtr<char> buf(new char[len]);
          PRUint32 got = 0;

          monitor.Exit();
          if (NS_FAILED(mStream->Read(buf.get(), len, &got))) {
            NS_WARNING("Stream read failed");
          }

          if (got == 0) {
            ChangeState(STATE_ENDED);
          }

          // If we got less data than requested, go ahead and write what we
          // got to the audio hardware.  It's unlikely that this can happen
          // since we never attempt to read more data than what is already
          // buffered.
          len = RoundDownToSample(got);

          // Calculate difference between the current media stream position
          // and the expected end of the PCM data.
          PRInt64 endDelta = mWavePCMOffset + mWaveLength - mStream->Tell();
          if (endDelta < 0) {
            // Read past the end of PCM data.  Adjust len to avoid playing
            // back trailing data.
            len -= -endDelta;
            if (RoundDownToSample(len) != len) {
              NS_WARNING("PCM data does not end with complete sample");
              len = RoundDownToSample(len);
            }
            ChangeState(STATE_ENDED);
          }

          PRUint32 lengthInSamples = len;
          if (mSampleFormat == nsAudioStream::FORMAT_S16_LE) {
            lengthInSamples /= sizeof(short);
          }
          mAudioStream->Write(buf.get(), lengthInSamples);
          monitor.Enter();
        }

        // To avoid waking up too frequently to top up these buffers,
        // calculate the duration of the currently buffered data and sleep
        // until most of the buffered data has been consumed.  We can't
        // sleep for the entire duration because we might not wake up in
        // time to refill the buffers, causing an underrun.  To avoid this,
        // wake up when approximately half the buffered data has been
        // consumed.  This could be made smarter, but at least avoids waking
        // up frequently to perform small buffer refills.
        float nextWakeup = BytesToTime(mAudioBufferSize - mAudioStream->Available() * sizeof(short)) * 1000.0 / 2.0;
        monitor.Wait(PR_MillisecondsToInterval(PRUint32(nextWakeup)));
      }
      break;
    }

    case STATE_SEEKING:
      {
        CloseAudioStream();

        mSeekTime = NS_MIN(mSeekTime, GetDuration());
        float seekTime = mSeekTime;

        monitor.Exit();
        nsCOMPtr<nsIRunnable> startEvent =
          NS_NEW_RUNNABLE_METHOD(nsWaveDecoder, mDecoder, SeekingStarted);
        NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
        monitor.Enter();

        if (mState == STATE_SHUTDOWN) {
          break;
        }

        // Calculate relative offset within PCM data.
        PRInt64 position = RoundDownToSample(TimeToBytes(seekTime));
        NS_ABORT_IF_FALSE(position >= 0 && position <= mWaveLength, "Invalid seek position");

        // If position==0, instead of seeking to position+mWavePCMOffset,
        // we want to first seek to 0 before seeking to
        // position+mWavePCMOffset. This allows the request's data to come
        // from the netwerk cache (non-zero byte-range requests can't be cached
        // yet). The second seek will simply advance the read cursor, it won't
        // start a new HTTP request.
        PRBool seekToZeroFirst = position == 0 &&
                                 (mWavePCMOffset < SEEK_VS_READ_THRESHOLD);

        // Convert to absolute offset within stream.
        position += mWavePCMOffset;

        monitor.Exit();
        nsresult rv;
        if (seekToZeroFirst) {
          rv = mStream->Seek(nsISeekableStream::NS_SEEK_SET, 0);
          if (NS_FAILED(rv)) {
            NS_WARNING("Seek to zero failed");
          }
        }
        rv = mStream->Seek(nsISeekableStream::NS_SEEK_SET, position);
        if (NS_FAILED(rv)) {
          NS_WARNING("Seek failed");
        }
        monitor.Enter();

        if (mState == STATE_SHUTDOWN) {
          break;
        }

        monitor.Exit();
        nsCOMPtr<nsIRunnable> stopEvent =
          NS_NEW_RUNNABLE_METHOD(nsWaveDecoder, mDecoder, SeekingStopped);
        NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);
        monitor.Enter();

        if (mState == STATE_SEEKING && mSeekTime == seekTime) {
          // Special case: if a seek was requested during metadata load,
          // mNextState will have been clobbered.  This can only happen when
          // we're instantiating a decoder to service a seek request after
          // playback has ended, so we know that the clobbered mNextState
          // was PAUSED.
          State nextState = mNextState;
          if (nextState == STATE_SEEKING) {
            nextState = STATE_PAUSED;
          }
          ChangeState(nextState);
        }
      }
      break;

    case STATE_PAUSED:
      if (mAudioStream) {
        mAudioStream->Pause();
      }
      monitor.Wait();
      break;

    case STATE_ENDED:
      if (mAudioStream) {
        monitor.Exit();
        mAudioStream->Drain();
        monitor.Enter();
        mTimeOffset += mAudioStream->GetTime();
      }

      // Dispose the audio stream early (before SHUTDOWN) so that
      // GetCurrentTime no longer attempts to query the audio backend for
      // stream time.
      CloseAudioStream();

      if (mState != STATE_SHUTDOWN) {
        nsCOMPtr<nsIRunnable> event =
          NS_NEW_RUNNABLE_METHOD(nsWaveDecoder, mDecoder, PlaybackEnded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }

      while (mState != STATE_SHUTDOWN) {
        monitor.Wait();
      }
      break;

    case STATE_ERROR:
      monitor.Wait();
      if (mState != STATE_SHUTDOWN) {
        NS_WARNING("Invalid state transition");
        ChangeState(STATE_ERROR);
      }
      break;

    case STATE_SHUTDOWN:
      if (mAudioStream) {
        mTimeOffset += mAudioStream->GetTime();
      }
      CloseAudioStream();
      return NS_OK;
    }
  }

  return NS_OK;
}

void
nsWaveStateMachine::UpdateTimeOffset(float aTime)
{
  nsAutoMonitor monitor(mMonitor);
  mTimeOffset = NS_MIN(aTime, GetDuration());
  if (mTimeOffset < 0.0) {
    mTimeOffset = 0.0;
  }
}

void
nsWaveStateMachine::ChangeState(State aState)
{
  nsAutoMonitor monitor(mMonitor);
  mState = aState;
  monitor.NotifyAll();
}

void
nsWaveStateMachine::OpenAudioStream()
{
  mAudioStream = new nsAudioStream();
  if (!mAudioStream) {
    LOG(PR_LOG_ERROR, ("Could not create audio stream"));
  } else {
    NS_ABORT_IF_FALSE(mMetadataValid,
                      "Attempting to initialize audio stream with invalid metadata");
    mAudioStream->Init(mChannels, mSampleRate, mSampleFormat);
    mAudioStream->SetVolume(mInitialVolume);
    mAudioBufferSize = mAudioStream->Available() * sizeof(short);
  }
}

void
nsWaveStateMachine::CloseAudioStream()
{
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }
}

nsMediaDecoder::Statistics
nsWaveStateMachine::GetStatistics()
{
  nsMediaDecoder::Statistics result;
  nsAutoMonitor monitor(mMonitor);
  PRIntervalTime now = PR_IntervalNow();
  result.mDownloadRate = mDownloadStatistics.GetRate(now, &result.mDownloadRateReliable);
  result.mPlaybackRate = mSampleRate*mChannels*mSampleSize;
  result.mPlaybackRateReliable = PR_TRUE;
  result.mTotalBytes = mTotalBytes;
  result.mDownloadPosition = mDownloadPosition;
  result.mDecoderPosition = mPlaybackPosition;
  result.mPlaybackPosition = mPlaybackPosition;
  return result;
}

void
nsWaveStateMachine::SetTotalBytes(PRInt64 aBytes) {
  nsAutoMonitor monitor(mMonitor);
  mTotalBytes = aBytes;
}

void
nsWaveStateMachine::NotifyBytesDownloaded(PRInt64 aBytes)
{
  nsAutoMonitor monitor(mMonitor);
  mDownloadStatistics.AddBytes(aBytes);
  mDownloadPosition += aBytes;
}

void
nsWaveStateMachine::NotifyDownloadSeeked(PRInt64 aOffset)
{
  nsAutoMonitor monitor(mMonitor);
  mDownloadPosition = mPlaybackPosition = aOffset;
}

void
nsWaveStateMachine::NotifyDownloadEnded(nsresult aStatus)
{
  if (aStatus == NS_BINDING_ABORTED)
    return;
  nsAutoMonitor monitor(mMonitor);
  mDownloadStatistics.Stop(PR_IntervalNow());
}

void
nsWaveStateMachine::NotifyBytesConsumed(PRInt64 aBytes)
{
  nsAutoMonitor monitor(mMonitor);
  mPlaybackPosition += aBytes;
}

static PRUint32
ReadUint32BE(const char** aBuffer)
{
  PRUint32 result =
    PRUint8((*aBuffer)[0]) << 24 |
    PRUint8((*aBuffer)[1]) << 16 |
    PRUint8((*aBuffer)[2]) << 8 |
    PRUint8((*aBuffer)[3]);
  *aBuffer += sizeof(PRUint32);
  return result;
}

static PRUint32
ReadUint32LE(const char** aBuffer)
{
  PRUint32 result =
    PRUint8((*aBuffer)[3]) << 24 |
    PRUint8((*aBuffer)[2]) << 16 |
    PRUint8((*aBuffer)[1]) << 8 |
    PRUint8((*aBuffer)[0]);
  *aBuffer += sizeof(PRUint32);
  return result;
}

static PRUint16
ReadUint16LE(const char** aBuffer)
{
  PRUint16 result =
    PRUint8((*aBuffer)[1]) << 8 |
    PRUint8((*aBuffer)[0]) << 0;
  *aBuffer += sizeof(PRUint16);
  return result;
}

static PRBool
ReadAll(nsMediaStream* aStream, char* aBuf, PRUint32 aSize)
{
  PRUint32 got = 0;
  do {
    PRUint32 read = 0;
    if (NS_FAILED(aStream->Read(aBuf + got, aSize - got, &read))) {
      NS_WARNING("Stream read failed");
      return PR_FALSE;
    }
    got += read;
  } while (got != aSize);
  return PR_TRUE;
}

PRBool
nsWaveStateMachine::LoadRIFFChunk()
{
  char riffHeader[RIFF_INITIAL_SIZE];
  const char* p = riffHeader;

  NS_ABORT_IF_FALSE(mStream->Tell() == 0,
                    "LoadRIFFChunk called when stream in invalid state");

  if (!ReadAll(mStream, riffHeader, sizeof(riffHeader))) {
    return PR_FALSE;
  }

  if (ReadUint32BE(&p) != RIFF_CHUNK_MAGIC) {
    NS_WARNING("Stream data not in RIFF format");
    return PR_FALSE;
  }

  // Skip over RIFF size field.
  p += 4;

  if (ReadUint32BE(&p) != WAVE_CHUNK_MAGIC) {
    NS_WARNING("Expected WAVE chunk");
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsWaveStateMachine::LoadFormatChunk()
{
  PRUint32 rate, channels, sampleSize, sampleFormat;
  char waveFormat[WAVE_FORMAT_SIZE];
  const char* p = waveFormat;

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mStream->Tell() % 2 == 0,
                    "LoadFormatChunk called with unaligned stream");

  if (!ReadAll(mStream, waveFormat, sizeof(waveFormat))) {
    return PR_FALSE;
  }

  if (ReadUint32BE(&p) != FRMT_CHUNK_MAGIC) {
    NS_WARNING("Expected format chunk");
    return PR_FALSE;
  }

  PRUint32 fmtsize = ReadUint32LE(&p);

  if (ReadUint16LE(&p) != WAVE_FORMAT_ENCODING_PCM) {
    NS_WARNING("WAVE is not uncompressed PCM, compressed encodings are not supported");
    return PR_FALSE;
  }

  channels = ReadUint16LE(&p);
  rate = ReadUint32LE(&p);

  // Skip over average bytes per second field.
  p += 4;

  sampleSize = ReadUint16LE(&p);

  sampleFormat = ReadUint16LE(&p);

  // PCM encoded WAVEs are not expected to have an extended "format" chunk,
  // but I have found WAVEs that have a extended "format" chunk with an
  // extension size of 0 bytes.  Be polite and handle this rather than
  // considering the file invalid.  This code skips any extension of the
  // "format" chunk.
  if (fmtsize > WAVE_FORMAT_CHUNK_SIZE) {
    char extLength[2];
    const char* p = extLength;

    if (!ReadAll(mStream, extLength, sizeof(extLength))) {
      return PR_FALSE;
    }

    PRUint16 extra = ReadUint16LE(&p);
    if (fmtsize - (WAVE_FORMAT_CHUNK_SIZE + 2) != extra) {
      NS_WARNING("Invalid extended format chunk size");
      return PR_FALSE;
    }
    extra += extra % 2;

    if (extra > 0) {
      nsAutoArrayPtr<char> chunkExtension(new char[extra]);
      if (!ReadAll(mStream, chunkExtension.get(), extra)) {
        return PR_FALSE;
      }
    }
  }

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mStream->Tell() % 2 == 0,
                    "LoadFormatChunk left stream unaligned");

  // Make sure metadata is fairly sane.  The rate check is fairly arbitrary,
  // but the channels check is intentionally limited to mono or stereo
  // because that's what the audio backend currently supports.
  if (rate < 100 || rate > 96000 ||
      channels < 1 || channels > 2 ||
      (sampleSize != 1 && sampleSize != 2 && sampleSize != 4) ||
      (sampleFormat != 8 && sampleFormat != 16)) {
    NS_WARNING("Invalid WAVE metadata");
    return PR_FALSE;
  }

  nsAutoMonitor monitor(mMonitor);
  mSampleRate = rate;
  mChannels = channels;
  mSampleSize = sampleSize;
  if (sampleFormat == 8) {
    mSampleFormat = nsAudioStream::FORMAT_U8;
  } else {
    mSampleFormat = nsAudioStream::FORMAT_S16_LE;
  }
  return PR_TRUE;
}

PRBool
nsWaveStateMachine::FindDataOffset()
{
  PRUint32 length;
  PRInt64 offset;

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mStream->Tell() % 2 == 0,
                    "FindDataOffset called with unaligned stream");

  // The "data" chunk may not directly follow the "format" chunk, so skip
  // over any intermediate chunks.
  for (;;) {
    char chunkHeader[8];
    const char* p = chunkHeader;

    if (!ReadAll(mStream, chunkHeader, sizeof(chunkHeader))) {
      return PR_FALSE;
    }

    PRUint32 magic = ReadUint32BE(&p);

    if (magic == DATA_CHUNK_MAGIC) {
      length = ReadUint32LE(&p);
      break;
    }

    if (magic == FRMT_CHUNK_MAGIC) {
      LOG(PR_LOG_ERROR, ("Invalid WAVE: expected \"data\" chunk but found \"format\" chunk"));
      return PR_FALSE;
    }

    PRUint32 size = ReadUint32LE(&p);
    size += size % 2;

    nsAutoArrayPtr<char> chunk(new char[size]);
    if (!ReadAll(mStream, chunk.get(), size)) {
      return PR_FALSE;
    }
  }

  offset = mStream->Tell();
  if (!offset) {
    NS_WARNING("PCM data offset not found");
    return PR_FALSE;
  }

  if (offset < 0 || offset > PR_UINT32_MAX) {
    NS_WARNING("offset out of range");
    return PR_FALSE;
  }

  nsAutoMonitor monitor(mMonitor);
  mWaveLength = length;
  mWavePCMOffset = PRUint32(offset);
  return PR_TRUE;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsWaveDecoder, nsIObserver)

nsWaveDecoder::nsWaveDecoder()
  : mInitialVolume(1.0),
    mStream(nsnull),
    mTimeOffset(0.0),
    mEndedCurrentTime(0.0),
    mEndedDuration(std::numeric_limits<float>::quiet_NaN()),
    mEnded(PR_FALSE),
    mNotifyOnShutdown(PR_FALSE),
    mSeekable(PR_TRUE),
    mResourceLoaded(PR_FALSE),
    mMetadataLoadedReported(PR_FALSE),
    mResourceLoadedReported(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsWaveDecoder);
}

nsWaveDecoder::~nsWaveDecoder()
{
  MOZ_COUNT_DTOR(nsWaveDecoder);
}

void
nsWaveDecoder::GetCurrentURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
}

nsIPrincipal*
nsWaveDecoder::GetCurrentPrincipal()
{
  if (!mStream) {
    return nsnull;
  }
  return mStream->GetCurrentPrincipal();
}

float
nsWaveDecoder::GetCurrentTime()
{
  if (mPlaybackStateMachine) {
    return mPlaybackStateMachine->GetCurrentTime();
  }
  return mEndedCurrentTime;
}

nsresult
nsWaveDecoder::Seek(float aTime)
{
  mTimeOffset = aTime;

  if (!mPlaybackStateMachine) {
    Load(mURI, nsnull, nsnull);
  }

  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->Seek(mTimeOffset);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsWaveDecoder::PlaybackRateChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

float
nsWaveDecoder::GetDuration()
{
  if (mPlaybackStateMachine) {
    return mPlaybackStateMachine->GetDuration();
  }
  return mEndedDuration;
}

void
nsWaveDecoder::Pause()
{
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->Pause();
  }
}

float
nsWaveDecoder::GetVolume()
{
  if (!mPlaybackStateMachine) {
    return mInitialVolume;
  }
  return mPlaybackStateMachine->GetVolume();
}

void
nsWaveDecoder::SetVolume(float aVolume)
{
  mInitialVolume = aVolume;
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->SetVolume(aVolume);
  }
}

nsresult
nsWaveDecoder::Play()
{
  if (!mPlaybackStateMachine) {
    Load(mURI, nsnull, nsnull);
  }

  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->Play();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

void
nsWaveDecoder::Stop()
{
  if (mStopping) {
    return;
  }

  mStopping = PR_TRUE;

  StopProgress();

  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->Shutdown();
  }

  if (mStream) {
    mStream->Cancel();
  }

  if (mPlaybackThread) {
    mPlaybackThread->Shutdown();
  }

  if (mPlaybackStateMachine) {
    mEndedCurrentTime = mPlaybackStateMachine->GetCurrentTime();
    mEndedDuration = mPlaybackStateMachine->GetDuration();
    mEnded = mPlaybackStateMachine->IsEnded();
  }

  mPlaybackThread = nsnull;
  mPlaybackStateMachine = nsnull;
  mStream = nsnull;

  UnregisterShutdownObserver();
}

nsresult
nsWaveDecoder::Load(nsIURI* aURI, nsIChannel* aChannel, nsIStreamListener** aStreamListener)
{
  mStopping = PR_FALSE;

  // Reset progress member variables
  mResourceLoaded = PR_FALSE;

  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  if (aURI) {
    NS_ASSERTION(!aStreamListener, "No listener should be requested here");
    mURI = aURI;
  } else {
    NS_ASSERTION(aChannel, "Either a URI or a channel is required");
    NS_ASSERTION(aStreamListener, "A listener should be requested here");

    nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(mURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  RegisterShutdownObserver();

  mStream = new nsMediaStream();
  NS_ENSURE_TRUE(mStream, NS_ERROR_OUT_OF_MEMORY);

  mPlaybackStateMachine = new nsWaveStateMachine(this, mStream.get(),
                                                 BUFFERING_TIMEOUT * 1000,
                                                 mInitialVolume);
  NS_ENSURE_TRUE(mPlaybackStateMachine, NS_ERROR_OUT_OF_MEMORY);

  // Open the stream *after* setting mPlaybackStateMachine, to ensure
  // that callbacks (e.g. setting stream size) will actually work
  nsresult rv = mStream->Open(this, mURI, aChannel, aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewThread(getter_AddRefs(mPlaybackThread));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPlaybackThread->Dispatch(mPlaybackStateMachine, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsWaveDecoder::MetadataLoaded()
{
  if (mShuttingDown) {
    return;
  }

  if (mElement) {
    mElement->MetadataLoaded();
    mElement->FirstFrameLoaded();
  }

  mMetadataLoadedReported = PR_TRUE;

  if (mResourceLoaded) {
    ResourceLoaded();
  } else {
    StartProgress();
  }
}

void
nsWaveDecoder::PlaybackEnded()
{
  if (mShuttingDown) {
    return;
  }

  Stop();
  if (mElement) {
    mElement->PlaybackEnded();
  }
}

void
nsWaveDecoder::ResourceLoaded()
{
  if (mShuttingDown) {
    return;
  }
 
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->StreamEnded(PR_TRUE);
  }

  mResourceLoaded = PR_TRUE;

  if (!mMetadataLoadedReported || mResourceLoadedReported)
    return;

  StopProgress();

  if (mElement) {
    // Ensure the final progress event gets fired
    mElement->DispatchAsyncProgressEvent(NS_LITERAL_STRING("progress"));
    mElement->ResourceLoaded();
  }

  mResourceLoadedReported = PR_TRUE;
}

void
nsWaveDecoder::NetworkError()
{
  if (mShuttingDown) {
    return;
  }
  if (mElement) {
    mElement->NetworkError();
  }
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->StreamEnded(PR_FALSE);
  }
  Stop();
}

PRBool
nsWaveDecoder::IsSeeking() const
{
  if (mPlaybackStateMachine) {
    return mPlaybackStateMachine->IsSeeking();
  }
  return PR_FALSE;
}

PRBool
nsWaveDecoder::IsEnded() const
{
  if (mPlaybackStateMachine) {
    return mPlaybackStateMachine->IsEnded();
  }
  return mEnded;
}

nsMediaDecoder::Statistics
nsWaveDecoder::GetStatistics()
{
  if (!mPlaybackStateMachine)
    return Statistics();
  return mPlaybackStateMachine->GetStatistics();
}

void
nsWaveDecoder::NotifyBytesDownloaded(PRInt64 aBytes)
{
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->NotifyBytesDownloaded(aBytes);
  }
  UpdateReadyStateForData();
}

void
nsWaveDecoder::NotifyDownloadSeeked(PRInt64 aBytes)
{
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->NotifyDownloadSeeked(aBytes);
  }
}

void
nsWaveDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->NotifyDownloadEnded(aStatus);
  }
  if (aStatus != NS_BINDING_ABORTED) {
    if (NS_SUCCEEDED(aStatus)) {
      ResourceLoaded();
    } else if (aStatus != NS_BASE_STREAM_CLOSED) {
      NetworkError();
    }
  }
  UpdateReadyStateForData();
}

void
nsWaveDecoder::NotifyBytesConsumed(PRInt64 aBytes)
{
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->NotifyBytesConsumed(aBytes);
  }
}

void
nsWaveDecoder::SetTotalBytes(PRInt64 aBytes)
{
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->SetTotalBytes(aBytes);
  } else {
    NS_WARNING("Forgot total bytes since there is no state machine set up");
  }
}

// An event that gets posted to the main thread, when the media element is
// being destroyed, to destroy the decoder. Since the decoder shutdown can
// block and post events this cannot be done inside destructor calls. So
// this event is posted asynchronously to the main thread to perform the
// shutdown. It keeps a strong reference to the decoder to ensure it does
// not get deleted when the element is deleted.
class nsWaveDecoderShutdown : public nsRunnable
{
public:
  nsWaveDecoderShutdown(nsWaveDecoder* aDecoder)
    : mDecoder(aDecoder)
  {
  }

  NS_IMETHOD Run()
  {
    mDecoder->Stop();
    return NS_OK;
  }

private:
  nsRefPtr<nsWaveDecoder> mDecoder;
};

void
nsWaveDecoder::Shutdown()
{
  mShuttingDown = PR_TRUE;

  nsMediaDecoder::Shutdown();

  nsCOMPtr<nsIRunnable> event = new nsWaveDecoderShutdown(this);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

nsresult
nsWaveDecoder::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }
  return NS_OK;
}

void
nsWaveDecoder::UpdateReadyStateForData()
{
  if (!mElement || mShuttingDown || !mPlaybackStateMachine)
    return;

  PRBool haveDataToPlay =
    mPlaybackStateMachine->HasPendingData() && mMetadataLoadedReported;
  mElement->UpdateReadyStateForData(haveDataToPlay);
}

void
nsWaveDecoder::BufferingStopped()
{
  UpdateReadyStateForData();
}

void
nsWaveDecoder::SeekingStarted()
{
  if (mShuttingDown) {
    return;
  }

  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->UpdateTimeOffset(mTimeOffset);
  }

  if (mElement) {
    mElement->SeekStarted();
  }
}

void
nsWaveDecoder::SeekingStopped()
{
  if (mShuttingDown) {
    return;
  }

  if (mElement) {
    mElement->SeekCompleted();
    UpdateReadyStateForData();
  }
}

void
nsWaveDecoder::RegisterShutdownObserver()
{
  if (!mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown =
        NS_SUCCEEDED(observerService->AddObserver(this,
                                                  NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                                  PR_FALSE));
    } else {
      NS_WARNING("Could not get an observer service. Audio playback may not shutdown cleanly.");
    }
  }
}

void
nsWaveDecoder::UnregisterShutdownObserver()
{
  if (mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown = PR_FALSE;
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
  }
}

void
nsWaveDecoder::MediaErrorDecode()
{
  if (mShuttingDown) {
    return;
  }
#if 0
  if (mElement) {
    mElement->MediaErrorDecode();
  }
#else
  NS_WARNING("MediaErrorDecode fired, but not implemented.");
#endif
}

void
nsWaveDecoder::SetDuration(PRInt64 /* aDuration */)
{
  // Ignored by the wave decoder since we can compute the
  // duration directly from the wave data itself.
}

void
nsWaveDecoder::SetSeekable(PRBool aSeekable)
{
  mSeekable = aSeekable;
}

PRBool
nsWaveDecoder::GetSeekable()
{
  return mSeekable;
}

void
nsWaveDecoder::Suspend()
{
  if (mStream) {
    mStream->Suspend();
  }
}

void
nsWaveDecoder::Resume()
{
  if (mStream) {
    mStream->Resume();
  }
}
