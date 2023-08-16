/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PACKET_DUMPER_H_
#define _PACKET_DUMPER_H_

#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"

#include <vector>

namespace mozilla {
class PeerConnectionImpl;

class PacketDumper {
 public:
  static RefPtr<PacketDumper> GetPacketDumper(const std::string& aPcHandle);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PacketDumper)

  PacketDumper(const PacketDumper&) = delete;
  PacketDumper& operator=(const PacketDumper&) = delete;

  void Dump(size_t aLevel, dom::mozPacketDumpType aType, bool aSending,
            const void* aData, size_t aSize);

  nsresult EnablePacketDump(unsigned long aLevel, dom::mozPacketDumpType aType,
                            bool aSending);

  nsresult DisablePacketDump(unsigned long aLevel, dom::mozPacketDumpType aType,
                             bool aSending);

 private:
  friend class PeerConnectionImpl;
  explicit PacketDumper(const std::string& aPcHandle);
  ~PacketDumper() = default;
  bool ShouldDumpPacket(size_t aLevel, dom::mozPacketDumpType aType,
                        bool aSending) const;

  // This class is not cycle-collected, so it cannot hold onto a strong ref
  const std::string mPcHandle;
  std::vector<unsigned> mSendPacketDumpFlags
      MOZ_GUARDED_BY(mPacketDumpFlagsMutex);
  std::vector<unsigned> mRecvPacketDumpFlags
      MOZ_GUARDED_BY(mPacketDumpFlagsMutex);
  Atomic<bool> mPacketDumpEnabled{false};
  Atomic<int> mPacketDumpRtcpRecvCount{0};
  mutable Mutex mPacketDumpFlagsMutex;
};

}  // namespace mozilla

#endif  // _PACKET_DUMPER_H_
