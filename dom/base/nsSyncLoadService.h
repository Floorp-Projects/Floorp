/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A service that provides methods for synchronously loading a DOM in various ways.
 */

#ifndef nsSyncLoadService_h__
#define nsSyncLoadService_h__

#include "nscore.h"
#include "mozilla/net/ReferrerPolicy.h"

class nsIInputStream;
class nsILoadGroup;
class nsIStreamListener;
class nsIURI;
class nsIPrincipal;
class nsIDocument;
class nsIChannel;

class nsSyncLoadService
{
public:
    /**
     * Synchronously load the document from the specified URI.
     *
     * @param aURI URI to load the document from.
     * @param aContentPolicyType contentPolicyType to be set on the channel
     * @param aLoaderPrincipal Principal of loading document. For security
     *                         checks and referrer header.
     * @param aSecurityFlags securityFlags to be set on the channel
     * @param aLoadGroup The loadgroup to use for loading the document.
     * @param aForceToXML Whether to parse the document as XML, regardless of
     *                    content type.
     * @param referrerPolicy Referrer policy.
     * @param aResult [out] The document loaded from the URI.
     */
    static nsresult LoadDocument(nsIURI *aURI,
                                 nsContentPolicyType aContentPolicyType,
                                 nsIPrincipal *aLoaderPrincipal,
                                 nsSecurityFlags aSecurityFlags,
                                 nsILoadGroup *aLoadGroup,
                                 bool aForceToXML,
                                 mozilla::net::ReferrerPolicy aReferrerPolicy,
                                 nsIDocument** aResult);

    /**
     * Read input stream aIn in chunks and deliver synchronously to aListener.
     *
     * @param aIn The stream to be read. The ownership of this stream is taken.
     * @param aListener The listener that will receive
     *                  OnStartRequest/OnDataAvailable/OnStopRequest
     *                  notifications.
     * @param aChannel The channel that aIn was opened from.
     */
    static nsresult PushSyncStreamToListener(already_AddRefed<nsIInputStream> aIn,
                                             nsIStreamListener* aListener,
                                             nsIChannel* aChannel);
};

#endif // nsSyncLoadService_h__
