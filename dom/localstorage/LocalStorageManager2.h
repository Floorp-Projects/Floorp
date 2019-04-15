/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LocalStorageManager2_h
#define mozilla_dom_localstorage_LocalStorageManager2_h

#include "nsIDOMStorageManager.h"
#include "nsILocalStorageManager.h"

namespace mozilla {
namespace dom {

class LSRequestChild;
class LSRequestChildCallback;
class LSRequestParams;
class LSSimpleRequestParams;
class Promise;

/**
 * Under LSNG this exposes nsILocalStorageManager::Preload to ContentParent to
 * trigger preloading.  Otherwise, this is basically just a place for test logic
 * that doesn't make sense to put directly on the Storage WebIDL interface.
 *
 * Previously, the nsIDOMStorageManager XPCOM interface was also used by
 * nsGlobalWindowInner to interact with LocalStorage, but in these de-XPCOM
 * days, we've moved to just directly reference the relevant concrete classes
 * (ex: LSObject) directly.
 *
 * Note that testing methods are now also directly exposed on the Storage WebIDL
 * interface for simplicity/sanity.
 */
class LocalStorageManager2 final : public nsIDOMStorageManager,
                                   public nsILocalStorageManager {
 public:
  LocalStorageManager2();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER
  NS_DECL_NSILOCALSTORAGEMANAGER

  /**
   * Helper to trigger an LSRequest and resolve/reject the provided promise when
   * the result comes in.  This routine is notable because the LSRequest
   * mechanism is normally used synchronously from content, but here it's
   * exposed asynchronously.
   */
  LSRequestChild* StartRequest(const LSRequestParams& aParams,
                               LSRequestChildCallback* aCallback);

 private:
  ~LocalStorageManager2();

  /**
   * Helper to trigger an LSSimpleRequst and resolve/reject the provided promise
   * when the result comes in.
   */
  nsresult StartSimpleRequest(Promise* aPromise,
                              const LSSimpleRequestParams& aParams);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LocalStorageManager2_h
