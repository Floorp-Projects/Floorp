/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ModuleMapKey.h"

using namespace mozilla::dom;

ModuleMapKey::ModuleMapKey(const nsIURI* aURI, nsIGlobalObject* aWebExtGlobal)
    : nsURIHashKey(aURI), mWebExtGlobal(aWebExtGlobal) {}

ModuleMapKey::ModuleMapKey(const ModuleMapKey* aKey)
    : nsURIHashKey(aKey->mKey) {
  *this = *aKey;
}

ModuleMapKey::ModuleMapKey(ModuleMapKey&& aToMove) noexcept
    : nsURIHashKey(std::move(aToMove)) {
  mWebExtGlobal = std::move(aToMove.mWebExtGlobal);
}

bool ModuleMapKey::KeyEquals(KeyTypePointer aOther) const {
  if (mWebExtGlobal != aOther->mWebExtGlobal) {
    return false;
  }

  return nsURIHashKey::KeyEquals(
      static_cast<const nsURIHashKey*>(aOther)->GetKey());
}

// static
PLDHashNumber ModuleMapKey::HashKey(KeyTypePointer aKey) {
  PLDHashNumber hash = nsURIHashKey::HashKey(aKey->mKey);
  // XXX This seems wrong.
  hash =
      AddToHash(hash, reinterpret_cast<uintptr_t>(aKey->mWebExtGlobal.get()));
  return hash;
}
