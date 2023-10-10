/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Monitor.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WindowsProcessMitigations.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/ByteBuf.h"

#include "nsComponentManagerUtils.h"
#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsMimeTypes.h"
#include "nsIURL.h"
#include "nsIPipe.h"
#include "nsNetCID.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIIconURI.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsProxyRelease.h"
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

#include "Decoder.h"
#include "DecodePool.h"

// we need windows.h to read out registry information...
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <wchar.h>

using namespace mozilla;
using namespace mozilla::image;

using mozilla::ipc::ByteBuf;

struct ICONFILEHEADER {
  uint16_t ifhReserved;
  uint16_t ifhType;
  uint16_t ifhCount;
};

struct ICONENTRY {
  int8_t ieWidth;
  int8_t ieHeight;
  uint8_t ieColors;
  uint8_t ieReserved;
  uint16_t iePlanes;
  uint16_t ieBitCount;
  uint32_t ieSizeImage;
  uint32_t ieFileOffset;
};

struct IconPathInfo {
  nsCOMPtr<nsIFile> localFile;
  nsAutoString filePath;
  UINT infoFlags = 0;
};

using HIconPromise = MozPromise<HICON, nsresult, true>;

static UINT GetSizeInfoFlag(uint32_t aDesiredImageSize) {
  return aDesiredImageSize > 16 ? SHGFI_SHELLICONSIZE : SHGFI_SMALLICON;
}

static nsresult ExtractIconInfoFromUrl(nsIURI* aUrl, nsIFile** aLocalFile,
                                       uint32_t* aDesiredImageSize,
                                       nsCString& aContentType,
                                       nsCString& aFileExtension) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI(do_QueryInterface(aUrl, &rv));
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

  return file->Clone(aLocalFile);
}

static nsresult ExtractIconPathInfoFromUrl(nsIURI* aUrl,
                                           IconPathInfo* aIconPathInfo) {
  nsCString contentType;
  nsCString fileExt;
  nsCOMPtr<nsIFile> localFile;  // file we want an icon for
  uint32_t desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(aUrl, getter_AddRefs(localFile),
                                       &desiredImageSize, contentType, fileExt);
  NS_ENSURE_SUCCESS(rv, rv);

  // if the file exists, we are going to use it's real attributes...
  // otherwise we only want to use it for it's extension...
  UINT infoFlags = SHGFI_ICON;

  bool fileExists = false;

  nsAutoString filePath;
  CopyASCIItoUTF16(fileExt, filePath);
  if (localFile) {
    rv = localFile->Normalize();
    NS_ENSURE_SUCCESS(rv, rv);

    localFile->GetPath(filePath);
    if (filePath.Length() < 2 || filePath[1] != ':') {
      return NS_ERROR_MALFORMED_URI;  // UNC
    }

    if (filePath.Last() == ':') {
      filePath.Append('\\');
    } else {
      localFile->Exists(&fileExists);
      if (!fileExists) {
        localFile->GetLeafName(filePath);
      }
    }
  }

  if (!fileExists) {
    infoFlags |= SHGFI_USEFILEATTRIBUTES;
  }

  infoFlags |= GetSizeInfoFlag(desiredImageSize);

  // if we have a content type... then use it! but for existing files,
  // we want to show their real icon.
  if (!fileExists && !contentType.IsEmpty()) {
    nsCOMPtr<nsIMIMEService> mimeService(
        do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString defFileExt;
    mimeService->GetPrimaryExtension(contentType, fileExt, defFileExt);
    // If the mime service does not know about this mime type, we show
    // the generic icon.
    // In any case, we need to insert a '.' before the extension.
    filePath = u"."_ns + NS_ConvertUTF8toUTF16(defFileExt);
  }

  if (!localFile && !fileExists &&
      ((filePath.Length() == 1 && filePath.Last() == '.') ||
       filePath.Length() == 0)) {
    filePath = u".MozBogusExtensionMoz"_ns;
  }

  aIconPathInfo->localFile = std::move(localFile);
  aIconPathInfo->filePath = std::move(filePath);
  aIconPathInfo->infoFlags = infoFlags;

  return NS_OK;
}

static bool GetSpecialFolderIcon(nsIFile* aFile, int aFolder, UINT aInfoFlags,
                                 HICON* aIcon) {
  if (!aFile) {
    return false;
  }

  wchar_t fileNativePath[MAX_PATH];
  nsAutoString fileNativePathStr;
  aFile->GetPath(fileNativePathStr);
  ::GetShortPathNameW(fileNativePathStr.get(), fileNativePath,
                      ArrayLength(fileNativePath));

  struct IdListDeleter {
    void operator()(ITEMIDLIST* ptr) { ::CoTaskMemFree(ptr); }
  };

  UniquePtr<ITEMIDLIST, IdListDeleter> idList;
  HRESULT hr =
      ::SHGetSpecialFolderLocation(nullptr, aFolder, getter_Transfers(idList));
  if (FAILED(hr)) {
    return false;
  }

  wchar_t specialNativePath[MAX_PATH];
  ::SHGetPathFromIDListW(idList.get(), specialNativePath);
  ::GetShortPathNameW(specialNativePath, specialNativePath,
                      ArrayLength(specialNativePath));

  if (wcsicmp(fileNativePath, specialNativePath) != 0) {
    return false;
  }

  SHFILEINFOW sfi = {};
  aInfoFlags |= (SHGFI_PIDL | SHGFI_SYSICONINDEX);
  if (::SHGetFileInfoW((LPCWSTR)(LPCITEMIDLIST)idList.get(), 0, &sfi,
                       sizeof(sfi), aInfoFlags) == 0) {
    return false;
  }

  *aIcon = sfi.hIcon;
  return true;
}

static nsresult GetIconHandleFromPathInfo(const IconPathInfo& aPathInfo,
                                          HICON* aIcon) {
  MOZ_DIAGNOSTIC_ASSERT(!IsWin32kLockedDown());

  // Is this the "Desktop" folder?
  if (GetSpecialFolderIcon(aPathInfo.localFile, CSIDL_DESKTOP,
                           aPathInfo.infoFlags, aIcon)) {
    return NS_OK;
  }

  // Is this the "My Documents" folder?
  if (GetSpecialFolderIcon(aPathInfo.localFile, CSIDL_PERSONAL,
                           aPathInfo.infoFlags, aIcon)) {
    return NS_OK;
  }

  // There are other "Special Folders" and Namespace entities that we
  // are not fetching icons for, see:
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/
  //        shellcc/platform/shell/reference/enums/csidl.asp
  // If we ever need to get them, code to do so would be inserted here.

  // Not a special folder, or something else failed above.
  SHFILEINFOW sfi = {};
  if (::SHGetFileInfoW(aPathInfo.filePath.get(), FILE_ATTRIBUTE_ARCHIVE, &sfi,
                       sizeof(sfi), aPathInfo.infoFlags) != 0) {
    *aIcon = sfi.hIcon;
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

// Match stock icons with names
static mozilla::Maybe<SHSTOCKICONID> GetStockIconIDForName(
    const nsACString& aStockName) {
  return aStockName.EqualsLiteral("uac-shield") ? Some(SIID_SHIELD) : Nothing();
}

// Specific to Vista and above
static nsresult GetStockHIcon(nsIMozIconURI* aIconURI, HICON* aIcon) {
  uint32_t desiredImageSize;
  aIconURI->GetImageSize(&desiredImageSize);
  nsAutoCString stockIcon;
  aIconURI->GetStockIcon(stockIcon);

  Maybe<SHSTOCKICONID> stockIconID = GetStockIconIDForName(stockIcon);
  if (stockIconID.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UINT infoFlags = SHGSI_ICON;
  infoFlags |= GetSizeInfoFlag(desiredImageSize);

  SHSTOCKICONINFO sii = {0};
  sii.cbSize = sizeof(sii);
  HRESULT hr = SHGetStockIconInfo(*stockIconID, infoFlags, &sii);
  if (FAILED(hr)) {
    return NS_ERROR_FAILURE;
  }

  *aIcon = sii.hIcon;

  return NS_OK;
}

// Given a BITMAPINFOHEADER, returns the size of the color table.
static int GetColorTableSize(BITMAPINFOHEADER* aHeader) {
  int colorTableSize = -1;

  // http://msdn.microsoft.com/en-us/library/dd183376%28v=VS.85%29.aspx
  switch (aHeader->biBitCount) {
    case 0:
      colorTableSize = 0;
      break;
    case 1:
      colorTableSize = 2 * sizeof(RGBQUAD);
      break;
    case 4:
    case 8: {
      // The maximum possible size for the color table is 2**bpp, so check for
      // that and fail if we're not in those bounds
      unsigned int maxEntries = 1 << (aHeader->biBitCount);
      if (aHeader->biClrUsed > 0 && aHeader->biClrUsed <= maxEntries) {
        colorTableSize = aHeader->biClrUsed * sizeof(RGBQUAD);
      } else if (aHeader->biClrUsed == 0) {
        colorTableSize = maxEntries * sizeof(RGBQUAD);
      }
      break;
    }
    case 16:
    case 32:
      // If we have BI_BITFIELDS compression, we would normally need 3 DWORDS
      // for the bitfields mask which would be stored in the color table;
      // However, we instead force the bitmap to request data of type BI_RGB so
      // the color table should be of size 0. Setting aHeader->biCompression =
      // BI_RGB forces the later call to GetDIBits to return to us BI_RGB data.
      if (aHeader->biCompression == BI_BITFIELDS) {
        aHeader->biCompression = BI_RGB;
      }
      colorTableSize = 0;
      break;
    case 24:
      colorTableSize = 0;
      break;
  }

  if (colorTableSize < 0) {
    NS_WARNING("Unable to figure out the color table size for this bitmap");
  }

  return colorTableSize;
}

// Given a header and a size, creates a freshly allocated BITMAPINFO structure.
// It is the caller's responsibility to null-check and delete the structure.
static BITMAPINFO* CreateBitmapInfo(BITMAPINFOHEADER* aHeader,
                                    size_t aColorTableSize) {
  BITMAPINFO* bmi = (BITMAPINFO*)::operator new(
      sizeof(BITMAPINFOHEADER) + aColorTableSize, mozilla::fallible);
  if (bmi) {
    memcpy(bmi, aHeader, sizeof(BITMAPINFOHEADER));
    memset(bmi->bmiColors, 0, aColorTableSize);
  }
  return bmi;
}

static nsresult MakeIconBuffer(HICON aIcon, ByteBuf* aOutBuffer) {
  nsresult rv = NS_ERROR_FAILURE;

  if (aIcon) {
    // we got a handle to an icon. Now we want to get a bitmap for the icon
    // using GetIconInfo....
    ICONINFO iconInfo;
    if (GetIconInfo(aIcon, &iconInfo)) {
      // we got the bitmaps, first find out their size
      HDC hDC = CreateCompatibleDC(nullptr);  // get a device context for
                                              // the screen.
      BITMAPINFOHEADER maskHeader = {sizeof(BITMAPINFOHEADER)};
      BITMAPINFOHEADER colorHeader = {sizeof(BITMAPINFOHEADER)};
      int colorTableSize, maskTableSize;
      if (GetDIBits(hDC, iconInfo.hbmMask, 0, 0, nullptr,
                    (BITMAPINFO*)&maskHeader, DIB_RGB_COLORS) &&
          GetDIBits(hDC, iconInfo.hbmColor, 0, 0, nullptr,
                    (BITMAPINFO*)&colorHeader, DIB_RGB_COLORS) &&
          maskHeader.biHeight == colorHeader.biHeight &&
          maskHeader.biWidth == colorHeader.biWidth &&
          colorHeader.biBitCount > 8 && colorHeader.biSizeImage > 0 &&
          colorHeader.biWidth >= 0 && colorHeader.biWidth <= 255 &&
          colorHeader.biHeight >= 0 && colorHeader.biHeight <= 255 &&
          maskHeader.biSizeImage > 0 &&
          (colorTableSize = GetColorTableSize(&colorHeader)) >= 0 &&
          (maskTableSize = GetColorTableSize(&maskHeader)) >= 0) {
        uint32_t iconSize = sizeof(ICONFILEHEADER) + sizeof(ICONENTRY) +
                            sizeof(BITMAPINFOHEADER) + colorHeader.biSizeImage +
                            maskHeader.biSizeImage;

        if (!aOutBuffer->Allocate(iconSize)) {
          rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
          uint8_t* whereTo = aOutBuffer->mData;
          int howMuch;

          // the data starts with an icon file header
          ICONFILEHEADER iconHeader;
          iconHeader.ifhReserved = 0;
          iconHeader.ifhType = 1;
          iconHeader.ifhCount = 1;
          howMuch = sizeof(ICONFILEHEADER);
          memcpy(whereTo, &iconHeader, howMuch);
          whereTo += howMuch;

          // followed by the single icon entry
          ICONENTRY iconEntry;
          iconEntry.ieWidth = static_cast<int8_t>(colorHeader.biWidth);
          iconEntry.ieHeight = static_cast<int8_t>(colorHeader.biHeight);
          iconEntry.ieColors = 0;
          iconEntry.ieReserved = 0;
          iconEntry.iePlanes = 1;
          iconEntry.ieBitCount = colorHeader.biBitCount;
          iconEntry.ieSizeImage = sizeof(BITMAPINFOHEADER) +
                                  colorHeader.biSizeImage +
                                  maskHeader.biSizeImage;
          iconEntry.ieFileOffset = sizeof(ICONFILEHEADER) + sizeof(ICONENTRY);
          howMuch = sizeof(ICONENTRY);
          memcpy(whereTo, &iconEntry, howMuch);
          whereTo += howMuch;

          // followed by the bitmap info header
          // (doubling the height because icons have two bitmaps)
          colorHeader.biHeight *= 2;
          colorHeader.biSizeImage += maskHeader.biSizeImage;
          howMuch = sizeof(BITMAPINFOHEADER);
          memcpy(whereTo, &colorHeader, howMuch);
          whereTo += howMuch;
          colorHeader.biHeight /= 2;
          colorHeader.biSizeImage -= maskHeader.biSizeImage;

          // followed by the XOR bitmap data (colorHeader)
          // (you'd expect the color table to come here, but it apparently
          // doesn't)
          BITMAPINFO* colorInfo =
              CreateBitmapInfo(&colorHeader, colorTableSize);
          if (colorInfo &&
              GetDIBits(hDC, iconInfo.hbmColor, 0, colorHeader.biHeight,
                        whereTo, colorInfo, DIB_RGB_COLORS)) {
            whereTo += colorHeader.biSizeImage;

            // and finally the AND bitmap data (maskHeader)
            BITMAPINFO* maskInfo = CreateBitmapInfo(&maskHeader, maskTableSize);
            if (maskInfo &&
                GetDIBits(hDC, iconInfo.hbmMask, 0, maskHeader.biHeight,
                          whereTo, maskInfo, DIB_RGB_COLORS)) {
              rv = NS_OK;
            }  // if we got bitmap bits
            delete maskInfo;
          }  // if we got mask bits
          delete colorInfo;
        }  // if we allocated the buffer
      }    // if we got mask size

      DeleteDC(hDC);
      DeleteObject(iconInfo.hbmColor);
      DeleteObject(iconInfo.hbmMask);
    }  // if we got icon info
    DestroyIcon(aIcon);
  }  // if we got an hIcon

  return rv;
}

static nsresult GetIconHandleFromURLBlocking(nsIMozIconURI* aUrl,
                                             HICON* aIcon) {
  nsAutoCString stockIcon;
  aUrl->GetStockIcon(stockIcon);
  if (!stockIcon.IsEmpty()) {
    return GetStockHIcon(aUrl, aIcon);
  }

  IconPathInfo iconPathInfo;
  nsresult rv = ExtractIconPathInfoFromUrl(aUrl, &iconPathInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      "GetIconHandleFromURLBlocking",
      [&] { rv = GetIconHandleFromPathInfo(iconPathInfo, aIcon); });

  RefPtr<nsIEventTarget> target = DecodePool::Singleton()->GetIOEventTarget();

  nsresult dispatchResult = SyncRunnable::DispatchToThread(target, task);
  NS_ENSURE_SUCCESS(dispatchResult, dispatchResult);

  return rv;
}

static RefPtr<HIconPromise> GetIconHandleFromURLAsync(nsIMozIconURI* aUrl) {
  RefPtr<HIconPromise::Private> promise = new HIconPromise::Private(__func__);

  nsAutoCString stockIcon;
  aUrl->GetStockIcon(stockIcon);
  if (!stockIcon.IsEmpty()) {
    HICON hIcon = nullptr;
    nsresult rv = GetStockHIcon(aUrl, &hIcon);
    if (NS_SUCCEEDED(rv)) {
      promise->Resolve(hIcon, __func__);
    } else {
      promise->Reject(rv, __func__);
    }
    return promise;
  }

  IconPathInfo iconPathInfo;
  nsresult rv = ExtractIconPathInfoFromUrl(aUrl, &iconPathInfo);
  if (NS_FAILED(rv)) {
    promise->Reject(rv, __func__);
    return promise;
  }

  nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      "GetIconHandleFromURLAsync", [iconPathInfo, promise] {
        HICON hIcon = nullptr;
        nsresult rv = GetIconHandleFromPathInfo(iconPathInfo, &hIcon);
        if (NS_SUCCEEDED(rv)) {
          promise->Resolve(hIcon, __func__);
        } else {
          promise->Reject(rv, __func__);
        }
      });

  RefPtr<nsIEventTarget> target = DecodePool::Singleton()->GetIOEventTarget();

  rv = target->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    promise->Reject(rv, __func__);
  }

  return promise;
}

static RefPtr<nsIconChannel::ByteBufPromise> GetIconBufferFromURLAsync(
    nsIMozIconURI* aUrl) {
  RefPtr<nsIconChannel::ByteBufPromise::Private> promise =
      new nsIconChannel::ByteBufPromise::Private(__func__);

  GetIconHandleFromURLAsync(aUrl)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise](HICON aIcon) {
        ByteBuf iconBuffer;
        nsresult rv = MakeIconBuffer(aIcon, &iconBuffer);
        if (NS_SUCCEEDED(rv)) {
          promise->Resolve(std::move(iconBuffer), __func__);
        } else {
          promise->Reject(rv, __func__);
        }
      },
      [promise](nsresult rv) { promise->Reject(rv, __func__); });

  return promise;
}

static nsresult WriteByteBufToOutputStream(const ByteBuf& aBuffer,
                                           nsIAsyncOutputStream* aStream) {
  uint32_t written = 0;
  nsresult rv = aStream->Write(reinterpret_cast<const char*>(aBuffer.mData),
                               aBuffer.mLen, &written);
  NS_ENSURE_SUCCESS(rv, rv);

  return (written == aBuffer.mLen) ? NS_OK : NS_ERROR_UNEXPECTED;
}

NS_IMPL_ISUPPORTS(nsIconChannel, nsIChannel, nsIRequest, nsIRequestObserver,
                  nsIStreamListener)

// nsIconChannel methods
nsIconChannel::nsIconChannel() {}

nsIconChannel::~nsIconChannel() {
  if (mLoadInfo) {
    NS_ReleaseOnMainThread("nsIconChannel::mLoadInfo", mLoadInfo.forget());
  }
  if (mLoadGroup) {
    NS_ReleaseOnMainThread("nsIconChannel::mLoadGroup", mLoadGroup.forget());
  }
}

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

NS_IMETHODIMP nsIconChannel::SetCanceledReason(const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsIconChannel::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsIconChannel::CancelWithReason(nsresult aStatus,
                                              const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

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
nsIconChannel::GetLoadFlags(uint32_t* aLoadAttributes) {
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP
nsIconChannel::SetLoadFlags(uint32_t aLoadAttributes) {
  return mPump->SetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP
nsIconChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsIconChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsIconChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
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

// static
RefPtr<nsIconChannel::ByteBufPromise> nsIconChannel::GetIconAsync(
    nsIURI* aURI) {
  MOZ_ASSERT(XRE_IsParentProcess());

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI(do_QueryInterface(aURI, &rv));
  if (NS_FAILED(rv)) {
    return ByteBufPromise::CreateAndReject(rv, __func__);
  }

  return GetIconBufferFromURLAsync(iconURI);
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream** aStream) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(
      mLoadInfo->GetSecurityMode() == 0 ||
          mLoadInfo->GetInitialSecurityCheckDone() ||
          (mLoadInfo->GetSecurityMode() ==
               nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL &&
           mLoadInfo->GetLoadingPrincipal() &&
           mLoadInfo->GetLoadingPrincipal()->IsSystemPrincipal()),
      "security flags in loadInfo but doContentSecurityCheck() not called");

  // Double-check that we are actually an icon URL
  nsCOMPtr<nsIMozIconURI> iconURI(do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the handle for the given icon URI. This may involve the decode I/O
  // thread, as we can only call SHGetFileInfo() from that thread
  //
  // Since this API is synchronous, this call will not return until the decode
  // I/O thread returns with the icon handle
  //
  // Once we have the handle, we create a Windows ICO buffer with it and
  // dump the buffer into the output end of the pipe. The input end will
  // be returned to the caller
  HICON hIcon = nullptr;
  rv = GetIconHandleFromURLBlocking(iconURI, &hIcon);
  NS_ENSURE_SUCCESS(rv, rv);

  ByteBuf iconBuffer;
  rv = MakeIconBuffer(hIcon, &iconBuffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the asynchronous pipe with a blocking read end
  nsCOMPtr<nsIAsyncInputStream> inputStream;
  nsCOMPtr<nsIAsyncOutputStream> outputStream;
  NS_NewPipe2(getter_AddRefs(inputStream), getter_AddRefs(outputStream),
              false /*nonBlockingInput*/, false /*nonBlockingOutput*/,
              iconBuffer.mLen /*segmentSize*/, 1 /*segmentCount*/);

  rv = WriteByteBufToOutputStream(iconBuffer, outputStream);

  if (NS_SUCCEEDED(rv)) {
    inputStream.forget(aStream);
  }

  return rv;
}

NS_IMETHODIMP
nsIconChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  MOZ_ASSERT(
      mLoadInfo->GetSecurityMode() == 0 ||
          mLoadInfo->GetInitialSecurityCheckDone() ||
          (mLoadInfo->GetSecurityMode() ==
               nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL &&
           mLoadInfo->GetLoadingPrincipal() &&
           mLoadInfo->GetLoadingPrincipal()->IsSystemPrincipal()),
      "security flags in loadInfo but doContentSecurityCheck() not called");

  mListener = listener;

  rv = StartAsyncOpen();
  if (NS_FAILED(rv)) {
    mListener = nullptr;
    mCallbacks = nullptr;
    return rv;
  }

  // Add ourself to the load group, if available
  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }

  return NS_OK;
}

nsresult nsIconChannel::StartAsyncOpen() {
  // Double-check that we are actually an icon URL
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI(do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the asynchronous pipe with a non-blocking read end
  nsCOMPtr<nsIAsyncInputStream> inputStream;
  nsCOMPtr<nsIAsyncOutputStream> outputStream;
  NS_NewPipe2(getter_AddRefs(inputStream), getter_AddRefs(outputStream),
              true /*nonBlockingInput*/, false /*nonBlockingOutput*/,
              0 /*segmentSize*/, UINT32_MAX /*segmentCount*/);

  // If we are in content, we asynchronously request the ICO buffer from
  // the parent process because the APIs to load icons don't work with
  // Win32k Lockdown
  using ContentChild = mozilla::dom::ContentChild;
  if (auto* contentChild = ContentChild::GetSingleton()) {
    RefPtr<ContentChild::GetSystemIconPromise> iconPromise =
        contentChild->SendGetSystemIcon(mUrl);
    if (!iconPromise) {
      return NS_ERROR_UNEXPECTED;
    }

    iconPromise->Then(
        mozilla::GetCurrentSerialEventTarget(), __func__,
        [outputStream](std::tuple<nsresult, mozilla::Maybe<ByteBuf>>&& aArg) {
          nsresult rv = std::get<0>(aArg);
          mozilla::Maybe<ByteBuf> iconBuffer = std::move(std::get<1>(aArg));

          if (NS_SUCCEEDED(rv)) {
            MOZ_RELEASE_ASSERT(iconBuffer);
            rv = WriteByteBufToOutputStream(*iconBuffer, outputStream);
          }

          outputStream->CloseWithStatus(rv);
        },
        [outputStream](mozilla::ipc::ResponseRejectReason) {
          outputStream->CloseWithStatus(NS_ERROR_FAILURE);
        });
  } else {
    // Get the handle for the given icon URI. This may involve the decode I/O
    // thread, as we can only call SHGetFileInfo() from that thread
    //
    // Once we have the handle, we create a Windows ICO buffer with it and
    // dump the buffer into the output end of the pipe. The input end will be
    // pumped to our attached nsIStreamListener
    GetIconBufferFromURLAsync(iconURI)->Then(
        GetCurrentSerialEventTarget(), __func__,
        [outputStream](ByteBuf aIconBuffer) {
          nsresult rv =
              WriteByteBufToOutputStream(std::move(aIconBuffer), outputStream);
          outputStream->CloseWithStatus(rv);
        },
        [outputStream](nsresult rv) { outputStream->CloseWithStatus(rv); });
  }

  rv = mPump->Init(inputStream.get(), 0 /*segmentSize*/, 0 /*segmentCount*/,
                   false /*closeWhenDone*/, GetMainThreadSerialEventTarget());
  NS_ENSURE_SUCCESS(rv, rv);

  return mPump->AsyncRead(this);
}

NS_IMETHODIMP
nsIconChannel::GetContentType(nsACString& aContentType) {
  aContentType.AssignLiteral(IMAGE_ICO);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString& aContentType) {
  // It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentCharset(nsACString& aContentCharset) {
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString& aContentCharset) {
  // It doesn't make sense to set the content-charset on this type
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
nsIconChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
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
nsIconChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aNotificationCallbacks) {
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) {
  *aSecurityInfo = nullptr;
  return NS_OK;
}

// nsIRequestObserver methods
NS_IMETHODIMP nsIconChannel::OnStartRequest(nsIRequest* aRequest) {
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

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;

  return NS_OK;
}

// nsIStreamListener methods
NS_IMETHODIMP
nsIconChannel::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream,
                               uint64_t aOffset, uint32_t aCount) {
  if (mListener) {
    return mListener->OnDataAvailable(this, aStream, aOffset, aCount);
  }
  return NS_OK;
}
