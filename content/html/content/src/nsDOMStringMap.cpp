/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMStringMap.h"

#include "nsDOMClassInfoID.h"
#include "nsGenericHTMLElement.h"
#include "nsContentUtils.h"

DOMCI_DATA(DOMStringMap, nsDOMStringMap)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMStringMap)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMStringMap)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMStringMap)
  // Check that mElement exists in case the unlink code is run more than once.
  if (tmp->mElement) {
    // Call back to element to null out weak reference to this object.
    tmp->mElement->ClearDataset();
    tmp->mElement = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStringMap)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMStringMap)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMStringMap)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMStringMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMStringMap)

nsDOMStringMap::nsDOMStringMap(nsGenericHTMLElement* aElement)
  : mElement(aElement),
    mRemovingProp(false)
{
}

nsDOMStringMap::~nsDOMStringMap()
{
  // Check if element still exists, may have been unlinked by cycle collector.
  if (mElement) {
    // Call back to element to null out weak reference to this object.
    mElement->ClearDataset();
  }
}

class nsDOMStringMapRemoveProp : public nsRunnable {
public:
  nsDOMStringMapRemoveProp(nsDOMStringMap* aDataset, nsIAtom* aProperty)
  : mDataset(aDataset),
    mProperty(aProperty)
  {
  }

  NS_IMETHOD Run()
  {
    return mDataset->RemovePropInternal(mProperty);
  }

  virtual ~nsDOMStringMapRemoveProp()
  {
  }

protected:
  nsRefPtr<nsDOMStringMap> mDataset;
  nsCOMPtr<nsIAtom> mProperty;
};

/* [notxpcom] boolean hasDataAttr (in DOMString prop); */
NS_IMETHODIMP_(bool) nsDOMStringMap::HasDataAttr(const nsAString& aProp)
{
  nsAutoString attr;
  if (!DataPropToAttr(aProp, attr)) {
    return false;
  }

  nsCOMPtr<nsIAtom> attrAtom = do_GetAtom(attr);
  if (!attrAtom) {
    return false;
  }

  return mElement->HasAttr(kNameSpaceID_None, attrAtom);
}

/* [noscript] DOMString getDataAttr (in DOMString prop); */
NS_IMETHODIMP nsDOMStringMap::GetDataAttr(const nsAString& aProp,
                                          nsAString& aResult NS_OUTPARAM)
{
  nsAutoString attr;

  if (!DataPropToAttr(aProp, attr)) {
    aResult.SetIsVoid(true);
    return NS_OK;
  }

  nsCOMPtr<nsIAtom> attrAtom = do_GetAtom(attr);
  NS_ENSURE_TRUE(attrAtom, NS_ERROR_OUT_OF_MEMORY);

  if (!mElement->GetAttr(kNameSpaceID_None, attrAtom, aResult)) {
    aResult.SetIsVoid(true);
    return NS_OK;
  }

  return NS_OK;
}

/* [noscript] void setDataAttr (in DOMString prop, in DOMString value); */
NS_IMETHODIMP nsDOMStringMap::SetDataAttr(const nsAString& aProp,
                                          const nsAString& aValue)
{
  nsAutoString attr;
  NS_ENSURE_TRUE(DataPropToAttr(aProp, attr), NS_ERROR_DOM_SYNTAX_ERR);

  nsresult rv = nsContentUtils::CheckQName(attr, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAtom> attrAtom = do_GetAtom(attr);
  NS_ENSURE_TRUE(attrAtom, NS_ERROR_OUT_OF_MEMORY);

  return mElement->SetAttr(kNameSpaceID_None, attrAtom, aValue, true);
}

/* [notxpcom] void removeDataAttr (in DOMString prop); */
NS_IMETHODIMP_(void) nsDOMStringMap::RemoveDataAttr(const nsAString& aProp)
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
  if (!attrAtom) {
    return;
  }

  mElement->UnsetAttr(kNameSpaceID_None, attrAtom, true);
}

nsGenericHTMLElement* nsDOMStringMap::GetElement()
{
  return mElement;
}

/* [notxpcom] void removeProp (in nsIAtom attr); */
NS_IMETHODIMP_(void) nsDOMStringMap::RemoveProp(nsIAtom* aAttr)
{
  nsContentUtils::AddScriptRunner(new nsDOMStringMapRemoveProp(this, aAttr));
}

nsresult nsDOMStringMap::RemovePropInternal(nsIAtom* aAttr)
{
  nsAutoString attr;
  aAttr->ToString(attr);
  nsAutoString prop;
  NS_ENSURE_TRUE(AttrToDataProp(attr, prop), NS_OK);

  jsval val;
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  nsresult rv = nsContentUtils::WrapNative(cx, JS_GetGlobalForScopeChain(cx),
                                           this, &val);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, JSVAL_TO_OBJECT(val))) {
    return NS_ERROR_FAILURE;
  }

  // Guard against infinite recursion. Prevents the stack from looking like
  // ...
  // RemoveProp
  // ...
  // RemoveDataAttr
  // ...
  // RemoveProp
  mRemovingProp = true;
  jsval dummy;
  JS_DeleteUCProperty2(cx, JSVAL_TO_OBJECT(val), prop.get(), prop.Length(),
                       &dummy);
  mRemovingProp = false;

  return NS_OK;
}

/**
 * Returns a list of dataset properties corresponding to the data
 * attributes on the element.
 */
nsresult nsDOMStringMap::GetDataPropList(nsTArray<nsString>& aResult)
{
  PRUint32 attrCount = mElement->GetAttrCount();

  // Iterate through all the attributes and add property
  // names corresponding to data attributes to return array.
  for (PRUint32 i = 0; i < attrCount; ++i) {
    nsAutoString attrString;
    const nsAttrName* attrName = mElement->GetAttrNameAt(i);
    attrName->LocalName()->ToString(attrString);

    nsAutoString prop;
    if (!AttrToDataProp(attrString, prop)) {
      continue;
    }

    aResult.AppendElement(prop);
  }

  return NS_OK;
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
