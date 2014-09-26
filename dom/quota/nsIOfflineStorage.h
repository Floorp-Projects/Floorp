/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIOfflineStorage_h__
#define nsIOfflineStorage_h__

#include "mozilla/dom/quota/PersistenceType.h"

#define NS_OFFLINESTORAGE_IID \
  {0x91c57bf2, 0x0eda, 0x4db6, {0x9f, 0xf6, 0xcb, 0x38, 0x26, 0x8d, 0xb3, 0x01}}

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class ContentParent;

namespace quota {

class Client;

}
}
}

class nsIOfflineStorage : public nsISupports
{
public:
  typedef mozilla::dom::ContentParent ContentParent;
  typedef mozilla::dom::quota::Client Client;
  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_OFFLINESTORAGE_IID)

  NS_IMETHOD_(const nsACString&)
  Id() = 0;

  NS_IMETHOD_(Client*)
  GetClient() = 0;

  NS_IMETHOD_(bool)
  IsOwnedByWindow(nsPIDOMWindow* aOwner) = 0;

  NS_IMETHOD_(bool)
  IsOwnedByProcess(ContentParent* aOwner) = 0;

  NS_IMETHOD_(PersistenceType)
  Type()
  {
    return mPersistenceType;
  }

  NS_IMETHOD_(const nsACString&)
  Group()
  {
    return mGroup;
  }

  NS_IMETHOD_(const nsACString&)
  Origin() = 0;

  // Implementation of this method should close the storage (without aborting
  // running operations nor discarding pending operations).
  NS_IMETHOD_(nsresult)
  Close() = 0;

  // Whether or not the storage has had Close called on it.
  NS_IMETHOD_(bool)
  IsClosed() = 0;

  // Implementation of this method should close the storage, all running
  // operations should be aborted and pending operations should be discarded.
  NS_IMETHOD_(void)
  Invalidate() = 0;

protected:
  nsIOfflineStorage()
  : mPersistenceType(mozilla::dom::quota::PERSISTENCE_TYPE_INVALID)
  { }

  virtual ~nsIOfflineStorage()
  { }

  PersistenceType mPersistenceType;
  nsCString mGroup;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIOfflineStorage, NS_OFFLINESTORAGE_IID)

#define NS_DECL_NSIOFFLINESTORAGE                                              \
  NS_IMETHOD_(const nsACString&)                                               \
  Id() MOZ_OVERRIDE;                                                           \
                                                                               \
  NS_IMETHOD_(Client*)                                                         \
  GetClient() MOZ_OVERRIDE;                                                    \
                                                                               \
  NS_IMETHOD_(bool)                                                            \
  IsOwnedByWindow(nsPIDOMWindow* aOwner) MOZ_OVERRIDE;                         \
                                                                               \
  NS_IMETHOD_(bool)                                                            \
  IsOwnedByProcess(ContentParent* aOwner) MOZ_OVERRIDE;                        \
                                                                               \
  NS_IMETHOD_(const nsACString&)                                               \
  Origin() MOZ_OVERRIDE;                                                       \
                                                                               \
  NS_IMETHOD_(nsresult)                                                        \
  Close() MOZ_OVERRIDE;                                                        \
                                                                               \
  NS_IMETHOD_(bool)                                                            \
  IsClosed() MOZ_OVERRIDE;                                                     \
                                                                               \
  NS_IMETHOD_(void)                                                            \
  Invalidate() MOZ_OVERRIDE;

#endif // nsIOfflineStorage_h__
