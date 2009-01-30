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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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
#include <stdio.h>
#include <math.h>
#include "prlog.h"
#include "prmem.h"
#include "nsAutoPtr.h"
#include "nsAudioStream.h"
#include "nsAlgorithm.h"
extern "C" {
#include "sydneyaudio/sydney_audio.h"
}

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nsnull;
#endif

#define FAKE_BUFFER_SIZE 176400

static float CurrentTimeInSeconds()
{
  return PR_IntervalToMilliseconds(PR_IntervalNow()) / 1000.0;
}

void nsAudioStream::InitLibrary()
{
#ifdef PR_LOGGING
  gAudioStreamLog = PR_NewLogModule("nsAudioStream");
#endif
}

void nsAudioStream::ShutdownLibrary()
{
}

nsAudioStream::nsAudioStream() :
  mVolume(1.0),
  mAudioHandle(0),
  mRate(0),
  mChannels(0),
  mSavedPauseBytes(0),
  mPauseBytes(0),
  mPauseTime(0.0),
  mSamplesBuffered(0),
  mFormat(FORMAT_S16_LE),
  mPaused(PR_FALSE)
{
}

void nsAudioStream::Init(PRInt32 aNumChannels, PRInt32 aRate, SampleFormat aFormat)
{
  mRate = aRate;
  mChannels = aNumChannels;
  mFormat = aFormat;
  mStartTime = CurrentTimeInSeconds();
  if (sa_stream_create_pcm(reinterpret_cast<sa_stream_t**>(&mAudioHandle),
                           NULL, 
                           SA_MODE_WRONLY, 
                           SA_PCM_FORMAT_S16_NE,
                           aRate,
                           aNumChannels) != SA_SUCCESS) {
    mAudioHandle = nsnull;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsAudioStream: sa_stream_create_pcm error"));
    return;
  }
  
  if (sa_stream_open(static_cast<sa_stream_t*>(mAudioHandle)) != SA_SUCCESS) {
    sa_stream_destroy(static_cast<sa_stream_t*>(mAudioHandle));
    mAudioHandle = nsnull;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsAudioStream: sa_stream_open error"));
    return;
  }
}

void nsAudioStream::Shutdown()
{
  if (!mAudioHandle) 
    return;

  sa_stream_destroy(static_cast<sa_stream_t*>(mAudioHandle));
  mAudioHandle = nsnull;
}

void nsAudioStream::Write(const void* aBuf, PRUint32 aCount)
{
  NS_ABORT_IF_FALSE(aCount % mChannels == 0,
                    "Buffer size must be divisible by channel count");

  mSamplesBuffered += aCount;
  PRUint32 offset = mBufferOverflow.Length();
  PRInt32 count = aCount + offset;

  if (!mAudioHandle)
    return;

  nsAutoArrayPtr<short> s_data(new short[count]);

  if (s_data) {
    for (PRUint32 i=0; i < offset; ++i) {
      s_data[i] = mBufferOverflow.ElementAt(i);
    }
    mBufferOverflow.Clear();

    switch (mFormat) {
      case FORMAT_U8: {
        const PRUint8* buf = static_cast<const PRUint8*>(aBuf);
        PRInt32 volume = PRInt32((1 << 16) * mVolume);
        for (PRUint32 i = 0; i < aCount; ++i) {
          s_data[i + offset] = short(((PRInt32(buf[i]) - 128) * volume) >> 8);
        }
        break;
      }
      case FORMAT_S16_LE: {
        const short* buf = static_cast<const short*>(aBuf);
        PRInt32 volume = PRInt32((1 << 16) * mVolume);
        for (PRUint32 i = 0; i < aCount; ++i) {
          short s = buf[i];
#if defined(IS_BIG_ENDIAN)
          s = ((s & 0x00ff) << 8) | ((s & 0xff00) >> 8);
#endif
          s_data[i + offset] = short((PRInt32(s) * volume) >> 16);
        }
        break;
      }
      case FORMAT_FLOAT32: {
        const float* buf = static_cast<const float*>(aBuf);
        for (PRUint32 i = 0; i <  aCount; ++i) {
          float scaled_value = floorf(0.5 + 32768 * buf[i] * mVolume);
          if (buf[i] < 0.0) {
            s_data[i + offset] = (scaled_value < -32768.0) ?
              -32768 :
              short(scaled_value);
          } else {
            s_data[i+offset] = (scaled_value > 32767.0) ?
              32767 :
              short(scaled_value);
          }
        }
        break;
      }
    }

    PRInt32 available = Available();
    if (available < count) {
      mBufferOverflow.AppendElements(s_data.get() + available, (count - available));
      count = available;
    }

    if (sa_stream_write(static_cast<sa_stream_t*>(mAudioHandle),
       s_data.get(), count * sizeof(short)) != SA_SUCCESS) {
      PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsAudioStream: sa_stream_write error"));
      Shutdown();
    }
  }
}

PRInt32 nsAudioStream::Available()
{
  // If the audio backend failed to open, lie and say we'll accept some
  // data.
  if (!mAudioHandle)
    return FAKE_BUFFER_SIZE;

  size_t s = 0; 
  sa_stream_get_write_size(static_cast<sa_stream_t*>(mAudioHandle), &s);
  return s / sizeof(short);
}

float nsAudioStream::GetVolume()
{
  return mVolume;
}

void nsAudioStream::SetVolume(float aVolume)
{
  NS_ASSERTION(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");
  mVolume = aVolume;
}

void nsAudioStream::Drain()
{
  if (!mAudioHandle) {
    // mSamplesBuffered already accounts for the data in the
    // mBufferOverflow array.
    PRUint32 drainTime = (float(mSamplesBuffered) / mRate / mChannels - GetTime()) * 1000.0;
    PR_Sleep(PR_MillisecondsToInterval(drainTime));
    return;
  }

  // Write any remaining unwritten sound data in the overflow buffer
  if (!mBufferOverflow.IsEmpty()) {
    if (sa_stream_write(static_cast<sa_stream_t*>(mAudioHandle),
                        mBufferOverflow.Elements(),
                        mBufferOverflow.Length() * sizeof(short)) != SA_SUCCESS)
      return;
  }

  if (sa_stream_drain(static_cast<sa_stream_t*>(mAudioHandle)) != SA_SUCCESS) {
        PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsAudioStream: sa_stream_drain error"));
        Shutdown();
  }
}

void nsAudioStream::Pause()
{
  if (mPaused)
    return;

  // Save the elapsed playback time.  Used to offset the wall-clock time
  // when resuming.
  mPauseTime = CurrentTimeInSeconds() - mStartTime;

  mPaused = PR_TRUE;

  if (!mAudioHandle)
    return;

  int64_t bytes = 0;
#if !defined(WIN32)
  sa_stream_get_position(static_cast<sa_stream_t*>(mAudioHandle), SA_POSITION_WRITE_SOFTWARE, &bytes);
#endif
  mSavedPauseBytes = bytes;

  sa_stream_pause(static_cast<sa_stream_t*>(mAudioHandle));
}

void nsAudioStream::Resume()
{
  if (!mPaused)
    return;

  // Reset the start time to the current time offset backwards by the
  // elapsed time saved when the stream paused.
  mStartTime = CurrentTimeInSeconds() - mPauseTime;

  mPaused = PR_FALSE;

  if (!mAudioHandle)
    return;

  sa_stream_resume(static_cast<sa_stream_t*>(mAudioHandle));

#if !defined(WIN32)
  mPauseBytes += mSavedPauseBytes;
#endif
}

double nsAudioStream::GetTime()
{
  // If the audio backend failed to open, emulate the current playback
  // position using the system clock.
  if (!mAudioHandle) {
    if (mPaused) {
      return mPauseTime;
    }
    float curTime = CurrentTimeInSeconds() - mStartTime;
    float maxTime = float(mSamplesBuffered) / mRate / mChannels;
    return NS_MIN(curTime, maxTime);
  }

  int64_t bytes = 0;
#if defined(WIN32)
  sa_stream_get_position(static_cast<sa_stream_t*>(mAudioHandle), SA_POSITION_WRITE_HARDWARE, &bytes);
#else
  sa_stream_get_position(static_cast<sa_stream_t*>(mAudioHandle), SA_POSITION_WRITE_SOFTWARE, &bytes);
#endif
  return double(bytes + mPauseBytes) / (sizeof(short) * mChannels * mRate);
}
