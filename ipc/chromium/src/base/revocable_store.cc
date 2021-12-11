/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/revocable_store.h"

#include "base/logging.h"

RevocableStore::Revocable::Revocable(RevocableStore* store)
    : store_reference_(store->owning_reference_) {
  // We AddRef() the owning reference.
  DCHECK(store_reference_->store());
  store_reference_->store()->Add(this);
}

RevocableStore::Revocable::~Revocable() {
  if (!revoked()) {
    // Notify the store of our destruction.
    --(store_reference_->store()->count_);
  }
}

RevocableStore::RevocableStore() : count_(0) {
  // Create a new owning reference.
  owning_reference_ = new StoreRef(this);
}

RevocableStore::~RevocableStore() {
  // Revoke all the items in the store.
  owning_reference_->set_store(NULL);
}

void RevocableStore::Add(Revocable* item) {
  DCHECK(!item->revoked());
  ++count_;
}

void RevocableStore::RevokeAll() {
  // We revoke all the existing items in the store and reset our count.
  owning_reference_->set_store(NULL);
  count_ = 0;

  // Then we create a new owning reference for new items that get added.
  // This Release()s the old owning reference, allowing it to be freed after
  // all the items that were in the store are eventually destroyed.
  owning_reference_ = new StoreRef(this);
}
