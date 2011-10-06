/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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
 * The Original Code is Mozilla Audio IPC
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
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

#include "mozilla/dom/AudioParent.h"
#include "mozilla/unused.h"
#include "nsThreadUtils.h"

// C++ file contents
namespace mozilla {
namespace dom {

class AudioWriteEvent : public nsRunnable
{
 public:
  AudioWriteEvent(nsAudioStream* owner, nsCString data, PRUint32 frames)
  {
    mOwner = owner;
    mData  = data;
    mFrames = frames;
  }

  NS_IMETHOD Run()
  {
    mOwner->Write(mData.get(), mFrames);
    return NS_OK;
  }

 private:
    nsRefPtr<nsAudioStream> mOwner;
    nsCString mData;
    PRUint32  mFrames;
};

class AudioPauseEvent : public nsRunnable
{
 public:
  AudioPauseEvent(nsAudioStream* owner, bool aPause)
  {
    mOwner = owner;
    mPause = aPause;
  }

  NS_IMETHOD Run()
  {
    if (mPause)
        mOwner->Pause();
    else
        mOwner->Resume();
    return NS_OK;
  }

 private:
    nsRefPtr<nsAudioStream> mOwner;
    bool mPause;
};

class AudioStreamShutdownEvent : public nsRunnable
{
 public:
  AudioStreamShutdownEvent(nsAudioStream* owner)
  {
    mOwner = owner;
  }

  NS_IMETHOD Run()
  {
    mOwner->Shutdown();
    return NS_OK;
  }

 private:
    nsRefPtr<nsAudioStream> mOwner;
};


class AudioMinWriteSizeDone : public nsRunnable
{
 public:
  AudioMinWriteSizeDone(AudioParent* owner, PRInt32 minFrames)
  {
    mOwner = owner;
    mMinFrames = minFrames;
  }

  NS_IMETHOD Run()
  {
    mOwner->SendMinWriteSizeDone(mMinFrames);
    return NS_OK;
  }

 private:
    nsRefPtr<AudioParent> mOwner;
    PRInt32 mMinFrames;
};

class AudioMinWriteSizeEvent : public nsRunnable
{
 public:
  AudioMinWriteSizeEvent(AudioParent* parent, nsAudioStream* owner)
  {
    mParent = parent;
    mOwner = owner;
  }

  NS_IMETHOD Run()
  {
    PRInt32 minFrames = mOwner->GetMinWriteSize();
    nsCOMPtr<nsIRunnable> event = new AudioMinWriteSizeDone(mParent, minFrames);
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

 private:
    nsRefPtr<nsAudioStream> mOwner;
    nsRefPtr<AudioParent> mParent;
};

class AudioDrainDoneEvent : public nsRunnable
{
 public:
  AudioDrainDoneEvent(AudioParent* owner)
  {
    mOwner = owner;
  }

  NS_IMETHOD Run()
  {
    mOwner->SendDrainDone();
    return NS_OK;
  }

 private:
    nsRefPtr<AudioParent> mOwner;
};

class AudioDrainEvent : public nsRunnable
{
 public:
  AudioDrainEvent(AudioParent* parent, nsAudioStream* owner)
  {
    mParent = parent;
    mOwner = owner;
  }

  NS_IMETHOD Run()
  {
    mOwner->Drain();
    nsCOMPtr<nsIRunnable> event = new AudioDrainDoneEvent(mParent);
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

 private:
    nsRefPtr<nsAudioStream> mOwner;
    nsRefPtr<AudioParent> mParent;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(AudioParent, nsITimerCallback)

nsresult
AudioParent::Notify(nsITimer* timer)
{
  if (!mIPCOpen) {
    timer->Cancel();
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(mStream, "AudioStream not initialized.");
  PRInt64 position = mStream->GetPositionInFrames();
  unused << SendPositionInFramesUpdate(position, PR_IntervalNow());
  return NS_OK;
}

bool
AudioParent::RecvWrite(const nsCString& data, const PRUint32& frames)
{
  if (!mStream)
    return false;
  nsCOMPtr<nsIRunnable> event = new AudioWriteEvent(mStream, data, frames);
  nsCOMPtr<nsIThread> thread = mStream->GetThread();
  thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  return true;
}

bool
AudioParent::RecvSetVolume(const float& aVolume)
{
  if (!mStream)
      return false;
  mStream->SetVolume(aVolume);
  return true;
}

bool
AudioParent::RecvMinWriteSize()
{
  if (!mStream)
    return false;
  nsCOMPtr<nsIRunnable> event = new AudioMinWriteSizeEvent(this, mStream);
  nsCOMPtr<nsIThread> thread = mStream->GetThread();
  thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  return true;
}

bool
AudioParent::RecvDrain()
{
  if (!mStream)
    return false;
  nsCOMPtr<nsIRunnable> event = new AudioDrainEvent(this, mStream);
  nsCOMPtr<nsIThread> thread = mStream->GetThread();
  thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  return true;
}

bool
AudioParent::RecvPause()
{
  if (!mStream)
    return false;
  nsCOMPtr<nsIRunnable> event = new AudioPauseEvent(mStream, PR_TRUE);
  nsCOMPtr<nsIThread> thread = mStream->GetThread();
  thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  return true;
}

bool
AudioParent::RecvResume()
{
  if (!mStream)
    return false;
  nsCOMPtr<nsIRunnable> event = new AudioPauseEvent(mStream, PR_FALSE);
  nsCOMPtr<nsIThread> thread = mStream->GetThread();
  thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  return true;
}

bool
AudioParent::RecvShutdown()
{
  Shutdown();
  unused << PAudioParent::Send__delete__(this);
  return true;
}

bool
AudioParent::SendMinWriteSizeDone(PRInt32 minFrames)
{
  if (mIPCOpen)
    return PAudioParent::SendMinWriteSizeDone(minFrames);
  return true;
}

bool
AudioParent::SendDrainDone()
{
  if (mIPCOpen)
    return PAudioParent::SendDrainDone();
  return true;
}

AudioParent::AudioParent(PRInt32 aNumChannels, PRInt32 aRate, PRInt32 aFormat)
  : mIPCOpen(PR_TRUE)
{
  mStream = nsAudioStream::AllocateStream();
  NS_ASSERTION(mStream, "AudioStream allocation failed.");
  if (NS_FAILED(mStream->Init(aNumChannels,
                              aRate,
                              (nsAudioStream::SampleFormat) aFormat))) {
      NS_WARNING("AudioStream initialization failed.");
      mStream = nsnull;
      return;
  }

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  mTimer->InitWithCallback(this, 1000, nsITimer::TYPE_REPEATING_SLACK);
}

AudioParent::~AudioParent()
{
}

void
AudioParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIPCOpen = PR_FALSE;

  Shutdown();
}

void
AudioParent::Shutdown()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  if (mStream) {
      nsCOMPtr<nsIRunnable> event = new AudioStreamShutdownEvent(mStream);
      nsCOMPtr<nsIThread> thread = mStream->GetThread();
      thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
      mStream = nsnull;
  }
}

} // namespace dom
} // namespace mozilla
