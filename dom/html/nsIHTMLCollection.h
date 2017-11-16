/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIHTMLCollection_h___
#define nsIHTMLCollection_h___

#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"
#include "nsWrapperCache.h"
#include "js/GCAPI.h"
#include "js/TypeDecls.h"

class nsINode;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

// IID for the nsIHTMLCollection interface
#define NS_IHTMLCOLLECTION_IID \
{ 0x4e169191, 0x5196, 0x4e17, \
  { 0xa4, 0x79, 0xd5, 0x35, 0x0b, 0x5b, 0x0a, 0xcd } }

/**
 * An internal interface
 */
class nsIHTMLCollection : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHTMLCOLLECTION_IID)

  /**
   * Get the root node for this HTML collection.
   */
  virtual nsINode* GetParentObject() = 0;

  virtual uint32_t Length() = 0;
  virtual mozilla::dom::Element* GetElementAt(uint32_t index) = 0;
  mozilla::dom::Element* Item(uint32_t index)
  {
    return GetElementAt(index);
  }
  mozilla::dom::Element* IndexedGetter(uint32_t index, bool& aFound)
  {
    mozilla::dom::Element* item = Item(index);
    aFound = !!item;
    return item;
  }
  mozilla::dom::Element* NamedItem(const nsAString& aName)
  {
    bool dummy;
    return NamedGetter(aName, dummy);
  }
  mozilla::dom::Element* NamedGetter(const nsAString& aName, bool& aFound)
  {
    return GetFirstNamedElement(aName, aFound);
  }
  virtual mozilla::dom::Element*
  GetFirstNamedElement(const nsAString& aName, bool& aFound) = 0;

  virtual void GetSupportedNames(nsTArray<nsString>& aNames) = 0;

  JSObject* GetWrapperPreserveColor()
  {
    return GetWrapperPreserveColorInternal();
  }
  JSObject* GetWrapper()
  {
    JSObject* obj = GetWrapperPreserveColor();
    if (obj) {
      JS::ExposeObjectToActiveJS(obj);
    }
    return obj;
  }
  void PreserveWrapper(nsISupports* aScriptObjectHolder)
  {
    PreserveWrapperInternal(aScriptObjectHolder);
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) = 0;
protected:
  // Hook for calling nsWrapperCache::GetWrapperPreserveColor.
  virtual JSObject* GetWrapperPreserveColorInternal() = 0;
  // Hook for calling nsWrapperCache::PreserveWrapper.
  virtual void PreserveWrapperInternal(nsISupports* aScriptObjectHolder) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLCollection, NS_IHTMLCOLLECTION_IID)

#endif /* nsIHTMLCollection_h___ */
