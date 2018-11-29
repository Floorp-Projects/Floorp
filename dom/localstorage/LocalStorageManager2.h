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

class LSSimpleRequestParams;
class Promise;

class LocalStorageManager2 final
  : public nsIDOMStorageManager
  , public nsILocalStorageManager
{
public:
  LocalStorageManager2();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER
  NS_DECL_NSILOCALSTORAGEMANAGER

private:
  ~LocalStorageManager2();

  nsresult
  StartSimpleRequest(Promise* aPromise,
                     const LSSimpleRequestParams& aParams);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_LocalStorageManager2_h
