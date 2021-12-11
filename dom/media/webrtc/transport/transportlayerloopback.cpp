/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "logging.h"
#include "nspr.h"
#include "prlock.h"

#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

#include "transportflow.h"
#include "transportlayerloopback.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

nsresult TransportLayerLoopback::Init() {
  nsresult rv;
  target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!NS_SUCCEEDED(rv)) return rv;

  timer_ = NS_NewTimer(target_);
  MOZ_ASSERT(timer_);
  if (!timer_) return NS_ERROR_FAILURE;

  packets_lock_ = PR_NewLock();
  MOZ_ASSERT(packets_lock_);
  if (!packets_lock_) return NS_ERROR_FAILURE;

  deliverer_ = new Deliverer(this);

  timer_->InitWithCallback(deliverer_, 100, nsITimer::TYPE_REPEATING_SLACK);

  return NS_OK;
}

// Connect to the other side
void TransportLayerLoopback::Connect(TransportLayerLoopback* peer) {
  peer_ = peer;

  TL_SET_STATE(TS_OPEN);
}

TransportResult TransportLayerLoopback::SendPacket(MediaPacket& packet) {
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "SendPacket(" << packet.len() << ")");

  if (!peer_) {
    MOZ_MTLOG(ML_ERROR, "Discarding packet because peer not attached");
    return TE_ERROR;
  }

  size_t len = packet.len();
  nsresult res = peer_->QueuePacket(packet);
  if (!NS_SUCCEEDED(res)) return TE_ERROR;

  return static_cast<TransportResult>(len);
}

nsresult TransportLayerLoopback::QueuePacket(MediaPacket& packet) {
  MOZ_ASSERT(packets_lock_);

  PR_Lock(packets_lock_);

  if (combinePackets_ && !packets_.empty()) {
    MediaPacket* prevPacket = packets_.front();

    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << " Enqueuing combined packets of length "
                                   << prevPacket->len() << " and "
                                   << packet.len());
    auto combined = MakeUnique<uint8_t[]>(prevPacket->len() + packet.len());
    memcpy(combined.get(), prevPacket->data(), prevPacket->len());
    memcpy(combined.get() + prevPacket->len(), packet.data(), packet.len());
    prevPacket->Take(std::move(combined), prevPacket->len() + packet.len());
  } else {
    MOZ_MTLOG(ML_DEBUG,
              LAYER_INFO << " Enqueuing packet of length " << packet.len());
    packets_.push(new MediaPacket(std::move(packet)));
  }

  PRStatus r = PR_Unlock(packets_lock_);
  MOZ_ASSERT(r == PR_SUCCESS);
  if (r != PR_SUCCESS) return NS_ERROR_FAILURE;

  return NS_OK;
}

void TransportLayerLoopback::DeliverPackets() {
  while (!packets_.empty()) {
    UniquePtr<MediaPacket> packet(packets_.front());
    packets_.pop();

    MOZ_MTLOG(ML_DEBUG,
              LAYER_INFO << " Delivering packet of length " << packet->len());
    SignalPacketReceived(this, *packet);
  }
}

NS_IMPL_ISUPPORTS(TransportLayerLoopback::Deliverer, nsITimerCallback, nsINamed)

NS_IMETHODIMP TransportLayerLoopback::Deliverer::Notify(nsITimer* timer) {
  if (!layer_) return NS_OK;

  layer_->DeliverPackets();

  return NS_OK;
}

NS_IMETHODIMP TransportLayerLoopback::Deliverer::GetName(nsACString& aName) {
  aName.AssignLiteral("TransportLayerLoopback::Deliverer");
  return NS_OK;
}

}  // namespace mozilla
