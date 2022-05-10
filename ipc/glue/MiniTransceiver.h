/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __MINITRANSCEIVER_H_
#define __MINITRANSCEIVER_H_

#include "chrome/common/ipc_message.h"
#include "mozilla/Assertions.h"

struct msghdr;

namespace mozilla {
namespace ipc {

enum class DataBufferClear { None, AfterReceiving };

/**
 * This simple implementation handles the transmissions of IPC
 * messages.
 *
 * It works according to a strict request-response paradigm, no
 * concurrent messaging is allowed. Sending a message from A to B must
 * be followed by another one from B to A. Because of this we don't
 * need to handle data crossing the boundaries of a
 * message. Transmission is done via blocking I/O to avoid the
 * complexity of asynchronous I/O.
 */
class MiniTransceiver {
 public:
  /**
   * \param aFd should be a blocking, no O_NONBLOCK, fd.
   * \param aClearDataBuf is true to clear data buffers after
   *                      receiving a message.
   */
  explicit MiniTransceiver(
      int aFd, DataBufferClear aDataBufClear = DataBufferClear::None);

  bool Send(IPC::Message& aMsg);
  inline bool SendInfallible(IPC::Message& aMsg, const char* aCrashMessage) {
    bool Ok = Send(aMsg);
    if (!Ok) {
      MOZ_CRASH_UNSAFE(aCrashMessage);
    }
    return Ok;
  }

  /**
   * \param aMsg will hold the content of the received message.
   * \return false if the fd is closed or with an error.
   */
  bool Recv(UniquePtr<IPC::Message>& aMsg);
  inline bool RecvInfallible(UniquePtr<IPC::Message>& aMsg,
                             const char* aCrashMessage) {
    bool Ok = Recv(aMsg);
    if (!Ok) {
      MOZ_CRASH_UNSAFE(aCrashMessage);
    }
    return Ok;
  }

  int GetFD() { return mFd; }

 private:
  /**
   * Set control buffer to make file descriptors ready to be sent
   * through a socket.
   */
  void PrepareFDs(msghdr* aHdr, IPC::Message& aMsg);
  /**
   * Collect buffers of the message and make them ready to be sent.
   *
   * \param aHdr is the structure going to be passed to sendmsg().
   * \param aMsg is the Message to send.
   */
  size_t PrepareBuffers(msghdr* aHdr, IPC::Message& aMsg);
  /**
   * Collect file descriptors received.
   *
   * \param aAllFds is where to store file descriptors.
   * \param aMaxFds is how many file descriptors can be stored in aAllFds.
   * \return the number of received file descriptors.
   */
  unsigned RecvFDs(msghdr* aHdr, int* aAllFds, unsigned aMaxFds);
  /**
   * Received data from the socket.
   *
   * \param aDataBuf is where to store the data from the socket.
   * \param aBufSize is the size of the buffer.
   * \param aMsgSize returns how many bytes were readed from the socket.
   * \param aFdsBuf is the buffer to return file desriptors received.
   * \param aMaxFds is the number of file descriptors that can be held.
   * \param aNumFds returns the number of file descriptors received.
   * \return true if sucess, or false for error.
   */
  bool RecvData(char* aDataBuf, size_t aBufSize, uint32_t* aMsgSize,
                int* aFdsBuf, unsigned aMaxFds, unsigned* aNumFds);

  int mFd;  // The file descriptor of the socket for IPC.

#ifdef DEBUG
  enum State {
    STATE_NONE,
    STATE_SENDING,
    STATE_RECEIVING,
  };
  State mState;
#endif

  // Clear all received data in temp buffers to avoid data leaking.
  DataBufferClear mDataBufClear;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // __MINITRANSCEIVER_H_
