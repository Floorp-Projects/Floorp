/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ipc/MiniTransceiver.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "base/eintr_wrapper.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Sprintf.h"
#include "mozilla/ScopeExit.h"
#include "nsDebug.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

namespace mozilla::ipc {

static const size_t kMaxIOVecSize = 64;
static const size_t kMaxDataSize = 8 * 1024;
static const size_t kMaxNumFds = 16;

MiniTransceiver::MiniTransceiver(int aFd, DataBufferClear aDataBufClear)
    : mFd(aFd),
#ifdef DEBUG
      mState(STATE_NONE),
#endif
      mDataBufClear(aDataBufClear) {
}

namespace {

/**
 * Initialize the IO vector for sending data and the control buffer for sending
 * FDs.
 */
static void InitMsgHdr(msghdr* aHdr, int aIOVSize, int aMaxNumFds) {
  aHdr->msg_name = nullptr;
  aHdr->msg_namelen = 0;
  aHdr->msg_flags = 0;

  // Prepare the IO vector to receive the content of message.
  auto iov = new iovec[aIOVSize];
  aHdr->msg_iov = iov;
  aHdr->msg_iovlen = aIOVSize;

  // Prepare the control buffer to receive file descriptors.
  auto cbuf = new char[CMSG_SPACE(sizeof(int) * aMaxNumFds)];
  aHdr->msg_control = cbuf;
  aHdr->msg_controllen = CMSG_SPACE(sizeof(int) * aMaxNumFds);
}

/**
 * Delete resources allocated by InitMsgHdr().
 */
static void DeinitMsgHdr(msghdr* aHdr) {
  delete aHdr->msg_iov;
  delete static_cast<char*>(aHdr->msg_control);
}

}  // namespace

void MiniTransceiver::PrepareFDs(msghdr* aHdr, IPC::Message& aMsg) {
  // Set control buffer to send file descriptors of the Message.
  int num_fds = aMsg.file_descriptor_set()->size();

  cmsghdr* cmsg = CMSG_FIRSTHDR(aHdr);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int) * num_fds);
  aMsg.file_descriptor_set()->GetDescriptors(
      reinterpret_cast<int*>(CMSG_DATA(cmsg)));

  // This number will be sent in the header of the message. So, we
  // can check it at the other side.
  aMsg.header()->num_fds = num_fds;
}

size_t MiniTransceiver::PrepareBuffers(msghdr* aHdr, IPC::Message& aMsg) {
  // Set iovec to send for all buffers of the Message.
  iovec* iov = aHdr->msg_iov;
  size_t iovlen = 0;
  size_t bytes_to_send = 0;
  for (Pickle::BufferList::IterImpl iter(aMsg.Buffers()); !iter.Done();
       iter.Advance(aMsg.Buffers(), iter.RemainingInSegment())) {
    char* data = iter.Data();
    size_t size = iter.RemainingInSegment();
    iov[iovlen].iov_base = data;
    iov[iovlen].iov_len = size;
    iovlen++;
    MOZ_ASSERT(iovlen <= kMaxIOVecSize);
    bytes_to_send += size;
  }
  MOZ_ASSERT(bytes_to_send <= kMaxDataSize);
  aHdr->msg_iovlen = iovlen;

  return bytes_to_send;
}

bool MiniTransceiver::Send(IPC::Message& aMsg) {
#ifdef DEBUG
  if (mState == STATE_SENDING) {
    MOZ_CRASH(
        "STATE_SENDING: It violates of request-response and no concurrent "
        "rules");
  }
  mState = STATE_SENDING;
#endif

  auto clean_fdset =
      MakeScopeExit([&] { aMsg.file_descriptor_set()->CommitAll(); });

  int num_fds = aMsg.file_descriptor_set()->size();
  msghdr hdr;
  InitMsgHdr(&hdr, kMaxIOVecSize, num_fds);

  UniquePtr<msghdr, decltype(&DeinitMsgHdr)> uniq(&hdr, &DeinitMsgHdr);

  PrepareFDs(&hdr, aMsg);
  DebugOnly<size_t> bytes_to_send = PrepareBuffers(&hdr, aMsg);

  ssize_t bytes_written = HANDLE_EINTR(sendmsg(mFd, &hdr, 0));

  if (bytes_written < 0) {
    char error[128];
    SprintfLiteral(error, "sendmsg: %s", strerror(errno));
    NS_WARNING(error);
    return false;
  }
  MOZ_ASSERT(bytes_written == (ssize_t)bytes_to_send,
             "The message is too big?!");

  return true;
}

unsigned MiniTransceiver::RecvFDs(msghdr* aHdr, int* aAllFds,
                                  unsigned aMaxFds) {
  if (aHdr->msg_controllen == 0) {
    return 0;
  }

  unsigned num_all_fds = 0;
  for (cmsghdr* cmsg = CMSG_FIRSTHDR(aHdr); cmsg;
       cmsg = CMSG_NXTHDR(aHdr, cmsg)) {
    MOZ_ASSERT(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS,
               "Accept only SCM_RIGHTS to receive file descriptors");

    unsigned payload_sz = cmsg->cmsg_len - CMSG_LEN(0);
    MOZ_ASSERT(payload_sz % sizeof(int) == 0);

    // Add fds to |aAllFds|
    unsigned num_part_fds = payload_sz / sizeof(int);
    int* part_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
    MOZ_ASSERT(num_all_fds + num_part_fds <= aMaxFds);

    memcpy(aAllFds + num_all_fds, part_fds, num_part_fds * sizeof(int));
    num_all_fds += num_part_fds;
  }
  return num_all_fds;
}

bool MiniTransceiver::RecvData(char* aDataBuf, size_t aBufSize,
                               uint32_t* aMsgSize, int* aFdsBuf,
                               unsigned aMaxFds, unsigned* aNumFds) {
  msghdr hdr;
  InitMsgHdr(&hdr, 1, aMaxFds);

  UniquePtr<msghdr, decltype(&DeinitMsgHdr)> uniq(&hdr, &DeinitMsgHdr);

  // The buffer to collect all fds received from the socket.
  int* all_fds = aFdsBuf;
  unsigned num_all_fds = 0;

  size_t total_readed = 0;
  uint32_t msgsz = 0;
  while (msgsz == 0 || total_readed < msgsz) {
    // Set IO vector with the begin of the unused buffer.
    hdr.msg_iov->iov_base = aDataBuf + total_readed;
    hdr.msg_iov->iov_len = (msgsz == 0 ? aBufSize : msgsz) - total_readed;

    // Read the socket
    ssize_t bytes_readed = HANDLE_EINTR(recvmsg(mFd, &hdr, 0));
    if (bytes_readed <= 0) {
      // Closed or error!
      return false;
    }
    total_readed += bytes_readed;
    MOZ_ASSERT(total_readed <= aBufSize);

    if (msgsz == 0) {
      // Parse the size of the message.
      // Get 0 if data in the buffer is no enough to get message size.
      msgsz = IPC::Message::MessageSize(aDataBuf, aDataBuf + total_readed);
    }

    num_all_fds += RecvFDs(&hdr, all_fds + num_all_fds, aMaxFds - num_all_fds);
  }

  *aMsgSize = msgsz;
  *aNumFds = num_all_fds;
  return true;
}

bool MiniTransceiver::Recv(IPC::Message& aMsg) {
#ifdef DEBUG
  if (mState == STATE_RECEIVING) {
    MOZ_CRASH(
        "STATE_RECEIVING: It violates of request-response and no concurrent "
        "rules");
  }
  mState = STATE_RECEIVING;
#endif

  UniquePtr<char[]> databuf = MakeUnique<char[]>(kMaxDataSize);
  uint32_t msgsz = 0;
  int all_fds[kMaxNumFds];
  unsigned num_all_fds = 0;

  if (!RecvData(databuf.get(), kMaxDataSize, &msgsz, all_fds, kMaxDataSize,
                &num_all_fds)) {
    return false;
  }

  // Create Message from databuf & all_fds.
  UniquePtr<IPC::Message> msg = MakeUnique<IPC::Message>(databuf.get(), msgsz);
  msg->file_descriptor_set()->SetDescriptors(all_fds, num_all_fds);

  if (mDataBufClear == DataBufferClear::AfterReceiving) {
    // Avoid content processes from reading the content of
    // messages.
    memset(databuf.get(), 0, msgsz);
  }

  MOZ_ASSERT(msg->header()->num_fds == msg->file_descriptor_set()->size(),
             "The number of file descriptors in the header is different from"
             " the number actually received");

  aMsg = std::move(*msg);
  return true;
}

}  // namespace mozilla::ipc
