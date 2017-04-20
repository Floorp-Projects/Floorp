/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginMessageUtils.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

#include "PluginInstanceParent.h"
#include "PluginInstanceChild.h"
#include "PluginScriptableObjectParent.h"
#include "PluginScriptableObjectChild.h"

using std::string;

using mozilla::ipc::MessageChannel;

namespace {

class DeferNPObjectReleaseRunnable : public mozilla::Runnable
{
public:
  DeferNPObjectReleaseRunnable(const NPNetscapeFuncs* f, NPObject* o)
    : Runnable("DeferNPObjectReleaseRunnable")
    , mFuncs(f)
    , mObject(o)
  {
    NS_ASSERTION(o, "no release null objects");
  }

  NS_IMETHOD Run();

private:
  const NPNetscapeFuncs* mFuncs;
  NPObject* mObject;
};

NS_IMETHODIMP
DeferNPObjectReleaseRunnable::Run()
{
  mFuncs->releaseobject(mObject);
  return NS_OK;
}

} // namespace

namespace mozilla {
namespace plugins {

NPRemoteWindow::NPRemoteWindow() :
  window(0), x(0), y(0), width(0), height(0), type(NPWindowTypeDrawable)
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
  , visualID(0)
  , colormap(0)
#endif /* XP_UNIX */
#if defined(XP_MACOSX)
  ,contentsScaleFactor(1.0)
#endif
{
  clipRect.top = 0;
  clipRect.left = 0;
  clipRect.bottom = 0;
  clipRect.right = 0;
}

ipc::RacyInterruptPolicy
MediateRace(const MessageChannel::MessageInfo& parent,
            const MessageChannel::MessageInfo& child)
{
  switch (parent.type()) {
  case PPluginInstance::Msg_Paint__ID:
  case PPluginInstance::Msg_NPP_SetWindow__ID:
  case PPluginInstance::Msg_NPP_HandleEvent_Shmem__ID:
  case PPluginInstance::Msg_NPP_HandleEvent_IOSurface__ID:
    // our code relies on the frame list not changing during paints and
    // reflows
    return ipc::RIPParentWins;

  default:
    return ipc::RIPChildWins;
  }
}

#if defined(OS_LINUX) || defined(OS_SOLARIS)
static string
ReplaceAll(const string& haystack, const string& needle, const string& with)
{
  string munged = haystack;
  string::size_type i = 0;

  while (string::npos != (i = munged.find(needle, i))) {
    munged.replace(i, needle.length(), with);
    i += with.length();
  }

  return munged;
}
#endif

string
MungePluginDsoPath(const string& path)
{
#if defined(OS_LINUX) || defined(OS_SOLARIS)
  // https://bugzilla.mozilla.org/show_bug.cgi?id=519601
  return ReplaceAll(path, "netscape", "netsc@pe");
#else
  return path;
#endif
}

string
UnmungePluginDsoPath(const string& munged)
{
#if defined(OS_LINUX) || defined(OS_SOLARIS)
  return ReplaceAll(munged, "netsc@pe", "netscape");
#else
  return munged;
#endif
}


LogModule*
GetPluginLog()
{
  static LazyLogModule sLog("IPCPlugins");
  return sLog;
}

void
DeferNPObjectLastRelease(const NPNetscapeFuncs* f, NPObject* o)
{
  if (!o)
    return;

  if (o->referenceCount > 1) {
    f->releaseobject(o);
    return;
  }

  NS_DispatchToCurrentThread(new DeferNPObjectReleaseRunnable(f, o));
}

void DeferNPVariantLastRelease(const NPNetscapeFuncs* f, NPVariant* v)
{
  if (!NPVARIANT_IS_OBJECT(*v)) {
    f->releasevariantvalue(v);
    return;
  }
  DeferNPObjectLastRelease(f, v->value.objectValue);
  VOID_TO_NPVARIANT(*v);
}

#ifdef XP_WIN
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
  wcscpy(aLpofn->lpstrFile, mFile.c_str());
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
      static_cast<LPTSTR>(moz_xmalloc(sizeof(wchar_t) * (mCustomFilterIn.size() + 1) + mNMaxCustFilterOut));
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

void
OpenFileNameIPC::FreeOfnStrings(LPOPENFILENAMEW aLpofn) const
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
#endif  // XP_WIN

} // namespace plugins
} // namespace mozilla
