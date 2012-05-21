/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioParent_h
#define mozilla_dom_AudioParent_h

#include "mozilla/dom/PAudioParent.h"
#include "nsAudioStream.h"
#include "nsITimer.h"

namespace mozilla {
namespace dom {
class AudioParent : public PAudioParent, public nsITimerCallback
{
 public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK

    virtual bool
    RecvWrite(const nsCString& data, const PRUint32& count);

    virtual bool
    RecvSetVolume(const float& aVolume);

    virtual bool
    RecvMinWriteSize();

    virtual bool
    RecvDrain();

    virtual bool
    RecvPause();

    virtual bool
    RecvResume();

    virtual bool
    RecvShutdown();

    virtual bool
    SendMinWriteSizeDone(PRInt32 minFrames);

    virtual bool
    SendDrainDone();

    AudioParent(PRInt32 aNumChannels, PRInt32 aRate, PRInt32 aFormat);
    virtual ~AudioParent();
    virtual void ActorDestroy(ActorDestroyReason);

    nsRefPtr<nsAudioStream> mStream;
    nsCOMPtr<nsITimer> mTimer;

private:
    void Shutdown();

    bool mIPCOpen;
};
} // namespace dom
} // namespace mozilla

#endif
