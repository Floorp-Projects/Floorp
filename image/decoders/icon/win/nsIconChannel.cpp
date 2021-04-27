/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Monitor.h"
#include "mozilla/WindowsProcessMitigations.h"

#include "nsComponentManagerUtils.h"
#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsIURL.h"
#include "nsIPipe.h"
#include "nsNetCID.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIInputStream.h"
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

// Match stock icons with names
static SHSTOCKICONID GetStockIconIDForName(const nsACString& aStockName) {
  return aStockName.EqualsLiteral("uac-shield") ? SIID_SHIELD : SIID_INVALID;
}

class nsIconChannel::IconAsyncOpenTask final : public Runnable {
 public:
  IconAsyncOpenTask(nsIconChannel* aChannel, nsIEventTarget* aTarget,
                    nsCOMPtr<nsIFile>&& aLocalFile, nsAutoString& aPath,
                    UINT aInfoFlags)
      : Runnable("IconAsyncOpenTask"),
        mChannel(aChannel),

        mTarget(aTarget),
        mLocalFile(std::move(aLocalFile)),
        mPath(aPath),
        mInfoFlags(aInfoFlags) {}

  NS_IMETHOD Run() override;

 private:
  RefPtr<nsIconChannel> mChannel;
  nsCOMPtr<nsIEventTarget> mTarget;
  nsCOMPtr<nsIFile> mLocalFile;
  nsAutoString mPath;
  UINT mInfoFlags;
};

NS_IMETHODIMP nsIconChannel::IconAsyncOpenTask::Run() {
  HICON hIcon = nullptr;
  nsresult rv =
      mChannel->GetHIconFromFile(mLocalFile, mPath, mInfoFlags, &hIcon);
  // Effectively give ownership of mChannel to the runnable so it get released
  // on the main thread.
  RefPtr<nsIconChannel> channel = mChannel.forget();
  nsCOMPtr<nsIRunnable> task = NewRunnableMethod<HICON, nsresult>(
      "nsIconChannel::FinishAsyncOpen", channel,
      &nsIconChannel::FinishAsyncOpen, hIcon, rv);
  mTarget->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
  return NS_OK;
}

class nsIconChannel::IconSyncOpenTask final : public Runnable {
 public:
  IconSyncOpenTask(nsIconChannel* aChannel, nsIEventTarget* aTarget,
                   nsCOMPtr<nsIFile>&& aLocalFile, nsAutoString& aPath,
                   UINT aInfoFlags)
      : Runnable("IconSyncOpenTask"),
        mMonitor("IconSyncOpenTask"),
        mDone(false),
        mChannel(aChannel),
        mTarget(aTarget),
        mLocalFile(std::move(aLocalFile)),
        mPath(aPath),
        mInfoFlags(aInfoFlags),
        mHIcon(nullptr),
        mRv(NS_OK) {}

  NS_IMETHOD Run() override;

  Monitor& GetMonitor() { return mMonitor; }
  bool Done() const {
    mMonitor.AssertCurrentThreadOwns();
    return mDone;
  }
  HICON GetHIcon() const {
    mMonitor.AssertCurrentThreadOwns();
    return mHIcon;
  }
  nsresult GetRv() const {
    mMonitor.AssertCurrentThreadOwns();
    return mRv;
  }

 private:
  Monitor mMonitor;
  bool mDone;
  // Parameters in
  RefPtr<nsIconChannel> mChannel;
  nsCOMPtr<nsIEventTarget> mTarget;
  nsCOMPtr<nsIFile> mLocalFile;
  nsAutoString mPath;
  UINT mInfoFlags;
  // Return values
  HICON mHIcon;
  nsresult mRv;
};

NS_IMETHODIMP
nsIconChannel::IconSyncOpenTask::Run() {
  MonitorAutoLock lock(mMonitor);
  mRv = mChannel->GetHIconFromFile(mLocalFile, mPath, mInfoFlags, &mHIcon);
  mDone = true;
  mMonitor.NotifyAll();
  // Do this little dance because nsIconChannel multiple inherits from
  // nsISupports.
  nsCOMPtr<nsIChannel> channel = mChannel.forget();
  NS_ProxyRelease("IconSyncOpenTask::mChannel", mTarget, channel.forget());
  return NS_OK;
}

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

NS_IMPL_ISUPPORTS(nsIconChannel, nsIChannel, nsIRequest, nsIRequestObserver,
                  nsIStreamListener)

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

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream** aStream) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  HICON hIcon = nullptr;
  rv = GetHIcon(/* aNonBlocking */ false, &hIcon);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return MakeInputStream(aStream, /* aNonBlocking */ false, hIcon);
}

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile** aLocalFile,
                                               uint32_t* aDesiredImageSize,
                                               nsCString& aContentType,
                                               nsCString& aFileExtension) {
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

  return file->Clone(aLocalFile);
}

void nsIconChannel::OnAsyncError(nsresult aStatus) {
  OnStartRequest(this);
  OnStopRequest(this, aStatus);
}

void nsIconChannel::FinishAsyncOpen(HICON aIcon, nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(aStatus)) {
    OnAsyncError(aStatus);
    return;
  }

  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream),
                                /* aNonBlocking */ true, aIcon);
  if (NS_FAILED(rv)) {
    OnAsyncError(rv);
    return;
  }

  rv = mPump->Init(inStream, 0, 0, false, mListenerTarget);
  if (NS_FAILED(rv)) {
    OnAsyncError(rv);
    return;
  }

  rv = mPump->AsyncRead(this);
  if (NS_FAILED(rv)) {
    OnAsyncError(rv);
  }
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

  nsCOMPtr<nsIInputStream> inStream;
  rv = EnsurePipeCreated(/* aIconSize */ 0, /* aNonBlocking */ true);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  mListenerTarget = nsContentUtils::GetEventTargetByLoadInfo(
      mLoadInfo, mozilla::TaskCategory::Other);
  if (!mListenerTarget) {
    mListenerTarget = do_GetMainThread();
  }

  // If we pass aNonBlocking as true, GetHIcon will always have dispatched
  // upon success.
  HICON hIcon = nullptr;
  rv = GetHIcon(/* aNonBlocking */ true, &hIcon);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    mInputStream = nullptr;
    mOutputStream = nullptr;
    mListenerTarget = nullptr;
    return rv;
  }

  // We shouldn't have the icon yet if it is non-blocking.
  MOZ_ASSERT(!hIcon);

  // Add ourself to the load group, if available
  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }

  // Store our real listener
  mListener = aListener;
  return NS_OK;
}

static DWORD GetSpecialFolderIcon(nsIFile* aFile, int aFolder,
                                  SHFILEINFOW* aSFI, UINT aInfoFlags) {
  DWORD shellResult = 0;

  if (!aFile) {
    return shellResult;
  }

  wchar_t fileNativePath[MAX_PATH];
  nsAutoString fileNativePathStr;
  aFile->GetPath(fileNativePathStr);
  ::GetShortPathNameW(fileNativePathStr.get(), fileNativePath,
                      ArrayLength(fileNativePath));

  LPITEMIDLIST idList;
  HRESULT hr = ::SHGetSpecialFolderLocation(nullptr, aFolder, &idList);
  if (SUCCEEDED(hr)) {
    wchar_t specialNativePath[MAX_PATH];
    ::SHGetPathFromIDListW(idList, specialNativePath);
    ::GetShortPathNameW(specialNativePath, specialNativePath,
                        ArrayLength(specialNativePath));

    if (!wcsicmp(fileNativePath, specialNativePath)) {
      aInfoFlags |= (SHGFI_PIDL | SHGFI_SYSICONINDEX);
      shellResult = ::SHGetFileInfoW((LPCWSTR)(LPCITEMIDLIST)idList, 0, aSFI,
                                     sizeof(*aSFI), aInfoFlags);
    }
  }
  CoTaskMemFree(idList);
  return shellResult;
}

static UINT GetSizeInfoFlag(uint32_t aDesiredImageSize) {
  return (UINT)(aDesiredImageSize > 16 ? SHGFI_SHELLICONSIZE : SHGFI_SMALLICON);
}

nsresult nsIconChannel::GetHIconFromFile(bool aNonBlocking, HICON* hIcon) {
  if (IsWin32kLockedDown()) {
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "GetHIconFromFile requires call to SHGetFileInfo, "
                          "which cannot be used when win32k is disabled.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCString contentType;
  nsCString fileExt;
  nsCOMPtr<nsIFile> localFile;  // file we want an icon for
  uint32_t desiredImageSize;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile),
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

  if (aNonBlocking) {
    RefPtr<nsIEventTarget> target = DecodePool::Singleton()->GetIOEventTarget();
    RefPtr<IconAsyncOpenTask> task = new IconAsyncOpenTask(
        this, mListenerTarget, std::move(localFile), filePath, infoFlags);
    target->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  // We cannot call SHGetFileInfo on more than one thread (at a time), so it
  // must be called on the same thread every time, even now when we need sync
  // behaviour. So we synchronously wait on the other thread to finish.
  RefPtr<nsIEventTarget> target = DecodePool::Singleton()->GetIOEventTarget();
  RefPtr<IconSyncOpenTask> task = new IconSyncOpenTask(
      this, mListenerTarget, std::move(localFile), filePath, infoFlags);
  MonitorAutoLock lock(task->GetMonitor());
  target->Dispatch(task, NS_DISPATCH_NORMAL);
  do {
    task->GetMonitor().Wait();
  } while (!task->Done());
  *hIcon = task->GetHIcon();
  return task->GetRv();
}

nsresult nsIconChannel::GetHIconFromFile(nsIFile* aLocalFile,
                                         const nsAutoString& aPath,
                                         UINT aInfoFlags, HICON* hIcon) {
  SHFILEINFOW sfi;

  // Is this the "Desktop" folder?
  DWORD shellResult =
      GetSpecialFolderIcon(aLocalFile, CSIDL_DESKTOP, &sfi, aInfoFlags);
  if (!shellResult) {
    // Is this the "My Documents" folder?
    shellResult =
        GetSpecialFolderIcon(aLocalFile, CSIDL_PERSONAL, &sfi, aInfoFlags);
  }

  // There are other "Special Folders" and Namespace entities that we
  // are not fetching icons for, see:
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/
  //        shellcc/platform/shell/reference/enums/csidl.asp
  // If we ever need to get them, code to do so would be inserted here.

  // Not a special folder, or something else failed above.
  if (!shellResult) {
    shellResult = ::SHGetFileInfoW(aPath.get(), FILE_ATTRIBUTE_ARCHIVE, &sfi,
                                   sizeof(sfi), aInfoFlags);
  }

  if (!shellResult || !sfi.hIcon) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *hIcon = sfi.hIcon;
  return NS_OK;
}

nsresult nsIconChannel::GetStockHIcon(nsIMozIconURI* aIconURI, HICON* hIcon) {
  nsresult rv = NS_OK;

  uint32_t desiredImageSize;
  aIconURI->GetImageSize(&desiredImageSize);
  nsAutoCString stockIcon;
  aIconURI->GetStockIcon(stockIcon);

  SHSTOCKICONID stockIconID = GetStockIconIDForName(stockIcon);
  if (stockIconID == SIID_INVALID) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UINT infoFlags = SHGSI_ICON;
  infoFlags |= GetSizeInfoFlag(desiredImageSize);

  SHSTOCKICONINFO sii = {0};
  sii.cbSize = sizeof(sii);
  HRESULT hr = SHGetStockIconInfo(stockIconID, infoFlags, &sii);

  if (SUCCEEDED(hr)) {
    *hIcon = sii.hIcon;
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
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

nsresult nsIconChannel::EnsurePipeCreated(uint32_t aIconSize,
                                          bool aNonBlocking) {
  if (mInputStream || mOutputStream) {
    return NS_OK;
  }

  return NS_NewPipe(getter_AddRefs(mInputStream), getter_AddRefs(mOutputStream),
                    aIconSize, aIconSize > 0 ? aIconSize : UINT32_MAX,
                    aNonBlocking);
}

nsresult nsIconChannel::GetHIcon(bool aNonBlocking, HICON* aIcon) {
  // Check whether the icon requested's a file icon or a stock icon
  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  // GetDIBits does not exist on windows mobile.
  nsCOMPtr<nsIMozIconURI> iconURI(do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString stockIcon;
  iconURI->GetStockIcon(stockIcon);
  if (!stockIcon.IsEmpty()) {
    return GetStockHIcon(iconURI, aIcon);
  }

  return GetHIconFromFile(aNonBlocking, aIcon);
}

nsresult nsIconChannel::MakeInputStream(nsIInputStream** _retval,
                                        bool aNonBlocking, HICON aIcon) {
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

        UniquePtr<char[]> buffer = MakeUnique<char[]>(iconSize);
        if (!buffer) {
          rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
          char* whereTo = buffer.get();
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
              // Now, create a pipe and stuff our data into it
              rv = EnsurePipeCreated(iconSize, aNonBlocking);
              if (NS_SUCCEEDED(rv)) {
                uint32_t written;
                rv = mOutputStream->Write(buffer.get(), iconSize, &written);
                if (NS_SUCCEEDED(rv)) {
                  NS_ADDREF(*_retval = mInputStream);
                }
              }

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

  // If we didn't make a stream, then fail.
  if (!*_retval && NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  mInputStream = nullptr;
  mOutputStream = nullptr;
  return rv;
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
nsIconChannel::GetSecurityInfo(nsISupports** aSecurityInfo) {
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
  mListenerTarget = nullptr;

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
