/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioChild_h
#define mozilla_dom_AudioChild_h

#include "mozilla/dom/PAudioChild.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {
namespace dom {

class AudioChild : public PAudioChild
{
 public:
    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();

    AudioChild();
    virtual ~AudioChild();
    virtual bool RecvPositionInFramesUpdate(const PRInt64&, const PRInt64&);
    virtual bool RecvDrainDone();
    virtual PRInt32 WaitForMinWriteSize();
    virtual bool RecvMinWriteSizeDone(const PRInt32& frameCount);
    virtual void WaitForDrain();
    virtual bool RecvWriteDone();
    virtual void WaitForWrite();
    virtual void ActorDestroy(ActorDestroyReason);

    PRInt64 GetLastKnownPosition();
    PRInt64 GetLastKnownPositionTimestamp();

    bool IsIPCOpen() { return mIPCOpen; };
 private:
    nsAutoRefCnt mRefCnt;
    NS_DECL_OWNINGTHREAD
    PRInt64 mLastPosition;
    PRInt64 mLastPositionTimestamp;
    PRUint64 mWriteCounter;
    PRInt32 mMinWriteSize;
    mozilla::ReentrantMonitor mAudioReentrantMonitor;
    bool mIPCOpen;
    bool mDrained;
};

} // namespace dom
} // namespace mozilla

#endif
