/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageObserver_h
#define mozilla_dom_StorageObserver_h

#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class StorageObserver;

// Implementers are StorageManager and StorageDBParent to forward to
// child processes.
class StorageObserverSink
{
public:
  virtual ~StorageObserverSink() {}

private:
  friend class StorageObserver;
  virtual nsresult Observe(const char* aTopic,
                           const nsAString& aOriginAttributesPattern,
                           const nsACString& aOriginScope) = 0;
};

// Statically (through layout statics) initialized observer receiving and
// processing chrome clearing notifications, such as cookie deletion etc.
class StorageObserver : public nsIObserver
                      , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsresult Init();
  static nsresult Shutdown();
  static StorageObserver* Self() { return sSelf; }

  void AddSink(StorageObserverSink* aObs);
  void RemoveSink(StorageObserverSink* aObs);
  void Notify(const char* aTopic,
              const nsAString& aOriginAttributesPattern = EmptyString(),
              const nsACString& aOriginScope = EmptyCString());

private:
  virtual ~StorageObserver() {}

  static void TestingPrefChanged(const char* aPrefName, void* aClosure);

  static StorageObserver* sSelf;

  // Weak references
  nsTArray<StorageObserverSink*> mSinks;
  nsCOMPtr<nsITimer> mDBThreadStartDelayTimer;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageObserver_h
