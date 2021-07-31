/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_LOCALSTORAGE_INITIALIZATIONTYPES_H_
#define DOM_LOCALSTORAGE_INITIALIZATIONTYPES_H_

#include "mozilla/TypedEnumBits.h"
#include <mozilla/dom/quota/FirstInitializationAttempts.h>
#include "nsTHashMap.h"

namespace mozilla {
struct CreateIfNonExistent;
}

namespace mozilla::dom {

enum class LSOriginInitialization {
  None = 0,
  Datastore = 1 << 0,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(LSOriginInitialization)

using LSOriginInitializationInfo =
    quota::FirstInitializationAttempts<LSOriginInitialization, Nothing>;

class LSInitializationInfo final {
  nsTHashMap<nsCStringHashKey, LSOriginInitializationInfo>
      mOriginInitializationInfos;

 public:
  LSOriginInitializationInfo& MutableOriginInitializationInfoRef(
      const nsACString& aOrigin) {
    return *mOriginInitializationInfos.Lookup(aOrigin);
  }

  LSOriginInitializationInfo& MutableOriginInitializationInfoRef(
      const nsACString& aOrigin, const CreateIfNonExistent&) {
    return mOriginInitializationInfos.LookupOrInsert(aOrigin);
  }
};

}  // namespace mozilla::dom

#endif  // DOM_LOCALSTORAGE_INITIALIZATIONTYPES_H_
