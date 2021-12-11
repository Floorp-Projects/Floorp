/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStringMap_h
#define nsDOMStringMap_h

#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "js/friend/DOMProxy.h"  // JS::ExpandoAndGeneration
#include "js/RootingAPI.h"       // JS::Handle

// XXX Avoid including this here by moving function bodies to the cpp file
#include "mozilla/dom/Element.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class DOMString;
class DocGroup;
}  // namespace dom
}  // namespace mozilla

class nsDOMStringMap : public nsStubMutationObserver, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMStringMap)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  nsINode* GetParentObject() { return mElement; }

  mozilla::dom::DocGroup* GetDocGroup() const;

  explicit nsDOMStringMap(mozilla::dom::Element* aElement);

  // WebIDL API
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;
  void NamedGetter(const nsAString& aProp, bool& found,
                   mozilla::dom::DOMString& aResult) const;
  void NamedSetter(const nsAString& aProp, const nsAString& aValue,
                   mozilla::ErrorResult& rv);
  void NamedDeleter(const nsAString& aProp, bool& found);
  void GetSupportedNames(nsTArray<nsString>& aNames);

  JS::ExpandoAndGeneration mExpandoAndGeneration;

 private:
  virtual ~nsDOMStringMap();

 protected:
  RefPtr<mozilla::dom::Element> mElement;
  // Flag to guard against infinite recursion.
  bool mRemovingProp;
  static bool DataPropToAttr(const nsAString& aProp, nsAutoString& aResult);
  static bool AttrToDataProp(const nsAString& aAttr, nsAutoString& aResult);
};

#endif
