/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
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
#include "nsThreadUtils.h"

// C++ file contents
namespace mozilla {
namespace dom {

class AudioWriteEvent : public nsRunnable
{
 public:
  AudioWriteEvent(nsAudioStream* owner, nsCString data, PRUint32 count)
  {
    mOwner = owner;
    mData  = data;
    mCount = count;
  }

  NS_IMETHOD Run()
  {
    mOwner->Write(mData.get(), mCount, true);
    return NS_OK;
  }

 private:
    nsRefPtr<nsAudioStream> mOwner;
    nsCString mData;
    PRUint32  mCount;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(AudioParent, nsITimerCallback)

nsresult
AudioParent::Notify(nsITimer* timer)
{
  if (!mStream) {
    timer->Cancel();
    return NS_ERROR_FAILURE;
  }

  PRInt64 offset = mStream->GetSampleOffset();
  SendSampleOffsetUpdate(offset, PR_IntervalNow());
  return NS_OK;
}
bool
AudioParent::RecvWrite(
        const nsCString& data,
        const PRUint32& count)
{
  nsCOMPtr<nsIRunnable> event = new AudioWriteEvent(mStream, data, count);
  nsCOMPtr<nsIThread> thread = nsAudioStream::GetGlobalThread();
  thread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
  return true;
}
    
bool
AudioParent::RecvSetVolume(const float& aVolume)
{
  if (mStream)
    mStream->SetVolume(aVolume);
  return true;
}

bool
AudioParent::RecvDrain()
{
  if (mStream)
    mStream->Drain();
  return true;
}

bool
AudioParent::RecvPause()
{
  if (mStream)
    mStream->Pause();
  return true;
}

bool
AudioParent::RecvResume()
{
  if (mStream)
    mStream->Resume();
  return true;
}

bool
AudioParent::Recv__delete__()
{
  if (mStream) {
    mStream->Shutdown();
    mStream = nsnull;
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
  return true;
}

AudioParent::AudioParent(PRInt32 aNumChannels, PRInt32 aRate, PRInt32 aFormat)
{
  mStream = nsAudioStream::AllocateStream();
  if (mStream)
    mStream->Init(aNumChannels,
                  aRate,
                  (nsAudioStream::SampleFormat) aFormat);
  if (!mStream)
    return; 

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  mTimer->InitWithCallback(this, 1000, nsITimer::TYPE_REPEATING_SLACK);
}

AudioParent::~AudioParent()
{
}

} // namespace dom
} // namespace mozilla
