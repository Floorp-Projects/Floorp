/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PluginFinder_h_
#define PluginFinder_h_

#include "nsIRunnable.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsPluginTags.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsIAsyncShutdown.h"

class nsIFile;
class nsPluginHost;

class nsInvalidPluginTag : public nsISupports {
  virtual ~nsInvalidPluginTag();

 public:
  explicit nsInvalidPluginTag(const char* aFullPath,
                              int64_t aLastModifiedTime = 0);

  NS_DECL_THREADSAFE_ISUPPORTS

  nsCString mFullPath;
  int64_t mLastModifiedTime;
  bool mSeen;

  RefPtr<nsInvalidPluginTag> mPrev;
  RefPtr<nsInvalidPluginTag> mNext;
};

static inline bool UnloadPluginsASAP() {
  return mozilla::StaticPrefs::dom_ipc_plugins_unloadTimeoutSecs() == 0;
}

/**
 * Class responsible for discovering plugins on disk, in part based
 * on cached information stored in pluginreg.dat and in the prefs.
 *
 * This runnable is designed to be used as a one-shot. It's created
 * when the pluginhost wants to find plugins, and the next time it
 * wants to do so, it should create a new one.
 */
class PluginFinder final : public nsIRunnable, public nsIAsyncShutdownBlocker {
  ~PluginFinder() = default;

 public:
  explicit PluginFinder(bool aFlashOnly);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  typedef std::function<void(
      bool /* aPluginsChanged */, RefPtr<nsPluginTag> /* aNewPlugins */,
      nsTArray<mozilla::Pair<
          bool, RefPtr<nsPluginTag>>>&) /* aBlocklistRequestPairs */>
      FoundPluginCallback;
  typedef std::function<void(bool /* aPluginsChanged */)> PluginChangeCallback;

  // The expectation is that one (and only one) of these gets called
  // by the plugin host. We can't easily do the work in the constructor
  // because in some cases we'll fail early (e.g. if we're called before
  // the profile is ready) and want to signal that to the consumer via
  // the nsresult return value, which we can't do in a constructor.
  nsresult DoFullSearch(const FoundPluginCallback& aCallback);
  nsresult HavePluginsChanged(const PluginChangeCallback& aCallback);

  static nsresult WritePluginInfo(
      bool aFlashOnly, nsPluginTag* aPlugins,
      nsInvalidPluginTag* aInvalidPlugins = nullptr);

 private:
  nsPluginTag* FirstPluginWithPath(const nsCString& path);
  bool ShouldAddPlugin(const nsPluginInfo& aInfo);

  nsresult ReadFlashInfo();
  static nsresult WriteFlashInfo(nsPluginTag* aPlugins);
  static nsresult EnsurePluginReg();

  nsresult ReadPluginInfo();
  nsresult ReadPluginInfoFromDisk();

  nsresult DeterminePluginDirs();

  nsresult ScanPluginsDirectory(nsIFile* aPluginsDir, bool* aPluginsChanged);
  nsresult FindPlugins();

  void RemoveCachedPluginsInfo(const char* filePath, nsPluginTag** result);

  nsTArray<nsCOMPtr<nsIFile>> mPluginDirs;

  // The plugins we've found on disk (to be passed to the host)
  RefPtr<nsPluginTag> mPlugins;
  // The plugin whose metadata we found in cache
  RefPtr<nsPluginTag> mCachedPlugins;
  // Any invalid plugins our cache info made us aware of prior to actually
  // examining the disk; this is a performance optimization to avoid trying
  // to load files that aren't plugins on every browser startup.
  RefPtr<nsInvalidPluginTag> mInvalidPlugins;

  // The bool (shouldSoftBlock) indicates whether we should disable the plugin
  // if it's soft-blocked in the blocklist
  // The plugintag is a reference to the actual plugin whose blocklist state
  // needs checking.
  nsTArray<mozilla::Pair<bool, RefPtr<nsPluginTag>>> mPluginBlocklistRequests;

  FoundPluginCallback mFoundPluginCallback;
  PluginChangeCallback mChangeCallback;
  RefPtr<nsPluginHost> mHost;

  bool mFlashOnly;
  bool mCreateList;
  bool mPluginsChanged;
  bool mFinishedFinding;
  bool mCalledOnMainthread;
  bool mShutdown;
};

#endif
