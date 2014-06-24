/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDOMStorageObserver_h__
#define nsIDOMStorageObserver_h__

#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class DOMStorageObserver;

// Implementers are DOMStorageManager and DOMStorageDBParent to forward to
// child processes.
class DOMStorageObserverSink
{
public:
  virtual ~DOMStorageObserverSink() {}

private:
  friend class DOMStorageObserver;
  virtual nsresult Observe(const char* aTopic, const nsACString& aScopePrefix) = 0;
};

// Statically (though layout statics) initialized observer receiving and processing
// chrome clearing notifications, such as cookie deletion etc.
class DOMStorageObserver : public nsIObserver
                         , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsresult Init();
  static nsresult Shutdown();
  static DOMStorageObserver* Self() { return sSelf; }

  void AddSink(DOMStorageObserverSink* aObs);
  void RemoveSink(DOMStorageObserverSink* aObs);
  void Notify(const char* aTopic, const nsACString& aData = EmptyCString());

private:
  virtual ~DOMStorageObserver() {}

  static DOMStorageObserver* sSelf;

  // Weak references
  nsTArray<DOMStorageObserverSink*> mSinks;
  nsCOMPtr<nsITimer> mDBThreadStartDelayTimer;
};

} // ::dom
} // ::mozilla

#endif
