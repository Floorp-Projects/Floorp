/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Brian Ryner.
 * Portions created by Brian Ryner are Copyright (C) 2000 Brian Ryner.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com>
 */

#include "inBitmapChannel.h"
#include "inIBitmapURI.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "inIBitmap.h"
#include "inIBitmapDepot.h"

inBitmapChannel::inBitmapChannel()
{
  NS_INIT_ISUPPORTS();
  mStatus = NS_OK;
}

inBitmapChannel::~inBitmapChannel() 
{}

NS_IMPL_THREADSAFE_ISUPPORTS2(inBitmapChannel, 
                              nsIChannel, 
                              nsIRequest)

nsresult
inBitmapChannel::Init(nsIURI* uri)
{
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest

NS_IMETHODIMP 
inBitmapChannel::GetName(nsACString &result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inBitmapChannel::IsPending(PRBool *result)
{
  NS_NOTREACHED("inBitmapChannel::IsPending");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inBitmapChannel::GetStatus(nsresult *status)
{
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::Cancel(nsresult status)
{
  NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
  nsresult rv = NS_ERROR_FAILURE;

  mStatus = status;
  return rv;
}

NS_IMETHODIMP 
inBitmapChannel::Suspend(void)
{
  NS_NOTREACHED("inBitmapChannel::Suspend");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inBitmapChannel::Resume(void)
{
  NS_NOTREACHED("inBitmapChannel::Resume");
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP 
inBitmapChannel::GetOriginalURI(nsIURI* *aURI)
{
  *aURI = mOriginalURI ? mOriginalURI : mUrl;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::SetOriginalURI(nsIURI* aURI)
{
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
inBitmapChannel::Open(nsIInputStream **_retval)
{
  printf("A OPENING BITMAP STREAM\n");
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP inBitmapChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<inIBitmapDepot> depot(do_GetService("@mozilla.org/inspector/bitmap-depot;1"));
  if (!depot)
    return NS_ERROR_FAILURE;
  nsCOMPtr<inIBitmapURI> uri(do_QueryInterface(mUrl));
  nsXPIDLString durl;
  uri->GetBitmapName(getter_Copies(durl));
  nsCOMPtr<inIBitmap> bitmap;
  depot->Get(durl, getter_AddRefs(bitmap));

  if (!bitmap)
    return NS_ERROR_FAILURE;
  PRUint32 w, h;
  bitmap->GetWidth(&w);
  bitmap->GetHeight(&h);  
  PRUint8* bitmapBuf;
  bitmap->GetBits(&bitmapBuf);
  
  aListener->OnStartRequest(this, ctxt);

  PRUint32 length = (w*h*3) + (sizeof(PRUint32)*2);
  PRUint8* buffer = new PRUint8[length];
  PRUint8* bufStart = buffer;
  PRUint32* wbuf = (PRUint32*) buffer;
  wbuf[0] = w;
  wbuf[1] = h;
  buffer = (PRUint8*)(wbuf+2);
  
  for (PRUint32 i = 0; i < w*h; ++i) {
    buffer[0] = bitmapBuf[0];
    buffer[1] = bitmapBuf[1];
    buffer[2] = bitmapBuf[2];
    buffer += 3;
    bitmapBuf += 3;
  }
  
  // turn our string into a stream...and make the appropriate calls on our consumer
  nsCOMPtr<nsISupports> streamSupports;
  NS_NewByteInputStream(getter_AddRefs(streamSupports), (const char*)bufStart, length);
  nsCOMPtr<nsIInputStream> inputStr (do_QueryInterface(streamSupports));
  aListener->OnDataAvailable(this, ctxt, inputStr, 0, length);
  aListener->OnStopRequest(this, ctxt, NS_OK);

  return NS_OK;
}

NS_IMETHODIMP inBitmapChannel::GetLoadFlags(PRUint32 *aLoadAttributes)
{
  *aLoadAttributes = mLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP inBitmapChannel::SetLoadFlags(PRUint32 aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP inBitmapChannel::GetContentType(nsACString &aContentType) 
{
  aContentType = NS_LITERAL_CSTRING("image/inspector-bitmap");
  return NS_OK;
}

NS_IMETHODIMP inBitmapChannel::SetContentType(const nsACString &aContentType)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP inBitmapChannel::GetContentCharset(nsACString &aContentCharset)
{
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP inBitmapChannel::SetContentCharset(const nsACString &aContentCharset)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP inBitmapChannel::GetContentLength(PRInt32 *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::SetContentLength(PRInt32 aContentLength)
{
  NS_NOTREACHED("inBitmapChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inBitmapChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::GetOwner(nsISupports* *aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP 
inBitmapChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nsnull;
  return NS_OK;
}

