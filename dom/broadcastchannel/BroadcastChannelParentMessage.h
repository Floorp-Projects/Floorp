/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_broadcastchannel_BroadcastChannelParentMessage_h
#define dom_broadcastchannel_BroadcastChannelParentMessage_h

#include "mozilla/dom/DOMTypes.h"

namespace mozilla {
namespace dom {

class BlobImpl;

class BroadcastChannelParentMessage final
{
public:
  NS_INLINE_DECL_REFCOUNTING(BroadcastChannelParentMessage)

  explicit BroadcastChannelParentMessage(const ClonedMessageData& aData);

  const ClonedMessageData& Data() const
  {
    return mData;
  }

private:
  ~BroadcastChannelParentMessage() = default;

  ClonedMessageData mData;
  nsTArray<RefPtr<BlobImpl>> mBlobs;
};

} // dom namespace
} // mozilla namespace

#endif // dom_broadcastchannel_BroadcastChannelParentMessage_h
