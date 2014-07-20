/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIconChannel_h_
#define nsIconChannel_h_

#include "mozilla/Attributes.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIIconURI.h"
#include "nsCOMPtr.h"

/**
 * This class is the gnome implementation of nsIconChannel. It basically asks
 * qt for an icon, and creates a new channel for
 * that file to which all calls will be proxied.
 */
class nsIconChannel MOZ_FINAL : public nsIChannel {
  public:
    NS_DECL_ISUPPORTS
    NS_FORWARD_NSIREQUEST(mRealChannel->)
    NS_FORWARD_NSICHANNEL(mRealChannel->)

    nsIconChannel() {};

    /**
     * Called by nsIconProtocolHandler after it creates this channel.
     * Must be called before calling any other function on this object.
     * If this method fails, no other function must be called on this object.
     */
    nsresult Init(nsIURI* aURI);
  private:
    ~nsIconChannel() {};

    /**
     * The channel to the temp icon file (e.g. to /tmp/2qy9wjqw.html).
     * Will always be non-null after a successful Init.
     */
    nsCOMPtr<nsIChannel> mRealChannel;
};

#endif
