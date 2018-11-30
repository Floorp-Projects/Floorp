/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_osfileconstants_h__
#define mozilla_osfileconstants_h__

#include "nsIObserver.h"
#include "nsIOSFileConstantsService.h"
#include "mozilla/Attributes.h"

namespace mozilla {

/**
 * XPConnect initializer, for use in the main thread.
 * This class is thread-safe but it must be first be initialized on the
 * main-thread.
 */
class OSFileConstantsService final : public nsIOSFileConstantsService,
                                     public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOSFILECONSTANTSSERVICE
  NS_DECL_NSIOBSERVER

  static already_AddRefed<OSFileConstantsService> GetOrCreate();

  bool DefineOSFileConstants(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

 private:
  nsresult InitOSFileConstants();

  OSFileConstantsService();
  ~OSFileConstantsService();

  bool mInitialized;

  struct Paths;
  UniquePtr<Paths> mPaths;

  /**
   * (Unix) the umask, which goes in OS.Constants.Sys but
   * can only be looked up (via the system-info service)
   * on the main thread.
   */
  uint32_t mUserUmask;
};

}  // namespace mozilla

#endif  // mozilla_osfileconstants_h__
