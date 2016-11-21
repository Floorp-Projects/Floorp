/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelParentMessage.h"

namespace mozilla {
namespace dom {

BroadcastChannelParentMessage::BroadcastChannelParentMessage(const ClonedMessageData& aData)
  : mData(aData)
{
  // We need to keep the array alive for the life-time of this operation.
  if (!aData.blobsParent().IsEmpty()) {
    mBlobs.SetCapacity(aData.blobsParent().Length());

    for (uint32_t i = 0, len = aData.blobsParent().Length(); i < len; ++i) {
      RefPtr<BlobImpl> impl =
        static_cast<BlobParent*>(aData.blobsParent()[i])->GetBlobImpl();
     MOZ_ASSERT(impl);
     mBlobs.AppendElement(impl);
    }
  }
}

} // dom namespace
} // mozilla namespace
