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

namespace mozilla::dom {

class ClientInfo;

// Attach a redirect listener to the given nsIChannel that will manage
// the various client values on the channel's LoadInfo.  This will
// properly handle creating a new ClientSource on cross-origin redirect
// and propagate the current reserved/initial client on same-origin
// redirect.
nsresult AddClientChannelHelper(nsIChannel* aChannel,
                                Maybe<ClientInfo>&& aReservedClientInfo,
                                Maybe<ClientInfo>&& aInitialClientInfo,
                                nsISerialEventTarget* aEventTarget);

// Use this variant in the content process if redirects will be handled in the
// parent process (by a channel with AddClientChannelHelperInParent),
// and this process only sees a single switch to the final channel,
// as done by DocumentChannel.
// This variant just handles allocating a ClientSource around an existing
// ClientInfo allocated in the parent process.
nsresult AddClientChannelHelperInChild(nsIChannel* aChannel,
                                       nsISerialEventTarget* aEventTarget);

// Use this variant in the parent process if redirects are handled there.
// Does the same as the default variant, except just allocates a ClientInfo
// and lets the content process create the corresponding ClientSource once
// it becomes available there.
nsresult AddClientChannelHelperInParent(nsIChannel* aChannel,
                                        Maybe<ClientInfo>&& aInitialClientInfo);

// If the channel's LoadInfo has a reserved ClientInfo, but no reserved
// ClientSource, then allocates a ClientSource using that existing
// ClientInfo.
void CreateReservedSourceIfNeeded(nsIChannel* aChannel,
                                  nsISerialEventTarget* aEventTarget);

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientChannelHelper_h
