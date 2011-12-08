/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et ft=cpp: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *   Kyle Machulis <kyle@nonpolynomial.com>
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

#include <fcntl.h>
#include <unistd.h>

#include <queue>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Util.h"
#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "nsXULAppAPI.h"
#include "Ril.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...)  printf(args);
#endif

using namespace base;
using namespace std;

namespace mozilla {
namespace ipc {

struct RilClient : public RefCounted<RilClient>,
                   public MessageLoopForIO::Watcher

{
    typedef queue<RilRawData*> RilRawDataQueue;

    RilClient() : mSocket(-1)
                , mMutex("RilClient.mMutex")
                , mBlockedOnWrite(false)
                , mCurrentRilRawData(NULL)
    { }
    virtual ~RilClient() { }

    bool OpenSocket();

    virtual void OnFileCanReadWithoutBlocking(int fd);
    virtual void OnFileCanWriteWithoutBlocking(int fd);

    ScopedClose mSocket;
    MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
    MessageLoopForIO::FileDescriptorWatcher mWriteWatcher;
    nsAutoPtr<RilRawData> mIncoming;
    Mutex mMutex;
    RilRawDataQueue mOutgoingQ;
    bool mBlockedOnWrite;
    MessageLoopForIO* mIOLoop;
    nsAutoPtr<RilRawData> mCurrentRilRawData;
    size_t mCurrentWriteOffset;
};

static RefPtr<RilClient> sClient;
static RefPtr<RilConsumer> sConsumer;

//-----------------------------------------------------------------------------
// This code runs on the IO thread.
//

class RilWriteTask : public Task {
    virtual void Run();
};

void RilWriteTask::Run() {
    sClient->OnFileCanWriteWithoutBlocking(sClient->mSocket.mFd);
}

static void
ConnectToRil(Monitor* aMonitor, bool* aSuccess)
{
    MOZ_ASSERT(!sClient);

    sClient = new RilClient();
    if (!(*aSuccess = sClient->OpenSocket())) {
        sClient = nsnull;
    }

    {
        MonitorAutoLock lock(*aMonitor);
        lock.Notify();
    }
    // aMonitor may have gone out of scope by now, don't touch it
}

bool
RilClient::OpenSocket()
{
#if defined(MOZ_WIDGET_GONK)
    // Using a network socket to test basic functionality
    // before we see how this works on the phone.
    struct sockaddr_un addr;
    socklen_t alen;
    size_t namelen;
    int err;
    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sun_path, "/dev/socket/rilb2g");
    addr.sun_family = AF_LOCAL;
    mSocket.mFd = socket(AF_LOCAL, SOCK_STREAM, 0);
    alen = strlen("/dev/socket/rilb2g") + offsetof(struct sockaddr_un, sun_path) + 1;
#else
    struct hostent *hp;
    struct sockaddr_in addr;
    socklen_t alen;

    hp = gethostbyname("localhost");
    if (hp == 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    addr.sin_port = htons(6200);
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
    mSocket.mFd = socket(hp->h_addrtype, SOCK_STREAM, 0);
    alen = sizeof(addr);
#endif

    if (mSocket.mFd < 0) {
        LOG("Cannot create socket for RIL!\n");
        return -1;
    }

    if (connect(mSocket.mFd, (struct sockaddr *) &addr, alen) < 0) {
        LOG("Cannot open socket for RIL!\n");
        close(mSocket.mFd);
        return false;
    }
    LOG("Socket open for RIL\n");

    // Set close-on-exec bit.
    int flags = fcntl(mSocket.mFd, F_GETFD);
    if (-1 == flags) {
        return false;
    }

    flags |= FD_CLOEXEC;
    if (-1 == fcntl(mSocket.mFd, F_SETFD, flags)) {
        return false;
    }

    // Select non-blocking IO.
    if (-1 == fcntl(mSocket.mFd, F_SETFL, O_NONBLOCK)) {
        return false;
    }
    mIOLoop = MessageLoopForIO::current();
    if (!mIOLoop->WatchFileDescriptor(mSocket.mFd,
                                      true,
                                      MessageLoopForIO::WATCH_READ,
                                      &mReadWatcher,
                                      this)) {
        return false;
    }
    return true;
}

void
RilClient::OnFileCanReadWithoutBlocking(int fd)
{
    // Keep reading data until either
    //
    //   - mIncoming is completely read
    //     If so, sConsumer->MessageReceived(mIncoming.forget())
    //
    //   - mIncoming isn't completely read, but there's no more
    //     data available on the socket
    //     If so, break;

    MOZ_ASSERT(fd == mSocket.mFd);
    while (true) {
        if (!mIncoming) {
            mIncoming = new RilRawData();
            int ret = read(fd, mIncoming->mData, 1024);
            if (ret <= 0) {
                LOG("Cannot read from network, error %d\n", ret);
                return;
            }
            mIncoming->mSize = ret;
            sConsumer->MessageReceived(mIncoming.forget());
            if (ret < 1024) {
                return;
            }
        }
    }
}

void
RilClient::OnFileCanWriteWithoutBlocking(int fd)
{
    // Try to write the bytes of mCurrentRilRawData.  If all were written, continue.
    //
    // Otherwise, save the byte position of the next byte to write
    // within mCurrentRilRawData, and request another write when the
    // system won't block.
    //

    MOZ_ASSERT(fd == mSocket.mFd);

    while (!mOutgoingQ.empty() || mCurrentRilRawData != NULL) {
        if(!mCurrentRilRawData) {
            mCurrentRilRawData = mOutgoingQ.front();
            mOutgoingQ.pop();
            mCurrentWriteOffset = 0;
        }
        const uint8_t *toWrite;

        toWrite = mCurrentRilRawData->mData;
 
        while (mCurrentWriteOffset < mCurrentRilRawData->mSize) {
            ssize_t write_amount = mCurrentRilRawData->mSize - mCurrentWriteOffset;
            ssize_t written;
            written = write (fd, toWrite + mCurrentWriteOffset,
                             write_amount);
            if(written > 0) {
                mCurrentWriteOffset += written;
            }
            if (written != write_amount) {
                break;
            }
        }

        if(mCurrentWriteOffset != mCurrentRilRawData->mSize) {
            MessageLoopForIO::current()->WatchFileDescriptor(
                fd,
                false,
                MessageLoopForIO::WATCH_WRITE,
                &mWriteWatcher,
                this);
            return;
        }
        mCurrentRilRawData = NULL;
    }
}


static void
DisconnectFromRil(Monitor* aMonitor)
{
    // XXX This might "strand" messages in the outgoing queue.  We'll
    // assume that's OK for now.
    sClient = nsnull;
    {
        MonitorAutoLock lock(*aMonitor);
        lock.Notify();
    }
}

//-----------------------------------------------------------------------------
// This code runs on any thread.
//

bool
StartRil(RilConsumer* aConsumer)
{
    MOZ_ASSERT(aConsumer);
    sConsumer = aConsumer;

    Monitor monitor("StartRil.monitor");
    bool success;
    {
        MonitorAutoLock lock(monitor);

        XRE_GetIOMessageLoop()->PostTask(
            FROM_HERE,
            NewRunnableFunction(ConnectToRil, &monitor, &success));

        lock.Wait();
    }

    return success;
}

bool
SendRilRawData(RilRawData** aMessage)
{
    if (!sClient) {
        return false;
    }

    RilRawData *msg = *aMessage;
    *aMessage = nsnull;

    {
        MutexAutoLock lock(sClient->mMutex);
        sClient->mOutgoingQ.push(msg);
    }
    sClient->mIOLoop->PostTask(FROM_HERE, new RilWriteTask());

    return true;
}

void
StopRil()
{
    Monitor monitor("StopRil.monitor");
    {
        MonitorAutoLock lock(monitor);

        XRE_GetIOMessageLoop()->PostTask(
            FROM_HERE,
            NewRunnableFunction(DisconnectFromRil, &monitor));

        lock.Wait();
    }

    sConsumer = nsnull;
}


} // namespace ipc
} // namespace mozilla
