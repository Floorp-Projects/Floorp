/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsDOMStringMap.h"

#include "nsGenericHTMLElement.h"
#include "nsContentUtils.h"
#include "mozilla/dom/DOMStringMapBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMStringMap)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMStringMap)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  // Check that mElement exists in case the unlink code is run more than once.
  if (tmp->mElement) {
    // Call back to element to null out weak reference to this object.
    tmp->mElement->ClearDataset();
    tmp->mElement = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMStringMap)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStringMap)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMStringMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMStringMap)

nsDOMStringMap::nsDOMStringMap(nsGenericHTMLElement* aElement)
  : mElement(aElement),
    mRemovingProp(false)
{
  SetIsDOMBinding();
}

nsDOMStringMap::~nsDOMStringMap()
{
  // Check if element still exists, may have been unlinked by cycle collector.
  if (mElement) {
    // Call back to element to null out weak reference to this object.
    mElement->ClearDataset();
  }
}

/* virtual */
JSObject*
nsDOMStringMap::WrapObject(JSContext *cx, JSObject *scope,
                           bool *triedToWrap)
{
  return DOMStringMapBinding::Wrap(cx, scope, this, triedToWrap);
}

void
nsDOMStringMap::NamedGetter(const nsAString& aProp, bool& found,
                            nsString& aResult) const
{
  nsAutoString attr;

  if (!DataPropToAttr(aProp, attr)) {
    found = false;
    return;
  }

  nsCOMPtr<nsIAtom> attrAtom = do_GetAtom(attr);
  MOZ_ASSERT(attrAtom, "Should be infallible");

  found = mElement->GetAttr(kNameSpaceID_None, attrAtom, aResult);
}

void
nsDOMStringMap::NamedSetter(const nsAString& aProp,
                            const nsAString& aValue,
                            ErrorResult& rv)
{
  nsAutoString attr;
  if (!DataPropToAttr(aProp, attr)) {
    rv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nsresult res = nsContentUtils::CheckQName(attr, false);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return;
  }

  nsCOMPtr<nsIAtom> attrAtom = do_GetAtom(attr);
  MOZ_ASSERT(attrAtom, "Should be infallible");

  res = mElement->SetAttr(kNameSpaceID_None, attrAtom, aValue, true);
  if (NS_FAILED(res)) {
    rv.Throw(res);
  }
}

void
nsDOMStringMap::NamedDeleter(const nsAString& aProp, bool& found)
{
  // Currently removing property, attribute is already removed.
  if (mRemovingProp) {
    return;
  }
  
  nsAutoString attr;
  if (!DataPropToAttr(aProp, attr)) {
    return;
  }

  nsCOMPtr<nsIAtom> attrAtom = do_GetAtom(attr);
  MOZ_ASSERT(attrAtom, "Should be infallible");

  found = mElement->HasAttr(kNameSpaceID_None, attrAtom);

  if (found) {
    mRemovingProp = true;
    mElement->UnsetAttr(kNameSpaceID_None, attrAtom, true);
    mRemovingProp = false;
  }
}

void
nsDOMStringMap::GetSupportedNames(nsTArray<nsString>& aNames)
{
  uint32_t attrCount = mElement->GetAttrCount();

  // Iterate through all the attributes and add property
  // names corresponding to data attributes to return array.
  for (uint32_t i = 0; i < attrCount; ++i) {
    nsAutoString attrString;
    const nsAttrName* attrName = mElement->GetAttrNameAt(i);
    // Skip the ones that are not in the null namespace
    if (attrName->NamespaceID() != kNameSpaceID_None) {
      continue;
    }
    attrName->LocalName()->ToString(attrString);

    nsAutoString prop;
    if (!AttrToDataProp(attrString, prop)) {
      continue;
    }

    aNames.AppendElement(prop);
  }
}

/**
 * Converts a dataset property name to the corresponding data attribute name.
 * (ex. aBigFish to data-a-big-fish).
 */
bool nsDOMStringMap::DataPropToAttr(const nsAString& aProp,
                                      nsAString& aResult)
{
  const PRUnichar* cur = aProp.BeginReading();
  const PRUnichar* end = aProp.EndReading();

  // String corresponding to the data attribute on the element.
  nsAutoString attr;
  // Length of attr will be at least the length of the property + 5 for "data-".
  attr.SetCapacity(aProp.Length() + 5);

  attr.Append(NS_LITERAL_STRING("data-"));

  // Iterate property by character to form attribute name.
  // Return syntax error if there is a sequence of "-" followed by a character
  // in the range "a" to "z".
  // Replace capital characters with "-" followed by lower case character.
  // Otherwise, simply append character to attribute name.
  for (; cur < end; ++cur) {
    const PRUnichar* next = cur + 1;
    if (PRUnichar('-') == *cur && next < end &&
        PRUnichar('a') <= *next && *next <= PRUnichar('z')) {
      // Syntax error if character following "-" is in range "a" to "z".
      return false;
    }

    if (PRUnichar('A') <= *cur && *cur <= PRUnichar('Z')) {
      // Uncamel-case characters in the range of "A" to "Z".
      attr.Append(PRUnichar('-'));
      attr.Append(*cur - 'A' + 'a');
    } else {
      attr.Append(*cur);
    }
  }

  aResult.Assign(attr);
  return true;
}

/**
 * Converts a data attribute name to the corresponding dataset property name.
 * (ex. data-a-big-fish to aBigFish).
 */
bool nsDOMStringMap::AttrToDataProp(const nsAString& aAttr,
                                      nsAString& aResult)
{
  // If the attribute name does not begin with "data-" then it can not be
  // a data attribute.
  if (!StringBeginsWith(aAttr, NS_LITERAL_STRING("data-"))) {
    return false;
  }

  // Start reading attribute from first character after "data-".
  const PRUnichar* cur = aAttr.BeginReading() + 5;
  const PRUnichar* end = aAttr.EndReading();

  // Dataset property name. Ensure that the string is large enough to store
  // all the characters in the property name.
  nsAutoString prop;

  // Iterate through attrName by character to form property name.
  // If there is a sequence of "-" followed by a character in the range "a" to
  // "z" then replace with upper case letter.
  // Otherwise append character to property name.
  for (; cur < end; ++cur) {
    const PRUnichar* next = cur + 1;
    if (PRUnichar('-') == *cur && next < end && 
        PRUnichar('a') <= *next && *next <= PRUnichar('z')) {
      // Upper case the lower case letters that follow a "-".
      prop.Append(*next - 'a' + 'A');
      // Consume character to account for "-" character.
      ++cur;
    } else {
      // Simply append character if camel case is not necessary.
      prop.Append(*cur);
    }
  }

  aResult.Assign(prop);
  return true;
}
