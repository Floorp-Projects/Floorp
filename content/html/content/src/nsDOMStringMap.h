/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStringMap_h
#define nsDOMStringMap_h

#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
class ErrorResult;
}

class nsDOMStringMap : public nsISupports,
                       public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMStringMap)

  nsINode* GetParentObject()
  {
    return mElement;
  }

  nsDOMStringMap(nsGenericHTMLElement* aElement);

  // WebIDL API
  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> scope) MOZ_OVERRIDE;
  void NamedGetter(const nsAString& aProp, bool& found, nsString& aResult) const;
  void NamedSetter(const nsAString& aProp, const nsAString& aValue,
                   mozilla::ErrorResult& rv);
  void NamedDeleter(const nsAString& aProp, bool &found);
  void GetSupportedNames(nsTArray<nsString>& aNames);

private:
  virtual ~nsDOMStringMap();

protected:
  nsRefPtr<nsGenericHTMLElement> mElement;
  // Flag to guard against infinite recursion.
  bool mRemovingProp;
  static bool DataPropToAttr(const nsAString& aProp, nsAString& aResult);
  static bool AttrToDataProp(const nsAString& aAttr, nsAString& aResult);
};

#endif
