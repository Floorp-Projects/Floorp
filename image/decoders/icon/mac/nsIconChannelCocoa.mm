/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsIconChannel.h"
#include "mozilla/Endian.h"
#include "nsIIconURI.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIStringStream.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIPipe.h"
#include "nsIOutputStream.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsILocalFileMac.h"
#include "nsIFileURL.h"
#include "nsTArray.h"
#include "nsObjCExceptions.h"
#include "nsProxyRelease.h"
#include "nsContentSecurityManager.h"

#include <Cocoa/Cocoa.h>

// nsIconChannel methods
nsIconChannel::nsIconChannel()
{
}

nsIconChannel::~nsIconChannel()
{
  if (mLoadInfo) {
    NS_ReleaseOnMainThread(mLoadInfo.forget());
  }
}

NS_IMPL_ISUPPORTS(nsIconChannel,
                  nsIChannel,
                  nsIRequest,
                  nsIRequestObserver,
                  nsIStreamListener)

nsresult
nsIconChannel::Init(nsIURI* uri)
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

NS_IMETHODIMP
nsIconChannel::GetName(nsACString& result)
{
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP
nsIconChannel::IsPending(bool* result)
{
  return mPump->IsPending(result);
}

NS_IMETHODIMP
nsIconChannel::GetStatus(nsresult* status)
{
  return mPump->GetStatus(status);
}

NS_IMETHODIMP
nsIconChannel::Cancel(nsresult status)
{
  return mPump->Cancel(status);
}

NS_IMETHODIMP
nsIconChannel::Suspend(void)
{
  return mPump->Suspend();
}

NS_IMETHODIMP
nsIconChannel::Resume(void)
{
  return mPump->Resume();
}

// nsIRequestObserver methods
NS_IMETHODIMP
nsIconChannel::OnStartRequest(nsIRequest* aRequest,
                                            nsISupports* aContext)
{
  if (mListener) {
    return mListener->OnStartRequest(this, aContext);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::OnStopRequest(nsIRequest* aRequest,
                                           nsISupports* aContext,
                                           nsresult aStatus)
{
  if (mListener) {
    mListener->OnStopRequest(this, aContext, aStatus);
    mListener = nullptr;
  }

  // Remove from load group
  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, aStatus);
  }

  return NS_OK;
}

// nsIStreamListener methods
NS_IMETHODIMP
nsIconChannel::OnDataAvailable(nsIRequest* aRequest,
                                             nsISupports* aContext,
                                             nsIInputStream* aStream,
                                             uint64_t aOffset,
                                             uint32_t aCount)
{
  if (mListener) {
    return mListener->OnDataAvailable(this, aContext, aStream, aOffset, aCount);
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsIconChannel::GetOriginalURI(nsIURI** aURI)
{
  *aURI = mOriginalURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetOriginalURI(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetURI(nsIURI** aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream** _retval)
{
  return MakeInputStream(_retval, false);
}

NS_IMETHODIMP
nsIconChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

nsresult
nsIconChannel::ExtractIconInfoFromUrl(nsIFile** aLocalFile,
                                               uint32_t* aDesiredImageSize,
                                               nsACString& aContentType,
                                               nsACString& aFileExtension)
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

NS_IMETHODIMP
nsIconChannel::AsyncOpen(nsIStreamListener* aListener,
                                       nsISupports* ctxt)
{
  MOZ_ASSERT(!mLoadInfo ||
             mLoadInfo->GetSecurityMode() == 0 ||
             mLoadInfo->GetInitialSecurityCheckDone() ||
             (mLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
              nsContentUtils::IsSystemPrincipal(mLoadInfo->LoadingPrincipal())),
             "security flags in loadInfo but asyncOpen2() not called");

  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Init our stream pump
  rv = mPump->Init(inStream, int64_t(-1), int64_t(-1), 0, 0, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPump->AsyncRead(this, ctxt);
  if (NS_SUCCEEDED(rv)) {
    // Store our real listener
    mListener = aListener;
    // Add ourself to the load group, if available
    if (mLoadGroup) {
      mLoadGroup->AddRequest(this, nullptr);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsIconChannel::AsyncOpen2(nsIStreamListener* aListener)
{
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(listener, nullptr);
}

nsresult
nsIconChannel::MakeInputStream(nsIInputStream** _retval,
                                        bool aNonBlocking)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsXPIDLCString contentType;
  nsAutoCString fileExt;
  nsCOMPtr<nsIFile> fileloc; // file we want an icon for
  uint32_t desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(fileloc),
                                       &desiredImageSize, contentType, fileExt);
  NS_ENSURE_SUCCESS(rv, rv);

  bool fileExists = false;
  if (fileloc) {
    // ensure that we DO NOT resolve aliases, very important for file views
    fileloc->SetFollowLinks(false);
    fileloc->Exists(&fileExists);
  }

  NSImage* iconImage = nil;

  // first try to get the icon from the file if it exists
  if (fileExists) {
    nsCOMPtr<nsILocalFileMac> localFileMac(do_QueryInterface(fileloc, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    CFURLRef macURL;
    if (NS_SUCCEEDED(localFileMac->GetCFURL(&macURL))) {
      iconImage = [[NSWorkspace sharedWorkspace]
                   iconForFile:[(NSURL*)macURL path]];
      ::CFRelease(macURL);
    }
  }

  // if we don't have an icon yet try to get one by extension
  if (!iconImage && !fileExt.IsEmpty()) {
    NSString* fileExtension = [NSString stringWithUTF8String:fileExt.get()];
    iconImage = [[NSWorkspace sharedWorkspace] iconForFileType:fileExtension];
  }

  // If we still don't have an icon, get the generic document icon.
  if (!iconImage) {
    iconImage = [[NSWorkspace sharedWorkspace]
                 iconForFileType:NSFileTypeUnknown];
  }

  if (!iconImage) {
    return NS_ERROR_FAILURE;
  }

  // we have an icon now, size it
  NSRect desiredSizeRect = NSMakeRect(0, 0, desiredImageSize, desiredImageSize);
  [iconImage setSize:desiredSizeRect.size];

  [iconImage lockFocus];
  NSBitmapImageRep* bitmapRep = [[[NSBitmapImageRep alloc]
                                  initWithFocusedViewRect:desiredSizeRect]
                                  autorelease];
  [iconImage unlockFocus];

  // we expect the following things to be true about our bitmapRep
  NS_ENSURE_TRUE(![bitmapRep isPlanar] &&
                 // Not necessarily: on a HiDPI-capable system, we'll get
                 // a 2x bitmap
                 // (unsigned int)[bitmapRep bytesPerPlane] ==
                 //    desiredImageSize * desiredImageSize * 4 &&
                 [bitmapRep bitsPerPixel] == 32 &&
                 [bitmapRep samplesPerPixel] == 4 &&
                 [bitmapRep hasAlpha] == YES,
                 NS_ERROR_UNEXPECTED);

  // check what size we actually got, and ensure it isn't too big to return
  uint32_t actualImageSize = [bitmapRep bytesPerRow] / 4;
  NS_ENSURE_TRUE(actualImageSize < 256, NS_ERROR_UNEXPECTED);

  // now we can validate the amount of data
  NS_ENSURE_TRUE((unsigned int)[bitmapRep bytesPerPlane] ==
                 actualImageSize * actualImageSize * 4,
                 NS_ERROR_UNEXPECTED);

  // rgba, pre-multiplied data
  uint8_t* bitmapRepData = (uint8_t*)[bitmapRep bitmapData];

  // create our buffer
  int32_t bufferCapacity = 2 + [bitmapRep bytesPerPlane];
  AutoTArray<uint8_t, 3 + 16 * 16 * 5> iconBuffer; // initial size is for
                                                     // 16x16
  iconBuffer.SetLength(bufferCapacity);

  uint8_t* iconBufferPtr = iconBuffer.Elements();

  // write header data into buffer
  *iconBufferPtr++ = actualImageSize;
  *iconBufferPtr++ = actualImageSize;

  uint32_t dataCount = [bitmapRep bytesPerPlane];
  uint32_t index = 0;
  while (index < dataCount) {
    // get data from the bitmap
    uint8_t r = bitmapRepData[index++];
    uint8_t g = bitmapRepData[index++];
    uint8_t b = bitmapRepData[index++];
    uint8_t a = bitmapRepData[index++];

    // write data out to our buffer
    // non-cairo uses native image format, but the A channel is ignored.
    // cairo uses ARGB (highest to lowest bits)
#if MOZ_LITTLE_ENDIAN
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
  rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream),
                  bufferCapacity, bufferCapacity, aNonBlocking);

  if (NS_SUCCEEDED(rv)) {
    uint32_t written;
    rv = outStream->Write((char*)iconBuffer.Elements(), bufferCapacity,
                          &written);
    if (NS_SUCCEEDED(rv)) {
      NS_IF_ADDREF(*_retval = inStream);
    }
  }

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsIconChannel::GetLoadFlags(uint32_t* aLoadAttributes)
{
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP
nsIconChannel::SetLoadFlags(uint32_t aLoadAttributes)
{
  return mPump->SetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP
nsIconChannel::GetContentType(nsACString& aContentType)
{
  aContentType.AssignLiteral(IMAGE_ICON_MS);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString& aContentType)
{
  //It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::GetContentCharset(nsACString& aContentCharset)
{
  aContentCharset.AssignLiteral(IMAGE_ICON_MS);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString& aContentCharset)
{
  //It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDisposition(uint32_t* aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::
  GetContentDispositionFilename(nsAString& aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::
  SetContentDispositionFilename(const nsAString& aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::
  GetContentDispositionHeader(nsACString& aContentDispositionHeader)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentLength(int64_t* aContentLength)
{
  *aContentLength = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::SetContentLength(int64_t aContentLength)
{
  NS_NOTREACHED("nsIconChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIconChannel::GetLoadGroup(nsILoadGroup** aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetOwner(nsISupports** aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetLoadInfo(nsILoadInfo** aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::
  GetNotificationCallbacks(nsIInterfaceRequestor** aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::
  SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetSecurityInfo(nsISupports** aSecurityInfo)
{
  *aSecurityInfo = nullptr;
  return NS_OK;
}

