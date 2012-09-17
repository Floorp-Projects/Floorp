/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIHTMLCollection_h___
#define nsIHTMLCollection_h___

#include "nsIDOMHTMLCollection.h"
#include "nsWrapperCache.h"

struct JSContext;
struct JSObject;
class nsINode;
namespace mozilla {
class ErrorResult;
}

// IID for the nsIHTMLCollection interface
#define NS_IHTMLCOLLECTION_IID \
{ 0x5c6012c3, 0xa816, 0x4f28, \
 { 0xab, 0x93, 0xe9, 0x8a, 0x36, 0x16, 0x88, 0xf2 } }

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
  nsGenericElement* Item(uint32_t index)
  {
    return GetElementAt(index);
  }
  nsGenericElement* IndexedGetter(uint32_t index, bool& aFound)
  {
    nsGenericElement* item = Item(index);
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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLCollection, NS_IHTMLCOLLECTION_IID)

#endif /* nsIHTMLCollection_h___ */
