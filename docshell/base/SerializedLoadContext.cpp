/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SerializedLoadContext.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsIWebSocketChannel.h"

namespace IPC {

SerializedLoadContext::SerializedLoadContext(nsILoadContext* aLoadContext)
  : mIsContent(false)
  , mUseRemoteTabs(false)
  , mUseTrackingProtection(false)
{
  Init(aLoadContext);
}

SerializedLoadContext::SerializedLoadContext(nsIChannel* aChannel)
  : mIsContent(false)
  , mUseRemoteTabs(false)
  , mUseTrackingProtection(false)
{
  if (!aChannel) {
    Init(nullptr);
    return;
  }

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aChannel, loadContext);
  Init(loadContext);

  if (!loadContext) {
    // Attempt to retrieve the private bit from the channel if it has been
    // overriden.
    bool isPrivate = false;
    bool isOverriden = false;
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(aChannel);
    if (pbChannel &&
        NS_SUCCEEDED(pbChannel->IsPrivateModeOverriden(&isPrivate,
                                                       &isOverriden)) &&
        isOverriden) {
      mIsPrivateBitValid = true;
    }
    mOriginAttributes.SyncAttributesWithPrivateBrowsing(isPrivate);
  }
}

SerializedLoadContext::SerializedLoadContext(nsIWebSocketChannel* aChannel)
  : mIsContent(false)
  , mUseRemoteTabs(false)
  , mUseTrackingProtection(false)
{
  nsCOMPtr<nsILoadContext> loadContext;
  if (aChannel) {
    NS_QueryNotificationCallbacks(aChannel, loadContext);
  }
  Init(loadContext);
}

void
SerializedLoadContext::Init(nsILoadContext* aLoadContext)
{
  if (aLoadContext) {
    mIsNotNull = true;
    mIsPrivateBitValid = true;
    aLoadContext->GetIsContent(&mIsContent);
    aLoadContext->GetUseRemoteTabs(&mUseRemoteTabs);
    aLoadContext->GetUseTrackingProtection(&mUseTrackingProtection);
    aLoadContext->GetOriginAttributes(mOriginAttributes);
  } else {
    mIsNotNull = false;
    mIsPrivateBitValid = false;
    // none of below values really matter when mIsNotNull == false:
    // we won't be GetInterfaced to nsILoadContext
    mIsContent = true;
    mUseRemoteTabs = false;
    mUseTrackingProtection = false;
  }
}

} // namespace IPC
