/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPService_h_
#define GMPService_h_

#include "nsString.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Mutex.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

template <class> struct already_AddRefed;

namespace mozilla {
namespace gmp {

class GMPParent;

class GeckoMediaPluginService MOZ_FINAL : public mozIGeckoMediaPluginService
                                        , public nsIObserver
{
public:
  static already_AddRefed<GeckoMediaPluginService> GetGeckoMediaPluginService();

  GeckoMediaPluginService();
  void Init();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZIGECKOMEDIAPLUGINSERVICE
  NS_DECL_NSIOBSERVER

private:
  ~GeckoMediaPluginService();

  GMPParent* SelectPluginForAPI(const nsAString& aOrigin,
                                const nsCString& aAPI,
                                const nsTArray<nsCString>& aTags);

  void UnloadPlugins();

  void LoadFromEnvironment();
  void ProcessPossiblePlugin(nsIFile* aDir);

  void AddOnGMPThread(const nsAString& aSearchDir);
  void RemoveOnGMPThread(const nsAString& aSearchDir);

  class PathRunnable : public nsRunnable
  {
  public:
    PathRunnable(GeckoMediaPluginService* service, const nsAString& path,
                 bool add)
      : mService(service)
      , mPath(path)
      , mAdd(add)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsRefPtr<GeckoMediaPluginService> mService;
    nsString mPath;
    bool mAdd;
  };

  Mutex mMutex; // Protects mGMPThread and mShuttingDown and mPlugins
  nsTArray<nsRefPtr<GMPParent>> mPlugins;
  nsCOMPtr<nsIThread> mGMPThread;
  bool mShuttingDown;
  bool mShuttingDownOnGMPThread;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPService_h_
