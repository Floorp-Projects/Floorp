/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ModuleMapKey_h__
#define ModuleMapKey_h__

#include "nsURIHashKey.h"
#include "nsIGlobalObject.h"

namespace mozilla::dom {

class ModuleMapKey : public nsURIHashKey {
 public:
  using KeyType = ModuleMapKey&;
  using KeyTypePointer = const ModuleMapKey*;

  ModuleMapKey() = default;
  ModuleMapKey(const nsIURI* aURI, nsIGlobalObject* aWebExtGlobal);
  explicit ModuleMapKey(const ModuleMapKey* aKey);
  ModuleMapKey(ModuleMapKey&& aToMove) noexcept;

  ModuleMapKey& operator=(const ModuleMapKey& aOther) = default;

  KeyType GetKey() { return *this; }
  KeyTypePointer GetKeyPointer() const { return this; }
  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  bool KeyEquals(KeyTypePointer aOther) const;
  static PLDHashNumber HashKey(KeyTypePointer aKey);

  nsCOMPtr<nsIGlobalObject> mWebExtGlobal;
};

inline void ImplCycleCollectionUnlink(ModuleMapKey& aField) {
  ImplCycleCollectionUnlink(aField.mWebExtGlobal);
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, ModuleMapKey& aField,
    const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mWebExtGlobal, aName, aFlags);
}

}  // namespace mozilla::dom

#endif
