/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Zack Rusin <zack@kde.org>
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
#include <strings.h>
#include <nsXPIDLString.h>

#include "nsIURI.h"

#include "EmbedContentListener.h"
#include "qgeckoembed.h"

#include "nsICategoryManager.h"
#include "nsIServiceManagerUtils.h"

EmbedContentListener::EmbedContentListener(QGeckoEmbed *aOwner)
{
    mOwner = aOwner;
}

EmbedContentListener::~EmbedContentListener()
{
}

NS_IMPL_ISUPPORTS2(EmbedContentListener,
                   nsIURIContentListener,
                   nsISupportsWeakReference)

NS_IMETHODIMP
EmbedContentListener::OnStartURIOpen(nsIURI     *aURI,
                                     PRBool     *aAbortOpen)
{
    nsresult rv;

    nsCAutoString specString;
    rv = aURI->GetSpec(specString);

    if (NS_FAILED(rv))
        return rv;

    //we stop loading here because we want to pass the
    //control to kio to check for mimetypes and all the other jazz
    bool abort = false;
    mOwner->startURIOpen(specString.get(), abort);
    *aAbortOpen = abort;

    return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::DoContent(const char         *aContentType,
                                PRBool             aIsContentPreferred,
                                nsIRequest         *aRequest,
                                nsIStreamListener **aContentHandler,
                                PRBool             *aAbortProcess)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::IsPreferred(const char        *aContentType,
                                  char             **aDesiredContentType,
                                  PRBool            *aCanHandleContent)
{
    return CanHandleContent(aContentType, PR_TRUE, aDesiredContentType,
                            aCanHandleContent);
}

NS_IMETHODIMP
EmbedContentListener::CanHandleContent(const char        *aContentType,
                                       PRBool           aIsContentPreferred,
                                       char             **aDesiredContentType,
                                       PRBool            *_retval)
{
    *_retval = PR_FALSE;
    qDebug("HANDLING:");

    if (aContentType) {
        nsCOMPtr<nsICategoryManager> catMgr;
        nsresult rv;
        catMgr = do_GetService("@mozilla.org/categorymanager;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsXPIDLCString value;
        rv = catMgr->GetCategoryEntry("Gecko-Content-Viewers",
                                      aContentType,
                                      getter_Copies(value));

        // If the category manager can't find what we're looking for
        // it returns NS_ERROR_NOT_AVAILABLE, we don't want to propagate
        // that to the caller since it's really not a failure

        if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)
            return rv;

        if (value && *value)
            *_retval = PR_TRUE;
    }

    qDebug("\tCan handle content %s: %d", aContentType, *_retval);
    return NS_OK;
}

NS_IMETHODIMP
EmbedContentListener::GetLoadCookie(nsISupports **aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::SetLoadCookie(nsISupports *aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::GetParentContentListener(nsIURIContentListener **aParent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EmbedContentListener::SetParentContentListener(nsIURIContentListener *aParent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

