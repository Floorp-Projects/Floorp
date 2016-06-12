/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMSerializer_h_
#define nsDOMSerializer_h_

#include "nsIDOMSerializer.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/XMLSerializerBinding.h"

class nsINode;

class nsDOMSerializer final : public nsIDOMSerializer,
                              public nsWrapperCache
{
public:
  nsDOMSerializer();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMSerializer)

  // nsIDOMSerializer
  NS_DECL_NSIDOMSERIALIZER

  // WebIDL API
  static already_AddRefed<nsDOMSerializer>
  Constructor(const mozilla::dom::GlobalObject& aOwner,
              mozilla::ErrorResult& rv)
  {
    RefPtr<nsDOMSerializer> domSerializer = new nsDOMSerializer(aOwner.GetAsSupports());
    return domSerializer.forget();
  }

  void
  SerializeToString(nsINode& aRoot, nsAString& aStr,
                    mozilla::ErrorResult& rv);

  void
  SerializeToStream(nsINode& aRoot, nsIOutputStream* aStream,
                    const nsAString& aCharset, mozilla::ErrorResult& rv);

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::XMLSerializerBinding::Wrap(aCx, this, aGivenProto);
  }

private:
  virtual ~nsDOMSerializer();

  explicit nsDOMSerializer(nsISupports* aOwner) : mOwner(aOwner)
  {
    MOZ_ASSERT(aOwner);
  }

  nsCOMPtr<nsISupports> mOwner;
};


#endif

