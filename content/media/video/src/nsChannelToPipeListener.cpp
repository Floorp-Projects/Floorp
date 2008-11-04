/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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
#include "nsAString.h"
#include "nsNetUtil.h"
#include "nsMediaDecoder.h"
#include "nsIScriptSecurityManager.h"
#include "nsChannelToPipeListener.h"
#include "nsICachingChannel.h"

#define HTTP_OK_CODE 200
#define HTTP_PARTIAL_RESPONSE_CODE 206

nsChannelToPipeListener::nsChannelToPipeListener(
    nsMediaDecoder* aDecoder,
    PRBool aSeeking) :
  mDecoder(aDecoder),
  mIntervalStart(0),
  mIntervalEnd(0),
  mTotalBytes(0),
  mSeeking(aSeeking)
{
}

nsresult nsChannelToPipeListener::Init() 
{
  nsresult rv = NS_NewPipe(getter_AddRefs(mInput), 
                           getter_AddRefs(mOutput),
                           0, 
                           PR_UINT32_MAX);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void nsChannelToPipeListener::Stop()
{
  mDecoder = nsnull;
  mInput = nsnull;
  mOutput = nsnull;
}

void nsChannelToPipeListener::Cancel()
{
  if (mOutput)
    mOutput->Close();

  if (mInput)
    mInput->Close();
}

double nsChannelToPipeListener::BytesPerSecond() const
{
  return mOutput ? mTotalBytes / ((PR_IntervalToMilliseconds(mIntervalEnd-mIntervalStart)) / 1000.0) : NS_MEDIA_UNKNOWN_RATE;
}

nsresult nsChannelToPipeListener::GetInputStream(nsIInputStream** aStream)
{
  NS_IF_ADDREF(*aStream = mInput);
  return NS_OK;
}

nsresult nsChannelToPipeListener::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  mIntervalStart = PR_IntervalNow();
  mIntervalEnd = mIntervalStart;
  mTotalBytes = 0;
  mDecoder->UpdateBytesDownloaded(mTotalBytes);
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(aRequest);
  if (hc) {
    PRUint32 responseStatus = 0; 
    hc->GetResponseStatus(&responseStatus);
    if (mSeeking && responseStatus == HTTP_OK_CODE) {
      // If we get an OK response but we were seeking,
      // and therefore expecting a partial response of
      // HTTP_PARTIAL_RESPONSE_CODE, tell the decoder
      // we don't support seeking.
      mDecoder->SetSeekable(PR_FALSE);
    }
    else if (!mSeeking && 
             (responseStatus == HTTP_OK_CODE ||
              responseStatus == HTTP_PARTIAL_RESPONSE_CODE)) {
      // We weren't seeking and got a valid response status,
      // set the length of the content.
      PRInt32 cl = 0;
      hc->GetContentLength(&cl);
      mDecoder->SetTotalBytes(cl);

      // If we get an HTTP_OK_CODE response to our byte range
      // request, then we don't support seeking.
      mDecoder->SetSeekable(responseStatus == HTTP_PARTIAL_RESPONSE_CODE);
    }
  }

  nsCOMPtr<nsICachingChannel> cc = do_QueryInterface(aRequest);
  if (cc) {
    PRBool fromCache = PR_FALSE;
    nsresult rv = cc->IsFromCache(&fromCache);

    if (NS_SUCCEEDED(rv) && !fromCache) {
      cc->SetCacheAsFile(PR_TRUE);
    }
  }

  /* Get our principal */
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  if (chan) {
    nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService("@mozilla.org/scriptsecuritymanager;1");
    if (secMan) {
      nsresult rv = secMan->GetChannelPrincipal(chan,
                                                getter_AddRefs(mPrincipal));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return NS_OK;
}

nsresult nsChannelToPipeListener::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatus) 
{
  mOutput = nsnull;
  if (mDecoder) {
    mDecoder->ResourceLoaded();
  }
  return NS_OK;
}

nsresult nsChannelToPipeListener::OnDataAvailable(nsIRequest* aRequest, 
                                                nsISupports* aContext, 
                                                nsIInputStream* aStream,
                                                PRUint32 aOffset,
                                                PRUint32 aCount)
{
  if (!mOutput)
    return NS_ERROR_FAILURE;

  PRUint32 bytes = 0;
  
  do {
    nsresult rv = mOutput->WriteFrom(aStream, aCount, &bytes);
    NS_ENSURE_SUCCESS(rv, rv);
    
    aCount -= bytes;
    mTotalBytes += bytes;
    mDecoder->UpdateBytesDownloaded(mTotalBytes);
  } while (aCount) ;
  
  nsresult rv = mOutput->Flush();
  NS_ENSURE_SUCCESS(rv, rv);
  mIntervalEnd = PR_IntervalNow();
  return NS_OK;
}

nsIPrincipal*
nsChannelToPipeListener::GetCurrentPrincipal()
{
  return mPrincipal;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsChannelToPipeListener, nsIRequestObserver, nsIStreamListener)

