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
nsresult
AddClientChannelHelper(nsIChannel* aChannel,
                       Maybe<ClientInfo>&& aReservedClientInfo,
                       Maybe<ClientInfo>&& aInitialClientInfo,
                       nsISerialEventTarget* aEventTarget);

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientChannelHelper_h
