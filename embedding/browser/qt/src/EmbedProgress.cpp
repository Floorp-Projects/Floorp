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
 *   Lars Knoll <knoll@kde.org>
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
#include "EmbedProgress.h"

#include "qgeckoembed.h"

#include <nsXPIDLString.h>
#include <nsIChannel.h>
#include <nsIWebProgress.h>
#include <nsIDOMWindow.h>

#include "nsIURI.h"
#include "nsCRT.h"
#include "nsString.h"

EmbedProgress::EmbedProgress(QGeckoEmbed *aOwner)
{
    qDebug("XXX EMBEDPROGRSS");
    mOwner = aOwner;
}

EmbedProgress::~EmbedProgress()
{
    qDebug("#########################################################################################");
}

NS_IMPL_ISUPPORTS2(EmbedProgress,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

NS_IMETHODIMP
EmbedProgress::OnStateChange(nsIWebProgress *aWebProgress,
                             nsIRequest     *aRequest,
                             PRUint32        aStateFlags,
                             nsresult        aStatus)
{
    // give the widget a chance to attach any listeners
    mOwner->contentStateChanged();
    // if we've got the start flag, emit the signal
    if ((aStateFlags & STATE_IS_NETWORK) &&
        (aStateFlags & STATE_START))
    {
        qDebug("net start");
        emit mOwner->netStart();
    }

    //XXX: emit state all here

    if ((aStateFlags & STATE_IS_NETWORK) &&
             (aStateFlags & STATE_STOP)) {
        //qDebug("progress: --stop");
        emit mOwner->netStop();
        mOwner->contentFinishedLoading();
    }

    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnProgressChange(nsIWebProgress *aWebProgress,
                                nsIRequest     *aRequest,
                                PRInt32         aCurSelfProgress,
                                PRInt32         aMaxSelfProgress,
                                PRInt32         aCurTotalProgress,
                                PRInt32         aMaxTotalProgress)
{
#if 0
    nsString tmpString;
    RequestToURIString(aRequest, tmpString);
    // is it the same as the current uri?
    if (mOwner->mURI.Equals(tmpString))
        mOwner->progressAll(QString(tmpString.get()), aCurTotalProgress, aMaxTotalProgress);
#endif
    //qDebug("progress self: %d %d", aCurSelfProgress, aMaxSelfProgress);

    mOwner->progress(aCurTotalProgress, aMaxTotalProgress);
    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnLocationChange(nsIWebProgress *aWebProgress,
                                nsIRequest     *aRequest,
                                nsIURI         *aLocation)
{
    nsCAutoString newURI;
    NS_ENSURE_ARG_POINTER(aLocation);
    aLocation->GetSpec(newURI);

    // Make sure that this is the primary frame change and not
    // just a subframe.
    PRBool isSubFrameLoad = PR_FALSE;
    if (aWebProgress) {
        nsCOMPtr<nsIDOMWindow> domWindow;
        nsCOMPtr<nsIDOMWindow> topDomWindow;

        aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));

        // get the root dom window
        if (domWindow)
            domWindow->GetTop(getter_AddRefs(topDomWindow));

        if (domWindow != topDomWindow)
            isSubFrameLoad = PR_TRUE;
    }

    if (!isSubFrameLoad)
        emit mOwner->locationChanged(newURI.get());

    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStatusChange(nsIWebProgress  *aWebProgress,
                              nsIRequest      *aRequest,
                              nsresult         aStatus,
                              const PRUnichar *aMessage)
{
    QString message = QString::fromUcs2(aMessage);
    emit mOwner->linkMessage(message);

    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnSecurityChange(nsIWebProgress *aWebProgress,
                                nsIRequest     *aRequest,
                                PRUint32         aState)
{
    //FIXME:
    //emit mOwner->securityChange(aRequest, aState);

    return NS_OK;
}

/* static */
void
EmbedProgress::RequestToURIString(nsIRequest *aRequest, nsString &aString)
{
    // is it a channel
    nsCOMPtr<nsIChannel> channel;
    channel = do_QueryInterface(aRequest);
    if (!channel)
        return;

    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    if (!uri)
        return;

    nsCAutoString uriString;
    uri->GetSpec(uriString);

    CopyUTF8toUTF16(uriString, aString);
}
