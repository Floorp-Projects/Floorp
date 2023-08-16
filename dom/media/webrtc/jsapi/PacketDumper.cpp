/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi/PacketDumper.h"
#include "jsapi/PeerConnectionImpl.h"
#include "mozilla/media/MediaUtils.h"  // NewRunnableFrom
#include "nsThreadUtils.h"             // NS_DispatchToMainThread

namespace mozilla {

/* static */
RefPtr<PacketDumper> PacketDumper::GetPacketDumper(
    const std::string& aPcHandle) {
  MOZ_ASSERT(NS_IsMainThread());
  PeerConnectionWrapper pcw(aPcHandle);
  if (pcw.impl()) {
    return pcw.impl()->GetPacketDumper();
  }

  return new PacketDumper("");
}

PacketDumper::PacketDumper(const std::string& aPcHandle)
    : mPcHandle(aPcHandle), mPacketDumpFlagsMutex("Packet dump flags mutex") {}

void PacketDumper::Dump(size_t aLevel, dom::mozPacketDumpType aType,
                        bool aSending, const void* aData, size_t aSize) {
  // Optimization; avoids making a copy of the buffer, but we need to lock a
  // mutex and check the flags. Could be optimized further, if we really want to
  if (!ShouldDumpPacket(aLevel, aType, aSending)) {
    return;
  }

  UniquePtr<uint8_t[]> ownedPacket = MakeUnique<uint8_t[]>(aSize);
  memcpy(ownedPacket.get(), aData, aSize);

  RefPtr<Runnable> dumpRunnable = media::NewRunnableFrom(std::bind(
      [this, self = RefPtr<PacketDumper>(this), aLevel, aType, aSending,
       aSize](UniquePtr<uint8_t[]>& aPacket) -> nsresult {
        // Check again; packet dump might have been disabled since the dispatch
        if (!ShouldDumpPacket(aLevel, aType, aSending)) {
          return NS_OK;
        }

        PeerConnectionWrapper pcw(mPcHandle);
        RefPtr<PeerConnectionImpl> pc = pcw.impl();
        if (!pc) {
          return NS_OK;
        }

        if (!aSending && (aType == dom::mozPacketDumpType::Rtcp ||
                          aType == dom::mozPacketDumpType::Srtcp)) {
          AutoTArray<size_t, 4> dumpLevels;
          {
            MutexAutoLock lock(mPacketDumpFlagsMutex);
            unsigned flag = 1 << (unsigned)aType;
            for (size_t i = 0; i < mRecvPacketDumpFlags.size(); ++i) {
              if (mRecvPacketDumpFlags[i] & flag) {
                dumpLevels.AppendElement(i);
              }
            }
          }
          for (size_t level : dumpLevels) {
            pc->DumpPacket_m(level, aType, aSending, aPacket, aSize);
          }
          return NS_OK;
        }

        pc->DumpPacket_m(aLevel, aType, aSending, aPacket, aSize);

        return NS_OK;
      },
      std::move(ownedPacket)));

  NS_DispatchToMainThread(dumpRunnable);
}

nsresult PacketDumper::EnablePacketDump(unsigned long aLevel,
                                        dom::mozPacketDumpType aType,
                                        bool aSending) {
  mPacketDumpEnabled = true;
  std::vector<unsigned>* packetDumpFlags;
  if (aSending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  unsigned flag = 1 << (unsigned)aType;

  MutexAutoLock lock(mPacketDumpFlagsMutex);
  if (aLevel >= packetDumpFlags->size()) {
    packetDumpFlags->resize(aLevel + 1);
  }

  mPacketDumpRtcpRecvCount += !aSending &&
                              (aType == dom::mozPacketDumpType::Rtcp ||
                               aType == dom::mozPacketDumpType::Srtcp) &&
                              !((*packetDumpFlags)[aLevel] & flag);

  (*packetDumpFlags)[aLevel] |= flag;
  return NS_OK;
}

nsresult PacketDumper::DisablePacketDump(unsigned long aLevel,
                                         dom::mozPacketDumpType aType,
                                         bool aSending) {
  std::vector<unsigned>* packetDumpFlags;
  if (aSending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  unsigned flag = 1 << (unsigned)aType;

  MutexAutoLock lock(mPacketDumpFlagsMutex);
  if (aLevel < packetDumpFlags->size()) {
    mPacketDumpRtcpRecvCount -= !aSending &&
                                (aType == dom::mozPacketDumpType::Rtcp ||
                                 aType == dom::mozPacketDumpType::Srtcp) &&
                                ((*packetDumpFlags)[aLevel] & flag);
    (*packetDumpFlags)[aLevel] &= ~flag;
  }

  return NS_OK;
}

bool PacketDumper::ShouldDumpPacket(size_t aLevel, dom::mozPacketDumpType aType,
                                    bool aSending) const {
  if (!mPacketDumpEnabled) {
    return false;
  }

  if (!aSending && (aType == dom::mozPacketDumpType::Rtcp ||
                    aType == dom::mozPacketDumpType::Srtcp)) {
    // Received (S)RTCP is dumped per-transport, not per-pipeline.
    return mPacketDumpRtcpRecvCount > 0;
  }

  MutexAutoLock lock(mPacketDumpFlagsMutex);

  const std::vector<unsigned>* packetDumpFlags;

  if (aSending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  if (aLevel < packetDumpFlags->size()) {
    unsigned flag = 1 << (unsigned)aType;
    return flag & packetDumpFlags->at(aLevel);
  }

  return false;
}

}  // namespace mozilla
