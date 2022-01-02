/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_REVOCABLE_STORE_H_
#define BASE_REVOCABLE_STORE_H_

#include "base/basictypes.h"
#include "nsISupportsImpl.h"

// |RevocableStore| is a container of items that can be removed from the store.
class RevocableStore {
 public:
  // A |StoreRef| is used to link the |RevocableStore| to its items.  There is
  // one StoreRef per store, and each item holds a reference to it.  If the
  // store wishes to revoke its items, it sets |store_| to null.  Items are
  // permitted to release their reference to the |StoreRef| when they no longer
  // require the store.
  class StoreRef final {
   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(StoreRef)
    explicit StoreRef(RevocableStore* aStore) : store_(aStore) {}

    void set_store(RevocableStore* aStore) { store_ = aStore; }
    RevocableStore* store() const { return store_; }

   protected:
    ~StoreRef() {}

   private:
    RevocableStore* store_;

    DISALLOW_EVIL_CONSTRUCTORS(StoreRef);
  };

  // An item in the store.  On construction, the object adds itself to the
  // store.
  class Revocable {
   public:
    explicit Revocable(RevocableStore* store);
    ~Revocable();

    // This item has been revoked if it no longer has a pointer to the store.
    bool revoked() const { return !store_reference_->store(); }

   private:
    // We hold a reference to the store through this ref pointer.  We release
    // this reference on destruction.
    RefPtr<StoreRef> store_reference_;

    DISALLOW_EVIL_CONSTRUCTORS(Revocable);
  };

  RevocableStore();
  ~RevocableStore();

  // Revokes all the items in the store.
  void RevokeAll();

  // Returns true if there are no items in the store.
  bool empty() const { return count_ == 0; }

 private:
  friend class Revocable;

  // Adds an item to the store.  To add an item to the store, construct it
  // with a pointer to the store.
  void Add(Revocable* item);

  // This is the reference the unrevoked items in the store hold.
  RefPtr<StoreRef> owning_reference_;

  // The number of unrevoked items in the store.
  int count_;

  DISALLOW_EVIL_CONSTRUCTORS(RevocableStore);
};

#endif  // BASE_REVOCABLE_STORE_H_
