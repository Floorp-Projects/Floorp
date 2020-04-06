/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsIconChannel.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/EndianUtils.h"
#include "nsIIconURI.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsString.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIPipe.h"
#include "nsIOutputStream.h"
#include "nsCExternalHandlerService.h"
#include "nsILocalFileMac.h"
#include "nsIFileURL.h"
#include "nsTArray.h"
#include "nsObjCExceptions.h"
#include "nsProxyRelease.h"
#include "nsContentSecurityManager.h"
#include "nsNetUtil.h"
#include "mozilla/RefPtr.h"

#include <Cocoa/Cocoa.h>

using namespace mozilla;

// nsIconChannel methods
nsIconChannel::nsIconChannel() {}

nsIconChannel::~nsIconChannel() {
  NS_ReleaseOnMainThreadSystemGroup("nsIconChannel::mLoadInfo", mLoadInfo.forget());
}

NS_IMPL_ISUPPORTS(nsIconChannel, nsIChannel, nsIRequest, nsIRequestObserver, nsIStreamListener)

nsresult nsIconChannel::Init(nsIURI* uri) {
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
nsIconChannel::GetName(nsACString& result) { return mUrl->GetSpec(result); }

NS_IMETHODIMP
nsIconChannel::IsPending(bool* result) { return mPump->IsPending(result); }

NS_IMETHODIMP
nsIconChannel::GetStatus(nsresult* status) { return mPump->GetStatus(status); }

NS_IMETHODIMP
nsIconChannel::Cancel(nsresult status) {
  mCanceled = true;
  return mPump->Cancel(status);
}

NS_IMETHODIMP
nsIconChannel::GetCanceled(bool* result) {
  *result = mCanceled;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Suspend(void) { return mPump->Suspend(); }

NS_IMETHODIMP
nsIconChannel::Resume(void) { return mPump->Resume(); }

// nsIRequestObserver methods
NS_IMETHODIMP
nsIconChannel::OnStartRequest(nsIRequest* aRequest) {
  if (mListener) {
    return mListener->OnStartRequest(this);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  if (mListener) {
    mListener->OnStopRequest(this, aStatus);
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
nsIconChannel::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream, uint64_t aOffset,
                               uint32_t aCount) {
  if (mListener) {
    return mListener->OnDataAvailable(this, aStream, aOffset, aCount);
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsIconChannel::GetOriginalURI(nsIURI** aURI) {
  *aURI = mOriginalURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetOriginalURI(nsIURI* aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetURI(nsIURI** aURI) {
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream** _retval) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  return MakeInputStream(_retval, false);
}

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile** aLocalFile, uint32_t* aDesiredImageSize,
                                               nsACString& aContentType,
                                               nsACString& aFileExtension) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI(do_QueryInterface(mUrl, &rv));
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

  nsCOMPtr<nsILocalFileMac> localFileMac(do_QueryInterface(file, &rv));
  if (NS_FAILED(rv) || !localFileMac) return NS_OK;

  *aLocalFile = file;
  NS_IF_ADDREF(*aLocalFile);

  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  MOZ_ASSERT(
      mLoadInfo->GetSecurityMode() == 0 || mLoadInfo->GetInitialSecurityCheckDone() ||
          (mLoadInfo->GetSecurityMode() == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
           mLoadInfo->GetLoadingPrincipal() &&
           mLoadInfo->GetLoadingPrincipal()->IsSystemPrincipal()),
      "security flags in loadInfo but doContentSecurityCheck() not called");

  nsCOMPtr<nsIInputStream> inStream;
  rv = MakeInputStream(getter_AddRefs(inStream), true);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  // Init our stream pump
  nsCOMPtr<nsIEventTarget> target =
      nsContentUtils::GetEventTargetByLoadInfo(mLoadInfo, mozilla::TaskCategory::Other);
  rv = mPump->Init(inStream, 0, 0, false, target);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  rv = mPump->AsyncRead(this, nullptr);
  if (NS_SUCCEEDED(rv)) {
    // Store our real listener
    mListener = aListener;
    // Add ourself to the load group, if available
    if (mLoadGroup) {
      mLoadGroup->AddRequest(this, nullptr);
    }
  } else {
    mCallbacks = nullptr;
  }

  return rv;
}

nsresult nsIconChannel::MakeInputStream(nsIInputStream** _retval, bool aNonBlocking) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsCString contentType;
  nsAutoCString fileExt;
  nsCOMPtr<nsIFile> fileloc;  // file we want an icon for
  uint32_t desiredImageSize;
  nsresult rv =
      ExtractIconInfoFromUrl(getter_AddRefs(fileloc), &desiredImageSize, contentType, fileExt);
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
      iconImage = [[NSWorkspace sharedWorkspace] iconForFile:[(NSURL*)macURL path]];
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
    iconImage = [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeUnknown];
  }

  if (!iconImage) {
    return NS_ERROR_FAILURE;
  }

  if (desiredImageSize > 255) {
    // The Icon image format represents width and height as u8, so it does not
    // allow for images sized 256 or more.
    return NS_ERROR_FAILURE;
  }

  // Set the actual size to *twice* the requested size.
  // We do this because our UI doesn't take the window's device pixel ratio into
  // account when it requests these icons; e.g. it will request an icon with
  // size 16, place it in a 16x16 CSS pixel sized image, and then display it in
  // a window on a HiDPI screen where the icon now covers 32x32 physical screen
  // pixels. So we just always double the size here in order to prevent blurriness.
  uint32_t size = (desiredImageSize < 128) ? desiredImageSize * 2 : desiredImageSize;
  uint32_t width = size;
  uint32_t height = size;

  // The "image format" we're outputting here (and which gets decoded by
  // nsIconDecoder) has the following format:
  //  - 1 byte for the image width, as u8
  //  - 1 byte for the image height, as u8
  //  - the raw image data as BGRA, width * height * 4 bytes.
  size_t bufferCapacity = 4 + width * height * 4;
  UniquePtr<uint8_t[]> fileBuf = MakeUnique<uint8_t[]>(bufferCapacity);
  fileBuf[0] = uint8_t(width);
  fileBuf[1] = uint8_t(height);
  fileBuf[2] = uint8_t(mozilla::gfx::SurfaceFormat::B8G8R8A8);
  fileBuf[3] = 0;
  uint8_t* imageBuf = &fileBuf[4];

  // Create a CGBitmapContext around imageBuf and draw iconImage to it.
  // This gives us the image data in the format we want: BGRA, four bytes per
  // pixel, in host endianness, with premultiplied alpha.
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx =
      CGBitmapContextCreate(imageBuf, width, height, 8 /* bitsPerComponent */, width * 4, cs,
                            kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  CGColorSpaceRelease(cs);

  NSGraphicsContext* oldContext = [NSGraphicsContext currentContext];
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx
                                                                                  flipped:NO]];

  [iconImage drawInRect:NSMakeRect(0, 0, width, height)];

  [NSGraphicsContext setCurrentContext:oldContext];

  CGContextRelease(ctx);

  // Now, create a pipe and stuff our data into it
  nsCOMPtr<nsIInputStream> inStream;
  nsCOMPtr<nsIOutputStream> outStream;
  rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream), bufferCapacity,
                  bufferCapacity, aNonBlocking);

  if (NS_SUCCEEDED(rv)) {
    uint32_t written;
    rv = outStream->Write((char*)fileBuf.get(), bufferCapacity, &written);
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
nsIconChannel::GetLoadFlags(uint32_t* aLoadAttributes) {
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP
nsIconChannel::SetLoadFlags(uint32_t aLoadAttributes) {
  return mPump->SetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP
nsIconChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) { return GetTRRModeImpl(aTRRMode); }

NS_IMETHODIMP
nsIconChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) { return SetTRRModeImpl(aTRRMode); }

NS_IMETHODIMP
nsIconChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP
nsIconChannel::GetContentType(nsACString& aContentType) {
  aContentType.AssignLiteral(IMAGE_ICON_MS);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString& aContentType) {
  // It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::GetContentCharset(nsACString& aContentCharset) {
  aContentCharset.AssignLiteral(IMAGE_ICON_MS);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString& aContentCharset) {
  // It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDisposition(uint32_t* aContentDisposition) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::SetContentDisposition(uint32_t aContentDisposition) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDispositionFilename(nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::SetContentDispositionFilename(const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDispositionHeader(nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentLength(int64_t* aContentLength) {
  *aContentLength = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::SetContentLength(int64_t aContentLength) {
  MOZ_ASSERT_UNREACHABLE("nsIconChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIconChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetOwner(nsISupports** aOwner) {
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetOwner(nsISupports* aOwner) {
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_RELEASE_ASSERT(aLoadInfo, "loadinfo can't be null");
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetNotificationCallbacks(nsIInterfaceRequestor** aNotificationCallbacks) {
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks) {
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetSecurityInfo(nsISupports** aSecurityInfo) {
  *aSecurityInfo = nullptr;
  return NS_OK;
}
