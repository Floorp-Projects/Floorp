/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientChannelHelper_h
#define _mozilla_dom_ClientChannelHelper_h

#include "mozilla/Maybe.h"
#include "nsError.h"

class nsIChannel;
class nsISerialEventTarget;

namespace mozilla {
namespace dom {

class ClientInfo;

// Attach a redirect listener to the given nsIChannel that will manage
// the various client values on the channel's LoadInfo.  This will
// properly handle creating a new ClientSource on cross-origin redirect
// and propagate the current reserved/initial client on same-origin
// redirect.
// Pass true for aManagedInParent if redirects will be handled in the
// parent process (by a channel with AddClientChannelHelperInParent),
// and this process only sees a single switch to the final channel,
// as done by DocumentChannel.
nsresult AddClientChannelHelper(nsIChannel* aChannel,
                                Maybe<ClientInfo>&& aReservedClientInfo,
                                Maybe<ClientInfo>&& aInitialClientInfo,
                                nsISerialEventTarget* aEventTarget,
                                bool aManagedInParent);

nsresult AddClientChannelHelperInParent(nsIChannel* aChannel,
                                        nsISerialEventTarget* aEventTarget);

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientChannelHelper_h
