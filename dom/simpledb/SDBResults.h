/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_simpledb_SDBResults_h
#define mozilla_dom_simpledb_SDBResults_h

#include "nsISDBResults.h"
#include "nsISupports.h"
#include "nsString.h"

namespace mozilla::dom {

class SDBResult : public nsISDBResult {
  nsCString mData;

 public:
  explicit SDBResult(const nsACString& aData);

 private:
  virtual ~SDBResult() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSISDBRESULT
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_simpledb_SDBResults_h
