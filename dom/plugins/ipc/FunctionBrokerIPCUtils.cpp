/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FunctionBrokerIPCUtils.h"

#if defined(XP_WIN)

#include <schannel.h>

/* these defines are missing from mingw headers */
#ifndef SP_PROT_TLS1_1_CLIENT
#define SP_PROT_TLS1_1_CLIENT 0x00000200
#endif

#ifndef SP_PROT_TLS1_2_CLIENT
#define SP_PROT_TLS1_2_CLIENT 0x00000800
#endif

namespace mozilla {
namespace plugins {

mozilla::LazyLogModule sPluginHooksLog("PluginHooks");

static const DWORD SCHANNEL_SUPPORTED_PROTOCOLS =
  SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT;

static const DWORD SCHANNEL_SUPPORTED_FLAGS =
  SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS |
  SCH_CRED_REVOCATION_CHECK_END_CERT;

void
OpenFileNameIPC::CopyFromOfn(LPOPENFILENAMEW aLpofn)
{
  mHwndOwner = nullptr;

  // Filter is double-NULL terminated.  mFilter should include the double-NULL.
  mHasFilter = aLpofn->lpstrFilter != nullptr;
  if (mHasFilter) {
    uint32_t dNullIdx = 0;
    while (aLpofn->lpstrFilter[dNullIdx] != L'\0' ||
           aLpofn->lpstrFilter[dNullIdx+1] != L'\0') {
      dNullIdx++;
    }
    mFilter.assign(aLpofn->lpstrFilter, dNullIdx+2);
  }
  mHasCustomFilter = aLpofn->lpstrCustomFilter != nullptr;
  if (mHasCustomFilter) {
    mCustomFilterIn = std::wstring(aLpofn->lpstrCustomFilter);
    mNMaxCustFilterOut =
      aLpofn->nMaxCustFilter - (wcslen(aLpofn->lpstrCustomFilter) + 1);
  }
  else {
    mNMaxCustFilterOut = 0;
  }
  mFilterIndex = aLpofn->nFilterIndex;
  mFile = std::wstring(aLpofn->lpstrFile);
  mNMaxFile = aLpofn->nMaxFile;
  mNMaxFileTitle =
    aLpofn->lpstrFileTitle != nullptr ? aLpofn->nMaxFileTitle : 0;
  mHasInitialDir = aLpofn->lpstrInitialDir != nullptr;
  if (mHasInitialDir) {
    mInitialDir = std::wstring(aLpofn->lpstrInitialDir);
  }
  mHasTitle = aLpofn->lpstrTitle != nullptr;
  if (mHasTitle) {
    mTitle = std::wstring(aLpofn->lpstrTitle);
  }
  mHasDefExt = aLpofn->lpstrDefExt != nullptr;
  if (mHasDefExt) {
    mDefExt = std::wstring(aLpofn->lpstrDefExt);
  }

  mFlags = aLpofn->Flags;
  // If the user sets OFN_ALLOWMULTISELECT then we require OFN_EXPLORER
  // as well.  Without OFN_EXPLORER, the method has ancient legacy
  // behavior that we don't support.
  MOZ_ASSERT((mFlags & OFN_EXPLORER) || !(mFlags & OFN_ALLOWMULTISELECT));

  // We ignore any visual customization and callbacks that the user set.
  mFlags &= ~(OFN_ENABLEHOOK | OFN_ENABLETEMPLATEHANDLE | OFN_ENABLETEMPLATE);

  mFlagsEx = aLpofn->FlagsEx;
}

void
OpenFileNameIPC::AddToOfn(LPOPENFILENAMEW aLpofn) const
{
  aLpofn->lStructSize = sizeof(OPENFILENAMEW);
  aLpofn->hwndOwner = mHwndOwner;
  if (mHasFilter) {
    memcpy(const_cast<LPWSTR>(aLpofn->lpstrFilter),
           mFilter.data(), mFilter.size() * sizeof(wchar_t));
  }
  if (mHasCustomFilter) {
    aLpofn->nMaxCustFilter = mCustomFilterIn.size() + 1 + mNMaxCustFilterOut;
    wcscpy(aLpofn->lpstrCustomFilter, mCustomFilterIn.c_str());
    memset(aLpofn->lpstrCustomFilter + mCustomFilterIn.size() + 1, 0,
           mNMaxCustFilterOut * sizeof(wchar_t));
  }
  else {
    aLpofn->nMaxCustFilter = 0;
  }
  aLpofn->nFilterIndex = mFilterIndex;
  if (mNMaxFile > 0) {
    wcsncpy(aLpofn->lpstrFile, mFile.c_str(),
            std::min(static_cast<uint32_t>(mFile.size()+1), mNMaxFile));
    aLpofn->lpstrFile[mNMaxFile - 1] = L'\0';
  }
  aLpofn->nMaxFile = mNMaxFile;
  aLpofn->nMaxFileTitle = mNMaxFileTitle;
  if (mHasInitialDir) {
    wcscpy(const_cast<LPWSTR>(aLpofn->lpstrInitialDir), mInitialDir.c_str());
  }
  if (mHasTitle) {
    wcscpy(const_cast<LPWSTR>(aLpofn->lpstrTitle), mTitle.c_str());
  }
  aLpofn->Flags = mFlags;  /* TODO: Consider adding OFN_NOCHANGEDIR */
  if (mHasDefExt) {
    wcscpy(const_cast<LPWSTR>(aLpofn->lpstrDefExt), mDefExt.c_str());
  }
  aLpofn->FlagsEx = mFlagsEx;
}

void
OpenFileNameIPC::AllocateOfnStrings(LPOPENFILENAMEW aLpofn) const
{
  if (mHasFilter) {
    // mFilter is double-NULL terminated and it includes the double-NULL in its length.
    aLpofn->lpstrFilter =
      static_cast<LPCTSTR>(moz_xmalloc(sizeof(wchar_t) * (mFilter.size())));
  }
  if (mHasCustomFilter) {
    aLpofn->lpstrCustomFilter =
      static_cast<LPTSTR>(moz_xmalloc(sizeof(wchar_t) * (mCustomFilterIn.size() + 1 + mNMaxCustFilterOut)));
  }
  aLpofn->lpstrFile =
    static_cast<LPTSTR>(moz_xmalloc(sizeof(wchar_t) * mNMaxFile));
  if (mNMaxFileTitle > 0) {
    aLpofn->lpstrFileTitle =
      static_cast<LPTSTR>(moz_xmalloc(sizeof(wchar_t) * mNMaxFileTitle));
  }
  if (mHasInitialDir) {
    aLpofn->lpstrInitialDir =
      static_cast<LPCTSTR>(moz_xmalloc(sizeof(wchar_t) * (mInitialDir.size() + 1)));
  }
  if (mHasTitle) {
    aLpofn->lpstrTitle =
      static_cast<LPCTSTR>(moz_xmalloc(sizeof(wchar_t) * (mTitle.size() + 1)));
  }
  if (mHasDefExt) {
    aLpofn->lpstrDefExt =
      static_cast<LPCTSTR>(moz_xmalloc(sizeof(wchar_t) * (mDefExt.size() + 1)));
  }
}

// static
void
OpenFileNameIPC::FreeOfnStrings(LPOPENFILENAMEW aLpofn)
{
  if (aLpofn->lpstrFilter) {
    free(const_cast<LPWSTR>(aLpofn->lpstrFilter));
  }
  if (aLpofn->lpstrCustomFilter) {
    free(aLpofn->lpstrCustomFilter);
  }
  if (aLpofn->lpstrFile) {
    free(aLpofn->lpstrFile);
  }
  if (aLpofn->lpstrFileTitle) {
    free(aLpofn->lpstrFileTitle);
  }
  if (aLpofn->lpstrInitialDir) {
    free(const_cast<LPWSTR>(aLpofn->lpstrInitialDir));
  }
  if (aLpofn->lpstrTitle) {
    free(const_cast<LPWSTR>(aLpofn->lpstrTitle));
  }
  if (aLpofn->lpstrDefExt) {
    free(const_cast<LPWSTR>(aLpofn->lpstrDefExt));
  }
}

void
OpenFileNameRetIPC::CopyFromOfn(LPOPENFILENAMEW aLpofn)
{
  if (aLpofn->lpstrCustomFilter != nullptr) {
    mCustomFilterOut =
      std::wstring(aLpofn->lpstrCustomFilter + wcslen(aLpofn->lpstrCustomFilter) + 1);
  }
  mFile.assign(aLpofn->lpstrFile, aLpofn->nMaxFile);
  if (aLpofn->lpstrFileTitle != nullptr) {
    mFileTitle.assign(aLpofn->lpstrFileTitle, wcslen(aLpofn->lpstrFileTitle) + 1);
  }
  mFileOffset = aLpofn->nFileOffset;
  mFileExtension = aLpofn->nFileExtension;
}

void
OpenFileNameRetIPC::AddToOfn(LPOPENFILENAMEW aLpofn) const
{
  if (aLpofn->lpstrCustomFilter) {
    LPWSTR secondString =
      aLpofn->lpstrCustomFilter + wcslen(aLpofn->lpstrCustomFilter) + 1;
    const wchar_t* customFilterOut = mCustomFilterOut.c_str();
    MOZ_ASSERT(wcslen(aLpofn->lpstrCustomFilter) + 1 +
               wcslen(customFilterOut) + 1 + 1 <= aLpofn->nMaxCustFilter);
    wcscpy(secondString, customFilterOut);
    secondString[wcslen(customFilterOut) + 1] = L'\0';  // terminated with two NULLs
  }
  MOZ_ASSERT(mFile.size() <= aLpofn->nMaxFile);
  memcpy(aLpofn->lpstrFile,
         mFile.data(), mFile.size() * sizeof(wchar_t));
  if (aLpofn->lpstrFileTitle != nullptr) {
    MOZ_ASSERT(mFileTitle.size() + 1 < aLpofn->nMaxFileTitle);
    wcscpy(aLpofn->lpstrFileTitle, mFileTitle.c_str());
  }
  aLpofn->nFileOffset = mFileOffset;
  aLpofn->nFileExtension = mFileExtension;
}

void
IPCSchannelCred::CopyFrom(const PSCHANNEL_CRED& aSCred)
{
  // We assert that the aSCred fields take supported values.
  // If they do not then we ignore the values we were given.
  MOZ_ASSERT(aSCred->dwVersion == SCHANNEL_CRED_VERSION);
  MOZ_ASSERT(aSCred->cCreds == 0);
  MOZ_ASSERT(aSCred->paCred == nullptr);
  MOZ_ASSERT(aSCred->hRootStore == nullptr);
  MOZ_ASSERT(aSCred->cMappers == 0);
  MOZ_ASSERT(aSCred->aphMappers == nullptr);
  MOZ_ASSERT(aSCred->cSupportedAlgs == 0);
  MOZ_ASSERT(aSCred->palgSupportedAlgs == nullptr);
  MOZ_ASSERT((aSCred->grbitEnabledProtocols & SCHANNEL_SUPPORTED_PROTOCOLS) ==
             aSCred->grbitEnabledProtocols);
  mEnabledProtocols =
    aSCred->grbitEnabledProtocols & SCHANNEL_SUPPORTED_PROTOCOLS;
  mMinStrength = aSCred->dwMinimumCipherStrength;
  mMaxStrength = aSCred->dwMaximumCipherStrength;
  MOZ_ASSERT(aSCred->dwSessionLifespan == 0);
  MOZ_ASSERT((aSCred->dwFlags & SCHANNEL_SUPPORTED_FLAGS) == aSCred->dwFlags);
  mFlags = aSCred->dwFlags & SCHANNEL_SUPPORTED_FLAGS;
  MOZ_ASSERT(aSCred->dwCredFormat == 0);
}

void
IPCSchannelCred::CopyTo(PSCHANNEL_CRED& aSCred) const
{
  // Validate values as they come from an untrusted process.
  memset(aSCred, 0, sizeof(SCHANNEL_CRED));
  aSCred->dwVersion = SCHANNEL_CRED_VERSION;
  aSCred->grbitEnabledProtocols =
    mEnabledProtocols & SCHANNEL_SUPPORTED_PROTOCOLS;
  aSCred->dwMinimumCipherStrength = mMinStrength;
  aSCred->dwMaximumCipherStrength = mMaxStrength;
  aSCred->dwFlags = mFlags & SCHANNEL_SUPPORTED_FLAGS;
}

void
IPCInternetBuffers::CopyFrom(const LPINTERNET_BUFFERSA& aBufs)
{
  mBuffers.Clear();

  LPINTERNET_BUFFERSA inetBuf = aBufs;
  while (inetBuf) {
    MOZ_ASSERT(inetBuf->dwStructSize == sizeof(INTERNET_BUFFERSA));
    Buffer* ipcBuf = mBuffers.AppendElement();

    ipcBuf->mHeader.SetIsVoid(inetBuf->lpcszHeader == nullptr);
    if (inetBuf->lpcszHeader) {
      ipcBuf->mHeader.Assign(inetBuf->lpcszHeader, inetBuf->dwHeadersLength);
    }
    ipcBuf->mHeaderTotal = inetBuf->dwHeadersTotal;

    ipcBuf->mBuffer.SetIsVoid(inetBuf->lpvBuffer == nullptr);
    if (inetBuf->lpvBuffer) {
      ipcBuf->mBuffer.Assign(static_cast<char*>(inetBuf->lpvBuffer),
                             inetBuf->dwBufferLength);
    }
    ipcBuf->mBufferTotal = inetBuf->dwBufferTotal;
    inetBuf = inetBuf->Next;
  }
}

void
IPCInternetBuffers::CopyTo(LPINTERNET_BUFFERSA& aBufs) const
{
  MOZ_ASSERT(!aBufs);

  LPINTERNET_BUFFERSA lastBuf = nullptr;
  for (size_t idx = 0; idx < mBuffers.Length(); ++idx) {
    const Buffer& ipcBuf = mBuffers[idx];
    LPINTERNET_BUFFERSA newBuf =
      static_cast<LPINTERNET_BUFFERSA>(moz_xcalloc(1, sizeof(INTERNET_BUFFERSA)));
    if (idx == 0) {
      aBufs = newBuf;
    } else {
      MOZ_ASSERT(lastBuf);
      lastBuf->Next = newBuf;
      lastBuf = newBuf;
    }

    newBuf->dwStructSize = sizeof(INTERNET_BUFFERSA);

    newBuf->dwHeadersTotal = ipcBuf.mHeaderTotal;
    if (!ipcBuf.mHeader.IsVoid()) {
      newBuf->lpcszHeader =
        static_cast<LPCSTR>(moz_xmalloc(ipcBuf.mHeader.Length()));
      memcpy(const_cast<char*>(newBuf->lpcszHeader), ipcBuf.mHeader.Data(),
             ipcBuf.mHeader.Length());
      newBuf->dwHeadersLength = ipcBuf.mHeader.Length();
    }

    newBuf->dwBufferTotal = ipcBuf.mBufferTotal;
    if (!ipcBuf.mBuffer.IsVoid()) {
      newBuf->lpvBuffer = moz_xmalloc(ipcBuf.mBuffer.Length());
      memcpy(newBuf->lpvBuffer, ipcBuf.mBuffer.Data(),
             ipcBuf.mBuffer.Length());
      newBuf->dwBufferLength = ipcBuf.mBuffer.Length();
    }
  }
}

/* static */ void
IPCInternetBuffers::FreeBuffers(LPINTERNET_BUFFERSA& aBufs)
{
  if (!aBufs) {
    return;
  }
  while (aBufs) {
    LPINTERNET_BUFFERSA temp = aBufs->Next;
    free(const_cast<char*>(aBufs->lpcszHeader));
    free(aBufs->lpvBuffer);
    free(aBufs);
    aBufs = temp;
  }
}

void
IPCPrintDlg::CopyFrom(const LPPRINTDLGW& aDlg)
{
  // DLP: Trouble -- my prior impl "worked" but didn't return anything
  // AFAIR.  So... ???  But it printed a page!!!  How?!
  MOZ_ASSERT_UNREACHABLE("TODO: DLP:");
}

void
IPCPrintDlg::CopyTo(LPPRINTDLGW& aDlg) const
{
  MOZ_ASSERT_UNREACHABLE("TODO: DLP:");
}

} // namespace plugins
} // namespace mozilla

#endif // defined(XP_WIN)
