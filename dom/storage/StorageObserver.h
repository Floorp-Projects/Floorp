/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageObserver_h
#define mozilla_dom_StorageObserver_h

#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "nsINamed.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "nsTObserverArray.h"
#include "nsString.h"

namespace mozilla::dom {

class StorageObserver;

// Main-thread interface implemented by legacy LocalStorageManager and current
// SessionStorageManager for direct consumption. Also implemented by legacy
// StorageDBParent and current SessionStorageObserverParent for propagation to
// content processes.
class StorageObserverSink
    : public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
 public:
  virtual ~StorageObserverSink() = default;

 private:
  friend class StorageObserver;
  virtual nsresult Observe(const char* aTopic,
                           const nsAString& aOriginAttributesPattern,
                           const nsACString& aOriginScope) = 0;
};

// Statically (through layout statics) initialized observer receiving and
// processing chrome clearing notifications, such as cookie deletion etc.
class StorageObserver : public nsIObserver,
                        public nsINamed,
                        public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSINAMED

  static nsresult Init();
  static nsresult Shutdown();
  static StorageObserver* Self() { return sSelf; }

  void AddSink(StorageObserverSink* aObs);
  void RemoveSink(StorageObserverSink* aObs);
  void Notify(const char* aTopic,
              const nsAString& aOriginAttributesPattern = u""_ns,
              const nsACString& aOriginScope = ""_ns);

  void NoteBackgroundThread(uint32_t aPrivateBrowsingId,
                            nsIEventTarget* aBackgroundThread);

 private:
  virtual ~StorageObserver() = default;

  nsresult GetOriginScope(const char16_t* aData, nsACString& aOriginScope);

  static void TestingPrefChanged(const char* aPrefName, void* aClosure);

  static StorageObserver* sSelf;

  nsCOMPtr<nsIEventTarget> mBackgroundThread[2];

  // Weak references
  nsTObserverArray<CheckedUnsafePtr<StorageObserverSink>> mSinks;
  nsCOMPtr<nsITimer> mDBThreadStartDelayTimer;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_StorageObserver_h
