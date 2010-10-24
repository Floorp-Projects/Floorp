/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
#include "nsISeekableStream.h"
#include "nsAudioStream.h"
#include "nsAutoLock.h"
#include "nsHTMLMediaElement.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsWaveDecoder.h"
#include "nsTimeRanges.h"

using mozilla::TimeDuration;
using mozilla::TimeStamp;

#ifdef PR_LOGGING
static PRLogModuleInfo* gWaveDecoderLog;
#define LOG(type, msg) PR_LOG(gWaveDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

// Maximum number of seconds to wait when buffering.
#define BUFFERING_TIMEOUT 3

// Duration the playback loop will sleep after refilling the backend's audio
// buffers.  The loop's goal is to keep AUDIO_BUFFER_LENGTH milliseconds of
// audio buffered to allow time to refill before the backend underruns.
// Should be a multiple of 10 to deal with poor timer granularity on some
// platforms.
#define AUDIO_BUFFER_WAKEUP 100
#define AUDIO_BUFFER_LENGTH (2 * AUDIO_BUFFER_WAKEUP)

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

// PCM encoding type from format chunk.  Linear PCM is the only encoding
// supported by nsAudioStream.
#define WAVE_FORMAT_ENCODING_PCM 1

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
  nsWaveStateMachine(nsWaveDecoder* aDecoder,
                     TimeDuration aBufferWaitTime, float aInitialVolume);
  ~nsWaveStateMachine();

  void SetStream(nsMediaStream* aStream) { mStream = aStream; }

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

  // Returns the number of channels extracted from the metadata.  Returns 0
  // if called before metadata validation has completed.  Threadsafe.
  PRUint32 GetChannels();

  // Returns the audio sample rate (number of samples per second) extracted
  // from the metadata.  Returns 0 if called before metadata validation has
  // completed.  Threadsafe.
  PRUint32 GetSampleRate();

  // Returns true if the state machine is seeking.  Threadsafe.
  PRBool IsSeeking();

  // Returns true if the state machine has reached the end of playback.  Threadsafe.
  PRBool IsEnded();

  // Main state machine loop. Runs forever, until shutdown state is reached.
  NS_IMETHOD Run();

  // Called by the decoder, on the main thread.
  nsMediaDecoder::Statistics GetStatistics();

  // Called on the decoder thread
  void NotifyBytesConsumed(PRInt64 aBytes);

  // Called by decoder and main thread.
  nsHTMLMediaElement::NextFrameStatus GetNextFrameStatus();

  // Clear the flag indicating that a playback position change event is
  // currently queued and return the current time. This is called from the
  // main thread.
  float GetTimeForPositionChange();

  nsresult GetBuffered(nsTimeRanges* aBuffered);

private:
  // Returns PR_TRUE if we're in shutdown state. Threadsafe.
  PRBool IsShutdown();

  // Reads from the media stream. Returns PR_FALSE on failure or EOF.  If
  // aBytesRead is non-null, the number of bytes read will be returned via
  // this.
  PRBool ReadAll(char* aBuf, PRInt64 aSize, PRInt64* aBytesRead);

  void UpdateReadyState() {
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

    nsCOMPtr<nsIRunnable> event;
    switch (GetNextFrameStatus()) {
      case nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING:
        event = NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::NextFrameUnavailableBuffering);
        break;
      case nsHTMLMediaElement::NEXT_FRAME_AVAILABLE:
        event = NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::NextFrameAvailable);
        break;
      case nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE:
        event = NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::NextFrameUnavailable);
        break;
      default:
        PR_NOT_REACHED("unhandled frame state");
    }

    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }

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

  // Read forward in the stream until aWantedChunk is found.  Return chunk
  // size in aChunkSize.  aChunkSize will not be rounded up if the chunk
  // size is odd.
  PRBool ScanForwardUntil(PRUint32 aWantedChunk, PRUint32* aChunkSize);

  // Scan forward in the stream looking for the WAVE format chunk.  If
  // found, parse and validate required metadata, then use it to set
  // mSampleRate, mChannels, mSampleSize, and mSampleFormat.
  PRBool LoadFormatChunk();

  // Scan forward in the stream looking for the start of the PCM data.  If
  // found, record the data length and offset in mWaveLength and
  // mWavePCMOffset.
  PRBool FindDataOffset();

  // Return the length of the PCM data.
  PRInt64 GetDataLength();

  // Fire a PlaybackPositionChanged event.  If aCoalesce is true and a
  // PlaybackPositionChanged event is already pending, an event is not
  // fired.
  void FirePositionChanged(PRBool aCoalesce);

  // Returns the number of seconds that aBytes represents based on the
  // current audio parameters.  e.g.  176400 bytes is 1 second at 16-bit
  // stereo 44.1kHz.
  float BytesToTime(PRInt64 aBytes) const
  {
    NS_ABORT_IF_FALSE(mMetadataValid, "Requires valid metadata");
    NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
    return float(aBytes) / mSampleRate / mSampleSize;
  }

  // Returns the number of bytes that aTime represents based on the current
  // audio parameters.  e.g.  1 second is 176400 bytes at 16-bit stereo
  // 44.1kHz.
  PRInt64 TimeToBytes(float aTime) const
  {
    NS_ABORT_IF_FALSE(mMetadataValid, "Requires valid metadata");
    NS_ABORT_IF_FALSE(aTime >= 0.0f, "Must be >= 0");
    return RoundDownToSample(PRInt64(aTime * mSampleRate * mSampleSize));
  }

  // Rounds aBytes down to the nearest complete sample.  Assumes beginning
  // of byte range is already sample aligned by caller.
  PRInt64 RoundDownToSample(PRInt64 aBytes) const
  {
    NS_ABORT_IF_FALSE(mMetadataValid, "Requires valid metadata");
    NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
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

  // Maximum time to spend waiting for data during buffering.
  TimeDuration mBufferingWait;

  // Machine time that buffering began, used with mBufferingWait to time out
  // buffering.
  TimeStamp mBufferingStart;

  // Download position where we should stop buffering.  Only accessed
  // in the decoder thread.
  PRInt64 mBufferingEndOffset;

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
  PRInt64 mWaveLength;

  // Start offset of the PCM data in the media stream.  Extends mWaveLength
  // bytes.
  PRInt64 mWavePCMOffset;

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

  // Current playback position in the stream.
  PRInt64 mPlaybackPosition;

  // Volume that the audio backend will be initialized with.
  float mInitialVolume;

  // Time position (in seconds) to seek to.  Set by Seek(float).
  float mSeekTime;

  // True once metadata has been parsed and validated. Users of mSampleRate,
  // mChannels, mSampleSize, mSampleFormat, mWaveLength, mWavePCMOffset must
  // check this flag before assuming the values are valid.
  PRPackedBool mMetadataValid;

  // True if an event to notify about a change in the playback position has
  // been queued, but not yet run.  It is set to false when the event is
  // run.  This allows coalescing of these events as they can be produced
  // many times per second.
  PRPackedBool mPositionChangeQueued;

  // True if paused.  Tracks only the play/paused state.
  PRPackedBool mPaused;
};

nsWaveStateMachine::nsWaveStateMachine(nsWaveDecoder* aDecoder,
                                       TimeDuration aBufferWaitTime,
                                       float aInitialVolume)
  : mDecoder(aDecoder),
    mStream(nsnull),
    mBufferingWait(aBufferWaitTime),
    mBufferingStart(),
    mBufferingEndOffset(0),
    mSampleRate(0),
    mChannels(0),
    mSampleSize(0),
    mSampleFormat(nsAudioStream::FORMAT_S16_LE),
    mWaveLength(0),
    mWavePCMOffset(0),
    mMonitor(nsnull),
    mState(STATE_LOADING_METADATA),
    mNextState(STATE_PAUSED),
    mPlaybackPosition(0),
    mInitialVolume(aInitialVolume),
    mSeekTime(0.0f),
    mMetadataValid(PR_FALSE),
    mPositionChangeQueued(PR_FALSE),
    mPaused(mNextState == STATE_PAUSED)
{
  mMonitor = nsAutoMonitor::NewMonitor("nsWaveStateMachine");
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
  mPaused = PR_FALSE;
  if (mState == STATE_ENDED) {
    Seek(0);
    return;
  }
  if (mState == STATE_LOADING_METADATA || mState == STATE_SEEKING) {
    mNextState = STATE_PLAYING;
  } else {
    ChangeState(STATE_PLAYING);
  }
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
  mPaused = PR_TRUE;
  if (mState == STATE_LOADING_METADATA || mState == STATE_SEEKING ||
      mState == STATE_BUFFERING || mState == STATE_ENDED) {
    mNextState = STATE_PAUSED;
  } else if (mState == STATE_PLAYING) {
    ChangeState(STATE_PAUSED);
  }
}

void
nsWaveStateMachine::Seek(float aTime)
{
  nsAutoMonitor monitor(mMonitor);
  mSeekTime = aTime;
  if (mSeekTime < 0.0f) {
    mSeekTime = 0.0f;
  }
  if (mState == STATE_LOADING_METADATA) {
    mNextState = STATE_SEEKING;
  } else if (mState != STATE_SEEKING) {
    if (mState == STATE_ENDED) {
      mNextState = mPaused ? STATE_PAUSED : STATE_PLAYING;
    } else {
      mNextState = mState;
    }
    ChangeState(STATE_SEEKING);
  }
}

float
nsWaveStateMachine::GetDuration()
{
  nsAutoMonitor monitor(mMonitor);
  if (mMetadataValid) {
    return BytesToTime(GetDataLength());
  }
  return std::numeric_limits<float>::quiet_NaN();
}

PRUint32
nsWaveStateMachine::GetChannels()
{
  nsAutoMonitor monitor(mMonitor);
  if (mMetadataValid) {
    return mChannels;
  }
  return 0;
}

PRUint32
nsWaveStateMachine::GetSampleRate()
{
  nsAutoMonitor monitor(mMonitor);
  if (mMetadataValid) {
    return mSampleRate;
  }
  return 0;
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

nsHTMLMediaElement::NextFrameStatus
nsWaveStateMachine::GetNextFrameStatus()
{
  nsAutoMonitor monitor(mMonitor);
  if (mState == STATE_BUFFERING)
    return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING;
  // If mMetadataValid is false then we can't call GetDataLength because
  // we haven't got the length from the Wave header yet. But we know that
  // if we haven't read the metadata then we don't have playable data.
  if (mMetadataValid &&
      mPlaybackPosition < mStream->GetCachedDataEnd(mPlaybackPosition) &&
      mPlaybackPosition < mWavePCMOffset + GetDataLength())
    return nsHTMLMediaElement::NEXT_FRAME_AVAILABLE;
  return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE;
}

float
nsWaveStateMachine::GetTimeForPositionChange()
{
  nsAutoMonitor monitor(mMonitor);
  mPositionChangeQueued = PR_FALSE;
  return BytesToTime(mPlaybackPosition - mWavePCMOffset);
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

        if (!loaded) {
          ChangeState(STATE_ERROR);
        }

        if (mState == STATE_LOADING_METADATA) {
          mMetadataValid = PR_TRUE;
          if (mNextState != STATE_SEEKING) {
            nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::MetadataLoaded);
            NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
          }
          ChangeState(mNextState);
        }
      }
      break;

    case STATE_BUFFERING: {
      TimeStamp now = TimeStamp::Now();
      if (now - mBufferingStart < mBufferingWait &&
          mStream->GetCachedDataEnd(mPlaybackPosition) < mBufferingEndOffset &&
          !mStream->IsDataCachedToEndOfStream(mPlaybackPosition) &&
          !mStream->IsSuspendedByCache()) {
        LOG(PR_LOG_DEBUG,
            ("In buffering: buffering data until %d bytes available or %f seconds\n",
             PRUint32(mBufferingEndOffset - mStream->GetCachedDataEnd(mPlaybackPosition)),
             (mBufferingWait - (now - mBufferingStart)).ToSeconds()));
        monitor.Wait(PR_MillisecondsToInterval(1000));
      } else {
        ChangeState(mNextState);
        UpdateReadyState();
      }

      break;
    }

    case STATE_PLAYING: {
      if (!mAudioStream) {
        OpenAudioStream();
        if (!mAudioStream) {
          ChangeState(STATE_ERROR);
          break;
        }
      }

      TimeStamp now = TimeStamp::Now();
      TimeStamp lastWakeup = now -
        TimeDuration::FromMilliseconds(AUDIO_BUFFER_LENGTH);

      do {
        TimeDuration sleepTime = now - lastWakeup;
        lastWakeup = now;

        // We aim to have AUDIO_BUFFER_LENGTH milliseconds of audio
        // buffered, but only sleep for AUDIO_BUFFER_WAKEUP milliseconds
        // (waking early to refill before the backend underruns).  Since we
        // wake early, we only buffer sleepTime milliseconds of audio since
        // there is still AUDIO_BUFFER_LENGTH - sleepTime milliseconds of
        // audio buffered.
        TimeDuration targetTime =
          TimeDuration::FromMilliseconds(AUDIO_BUFFER_LENGTH);
        if (sleepTime < targetTime) {
          targetTime = sleepTime;
        }

        PRInt64 len = TimeToBytes(float(targetTime.ToSeconds()));

        PRInt64 leftToPlay =
          GetDataLength() - (mPlaybackPosition - mWavePCMOffset);
        if (leftToPlay <= len) {
          len = leftToPlay;
          ChangeState(STATE_ENDED);
        }

        PRInt64 availableOffset = mStream->GetCachedDataEnd(mPlaybackPosition);

        // don't buffer if we're at the end of the stream, or if the
        // load has been suspended by the cache (in the latter case
        // we need to advance playback to free up cache space)
        if (mState != STATE_ENDED &&
            availableOffset < mPlaybackPosition + len &&
            !mStream->IsSuspendedByCache()) {
          mBufferingStart = now;
          mBufferingEndOffset = mPlaybackPosition +
            TimeToBytes(float(mBufferingWait.ToSeconds()));
          mBufferingEndOffset = PR_MAX(mPlaybackPosition + len,
                                       mBufferingEndOffset);
          mNextState = mState;
          ChangeState(STATE_BUFFERING);

          UpdateReadyState();
          break;
        }

        if (len > 0) {
          nsAutoArrayPtr<char> buf(new char[size_t(len)]);
          PRInt64 got = 0;

          monitor.Exit();
          PRBool ok = ReadAll(buf.get(), len, &got);
          monitor.Enter();

          // Reached EOF.
          if (!ok) {
            ChangeState(STATE_ENDED);
            if (got == 0) {
              break;
            }
          }

          // Calculate difference between the current media stream position
          // and the expected end of the PCM data.
          PRInt64 endDelta = mWavePCMOffset + mWaveLength - mPlaybackPosition;
          if (endDelta < 0) {
            // Read past the end of PCM data.  Adjust got to avoid playing
            // back trailing data.
            got -= -endDelta;
            ChangeState(STATE_ENDED);
          }

          if (mState == STATE_ENDED) {
            got = RoundDownToSample(got);
          }

          PRUint32 sampleSize = mSampleFormat == nsAudioStream::FORMAT_U8 ? 1 : 2;
          NS_ABORT_IF_FALSE(got % sampleSize == 0, "Must write complete samples");
          PRUint32 lengthInSamples = PRUint32(got / sampleSize);

          monitor.Exit();
          mAudioStream->Write(buf.get(), lengthInSamples, PR_FALSE);
          monitor.Enter();

          FirePositionChanged(PR_FALSE);
        }

        if (mState == STATE_PLAYING) {
          monitor.Wait(PR_MillisecondsToInterval(AUDIO_BUFFER_WAKEUP));
          now = TimeStamp::Now();
        }
      } while (mState == STATE_PLAYING);
      break;
    }

    case STATE_SEEKING:
      {
        CloseAudioStream();

        mSeekTime = NS_MIN(mSeekTime, GetDuration());
        float seekTime = mSeekTime;

        // Calculate relative offset within PCM data.
        PRInt64 position = RoundDownToSample(TimeToBytes(seekTime));
        NS_ABORT_IF_FALSE(position >= 0 && position <= GetDataLength(),
                          "Invalid seek position");
        // Convert to absolute offset within stream.
        position += mWavePCMOffset;

        // If in the midst of a seek, report the requested seek time
        // as the current time as required by step 8 of 4.8.10.9 'Seeking'
        // in the WHATWG spec.
        PRInt64 oldPosition = mPlaybackPosition;
        mPlaybackPosition = position;
        FirePositionChanged(PR_TRUE);

        monitor.Exit();
        nsCOMPtr<nsIRunnable> startEvent =
          NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::SeekingStarted);
        NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
        monitor.Enter();

        if (mState == STATE_SHUTDOWN) {
          break;
        }

        monitor.Exit();
        nsresult rv;
        rv = mStream->Seek(nsISeekableStream::NS_SEEK_SET, position);
        monitor.Enter();
        if (NS_FAILED(rv)) {
          NS_WARNING("Seek failed");
          mPlaybackPosition = oldPosition;
          FirePositionChanged(PR_TRUE);
        }

        if (mState == STATE_SHUTDOWN) {
          break;
        }

        if (mState == STATE_SEEKING && mSeekTime == seekTime) {
          // Special case #1: if a seek was requested during metadata load,
          // mNextState will have been clobbered.  This can only happen when
          // we're instantiating a decoder to service a seek request after
          // playback has ended, so we know that the clobbered mNextState
          // was PAUSED.
          // Special case #2: if a seek is requested after the state machine
          // entered STATE_ENDED but before the user has seen the ended
          // event, playback has not ended as far as the user's
          // concerned--the state machine needs to return to the last
          // playback state.
          // Special case #3: if seeking to the end of the media, transition
          // directly into STATE_ENDED.
          State nextState = mNextState;
          if (nextState == STATE_SEEKING) {
            nextState = STATE_PAUSED;
          } else if (nextState == STATE_ENDED) {
            nextState = mPaused ? STATE_PAUSED : STATE_PLAYING;
          } else if (GetDuration() == seekTime) {
            nextState = STATE_ENDED;
          }
          ChangeState(nextState);
        }

        monitor.Exit();
        nsCOMPtr<nsIRunnable> stopEvent =
          NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::SeekingStopped);
        NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);
        monitor.Enter();
      }
      break;

    case STATE_PAUSED:
      monitor.Wait();
      break;

    case STATE_ENDED:
      FirePositionChanged(PR_TRUE);

      if (mAudioStream) {
        monitor.Exit();
        mAudioStream->Drain();
        monitor.Enter();

        // After the drain call the audio stream is unusable. Close it so that
        // next time audio is used a new stream is created.
        CloseAudioStream();
      }

      if (mState == STATE_ENDED) {
        nsCOMPtr<nsIRunnable> event =
          NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::PlaybackEnded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

        // We've finished playback. Shutdown the state machine thread, 
        // in order to save memory on thread stacks, particuarly on Linux.
        event = new ShutdownThreadEvent(mDecoder->mPlaybackThread);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        mDecoder->mPlaybackThread = nsnull;
        return NS_OK;
      }
      break;

    case STATE_ERROR:
      {
        nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::DecodeError);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

        monitor.Wait();

        if (mState != STATE_SHUTDOWN) {
          NS_WARNING("Invalid state transition");
          ChangeState(STATE_ERROR);
        }
      }
      break;

    case STATE_SHUTDOWN:
      CloseAudioStream();
      return NS_OK;
    }
  }

  return NS_OK;
}

#if defined(DEBUG)
static PRBool
IsValidStateTransition(State aStartState, State aEndState)
{
  if (aEndState == STATE_SHUTDOWN) {
    return PR_TRUE;
  }

  if (aStartState == aEndState) {
    LOG(PR_LOG_WARNING, ("Transition to current state requested"));
    return PR_TRUE;
  }

  switch (aStartState) {
  case STATE_LOADING_METADATA:
    if (aEndState == STATE_PLAYING || aEndState == STATE_SEEKING ||
        aEndState == STATE_PAUSED || aEndState == STATE_ERROR)
      return PR_TRUE;
    break;
  case STATE_BUFFERING:
    if (aEndState == STATE_PLAYING || aEndState == STATE_PAUSED ||
        aEndState == STATE_SEEKING)
      return PR_TRUE;
    break;
  case STATE_PLAYING:
    if (aEndState == STATE_BUFFERING || aEndState == STATE_SEEKING ||
        aEndState == STATE_ENDED || aEndState == STATE_PAUSED)
      return PR_TRUE;
    break;
  case STATE_SEEKING:
    if (aEndState == STATE_PLAYING || aEndState == STATE_PAUSED ||
        aEndState == STATE_ENDED)
      return PR_TRUE;
    break;
  case STATE_PAUSED:
    if (aEndState == STATE_PLAYING || aEndState == STATE_SEEKING)
      return PR_TRUE;
    break;
  case STATE_ENDED:
    if (aEndState == STATE_SEEKING)
      return PR_TRUE;
    /* fallthrough */
  case STATE_ERROR:
  case STATE_SHUTDOWN:
    break;
  }

  LOG(PR_LOG_ERROR, ("Invalid state transition from %d to %d", aStartState, aEndState));
  return PR_FALSE;
}
#endif

void
nsWaveStateMachine::ChangeState(State aState)
{
  nsAutoMonitor monitor(mMonitor);
  if (mState == STATE_SHUTDOWN) {
    LOG(PR_LOG_WARNING, ("In shutdown, state transition ignored"));
    return;
  }
#if defined(DEBUG)
  NS_ABORT_IF_FALSE(IsValidStateTransition(mState, aState), "Invalid state transition");
#endif
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
  result.mDownloadRate = mStream->GetDownloadRate(&result.mDownloadRateReliable);
  result.mPlaybackRate = mSampleRate*mChannels*mSampleSize;
  result.mPlaybackRateReliable = PR_TRUE;
  result.mTotalBytes = mStream->GetLength();
  result.mDownloadPosition = mStream->GetCachedDataEnd(mPlaybackPosition);
  result.mDecoderPosition = mPlaybackPosition;
  result.mPlaybackPosition = mPlaybackPosition;
  return result;
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

PRBool
nsWaveStateMachine::IsShutdown()
{
  nsAutoMonitor monitor(mMonitor);
  return mState == STATE_SHUTDOWN;
}

PRBool
nsWaveStateMachine::ReadAll(char* aBuf, PRInt64 aSize, PRInt64* aBytesRead = nsnull)
{
  PRUint32 got = 0;
  if (aBytesRead) {
    *aBytesRead = 0;
  }
  do {
    PRUint32 read = 0;
    if (NS_FAILED(mStream->Read(aBuf + got, PRUint32(aSize - got), &read))) {
      NS_WARNING("Stream read failed");
      return PR_FALSE;
    }
    if (IsShutdown() || read == 0) {
      return PR_FALSE;
    }
    NotifyBytesConsumed(read);
    got += read;
    if (aBytesRead) {
      *aBytesRead = got;
    }
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

  if (!ReadAll(riffHeader, sizeof(riffHeader))) {
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
nsWaveStateMachine::ScanForwardUntil(PRUint32 aWantedChunk, PRUint32* aChunkSize)
{
  NS_ABORT_IF_FALSE(aChunkSize, "Require aChunkSize argument");
  *aChunkSize = 0;

  for (;;) {
    char chunkHeader[8];
    const char* p = chunkHeader;

    if (!ReadAll(chunkHeader, sizeof(chunkHeader))) {
      return PR_FALSE;
    }

    PRUint32 magic = ReadUint32BE(&p);
    PRUint32 chunkSize = ReadUint32LE(&p);

    if (magic == aWantedChunk) {
      *aChunkSize = chunkSize;
      return PR_TRUE;
    }

    // RIFF chunks are two-byte aligned, so round up if necessary.
    chunkSize += chunkSize % 2;

    while (chunkSize > 0) {
      PRUint32 size = PR_MIN(chunkSize, 1 << 16);
      nsAutoArrayPtr<char> chunk(new char[size]);
      if (!ReadAll(chunk.get(), size)) {
        return PR_FALSE;
      }
      chunkSize -= size;
    }
  }
}

PRBool
nsWaveStateMachine::LoadFormatChunk()
{
  PRUint32 fmtSize, rate, channels, sampleSize, sampleFormat;
  char waveFormat[WAVE_FORMAT_CHUNK_SIZE];
  const char* p = waveFormat;

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mStream->Tell() % 2 == 0,
                    "LoadFormatChunk called with unaligned stream");

  // The "format" chunk may not directly follow the "riff" chunk, so skip
  // over any intermediate chunks.
  if (!ScanForwardUntil(FRMT_CHUNK_MAGIC, &fmtSize)) {
      return PR_FALSE;
  }

  if (!ReadAll(waveFormat, sizeof(waveFormat))) {
    return PR_FALSE;
  }

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
  if (fmtSize > WAVE_FORMAT_CHUNK_SIZE) {
    char extLength[2];
    const char* p = extLength;

    if (!ReadAll(extLength, sizeof(extLength))) {
      return PR_FALSE;
    }

    PRUint16 extra = ReadUint16LE(&p);
    if (fmtSize - (WAVE_FORMAT_CHUNK_SIZE + 2) != extra) {
      NS_WARNING("Invalid extended format chunk size");
      return PR_FALSE;
    }
    extra += extra % 2;

    if (extra > 0) {
      nsAutoArrayPtr<char> chunkExtension(new char[extra]);
      if (!ReadAll(chunkExtension.get(), extra)) {
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
  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mStream->Tell() % 2 == 0,
                    "FindDataOffset called with unaligned stream");

  // The "data" chunk may not directly follow the "format" chunk, so skip
  // over any intermediate chunks.
  PRUint32 length;
  if (!ScanForwardUntil(DATA_CHUNK_MAGIC, &length)) {
    return PR_FALSE;
  }

  PRInt64 offset = mStream->Tell();
  if (offset <= 0 || offset > PR_UINT32_MAX) {
    NS_WARNING("PCM data offset out of range");
    return PR_FALSE;
  }

  nsAutoMonitor monitor(mMonitor);
  mWaveLength = length;
  mWavePCMOffset = PRUint32(offset);
  return PR_TRUE;
}

PRInt64
nsWaveStateMachine::GetDataLength()
{
  NS_ABORT_IF_FALSE(mMetadataValid,
                    "Attempting to initialize audio stream with invalid metadata");

  PRInt64 length = mWaveLength;
  // If the decoder has a valid content length, and it's shorter than the
  // expected length of the PCM data, calculate the playback duration from
  // the content length rather than the expected PCM data length.
  PRInt64 streamLength = mStream->GetLength();
  if (streamLength >= 0) {
    PRInt64 dataLength = PR_MAX(0, streamLength - mWavePCMOffset);
    length = PR_MIN(dataLength, length);
  }
  return length;
}

void
nsWaveStateMachine::FirePositionChanged(PRBool aCoalesce)
{
  if (aCoalesce && mPositionChangeQueued) {
    return;
  }

  mPositionChangeQueued = PR_TRUE;
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(mDecoder, &nsWaveDecoder::PlaybackPositionChanged);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

nsresult
nsWaveStateMachine::GetBuffered(nsTimeRanges* aBuffered)
{
  PRInt64 startOffset = mStream->GetNextCachedData(mWavePCMOffset);
  while (startOffset >= 0) {
    PRInt64 endOffset = mStream->GetCachedDataEnd(startOffset);
    // Bytes [startOffset..endOffset] are cached.
    aBuffered->Add(BytesToTime(startOffset - mWavePCMOffset),
                   BytesToTime(endOffset - mWavePCMOffset));
    startOffset = mStream->GetNextCachedData(endOffset);
  }
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsWaveDecoder, nsIObserver)

nsWaveDecoder::nsWaveDecoder()
  : mInitialVolume(1.0f),
    mCurrentTime(0.0f),
    mEndedDuration(std::numeric_limits<float>::quiet_NaN()),
    mEnded(PR_FALSE),
    mSeekable(PR_TRUE),
    mResourceLoaded(PR_FALSE),
    mMetadataLoadedReported(PR_FALSE),
    mResourceLoadedReported(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsWaveDecoder);

#ifdef PR_LOGGING
  if (!gWaveDecoderLog) {
    gWaveDecoderLog = PR_NewLogModule("nsWaveDecoder");
  }
#endif
}

nsWaveDecoder::~nsWaveDecoder()
{
  MOZ_COUNT_DTOR(nsWaveDecoder);
  UnpinForSeek();
}

PRBool
nsWaveDecoder::Init(nsHTMLMediaElement* aElement)
{
  nsMediaDecoder::Init(aElement);

  nsContentUtils::RegisterShutdownObserver(this);

  mPlaybackStateMachine = new nsWaveStateMachine(this,
    TimeDuration::FromMilliseconds(BUFFERING_TIMEOUT),
    mInitialVolume);
  NS_ENSURE_TRUE(mPlaybackStateMachine, PR_FALSE);

  return PR_TRUE;
}

nsMediaStream*
nsWaveDecoder::GetCurrentStream()
{
  return mStream;
}

already_AddRefed<nsIPrincipal>
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
  return mCurrentTime;
}

nsresult
nsWaveDecoder::StartStateMachineThread()
{
  NS_ASSERTION(mPlaybackStateMachine, "Must have state machine");
  if (mPlaybackThread) {
    return NS_OK;
  }
  nsresult rv = NS_NewThread(getter_AddRefs(mPlaybackThread));
  NS_ENSURE_SUCCESS(rv, rv);

  return mPlaybackThread->Dispatch(mPlaybackStateMachine, NS_DISPATCH_NORMAL);
}

nsresult
nsWaveDecoder::Seek(float aTime)
{
  if (mPlaybackStateMachine) {
    PinForSeek();
    mPlaybackStateMachine->Seek(aTime);
    return StartStateMachineThread();
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
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->Play();
    return StartStateMachineThread();
  }

  return NS_ERROR_FAILURE;
}

void
nsWaveDecoder::Stop()
{
  if (mPlaybackStateMachine) {
    mPlaybackStateMachine->Shutdown();
  }

  if (mStream) {
    mStream->Close();
  }

  if (mPlaybackThread) {
    mPlaybackThread->Shutdown();
  }

  if (mPlaybackStateMachine) {
    mEndedDuration = mPlaybackStateMachine->GetDuration();
    mEnded = mPlaybackStateMachine->IsEnded();
  }

  mPlaybackThread = nsnull;
  mPlaybackStateMachine = nsnull;
  mStream = nsnull;

  nsContentUtils::UnregisterShutdownObserver(this);
}

nsresult
nsWaveDecoder::Load(nsMediaStream* aStream, nsIStreamListener** aStreamListener,
                    nsMediaDecoder* aCloneDonor)
{
  NS_ASSERTION(aStream, "A stream should be provided");

  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  mStream = aStream;

  nsresult rv = mStream->Open(aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  mPlaybackStateMachine->SetStream(mStream);

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
    mElement->MetadataLoaded(mPlaybackStateMachine->GetChannels(),
                             mPlaybackStateMachine->GetSampleRate());
    mElement->FirstFrameLoaded(mResourceLoaded);
  }

  mMetadataLoadedReported = PR_TRUE;

  if (mResourceLoaded) {
    ResourceLoaded();
  } else {
    StartProgress();
  }
  StartTimeUpdate();
}

void
nsWaveDecoder::PlaybackEnded()
{
  if (mShuttingDown) {
    return;
  }

  if (!mPlaybackStateMachine->IsEnded()) {
    return;
  }

  // Update ready state; now that we've finished playback, we should
  // switch to HAVE_CURRENT_DATA.
  UpdateReadyStateForData();
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

  mResourceLoaded = PR_TRUE;

  if (!mMetadataLoadedReported || mResourceLoadedReported)
    return;

  StopProgress();

  if (mElement) {
    // Ensure the final progress event gets fired
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
  Shutdown();
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
nsWaveDecoder::NotifySuspendedStatusChanged()
{
  if (mStream->IsSuspendedByCache() && mElement) {
    // if this is an autoplay element, we need to kick off its autoplaying
    // now so we consume data and hopefully free up cache space
    mElement->NotifyAutoplayDataReady();
  }
}

void
nsWaveDecoder::NotifyBytesDownloaded()
{
  UpdateReadyStateForData();
  Progress(PR_FALSE);
}

void
nsWaveDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  if (NS_SUCCEEDED(aStatus)) {
    ResourceLoaded();
  } else if (aStatus == NS_BINDING_ABORTED) {
    // Download has been cancelled by user.
    mElement->LoadAborted();
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    NetworkError();
  }
  UpdateReadyStateForData();
}

void
nsWaveDecoder::Shutdown()
{
  if (mShuttingDown)
    return;

  mShuttingDown = PR_TRUE;
  StopTimeUpdate();

  nsMediaDecoder::Shutdown();

  // An event that gets posted to the main thread, when the media element is
  // being destroyed, to destroy the decoder. Since the decoder shutdown can
  // block and post events this cannot be done inside destructor calls. So
  // this event is posted asynchronously to the main thread to perform the
  // shutdown.
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &nsWaveDecoder::Stop);
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
nsWaveDecoder::NextFrameUnavailableBuffering()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mPlaybackStateMachine)
    return;

  mElement->UpdateReadyStateForData(nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING);
}

void
nsWaveDecoder::NextFrameAvailable()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mPlaybackStateMachine)
    return;

  if (!mMetadataLoadedReported) {
    mElement->UpdateReadyStateForData(nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE);
  } else {
    mElement->UpdateReadyStateForData(nsHTMLMediaElement::NEXT_FRAME_AVAILABLE);
  }
}

void
nsWaveDecoder::NextFrameUnavailable()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mPlaybackStateMachine)
    return;

  mElement->UpdateReadyStateForData(nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE);
}

void
nsWaveDecoder::UpdateReadyStateForData()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
  if (!mElement || mShuttingDown || !mPlaybackStateMachine)
    return;

  nsHTMLMediaElement::NextFrameStatus frameStatus =
    mPlaybackStateMachine->GetNextFrameStatus();
  if (frameStatus == nsHTMLMediaElement::NEXT_FRAME_AVAILABLE &&
      !mMetadataLoadedReported) {
    frameStatus = nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE;
  }
  mElement->UpdateReadyStateForData(frameStatus);
}

void
nsWaveDecoder::SeekingStarted()
{
  if (mShuttingDown) {
    return;
  }

  if (mElement) {
    UpdateReadyStateForData();
    mElement->SeekStarted();
  }
}

void
nsWaveDecoder::SeekingStopped()
{
  UnpinForSeek();
  if (mShuttingDown) {
    return;
  }

  if (mElement) {
    UpdateReadyStateForData();
    mElement->SeekCompleted();
  }
}

void
nsWaveDecoder::DecodeError()
{
  if (mShuttingDown) {
    return;
  }
  if (mElement) {
    mElement->DecodeError();
  }
  Shutdown();
}

void
nsWaveDecoder::PlaybackPositionChanged()
{
  if (mShuttingDown) {
    return;
  }

  float lastTime = mCurrentTime;

  if (mPlaybackStateMachine) {
    mCurrentTime = mPlaybackStateMachine->GetTimeForPositionChange();
  }

  if (mElement && lastTime != mCurrentTime) {
    UpdateReadyStateForData();
    FireTimeUpdate();
  }
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
    mStream->Suspend(PR_TRUE);
  }
}

void
nsWaveDecoder::Resume(PRBool aForceBuffering)
{
  if (mStream) {
    mStream->Resume();
  }
}

void 
nsWaveDecoder::MoveLoadsToBackground()
{
  if (mStream) {
    mStream->MoveLoadsToBackground();
  }
}

nsresult
nsWaveDecoder::GetBuffered(nsTimeRanges* aBuffered)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  return mPlaybackStateMachine->GetBuffered(aBuffered);
}
