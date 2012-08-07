/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SerializedLoadContext.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIWebSocketChannel.h"

namespace IPC {

SerializedLoadContext::SerializedLoadContext(nsILoadContext* aLoadContext)
{
  Init(aLoadContext);
}

SerializedLoadContext::SerializedLoadContext(nsIChannel* aChannel)
{
  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aChannel, loadContext);
  Init(loadContext);
}

SerializedLoadContext::SerializedLoadContext(nsIWebSocketChannel* aChannel)
{
  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aChannel, loadContext);
  Init(loadContext);
}

void
SerializedLoadContext::Init(nsILoadContext* aLoadContext)
{
  if (aLoadContext) {
    mIsNotNull = true;
    aLoadContext->GetIsContent(&mIsContent);
    aLoadContext->GetUsePrivateBrowsing(&mUsePrivateBrowsing);
    aLoadContext->GetAppId(&mAppId);
    aLoadContext->GetIsInBrowserElement(&mIsInBrowserElement);
  } else {
    mIsNotNull = false;
    // none of below values really matter when mIsNotNull == false:
    // we won't be GetInterfaced to nsILoadContext
    mIsContent = true;
    mUsePrivateBrowsing = false;
    mAppId = 0;
    mIsInBrowserElement = false;
  }
}

} // namespace IPC

