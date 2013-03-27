/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIOfflineStorage_h__
#define nsIOfflineStorage_h__

#include "nsIFileStorage.h"

#define NS_OFFLINESTORAGE_IID \
  {0xe531b6e0, 0x55b8, 0x4f39, \
  { 0x95, 0xbb, 0x97, 0x21, 0x4c, 0xb0, 0xf6, 0x1a } }

namespace mozilla {
namespace dom {
namespace quota {
class Client;
}
}
}

class nsIOfflineStorage : public nsIFileStorage
{
public:
  typedef mozilla::dom::quota::Client Client;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_OFFLINESTORAGE_IID)

  NS_IMETHOD_(Client*)
  GetClient() = 0;

  NS_IMETHOD_(bool)
  IsOwned(nsPIDOMWindow* aOwner) = 0;

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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIOfflineStorage, NS_OFFLINESTORAGE_IID)

#define NS_DECL_NSIOFFLINESTORAGE                                              \
  NS_IMETHOD_(Client*)                                                         \
  GetClient() MOZ_OVERRIDE;                                                    \
                                                                               \
  NS_IMETHOD_(bool)                                                            \
  IsOwned(nsPIDOMWindow* aOwner) MOZ_OVERRIDE;                                 \
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

#define NS_DECL_NSIOFFLINESTORAGE_NOCLOSE                                      \
  NS_IMETHOD_(Client*)                                                         \
  GetClient() MOZ_OVERRIDE;                                                    \
                                                                               \
  NS_IMETHOD_(bool)                                                            \
  IsOwned(nsPIDOMWindow* aOwner) MOZ_OVERRIDE;                                 \
                                                                               \
  NS_IMETHOD_(const nsACString&)                                               \
  Origin() MOZ_OVERRIDE;                                                       \
                                                                               \
  NS_IMETHOD_(bool)                                                            \
  IsClosed() MOZ_OVERRIDE;                                                     \
                                                                               \
  NS_IMETHOD_(void)                                                            \
  Invalidate() MOZ_OVERRIDE;

#endif // nsIOfflineStorage_h__
