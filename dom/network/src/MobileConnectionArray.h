/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_MobileConnectionArray_h__
#define mozilla_dom_network_MobileConnectionArray_h__

#include "nsWrapperCache.h"
#include "mozilla/dom/network/MobileConnection.h"

class nsIDOMMozMobileConnection;

namespace mozilla {
namespace dom {
namespace network {

class MobileConnectionArray MOZ_FINAL : public nsISupports,
                                        public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MobileConnectionArray)

  MobileConnectionArray(nsPIDOMWindow* aWindow);

  nsPIDOMWindow*
  GetParentObject() const;

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  //  WebIDL
  nsIDOMMozMobileConnection*
  Item(uint32_t aIndex) const;

  uint32_t
  Length() const;

  nsIDOMMozMobileConnection*
  IndexedGetter(uint32_t aIndex, bool& aFound) const;

private:
  ~MobileConnectionArray();

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsTArray<nsRefPtr<MobileConnection>> mMobileConnections;
};

} // namespace network
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_network_MobileConnectionArray_h__