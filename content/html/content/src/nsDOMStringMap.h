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
#include "jsfriendapi.h" // For js::ExpandoAndGeneration

namespace mozilla {
class ErrorResult;
}

class nsDOMStringMap : public nsStubMutationObserver,
                       public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMStringMap)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  nsINode* GetParentObject()
  {
    return mElement;
  }

  explicit nsDOMStringMap(nsGenericHTMLElement* aElement);

  // WebIDL API
  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;
  void NamedGetter(const nsAString& aProp, bool& found,
                   mozilla::dom::DOMString& aResult) const;
  void NamedSetter(const nsAString& aProp, const nsAString& aValue,
                   mozilla::ErrorResult& rv);
  void NamedDeleter(const nsAString& aProp, bool &found);
  bool NameIsEnumerable(const nsAString& aName);
  void GetSupportedNames(unsigned, nsTArray<nsString>& aNames);

  js::ExpandoAndGeneration mExpandoAndGeneration;

private:
  virtual ~nsDOMStringMap();

protected:
  nsRefPtr<nsGenericHTMLElement> mElement;
  // Flag to guard against infinite recursion.
  bool mRemovingProp;
  static bool DataPropToAttr(const nsAString& aProp, nsAutoString& aResult);
  static bool AttrToDataProp(const nsAString& aAttr, nsAutoString& aResult);
};

#endif
