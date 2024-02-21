/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MTRANSPORTCHILD_H__
#define _MTRANSPORTCHILD_H__

#include "mozilla/dom/PMediaTransportChild.h"
#include "mozilla/Mutex.h"

namespace mozilla {
class MediaTransportHandlerIPC;

class MediaTransportChild : public dom::PMediaTransportChild {
 public:
#ifdef MOZ_WEBRTC
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaTransportChild, override)

  explicit MediaTransportChild(MediaTransportHandlerIPC* aUser);

  mozilla::ipc::IPCResult RecvOnCandidate(const string& transportId,
                                          const CandidateInfo& candidateInfo);
  mozilla::ipc::IPCResult RecvOnAlpnNegotiated(const string& alpn);
  mozilla::ipc::IPCResult RecvOnGatheringStateChange(const int& state);
  mozilla::ipc::IPCResult RecvOnConnectionStateChange(const int& state);
  mozilla::ipc::IPCResult RecvOnPacketReceived(const string& transportId,
                                               const MediaPacket& packet);
  mozilla::ipc::IPCResult RecvOnEncryptedSending(const string& transportId,
                                                 const MediaPacket& packet);
  mozilla::ipc::IPCResult RecvOnStateChange(const string& transportId,
                                            const int& state);
  mozilla::ipc::IPCResult RecvOnRtcpStateChange(const string& transportId,
                                                const int& state);

 private:
  virtual ~MediaTransportChild();
  friend class MediaTransportHandlerIPC;
  void Shutdown();
  // MediaTransportHandlerIPC owns us, and will inform us when it is going away
  Mutex mMutex;
  MediaTransportHandlerIPC* mUser;
#endif  // MOZ_WEBRTC
};

}  // namespace mozilla
#endif
