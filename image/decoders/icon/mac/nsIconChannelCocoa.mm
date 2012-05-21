/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "plstr.h"
#include "nsILocalFileMac.h"
#include "nsIFileURL.h"
#include "nsTArray.h"
#include "nsObjCExceptions.h"

#include <Cocoa/Cocoa.h>

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
}

nsIconChannel::~nsIconChannel() 
{}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsIconChannel, 
                              nsIChannel, 
                              nsIRequest,
			       nsIRequestObserver,
			       nsIStreamListener)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  mOriginalURI = uri;
  nsresult rv;
  mPump = do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(nsACString &result)
{
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP nsIconChannel::IsPending(bool *result)
{
  return mPump->IsPending(result);
}

NS_IMETHODIMP nsIconChannel::GetStatus(nsresult *status)
{
  return mPump->GetStatus(status);
}

NS_IMETHODIMP nsIconChannel::Cancel(nsresult status)
{
  return mPump->Cancel(status);
}

NS_IMETHODIMP nsIconChannel::Suspend(void)
{
  return mPump->Suspend();
}

NS_IMETHODIMP nsIconChannel::Resume(void)
{
  return mPump->Resume();
}

// nsIRequestObserver methods
NS_IMETHODIMP nsIconChannel::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  if (mListener)
    return mListener->OnStartRequest(this, aContext);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatus)
{
  if (mListener) {
    mListener->OnStopRequest(this, aContext, aStatus);
    mListener = nsnull;
  }

  // Remove from load group
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, aStatus);

  return NS_OK;
}

// nsIStreamListener methods
NS_IMETHODIMP nsIconChannel::OnDataAvailable(nsIRequest* aRequest,
                                             nsISupports* aContext,
                                             nsIInputStream* aStream,
                                             PRUint32 aOffset,
                                             PRUint32 aCount)
{
  if (mListener)
    return mListener->OnDataAvailable(this, aContext, aStream, aOffset, aCount);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP nsIconChannel::GetOriginalURI(nsIURI* *aURI)
{
  *aURI = mOriginalURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOriginalURI(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream **_retval)
{
  return MakeInputStream(_retval, false);
}

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile ** aLocalFile, PRUint32 * aDesiredImageSize, nsACString &aContentType, nsACString &aFileExtension)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI (do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  iconURI->GetImageSize(aDesiredImageSize);
  iconURI->GetContentType(aContentType);
  iconURI->GetFileExtension(aFileExtension);
  
  nsCOMPtr<nsIURL> url;
  rv = iconURI->GetIconURL(getter_AddRefs(url));
  if (NS_FAILED(rv) || !url) return NS_OK;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(url, &rv);
  if (NS_FAILED(rv) || !fileURL) return NS_OK;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) return NS_OK;
  
  nsCOMPtr<nsILocalFileMac> localFileMac (do_QueryInterface(file, &rv));
  if (NS_FAILED(rv) || !localFileMac) return NS_OK;
  
  *aLocalFile = file;
  NS_IF_ADDREF(*aLocalFile);

  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Init our stream pump
  rv = mPump->Init(inStream, PRInt64(-1), PRInt64(-1), 0, 0, false);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mPump->AsyncRead(this, ctxt);
  if (NS_SUCCEEDED(rv)) {
    // Store our real listener
    mListener = aListener;
    // Add ourself to the load group, if available
    if (mLoadGroup)
      mLoadGroup->AddRequest(this, nsnull);
  }

  return rv;
}

nsresult nsIconChannel::MakeInputStream(nsIInputStream** _retval, bool nonBlocking)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsXPIDLCString contentType;
  nsCAutoString fileExt;
  nsCOMPtr<nsIFile> fileloc; // file we want an icon for
  PRUint32 desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(fileloc), &desiredImageSize, contentType, fileExt);
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure that we DO NOT resolve aliases, very important for file views
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(fileloc);
  if (localFile)
    localFile->SetFollowLinks(false);

  bool fileExists = false;
  if (fileloc)
    localFile->Exists(&fileExists);

  NSImage* iconImage = nil;
  
  // first try to get the icon from the file if it exists
  if (fileExists) {
    nsCOMPtr<nsILocalFileMac> localFileMac(do_QueryInterface(fileloc, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    
    CFURLRef macURL;
    if (NS_SUCCEEDED(localFileMac->GetCFURL(&macURL))) {
      iconImage = [[NSWorkspace sharedWorkspace] iconForFile:[(NSURL*)macURL path]];
      ::CFRelease(macURL);
    }
  }

  // if we don't have an icon yet try to get one by extension
  if (!iconImage && !fileExt.IsEmpty()) {
    NSString* fileExtension = [NSString stringWithUTF8String:PromiseFlatCString(fileExt).get()];
    iconImage = [[NSWorkspace sharedWorkspace] iconForFileType:fileExtension];
  }

  // If we still don't have an icon, get the generic document icon.
  if (!iconImage)
    iconImage = [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeUnknown];

  if (!iconImage)
    return NS_ERROR_FAILURE;
  
  // we have an icon now, size it
  NSRect desiredSizeRect = NSMakeRect(0, 0, desiredImageSize, desiredImageSize);
  [iconImage setSize:desiredSizeRect.size];
  
  [iconImage lockFocus];
  NSBitmapImageRep* bitmapRep = [[[NSBitmapImageRep alloc] initWithFocusedViewRect:desiredSizeRect] autorelease];
  [iconImage unlockFocus];
  
  // we expect the following things to be true about our bitmapRep
  NS_ENSURE_TRUE(![bitmapRep isPlanar] &&
                 (unsigned int)[bitmapRep bytesPerPlane] == desiredImageSize * desiredImageSize * 4 &&
                 [bitmapRep bitsPerPixel] == 32 &&
                 [bitmapRep samplesPerPixel] == 4 &&
                 [bitmapRep hasAlpha] == YES,
                 NS_ERROR_UNEXPECTED);
  
  // rgba, pre-multiplied data
  PRUint8* bitmapRepData = (PRUint8*)[bitmapRep bitmapData];
  
  // create our buffer
  PRInt32 bufferCapacity = 2 + desiredImageSize * desiredImageSize * 4;
  nsAutoTArray<PRUint8, 3 + 16 * 16 * 5> iconBuffer; // initial size is for 16x16
  if (!iconBuffer.SetLength(bufferCapacity))
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRUint8* iconBufferPtr = iconBuffer.Elements();
  
  // write header data into buffer
  *iconBufferPtr++ = desiredImageSize;
  *iconBufferPtr++ = desiredImageSize;

  PRUint32 dataCount = (desiredImageSize * desiredImageSize) * 4;
  PRUint32 index = 0;
  while (index < dataCount) {
    // get data from the bitmap
    PRUint8 r = bitmapRepData[index++];
    PRUint8 g = bitmapRepData[index++];
    PRUint8 b = bitmapRepData[index++];
    PRUint8 a = bitmapRepData[index++];

    // write data out to our buffer
    // non-cairo uses native image format, but the A channel is ignored.
    // cairo uses ARGB (highest to lowest bits)
#if defined(IS_LITTLE_ENDIAN)
    *iconBufferPtr++ = b;
    *iconBufferPtr++ = g;
    *iconBufferPtr++ = r;
    *iconBufferPtr++ = a;
#else
    *iconBufferPtr++ = a;
    *iconBufferPtr++ = r;
    *iconBufferPtr++ = g;
    *iconBufferPtr++ = b;
#endif
  }

  NS_ASSERTION(iconBufferPtr == iconBuffer.Elements() + bufferCapacity,
               "buffer size miscalculation");
  
  // Now, create a pipe and stuff our data into it
  nsCOMPtr<nsIInputStream> inStream;
  nsCOMPtr<nsIOutputStream> outStream;
  rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream), bufferCapacity, bufferCapacity, nonBlocking);  

  if (NS_SUCCEEDED(rv)) {
    PRUint32 written;
    rv = outStream->Write((char*)iconBuffer.Elements(), bufferCapacity, &written);
    if (NS_SUCCEEDED(rv))
      NS_IF_ADDREF(*_retval = inStream);
  }

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nsnull;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsIconChannel::GetLoadFlags(PRUint32 *aLoadAttributes)
{
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP nsIconChannel::SetLoadFlags(PRUint32 aLoadAttributes)
{
  return mPump->SetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP nsIconChannel::GetContentType(nsACString &aContentType) 
{
  aContentType.AssignLiteral("image/icon");
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString &aContentType)
{
  //It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentCharset(nsACString &aContentCharset) 
{
  aContentCharset.AssignLiteral("image/icon");
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString &aContentCharset)
{
  //It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDisposition(PRUint32 *aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsIconChannel::GetContentLength(PRInt32 *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetContentLength(PRInt32 aContentLength)
{
  NS_NOTREACHED("nsIconChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetOwner(nsISupports* *aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nsnull;
  return NS_OK;
}

