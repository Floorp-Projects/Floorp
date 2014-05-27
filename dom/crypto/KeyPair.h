/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyPair_h
#define mozilla_dom_KeyPair_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"
#include "mozilla/dom/Key.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

class KeyPair MOZ_FINAL : public nsISupports,
                          public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(KeyPair)

public:
  KeyPair(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal)
    , mPublicKey(new Key(aGlobal))
    , mPrivateKey(new Key(aGlobal))
  {
    SetIsDOMBinding();
  }

  ~KeyPair() {}

  nsIGlobalObject* GetParentObject() const
  {
    return mGlobal;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  Key* PublicKey() const
  {
    return mPublicKey;
  }

  Key* PrivateKey() const
  {
    return mPrivateKey;
  }

private:
  nsRefPtr<nsIGlobalObject> mGlobal;
  nsRefPtr<Key> mPublicKey;
  nsRefPtr<Key> mPrivateKey;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyPair_h
