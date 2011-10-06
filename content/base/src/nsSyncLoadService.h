/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A service that provides methods for synchronously loading a DOM in various ways.
 */

#ifndef nsSyncLoadService_h__
#define nsSyncLoadService_h__

#include "nscore.h"

class nsIInputStream;
class nsILoadGroup;
class nsIStreamListener;
class nsIURI;
class nsIPrincipal;
class nsIDOMDocument;
class nsIChannel;

class nsSyncLoadService
{
public:
    /**
     * Synchronously load the document from the specified URI.
     *
     * @param aURI URI to load the document from.
     * @param aLoaderPrincipal Principal of loading document. For security
     *                         checks and referrer header. May be null if no
     *                         security checks should be done.
     * @param aLoadGroup The loadgroup to use for loading the document.
     * @param aForceToXML Whether to parse the document as XML, regardless of
     *                    content type.
     * @param aResult [out] The document loaded from the URI.
     */
    static nsresult LoadDocument(nsIURI *aURI, nsIPrincipal *aLoaderPrincipal,
                                 nsILoadGroup *aLoadGroup, bool aForceToXML,
                                 nsIDOMDocument** aResult);

    /**
     * Read input stream aIn in chunks and deliver synchronously to aListener.
     *
     * @param aIn The stream to be read.
     * @param aListener The listener that will receive
     *                  OnStartRequest/OnDataAvailable/OnStopRequest
     *                  notifications.
     * @param aChannel The channel that aIn was opened from.
     */
    static nsresult PushSyncStreamToListener(nsIInputStream* aIn,
                                             nsIStreamListener* aListener,
                                             nsIChannel* aChannel);
};

#endif // nsSyncLoadService_h__
