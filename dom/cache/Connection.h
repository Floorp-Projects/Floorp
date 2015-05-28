/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_Connection_h
#define mozilla_dom_cache_Connection_h

#include "mozIStorageConnection.h"

namespace mozilla {
namespace dom {
namespace cache {

class Connection final : public mozIStorageConnection
{
public:
  explicit Connection(mozIStorageConnection* aBase);

private:
  ~Connection();

  nsCOMPtr<mozIStorageConnection> mBase;
  bool mClosed;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEASYNCCONNECTION
  NS_DECL_MOZISTORAGECONNECTION
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_Connection_h
