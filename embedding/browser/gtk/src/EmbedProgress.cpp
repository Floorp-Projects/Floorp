/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
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
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Antonio Gomes <tonikitoo@gmail.com>
 *   Oleg Romashin <romaxa@gmail.com>
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

#ifdef MOZILLA_INTERNAL_API
#include "nsXPIDLString.h"
#endif
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIWebProgress.h"
#include "nsIDOMWindow.h"
#include "EmbedPasswordMgr.h"

#include "nsIURI.h"
#include "nsCRT.h"

static PRInt32 sStopSignalTimer = 0;
static gboolean
progress_emit_stop(void * data)
{
    g_return_val_if_fail(data, FALSE);
    EmbedPrivate *owner = (EmbedPrivate*)data;
    if (!owner->mLoadFinished) {
      owner->mLoadFinished = PR_TRUE;
      gtk_signal_emit(GTK_OBJECT(owner->mOwningWidget),
                      moz_embed_signals[NET_STOP]);
    }
    return FALSE;
}

EmbedProgress::EmbedProgress(void)
{
  mOwner = nsnull;
}

EmbedProgress::~EmbedProgress()
{
}

NS_IMPL_ISUPPORTS2(EmbedProgress,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

nsresult
EmbedProgress::Init(EmbedPrivate *aOwner)
{
  mOwner = aOwner;
  mStopLevel = 0;
  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStateChange(nsIWebProgress *aWebProgress,
           nsIRequest     *aRequest,
           PRUint32        aStateFlags,
           nsresult        aStatus)
{
  // give the widget a chance to attach any listeners
  mOwner->ContentStateChange();

  if (sStopSignalTimer &&
      (
       (aStateFlags & GTK_MOZ_EMBED_FLAG_TRANSFERRING)
       || (aStateFlags & GTK_MOZ_EMBED_FLAG_REDIRECTING)
       || (aStateFlags & GTK_MOZ_EMBED_FLAG_NEGOTIATING)
      )
     ) {
      g_source_remove(sStopSignalTimer);
      mStopLevel = 0;
      gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                      moz_embed_signals[NET_START]);
      mOwner->mLoadFinished = PR_FALSE;
  }

  // if we've got the start flag, emit the signal
  if ((aStateFlags & GTK_MOZ_EMBED_FLAG_IS_NETWORK) &&
      (aStateFlags & GTK_MOZ_EMBED_FLAG_START)) {
    // FIXME: workaround for broken progress values.
    mOwner->mOwningWidget->current_number_of_requests = 0;
    mOwner->mOwningWidget->total_number_of_requests = 0;

    if (mOwner->mLoadFinished) {
      mOwner->mLoadFinished = PR_FALSE;
      mStopLevel = 0;
      gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                      moz_embed_signals[NET_START]);
    }
  }
  // get the uri for this request
  nsCString tmpString;
  RequestToURIString(aRequest, tmpString);

  // FIXME: workaround for broken progress values.
  if (mOwner->mOwningWidget) {
    if (aStateFlags & GTK_MOZ_EMBED_FLAG_IS_REQUEST) {
      if (aStateFlags & GTK_MOZ_EMBED_FLAG_START)
        mOwner->mOwningWidget->total_number_of_requests ++;
      else if (aStateFlags & GTK_MOZ_EMBED_FLAG_STOP)
        mOwner->mOwningWidget->current_number_of_requests++;
    }

    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[PROGRESS_ALL],
                    (const gchar *) tmpString.get(),
                    mOwner->mOwningWidget->current_number_of_requests,
                    mOwner->mOwningWidget->total_number_of_requests);
  }
  // is it the same as the current URI?
  if (mOwner->mURI.Equals(NS_ConvertUTF8toUTF16(tmpString))) {
    // for people who know what they are doing
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[NET_STATE],
                    aStateFlags, aStatus);
  }

  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[NET_STATE_ALL],
                  (const gchar *)tmpString.get(),
                  (gint)aStateFlags, (gint)aStatus);

  // and for stop, too
  if (aStateFlags & GTK_MOZ_EMBED_FLAG_STOP) {
    if (aStateFlags & GTK_MOZ_EMBED_FLAG_IS_REQUEST)
      mStopLevel = 1;
    if (aStateFlags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT)
       mStopLevel = mStopLevel == 1 ? 2 : 0;
    if (aStateFlags & GTK_MOZ_EMBED_FLAG_IS_WINDOW) {
      mStopLevel = mStopLevel == 2 ? 3 : 0;
    }
  }

  if (aStateFlags & GTK_MOZ_EMBED_FLAG_STOP) {
    if (aStateFlags & GTK_MOZ_EMBED_FLAG_IS_NETWORK) {
      if (sStopSignalTimer)
        g_source_remove(sStopSignalTimer);
      progress_emit_stop(mOwner);
      // let our owner know that the load finished
      mOwner->ContentFinishedLoading();

    } else if (mStopLevel == 3) {
      if (sStopSignalTimer)
        g_source_remove(sStopSignalTimer);
      mStopLevel = 0;
      sStopSignalTimer = g_timeout_add(1000, progress_emit_stop, mOwner);
    }
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
  nsCString tmpString;
  RequestToURIString(aRequest, tmpString);

  // is it the same as the current uri?
  if (mOwner->mURI.Equals(NS_ConvertUTF8toUTF16(tmpString))) {
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[PROGRESS],
                    aCurTotalProgress, aMaxTotalProgress);
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnLocationChange(nsIWebProgress *aWebProgress,
                                nsIRequest     *aRequest,
                                nsIURI         *aLocation)
{
  nsCAutoString newURI;
  nsCAutoString prePath;
  NS_ENSURE_ARG_POINTER(aLocation);
  aLocation->GetSpec(newURI);
  aLocation->GetPrePath(prePath);

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

  if (!isSubFrameLoad) {
    mOwner->SetURI(newURI.get());
    mOwner->mPrePath.Assign(prePath.get());
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[LOCATION]);
  }
  mOwner->mNeedFav = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStatusChange(nsIWebProgress  *aWebProgress,
                              nsIRequest      *aRequest,
                              nsresult         aStatus,
                              const PRUnichar *aMessage)
{
  // need to make a copy so we can safely cast to a void *
  PRUnichar *tmpString = NS_strdup(aMessage);

  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[STATUS_CHANGE],
                  NS_STATIC_CAST(void *, aRequest),
                  NS_STATIC_CAST(gint, aStatus),
                  NS_STATIC_CAST(void *, tmpString));

  NS_Free(tmpString);

  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnSecurityChange(nsIWebProgress *aWebProgress,
                                nsIRequest     *aRequest,
                                PRUint32         aState)
{
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[SECURITY_CHANGE],
                  NS_STATIC_CAST(void *, aRequest),
                  aState);
  return NS_OK;
}

/* static */
void
EmbedProgress::RequestToURIString(nsIRequest *aRequest, nsCString& aString)
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

  uri->GetSpec(aString);
}

nsresult
EmbedProgress::HandleHTTPStatus(nsIRequest *aRequest, const char *aUri, PRBool &aSucceeded)
{
  nsresult rv;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest, &rv));
  aSucceeded = PR_FALSE;

  if (NS_FAILED(rv) || !httpChannel)
    return NS_ERROR_FAILURE;

  rv = httpChannel->GetRequestSucceeded(&aSucceeded);

  if (aSucceeded)
    return NS_OK;

  PRUint32 responseCode = 0;
  nsCString responseText;
  rv = httpChannel->GetResponseStatus(&responseCode);
  // it has to handle more http errors code ??? 401 ? responseCode >= 500 && responseCode <= 505
  rv = httpChannel->GetResponseStatusText(responseText);
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[NETWORK_ERROR],
                  responseCode, responseText.get(), (const gchar*)aUri);
  return rv;
}

