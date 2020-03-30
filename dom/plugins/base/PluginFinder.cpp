/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* PluginFinder.cpp - manages finding plugins on disk, and keeping plugin
 * lists in the host updated. */

#include "PluginFinder.h"

#include "nsIBlocklistService.h"
#include "nsIFile.h"
#include "nsIXULRuntime.h"
#if defined(XP_MACOSX)
#  include "nsILocalFileMac.h"
#endif
#include "nsIWritablePropertyBag2.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Telemetry.h"

#include "nscore.h"
#include "prio.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsPluginDirServiceProvider.h"
#include "nsPluginHost.h"
#include "nsPluginLogging.h"
#include "nsPluginManifestLineReader.h"
#include "nsPrintfCString.h"
#include "nsUnicharUtils.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

using namespace mozilla;

static const char* kPluginRegistryVersion = "0.19";

StaticRefPtr<nsIFile> sPluginRegFile;

#define kPluginRegistryFilename NS_LITERAL_CSTRING("pluginreg.dat")

#define NS_ITERATIVE_UNREF_LIST(type_, list_, mNext_) \
  {                                                   \
    while (list_) {                                   \
      type_ temp = list_->mNext_;                     \
      list_->mNext_ = nullptr;                        \
      list_ = temp;                                   \
    }                                                 \
  }

/* to cope with short read */
/* we should probably put this into a global library now that this is the second
   time we need this. */
static int32_t busy_beaver_PR_Read(PRFileDesc* fd, void* start, int32_t len) {
  int n;
  int32_t remaining = len;

  while (remaining > 0) {
    n = PR_Read(fd, start, remaining);
    if (n < 0) {
      /* may want to repeat if errno == EINTR */
      if ((len - remaining) == 0)  // no octet is ever read
        return -1;
      break;
    }
    remaining -= n;
    char* cp = (char*)start;
    cp += n;
    start = cp;
  }
  return len - remaining;
}

NS_IMPL_ISUPPORTS0(nsInvalidPluginTag)

nsInvalidPluginTag::nsInvalidPluginTag(const char* aFullPath,
                                       int64_t aLastModifiedTime)
    : mFullPath(aFullPath),
      mLastModifiedTime(aLastModifiedTime),
      mSeen(false) {}

nsInvalidPluginTag::~nsInvalidPluginTag() = default;

// flat file reg funcs
static bool ReadSectionHeader(nsPluginManifestLineReader& reader,
                              const char* token) {
  do {
    if (*reader.LinePtr() == '[') {
      char* p = reader.LinePtr() + (reader.LineLength() - 1);
      if (*p != ']') break;
      *p = 0;

      char* values[1];
      if (1 != reader.ParseLine(values, 1)) break;
      // ignore the leading '['
      if (PL_strcmp(values[0] + 1, token)) {
        break;  // it's wrong token
      }
      return true;
    }
  } while (reader.NextLine());
  return false;
}

nsPluginTag* PluginFinder::FirstPluginWithPath(const nsCString& path) {
  for (nsPluginTag* tag = mPlugins; tag; tag = tag->mNext) {
    if (tag->mFullPath.Equals(path)) {
      return tag;
    }
  }
  return nullptr;
}

NS_IMPL_ISUPPORTS(PluginFinder, nsIRunnable, nsIAsyncShutdownBlocker)

// nsIAsyncShutdownBlocker
nsresult PluginFinder::GetName(nsAString& aName) {
  aName.AssignLiteral("PluginFinder: finding plugins to load");
  return NS_OK;
}

NS_IMETHODIMP PluginFinder::BlockShutdown(
    nsIAsyncShutdownClient* aBarrierClient) {
  // Finding plugins isn't interruptable, but we can attempt to prevent loading
  // the plugins in nsPluginHost.
  mShutdown = true;
  return NS_OK;
}

NS_IMETHODIMP PluginFinder::GetState(nsIPropertyBag** aBagOut) {
  MOZ_ASSERT(aBagOut);
  nsCOMPtr<nsIWritablePropertyBag2> propertyBag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");

  if (NS_WARN_IF(!propertyBag)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  propertyBag->SetPropertyAsBool(NS_LITERAL_STRING("Finding"),
                                 !mFinishedFinding);
  propertyBag->SetPropertyAsBool(NS_LITERAL_STRING("CreatingList"),
                                 mCreateList);
  propertyBag->SetPropertyAsBool(NS_LITERAL_STRING("FlashOnly"), mFlashOnly);
  propertyBag->SetPropertyAsBool(NS_LITERAL_STRING("HavePlugins"), !!mPlugins);
  propertyBag.forget(aBagOut);
  return NS_OK;
}

PluginFinder::PluginFinder(bool aFlashOnly)
    : mFlashOnly(aFlashOnly),
      mCreateList(false),
      mPluginsChanged(false),
      mFinishedFinding(false),
      mCalledOnMainthread(false),  // overwritten from Run()
      mShutdown(false) {}

nsresult PluginFinder::DoFullSearch(const FoundPluginCallback& aCallback) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());
  mCreateList = true;
  mFoundPluginCallback = std::move(aCallback);

  nsresult rv = EnsurePluginReg();
  // We pass this back to the caller, and ignore all other errors.
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return rv;
  }

  // Figure out plugin directories:
  MOZ_TRY(DeterminePluginDirs());
  // If we're only interested in flash, provide an initial update from
  // cache. We'll do a scan async, away from the main thread, and if
  // there are updates then we'll pass them in.
  if (mFlashOnly) {
    ReadFlashInfo();
    // Don't do a blocklist check until we're done scanning,
    // as the version might change anyway.
    nsTArray<std::pair<bool, RefPtr<nsPluginTag>>> arr;
    mFoundPluginCallback(!!mPlugins, mPlugins, arr);
    // We've passed ownership of the flash plugin to the host, so make sure
    // we don't accidentally try to use it when we leave the mainthread.
    mPlugins = nullptr;
  }
  return NS_OK;
}

nsresult PluginFinder::HavePluginsChanged(
    const PluginChangeCallback& aCallback) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());
  mChangeCallback = std::move(aCallback);

  nsresult rv = EnsurePluginReg();
  // We pass this back to the caller, and ignore all other errors.
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return rv;
  }

  // Read cached flash info if in flash-only mode.
  if (mFlashOnly) {
    ReadFlashInfo();
  }

  // Figure out plugin directories:
  MOZ_TRY(DeterminePluginDirs());
  return NS_OK;
}

nsresult PluginFinder::Run() {
  if (!mFinishedFinding) {
    mCalledOnMainthread = NS_IsMainThread();
    mFinishedFinding = true;
    FindPlugins();  // Will call WritePluginInfo()
    if (!mCalledOnMainthread) {
      return NS_DispatchToMainThread(this);
    }
  }
  MOZ_ASSERT(NS_IsMainThread());
  if (!mShutdown) {
    // Write flash info here because we can only do so when on the main thread.
    // pluginreg.dat got written in FindPlugins if mPluginsChanged is true.
    if (mFlashOnly && mPluginsChanged) {
      WriteFlashInfo(mPlugins);
    }
    if (mCreateList) {
      mFoundPluginCallback(mPluginsChanged, mPlugins, mPluginBlocklistRequests);
    } else {
      mChangeCallback(mPluginsChanged);
    }
  }

  // The host will adopt these plugin tag instances when given them,
  // so we should delete our references.
  mPlugins = nullptr;  // Don't iterate over these as the host needs them.
  NS_ITERATIVE_UNREF_LIST(RefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);
  NS_ITERATIVE_UNREF_LIST(RefPtr<nsPluginTag>, mCachedPlugins, mNext);

  // We need to tell the host we're done. We're on the main thread,
  // but we could still be in the process of creating the pluginhost,
  // if we were called synchronously. In that case, the host is
  // responsible for cleaning us up, as we can't tell it to do so.
  // So if we were called *async*, tell the host we're done:
  if (mCreateList && !mCalledOnMainthread) {
    RefPtr<nsPluginHost> host = nsPluginHost::GetInst();
    host->FindingFinished();
  }
  return NS_OK;
}

nsresult PluginFinder::ReadFlashInfo() {
  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrefBranch> prefs = Preferences::GetRootBranch();
  nsAutoCString arch;
  nsresult rv = prefs->GetCharPref("plugin.flash.arch", arch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString ourArch;
  rv = runtime->GetXPCOMABI(ourArch);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Bail if we've changed architectures.
  if (!ourArch.Equals(arch)) {
    return NS_OK;
  }

  nsAutoCString version;
  rv = prefs->GetCharPref("plugin.flash.version", version);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString path;
  rv = prefs->GetCharPref("plugin.flash.path", path);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIFile> pluginFile;
#ifdef XP_WIN
  // "native" files don't use utf-8 paths, convert:
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(path), false,
                       getter_AddRefs(pluginFile));
#else
  rv = NS_NewNativeLocalFile(path, false, getter_AddRefs(pluginFile));
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString filename;
  rv = pluginFile->GetLeafName(filename);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t blocklistState;
  rv = Preferences::GetInt("plugin.flash.blockliststate", &blocklistState);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t lastModLo;
  int32_t lastModHi;
  rv = Preferences::GetInt("plugin.flash.lastmod_lo", &lastModLo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Preferences::GetInt("plugin.flash.lastmod_hi", &lastModHi);
  if (NS_FAILED(rv)) {
    return rv;
  }
  int64_t lastMod =
      (int64_t)(((uint64_t)lastModHi) << 32 | (uint32_t)lastModLo);

  nsAutoCString desc;
  rv = prefs->GetCharPref("plugin.flash.desc", desc);
  if (NS_FAILED(rv)) {
    return rv;
  }

  const char* const mimetypes[3] = {"application/x-shockwave-flash",
                                    "application/x-futuresplash", nullptr};
  const char* const extensions[3] = {"swf", "spl", nullptr};
  const int32_t mimetypecount = 2;
  // For some reason, the descriptions are different between Windows and
  // Linux/Mac:
#ifdef XP_WIN
  const char* const mimedescriptions[3] = {"Adobe Flash movie",
                                           "FutureSplash movie", nullptr};
#else
  const char* const mimedescriptions[3] = {"Shockwave Flash",
                                           "FutureSplash Player", nullptr};
#endif

  MOZ_ASSERT((!mPlugins) && (!mCachedPlugins));
  // We will pass this to the host:
  RefPtr<nsPluginTag> tag = new nsPluginTag(
      "Shockwave Flash", desc.get(), NS_ConvertUTF16toUTF8(filename).get(),
      path.get(), version.get(), mimetypes, mimedescriptions, extensions,
      mimetypecount, lastMod, blocklistState, true);
  mPlugins = tag;
  // And keep this in cache:
  RefPtr<nsPluginTag> cachedTag = new nsPluginTag(
      "Shockwave Flash", desc.get(), NS_ConvertUTF16toUTF8(filename).get(),
      path.get(), version.get(), mimetypes, mimedescriptions, extensions,
      mimetypecount, lastMod, blocklistState, true);
  mCachedPlugins = cachedTag;
  return NS_OK;
}

/* static */ nsresult PluginFinder::WriteFlashInfo(nsPluginTag* aPlugins) {
  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return NS_ERROR_FAILURE;
  }

  if (!aPlugins) {
    Preferences::ClearUser("plugin.flash.arch");
    Preferences::ClearUser("plugin.flash.blockliststate");
    Preferences::ClearUser("plugin.flash.desc");
    Preferences::ClearUser("plugin.flash.lastmod");
    Preferences::ClearUser("plugin.flash.path");
    Preferences::ClearUser("plugin.flash.version");
    return NS_OK;
  }
  nsPluginTag* tag = aPlugins;

  nsAutoCString arch;
  nsresult rv = runtime->GetXPCOMABI(arch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPrefBranch> prefs = Preferences::GetRootBranch();
  prefs->SetCharPref("plugin.flash.arch", arch);

  nsAutoCString path;
  tag->GetFullpath(path);
  prefs->SetCharPref("plugin.flash.path", path);

  int64_t lastModifiedTime;
  tag->GetLastModifiedTime(&lastModifiedTime);
  uint32_t lastModHi = ((uint64_t)lastModifiedTime) >> 32;
  Preferences::SetInt("plugin.flash.lastmod_hi", (int32_t)lastModHi);
  uint32_t lastModLo = ((uint64_t)lastModifiedTime) & 0xffffffff;
  Preferences::SetInt("plugin.flash.lastmod_lo", (int32_t)lastModLo);

  nsAutoCString version;
  tag->GetVersion(version);
  prefs->SetCharPref("plugin.flash.version", version);

  nsAutoCString desc;
  tag->GetDescription(desc);
  prefs->SetCharPref("plugin.flash.desc", desc);

  uint32_t blocklistState;
  tag->GetBlocklistState(&blocklistState);
  Preferences::SetInt("plugin.flash.blockliststate", blocklistState);
  return NS_OK;
}

namespace {

int64_t GetPluginLastModifiedTime(const nsCOMPtr<nsIFile>& localfile) {
  PRTime fileModTime = 0;

#if defined(XP_MACOSX)
  // On OS X the date of a bundle's "contents" (i.e. of its Info.plist file)
  // is a much better guide to when it was last modified than the date of
  // its package directory.  See bug 313700.
  nsCOMPtr<nsILocalFileMac> localFileMac = do_QueryInterface(localfile);
  if (localFileMac) {
    localFileMac->GetBundleContentsLastModifiedTime(&fileModTime);
  } else {
    localfile->GetLastModifiedTime(&fileModTime);
  }
#else
  localfile->GetLastModifiedTime(&fileModTime);
#endif

  return fileModTime;
}

struct CompareFilesByTime {
  bool LessThan(const nsCOMPtr<nsIFile>& a, const nsCOMPtr<nsIFile>& b) const {
    return GetPluginLastModifiedTime(a) < GetPluginLastModifiedTime(b);
  }

  bool Equals(const nsCOMPtr<nsIFile>& a, const nsCOMPtr<nsIFile>& b) const {
    return GetPluginLastModifiedTime(a) == GetPluginLastModifiedTime(b);
  }
};

}  // namespace

bool PluginFinder::ShouldAddPlugin(const nsPluginInfo& info) {
  if (!info.fName ||
      (strcmp(info.fName, "Shockwave Flash") != 0 && mFlashOnly)) {
    return false;
  }
  for (uint32_t i = 0; i < info.fVariantCount; ++i) {
    if (info.fMimeTypeArray[i] &&
        (!strcmp(info.fMimeTypeArray[i], "application/x-shockwave-flash") ||
         !strcmp(info.fMimeTypeArray[i],
                 "application/x-shockwave-flash-test"))) {
      return true;
    }
    if (mFlashOnly) {
      continue;
    }
    if (info.fMimeTypeArray[i] &&
        (!strcmp(info.fMimeTypeArray[i], "application/x-test") ||
         !strcmp(info.fMimeTypeArray[i], "application/x-Second-Test"))) {
      return true;
    }
  }
#ifdef PLUGIN_LOGGING
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
             ("ShouldAddPlugin : Ignoring non-flash plugin library %s\n",
              aPluginTag->FileName().get()));
#endif  // PLUGIN_LOGGING
  return false;
}

nsresult PluginFinder::ScanPluginsDirectory(nsIFile* pluginsDir,
                                            bool* aPluginsChanged) {
  MOZ_ASSERT(XRE_IsParentProcess());

  NS_ENSURE_ARG_POINTER(aPluginsChanged);
  nsresult rv;

  *aPluginsChanged = false;

#ifdef PLUGIN_LOGGING
  nsAutoCString dirPath;
  pluginsDir->GetNativePath(dirPath);
  PLUGIN_LOG(PLUGIN_LOG_BASIC,
             ("PluginFinder::ScanPluginsDirectory dir=%s\n", dirPath.get()));
#endif

  nsCOMPtr<nsIDirectoryEnumerator> iter;
  rv = pluginsDir->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) return rv;

  AutoTArray<nsCOMPtr<nsIFile>, 6> pluginFiles;

  nsCOMPtr<nsIFile> dirEntry;
  while (NS_SUCCEEDED(iter->GetNextFile(getter_AddRefs(dirEntry))) &&
         dirEntry) {
    // Sun's JRE 1.3.1 plugin must have symbolic links resolved or else it'll
    // crash. See bug 197855.
    dirEntry->Normalize();

    if (nsPluginsDir::IsPluginFile(dirEntry)) {
      pluginFiles.AppendElement(dirEntry);
    }
  }

  pluginFiles.Sort(CompareFilesByTime());

  for (int32_t i = (pluginFiles.Length() - 1); i >= 0; i--) {
    nsCOMPtr<nsIFile>& localfile = pluginFiles[i];

    nsString utf16FilePath;
    rv = localfile->GetPath(utf16FilePath);
    if (NS_FAILED(rv)) continue;

    const int64_t fileModTime = GetPluginLastModifiedTime(localfile);

    // Look for it in our cache
    NS_ConvertUTF16toUTF8 filePath(utf16FilePath);
    RefPtr<nsPluginTag> pluginTag;
    RemoveCachedPluginsInfo(filePath.get(), getter_AddRefs(pluginTag));

    bool seenBefore = false;
    uint32_t blocklistState = nsIBlocklistService::STATE_NOT_BLOCKED;

    if (pluginTag) {
      seenBefore = true;
      blocklistState = pluginTag->GetBlocklistState();
      // If plugin changed, delete cachedPluginTag and don't use cache
      if (fileModTime != pluginTag->mLastModifiedTime) {
        // Plugins has changed. Don't use cached plugin info.
        pluginTag = nullptr;

        // plugin file changed, flag this fact
        *aPluginsChanged = true;
      }

      // If we're not creating a list and we already know something changed then
      // we're done.
      if (!mCreateList) {
        if (*aPluginsChanged) {
          return NS_OK;
        }
        continue;
      }
    }

    bool isKnownInvalidPlugin = false;
    for (RefPtr<nsInvalidPluginTag> invalidPlugins = mInvalidPlugins;
         invalidPlugins; invalidPlugins = invalidPlugins->mNext) {
      // If already marked as invalid, ignore it
      if (invalidPlugins->mFullPath.Equals(filePath.get()) &&
          invalidPlugins->mLastModifiedTime == fileModTime) {
        if (mCreateList) {
          invalidPlugins->mSeen = true;
        }
        isKnownInvalidPlugin = true;
        break;
      }
    }
    if (isKnownInvalidPlugin) {
      continue;
    }

    // if it is not found in cache info list or has been changed, create a new
    // one
    if (!pluginTag) {
      nsPluginFile pluginFile(localfile);

      // create a tag describing this plugin.
      PRLibrary* library = nullptr;
      nsPluginInfo info;
      memset(&info, 0, sizeof(info));
      nsresult res;
      // Opening a block for the telemetry AutoTimer
      {
        Telemetry::AutoTimer<Telemetry::PLUGIN_LOAD_METADATA> telemetry;
        res = pluginFile.GetPluginInfo(info, &library);
      }
      // if we don't have mime type don't proceed, this is not a plugin
      if (NS_FAILED(res) || !info.fMimeTypeArray || !ShouldAddPlugin(info)) {
        RefPtr<nsInvalidPluginTag> invalidTag =
            new nsInvalidPluginTag(filePath.get(), fileModTime);
        pluginFile.FreePluginInfo(info);

        if (mCreateList) {
          invalidTag->mSeen = true;
        }
        invalidTag->mNext = mInvalidPlugins;
        if (mInvalidPlugins) {
          mInvalidPlugins->mPrev = invalidTag;
        }
        mInvalidPlugins = invalidTag;

        // Mark aPluginsChanged so pluginreg is rewritten
        *aPluginsChanged = true;
        continue;
      }

      pluginTag = new nsPluginTag(&info, fileModTime, blocklistState);
      pluginTag->mLibrary = library;
      pluginFile.FreePluginInfo(info);

      // We'll need to do a blocklist request later.
      mPluginBlocklistRequests.AppendElement(
          std::make_pair(!seenBefore, pluginTag));

      // Plugin unloading is tag-based. If we created a new tag and loaded
      // the library in the process then we want to attempt to unload it here.
      // Only do this if the pref is set for aggressive unloading.
      if (UnloadPluginsASAP()) {
        if (NS_IsMainThread()) {
          pluginTag->TryUnloadPlugin(false);
        } else {
          nsCOMPtr<nsIRunnable> unloadRunnable =
              mozilla::NewRunnableMethod<bool>(
                  "nsPluginTag::TryUnloadPlugin", pluginTag,
                  &nsPluginTag::TryUnloadPlugin, false);
          NS_DispatchToMainThread(unloadRunnable);
        }
      }
    }

    // do it if we still want it
    if (!seenBefore) {
      // We have a valid new plugin so report that plugins have changed.
      *aPluginsChanged = true;
    }

    // Don't add the same plugin again if it hasn't changed
    if (nsPluginTag* duplicate = FirstPluginWithPath(pluginTag->mFullPath)) {
      if (pluginTag->mLastModifiedTime == duplicate->mLastModifiedTime) {
        continue;
      }
    }

    // If we're not creating a plugin list, simply looking for changes,
    // then we're done.
    if (!mCreateList) {
      return NS_OK;
    }

    pluginTag->mNext = mPlugins;
    mPlugins = pluginTag;
  }

  return NS_OK;
}

// if mCreateList is false we will just scan for plugins
// and see if any changes have been made to the plugins.
// This is needed in ReloadPlugins to prevent possible recursive reloads
nsresult PluginFinder::FindPlugins() {
  Telemetry::AutoTimer<Telemetry::FIND_PLUGINS> telemetry;

  // Read cached plugins info. We ignore failures, as we can proceed
  // without a pluginreg file.
  ReadPluginInfo();

  for (nsIFile* pluginDir : mPluginDirs) {
    if (!pluginDir) {
      continue;
    }
    // Ensure we have a directory; if this isn't a directory, try the parent.
    // We do this because in some cases items in this list of supposed plugin
    // directories can be individual plugin files instead of directories.
    bool isDir = false;
    nsCOMPtr<nsIFile> parent;
    if (NS_FAILED(pluginDir->IsDirectory(&isDir)) || !isDir) {
      pluginDir->GetParent(getter_AddRefs(parent));
      pluginDir = parent;
      if (!pluginDir) {
        continue;
      }
    }
    // pluginDir not existing will be handled transparently in
    // ScanPluginsDirectory

    // don't pass mPluginsChanged directly, to prevent its
    // possible reset in subsequent ScanPluginsDirectory calls
    bool pluginschanged = false;
    ScanPluginsDirectory(pluginDir, &pluginschanged);

    if (pluginschanged) {
      mPluginsChanged = true;
      if (!mCreateList) {
        // We're just looking for changes, so we can stop looking.
        break;
      }
    }
  }

  // if we are just looking for possible changes,
  // no need to proceed if changes are detected
  if (!mCreateList && mPluginsChanged) {
    NS_ITERATIVE_UNREF_LIST(RefPtr<nsPluginTag>, mCachedPlugins, mNext);
    NS_ITERATIVE_UNREF_LIST(RefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);
    return NS_OK;
  }

  // We should also consider plugins to have changed if any plugins have been
  // removed. We'll know if any were removed if they weren't taken out of the
  // cached plugins list during our scan, thus we can assume something was
  // removed if the cached plugins list contains anything.
  if (!mPluginsChanged && mCachedPlugins) {
    mPluginsChanged = true;
  }

  // Remove unseen invalid plugins
  RefPtr<nsInvalidPluginTag> invalidPlugins = mInvalidPlugins;
  while (invalidPlugins) {
    if (!invalidPlugins->mSeen) {
      RefPtr<nsInvalidPluginTag> invalidPlugin = invalidPlugins;

      if (invalidPlugin->mPrev) {
        invalidPlugin->mPrev->mNext = invalidPlugin->mNext;
      } else {
        mInvalidPlugins = invalidPlugin->mNext;
      }
      if (invalidPlugin->mNext) {
        invalidPlugin->mNext->mPrev = invalidPlugin->mPrev;
      }

      invalidPlugins = invalidPlugin->mNext;

      invalidPlugin->mPrev = nullptr;
      invalidPlugin->mNext = nullptr;
    } else {
      invalidPlugins->mSeen = false;
      invalidPlugins = invalidPlugins->mNext;
    }
  }

  // if we are not creating the list, there is no need to proceed
  if (!mCreateList) {
    NS_ITERATIVE_UNREF_LIST(RefPtr<nsPluginTag>, mCachedPlugins, mNext);
    NS_ITERATIVE_UNREF_LIST(RefPtr<nsInvalidPluginTag>, mInvalidPlugins, mNext);
    return NS_OK;
  }

  // if we are creating the list, it is already done;
  // update the plugins info cache if changes are detected
  if (mPluginsChanged) {
    WritePluginInfo(mFlashOnly, mPlugins, mInvalidPlugins);
  }

  return NS_OK;
}

/* static */ nsresult PluginFinder::WritePluginInfo(
    bool aFlashOnly, nsPluginTag* aPlugins,
    nsInvalidPluginTag* aInvalidPlugins) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // We can't write to prefs from non-main threads. Run() will update prefs
  // from the main thread if we're flash only.
  // We can't separate this out completely because the blocklist updates
  // from nsPluginHost expect to be able to write here.
  if (NS_IsMainThread() && aFlashOnly) {
    WriteFlashInfo(aPlugins);
  }

  if (!sPluginRegFile) return NS_ERROR_FAILURE;

  // Get the tmp file by getting the parent and then re-appending
  // kPluginRegistryFilename followed by `.tmp`.
  nsCOMPtr<nsIFile> pluginReg;
  nsresult rv = sPluginRegFile->GetParent(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv)) return rv;

  nsAutoCString filename(kPluginRegistryFilename);
  filename.AppendLiteral(".tmp");
  rv = pluginReg->AppendNative(filename);
  if (NS_FAILED(rv)) return rv;

  PRFileDesc* fd = nullptr;
  rv = pluginReg->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                   0600, &fd);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString arch;
  rv = runtime->GetXPCOMABI(arch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PR_fprintf(fd, "Generated File. Do not edit.\n");

  PR_fprintf(fd, "\n[HEADER]\nVersion%c%s%c%c%c\nArch%c%s%c%c\n",
             PLUGIN_REGISTRY_FIELD_DELIMITER, kPluginRegistryVersion,
             aFlashOnly ? 't' : 'f', PLUGIN_REGISTRY_FIELD_DELIMITER,
             PLUGIN_REGISTRY_END_OF_LINE_MARKER,
             PLUGIN_REGISTRY_FIELD_DELIMITER, arch.get(),
             PLUGIN_REGISTRY_FIELD_DELIMITER,
             PLUGIN_REGISTRY_END_OF_LINE_MARKER);

  // Store all plugins in the mPlugins list - all plugins currently in use.
  PR_fprintf(fd, "\n[PLUGINS]\n");

  // Do not write plugin info if we're in flash-only mode.
  for (nsPluginTag* tag = aPlugins; !aFlashOnly && tag; tag = tag->mNext) {
    // store each plugin info into the registry
    // filename & fullpath are on separate line
    // because they can contain field delimiter char
    PR_fprintf(
        fd, "%s%c%c\n%s%c%c\n%s%c%c\n", (tag->FileName().get()),
        PLUGIN_REGISTRY_FIELD_DELIMITER, PLUGIN_REGISTRY_END_OF_LINE_MARKER,
        (tag->mFullPath.get()), PLUGIN_REGISTRY_FIELD_DELIMITER,
        PLUGIN_REGISTRY_END_OF_LINE_MARKER, (tag->Version().get()),
        PLUGIN_REGISTRY_FIELD_DELIMITER, PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    // lastModifiedTimeStamp|canUnload|tag->mFlags|fromExtension|blocklistState
    PR_fprintf(fd, "%lld%c%d%c%lu%c%d%c%d%c%c\n", tag->mLastModifiedTime,
               PLUGIN_REGISTRY_FIELD_DELIMITER,
               false,  // did store whether or not to unload in-process plugins
               PLUGIN_REGISTRY_FIELD_DELIMITER,
               0,  // legacy field for flags
               PLUGIN_REGISTRY_FIELD_DELIMITER, false,
               PLUGIN_REGISTRY_FIELD_DELIMITER, tag->BlocklistState(),
               PLUGIN_REGISTRY_FIELD_DELIMITER,
               PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    // description, name & mtypecount are on separate line
    PR_fprintf(fd, "%s%c%c\n%s%c%c\n%d\n", (tag->Description().get()),
               PLUGIN_REGISTRY_FIELD_DELIMITER,
               PLUGIN_REGISTRY_END_OF_LINE_MARKER, (tag->Name().get()),
               PLUGIN_REGISTRY_FIELD_DELIMITER,
               PLUGIN_REGISTRY_END_OF_LINE_MARKER, tag->MimeTypes().Length());

    // Add in each mimetype this plugin supports
    for (uint32_t i = 0; i < tag->MimeTypes().Length(); i++) {
      PR_fprintf(fd, "%d%c%s%c%s%c%s%c%c\n", i, PLUGIN_REGISTRY_FIELD_DELIMITER,
                 (tag->MimeTypes()[i].get()), PLUGIN_REGISTRY_FIELD_DELIMITER,
                 (tag->MimeDescriptions()[i].get()),
                 PLUGIN_REGISTRY_FIELD_DELIMITER, (tag->Extensions()[i].get()),
                 PLUGIN_REGISTRY_FIELD_DELIMITER,
                 PLUGIN_REGISTRY_END_OF_LINE_MARKER);
    }
  }

  PR_fprintf(fd, "\n[INVALID]\n");

  RefPtr<nsInvalidPluginTag> invalidPlugins = aInvalidPlugins;
  while (invalidPlugins) {
    // fullPath
    PR_fprintf(
        fd, "%s%c%c\n",
        (!invalidPlugins->mFullPath.IsEmpty() ? invalidPlugins->mFullPath.get()
                                              : ""),
        PLUGIN_REGISTRY_FIELD_DELIMITER, PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    // lastModifiedTimeStamp
    PR_fprintf(fd, "%lld%c%c\n", invalidPlugins->mLastModifiedTime,
               PLUGIN_REGISTRY_FIELD_DELIMITER,
               PLUGIN_REGISTRY_END_OF_LINE_MARKER);

    invalidPlugins = invalidPlugins->mNext;
  }

  PRStatus prrc;
  prrc = PR_Close(fd);
  if (prrc != PR_SUCCESS) {
    // we should obtain a refined value based on prrc;
    rv = NS_ERROR_FAILURE;
    MOZ_ASSERT(false, "PR_Close() failed.");
    return rv;
  }
  rv = pluginReg->MoveToNative(nullptr, kPluginRegistryFilename);
  return rv;
}

/* static */ nsresult PluginFinder::EnsurePluginReg() {
  if (sPluginRegFile) {
    return NS_OK;
  }

  if (!NS_IsMainThread()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService(
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIFile> pluginRegFile;
  directoryService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                        getter_AddRefs(pluginRegFile));

  if (!pluginRegFile) {
    // There is no profile yet, this will tell us if there is going to be one
    // in the future.
    directoryService->Get(NS_APP_PROFILE_DIR_STARTUP, NS_GET_IID(nsIFile),
                          getter_AddRefs(pluginRegFile));
    if (!pluginRegFile) {
      return NS_ERROR_FAILURE;
    }

    return NS_ERROR_NOT_AVAILABLE;
  }
  pluginRegFile->AppendNative(kPluginRegistryFilename);
  sPluginRegFile = pluginRegFile;
  ClearOnShutdown(&sPluginRegFile);
  return NS_OK;
}

nsresult PluginFinder::DeterminePluginDirs() {
  nsresult rv;
  nsCOMPtr<nsIProperties> dirService(
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the app-defined list of plugin dirs.
  nsCOMPtr<nsISimpleEnumerator> dirEnum;
  MOZ_TRY(dirService->Get(NS_APP_PLUGINS_DIR_LIST,
                          NS_GET_IID(nsISimpleEnumerator),
                          getter_AddRefs(dirEnum)));

  // Add any paths from MOZ_PLUGIN_PATH first.
#if defined(XP_WIN) || defined(XP_LINUX)
#  ifdef XP_WIN
#    define PATH_SEPARATOR ';'
#  else
#    define PATH_SEPARATOR ':'
#  endif
  const char* pathsenv = PR_GetEnv("MOZ_PLUGIN_PATH");
  if (pathsenv) {
    const nsDependentCString pathsStr(pathsenv);
    nsCCharSeparatedTokenizer paths(pathsStr, PATH_SEPARATOR);
    while (paths.hasMoreTokens()) {
      auto pathStr = paths.nextToken();
      nsCOMPtr<nsIFile> pathFile;
      rv = NS_NewNativeLocalFile(pathStr, true, getter_AddRefs(pathFile));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      bool exists;
      if (pathFile && NS_SUCCEEDED(pathFile->Exists(&exists)) && exists) {
        mPluginDirs.AppendElement(pathFile);
      }
    }
  }
#endif  // defined(XP_WIN) || defined(XP_LINUX)

  bool hasMore = false;
  while (NS_SUCCEEDED(dirEnum->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    nsresult rv = dirEnum->GetNext(getter_AddRefs(supports));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIFile> nextDir(do_QueryInterface(supports, &rv));
      if (NS_SUCCEEDED(rv)) {
        mPluginDirs.AppendElement(nextDir);
      }
    }
  }

  // In tests, load plugins from the profile.
  if (xpc::IsInAutomation()) {
    nsCOMPtr<nsIFile> profDir;
    rv = dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                         getter_AddRefs(profDir));
    if (NS_SUCCEEDED(rv)) {
      profDir->Append(NS_LITERAL_STRING("plugins"));
      mPluginDirs.AppendElement(profDir);
    }
  }

  // Get the directories from the windows registry. Note: these can
  // be files instead of directories. We'll have to filter them later.
#ifdef XP_WIN
  bool bScanPLIDs = Preferences::GetBool("plugin.scan.plid.all", false);
  if (bScanPLIDs) {
    GetPLIDDirectories(mPluginDirs);
  }
#endif /* XP_WIN */
  return NS_OK;
}

nsresult PluginFinder::ReadPluginInfo() {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Non-flash-only (for tests that use other plugins) and updates to the list
  // sadly still use the main thread - but assert about the flash startup case:
  if (mFlashOnly && mCreateList) {
    MOZ_ASSERT(!NS_IsMainThread());
  } else {
    MOZ_ASSERT(NS_IsMainThread());
  }

  nsresult rv = EnsurePluginReg();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Cloning ensures we don't have a stat cache and get an
  // accurate filesize.
  nsCOMPtr<nsIFile> pluginReg;
  rv = sPluginRegFile->Clone(getter_AddRefs(pluginReg));
  if (NS_FAILED(rv)) return rv;

  int64_t fileSize;
  rv = pluginReg->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) return rv;

  if (fileSize > INT32_MAX) {
    return NS_ERROR_FAILURE;
  }
  int32_t flen = int32_t(fileSize);
  if (flen == 0) {
    NS_WARNING("Plugins Registry Empty!");
    return NS_OK;  // ERROR CONDITION
  }

  nsPluginManifestLineReader reader;
  char* registry = reader.Init(flen);
  if (!registry) return NS_ERROR_OUT_OF_MEMORY;

  PRFileDesc* fd = nullptr;
  rv = pluginReg->OpenNSPRFileDesc(PR_RDONLY, 0444, &fd);
  if (NS_FAILED(rv)) return rv;

  // set rv to return an error on goto out
  rv = NS_ERROR_FAILURE;

  // We know how many octes we are supposed to read.
  // So let use the busy_beaver_PR_Read version.
  int32_t bread = busy_beaver_PR_Read(fd, registry, flen);

  PRStatus prrc;
  prrc = PR_Close(fd);
  if (prrc != PR_SUCCESS) {
    // Strange error: this is one of those "Should not happen" error.
    // we may want to report something more refined than  NS_ERROR_FAILURE.
    MOZ_ASSERT(false, "PR_Close() failed.");
    return rv;
  }

  // short read error, so to speak.
  if (flen > bread) return rv;

  if (!ReadSectionHeader(reader, "HEADER")) return rv;

  if (!reader.NextLine()) return rv;

  char* values[6];

  // VersionLiteral, kPluginRegistryVersion
  if (2 != reader.ParseLine(values, 2)) return rv;

  // VersionLiteral
  if (PL_strcmp(values[0], "Version")) return rv;

  // If we're reading an old registry, ignore it
  // If we flipped the flash-only pref, ignore it
  nsAutoCString expectedVersion(kPluginRegistryVersion);
  expectedVersion.Append(mFlashOnly ? 't' : 'f');

  if (!expectedVersion.Equals(values[1])) {
    return rv;
  }

  char* archValues[6];
  if (!reader.NextLine()) {
    return rv;
  }

  // ArchLiteral, Architecture
  if (2 != reader.ParseLine(archValues, 2)) {
    return rv;
  }

  // ArchLiteral
  if (PL_strcmp(archValues[0], "Arch")) {
    return rv;
  }

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return rv;
  }

  nsAutoCString arch;
  if (NS_FAILED(runtime->GetXPCOMABI(arch))) {
    return rv;
  }

  // If this is a registry from a different architecture then don't attempt to
  // read it
  if (PL_strcmp(archValues[1], arch.get())) {
    return rv;
  }

  if (!ReadSectionHeader(reader, "PLUGINS")) return rv;

  while (reader.NextLine()) {
    if (*reader.LinePtr() == '[') {
      break;
    }
    // Ignore all listed plugins if we're in flash-only mode.
    // In that case, we're only reading this file to find invalid plugin info
    // so we can avoid reading/loading those.
    if (mFlashOnly) {
      continue;
    }

    const char* filename = reader.LinePtr();
    if (!reader.NextLine()) return rv;

    const char* fullpath = reader.LinePtr();
    if (!reader.NextLine()) return rv;

    const char* version;
    version = reader.LinePtr();
    if (!reader.NextLine()) return rv;

    // lastModifiedTimeStamp|canUnload|tag.mFlag|fromExtension|blocklistState
    if (5 != reader.ParseLine(values, 5)) return rv;

    int64_t lastmod = nsCRT::atoll(values[0]);
    uint16_t blocklistState = atoi(values[4]);
    if (!reader.NextLine()) return rv;

    char* description = reader.LinePtr();
    if (!reader.NextLine()) return rv;

    const char* name = reader.LinePtr();
    if (!reader.NextLine()) return rv;

    const long PLUGIN_REG_MIMETYPES_ARRAY_SIZE = 12;
    const long PLUGIN_REG_MAX_MIMETYPES = 1000;

    long mimetypecount = std::strtol(reader.LinePtr(), nullptr, 10);
    if (mimetypecount == LONG_MAX || mimetypecount == LONG_MIN ||
        mimetypecount >= PLUGIN_REG_MAX_MIMETYPES || mimetypecount < 0) {
      return NS_ERROR_FAILURE;
    }

    char* stackalloced[PLUGIN_REG_MIMETYPES_ARRAY_SIZE * 3];
    char** mimetypes;
    char** mimedescriptions;
    char** extensions;
    char** heapalloced = 0;
    if (mimetypecount > PLUGIN_REG_MIMETYPES_ARRAY_SIZE - 1) {
      heapalloced = new char*[mimetypecount * 3];
      mimetypes = heapalloced;
    } else {
      mimetypes = stackalloced;
    }
    mimedescriptions = mimetypes + mimetypecount;
    extensions = mimedescriptions + mimetypecount;

    int mtr = 0;  // mimetype read
    for (; mtr < mimetypecount; mtr++) {
      if (!reader.NextLine()) break;

      // line number|mimetype|description|extension
      if (4 != reader.ParseLine(values, 4)) break;
      int line = atoi(values[0]);
      if (line != mtr) break;
      mimetypes[mtr] = values[1];
      mimedescriptions[mtr] = values[2];
      extensions[mtr] = values[3];
    }

    if (mtr != mimetypecount) {
      delete[] heapalloced;
      return rv;
    }

    RefPtr<nsPluginTag> tag = new nsPluginTag(
        name, description, filename, fullpath, version,
        (const char* const*)mimetypes, (const char* const*)mimedescriptions,
        (const char* const*)extensions, mimetypecount, lastmod, blocklistState,
        true);

    delete[] heapalloced;

    // Import flags from registry into prefs for old registry versions
    MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_BASIC,
            ("LoadCachedPluginsInfo : Loading Cached plugininfo for %s\n",
             tag->FileName().get()));

    tag->mNext = mCachedPlugins;
    mCachedPlugins = tag;
  }

  if (!ReadSectionHeader(reader, "INVALID")) {
    return rv;
  }

  while (reader.NextLine()) {
    const char* fullpath = reader.LinePtr();
    if (!reader.NextLine()) {
      return rv;
    }

    const char* lastModifiedTimeStamp = reader.LinePtr();
    int64_t lastmod = nsCRT::atoll(lastModifiedTimeStamp);

    RefPtr<nsInvalidPluginTag> invalidTag =
        new nsInvalidPluginTag(fullpath, lastmod);

    invalidTag->mNext = mInvalidPlugins;
    if (mInvalidPlugins) {
      mInvalidPlugins->mPrev = invalidTag;
    }
    mInvalidPlugins = invalidTag;
  }

  return NS_OK;
}

void PluginFinder::RemoveCachedPluginsInfo(const char* filePath,
                                           nsPluginTag** result) {
  RefPtr<nsPluginTag> prev;
  RefPtr<nsPluginTag> tag = mCachedPlugins;
  while (tag) {
    if (tag->mFullPath.Equals(filePath)) {
      // Found it. Remove it from our list
      if (prev)
        prev->mNext = tag->mNext;
      else
        mCachedPlugins = tag->mNext;
      tag->mNext = nullptr;
      *result = tag;
      NS_ADDREF(*result);
      break;
    }
    prev = tag;
    tag = tag->mNext;
  }
}
