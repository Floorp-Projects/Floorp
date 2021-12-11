/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_KEY_VALUE_STORAGE_H
#define MOZILLA_KEY_VALUE_STORAGE_H

#include "mozilla/MozPromise.h"
#include "nsIKeyValue.h"

namespace mozilla {

/* A wrapper class around kv store service, which allows storing a pair of key
 * value permanently. The class must be used from the parent process, where
 * there is no sandbox because it requires access to the directory that the
 * database is located. */
class KeyValueStorage final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(KeyValueStorage)

  /* Store permanently the value in the Key. */
  RefPtr<GenericPromise> Put(const nsACString& aName, const nsACString& aKey,
                             int32_t aValue);
  /* Get the value stored in the aKey. If the aKey does not exist the promise is
   * resolved with the value -1. */
  typedef MozPromise<int32_t, nsresult, true> GetPromise;
  RefPtr<GetPromise> Get(const nsACString& aName, const nsACString& aKey);

  /* Clear all the key/value pairs from the aName database. */
  RefPtr<GenericPromise> Clear(const nsACString& aName);

 private:
  /* Create, if doesn't exist, and initialize the database with a given name. */
  RefPtr<GenericPromise> Init();
  RefPtr<GenericPromise> Put(const nsACString& aKey, int32_t aValue);
  RefPtr<GetPromise> Get(const nsACString& aKey);
  RefPtr<GenericPromise> Clear();
  ~KeyValueStorage() = default;

  RefPtr<nsIKeyValueDatabase> mDatabase;
  nsCString mDatabaseName;
};

}  // namespace mozilla

#endif  // MOZILLA_KEY_VALUE_STORAGE_H
