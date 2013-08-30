/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMSerializer_h_
#define nsDOMSerializer_h_

#include "nsIDOMSerializer.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/XMLSerializerBinding.h"
#include "nsAutoPtr.h"

class nsINode;

class nsDOMSerializer MOZ_FINAL : public nsIDOMSerializer,
                                  public nsWrapperCache
{
public:
  nsDOMSerializer();
  virtual ~nsDOMSerializer();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMSerializer)

  // nsIDOMSerializer
  NS_DECL_NSIDOMSERIALIZER

  // WebIDL API
  static already_AddRefed<nsDOMSerializer>
  Constructor(const mozilla::dom::GlobalObject& aOwner,
              mozilla::ErrorResult& rv)
  {
    nsRefPtr<nsDOMSerializer> domSerializer = new nsDOMSerializer(aOwner.GetAsSupports());
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

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::XMLSerializerBinding::Wrap(aCx, aScope, this);
  }

private:
  nsDOMSerializer(nsISupports* aOwner) : mOwner(aOwner)
  {
    MOZ_ASSERT(aOwner);
    SetIsDOMBinding();
  }

  nsCOMPtr<nsISupports> mOwner;
};


#endif

