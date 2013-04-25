/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIHTMLCollection_h___
#define nsIHTMLCollection_h___

#include "nsIDOMHTMLCollection.h"
#include "nsWrapperCache.h"

struct JSContext;
class JSObject;
class nsINode;
class nsString;
template<class> class nsTArray;

namespace mozilla {
class ErrorResult;

namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

// IID for the nsIHTMLCollection interface
#define NS_IHTMLCOLLECTION_IID \
{ 0x5643235d, 0x9a72, 0x4b6a, \
 { 0xa6, 0x0c, 0x64, 0x63, 0x72, 0xb7, 0x53, 0x4a } }

/**
 * An internal interface
 */
class nsIHTMLCollection : public nsIDOMHTMLCollection
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHTMLCOLLECTION_IID)

  /**
   * Get the root node for this HTML collection.
   */
  virtual nsINode* GetParentObject() = 0;

  using nsIDOMHTMLCollection::Item;
  using nsIDOMHTMLCollection::NamedItem;

  uint32_t Length()
  {
    uint32_t length;
    GetLength(&length);
    return length;
  }
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
  virtual JSObject* NamedItem(JSContext* cx, const nsAString& name,
                              mozilla::ErrorResult& error) = 0;
  JSObject* NamedGetter(JSContext* cx, const nsAString& name,
                        bool& found, mozilla::ErrorResult& error)
  {
    JSObject* namedItem = NamedItem(cx, name, error);
    found = !!namedItem;
    return namedItem;
  }

  virtual void GetSupportedNames(nsTArray<nsString>& aNames) = 0;

  JSObject* GetWrapperPreserveColor()
  {
    nsWrapperCache* cache;
    CallQueryInterface(this, &cache);
    return cache->GetWrapperPreserveColor();
  }
  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> scope) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLCollection, NS_IHTMLCOLLECTION_IID)

#endif /* nsIHTMLCollection_h___ */
