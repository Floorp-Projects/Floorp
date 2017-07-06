/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOptionsCollection.h"

#include "HTMLOptGroupElement.h"
#include "mozAutoDocUpdate.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLOptionsCollectionBinding.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "nsContentCreatorFunctions.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIFormControlFrame.h"
#include "nsIForm.h"
#include "nsIFormProcessor.h"
#include "nsIListControlFrame.h"
#include "nsLayoutUtils.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleConsts.h"
#include "jsfriendapi.h"

namespace mozilla {
namespace dom {

HTMLOptionsCollection::HTMLOptionsCollection(HTMLSelectElement* aSelect)
{
  // Do not maintain a reference counted reference. When
  // the select goes away, it will let us know.
  mSelect = aSelect;
}

HTMLOptionsCollection::~HTMLOptionsCollection()
{
  DropReference();
}

void
HTMLOptionsCollection::DropReference()
{
  // Drop our (non ref-counted) reference
  mSelect = nullptr;
}

nsresult
HTMLOptionsCollection::GetOptionIndex(Element* aOption,
                                      int32_t aStartIndex,
                                      bool aForward,
                                      int32_t* aIndex)
{
  // NOTE: aIndex shouldn't be set if the returned value isn't NS_OK.

  int32_t index;

  // Make the common case fast
  if (aStartIndex == 0 && aForward) {
    index = mElements.IndexOf(aOption);
    if (index == -1) {
      return NS_ERROR_FAILURE;
    }

    *aIndex = index;
    return NS_OK;
  }

  int32_t high = mElements.Length();
  int32_t step = aForward ? 1 : -1;

  for (index = aStartIndex; index < high && index > -1; index += step) {
    if (mElements[index] == aOption) {
      *aIndex = index;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(HTMLOptionsCollection, mElements)

// nsISupports

// QueryInterface implementation for HTMLOptionsCollection
NS_INTERFACE_TABLE_HEAD(HTMLOptionsCollection)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(HTMLOptionsCollection,
                     nsIHTMLCollection,
                     nsIDOMHTMLOptionsCollection,
                     nsIDOMHTMLCollection)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(HTMLOptionsCollection)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(HTMLOptionsCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HTMLOptionsCollection)


JSObject*
HTMLOptionsCollection::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLOptionsCollectionBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
HTMLOptionsCollection::GetLength(uint32_t* aLength)
{
  *aLength = mElements.Length();

  return NS_OK;
}

NS_IMETHODIMP
HTMLOptionsCollection::SetLength(uint32_t aLength)
{
  if (!mSelect) {
    return NS_ERROR_UNEXPECTED;
  }

  return mSelect->SetLength(aLength);
}

void
HTMLOptionsCollection::IndexedSetter(uint32_t aIndex,
                                     HTMLOptionElement* aOption,
                                     ErrorResult& aError)
{
  if (!mSelect) {
    return;
  }

  // if the new option is null, just remove this option.  Note that it's safe
  // to pass a too-large aIndex in here.
  if (!aOption) {
    mSelect->Remove(aIndex);

    // We're done.
    return;
  }

  // Now we're going to be setting an option in our collection
  if (aIndex > mElements.Length()) {
    // Fill our array with blank options up to (but not including, since we're
    // about to change it) aIndex, for compat with other browsers.
    nsresult rv = SetLength(aIndex);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aError.Throw(rv);
      return;
    }
  }

  NS_ASSERTION(aIndex <= mElements.Length(), "SetLength lied");

  if (aIndex == mElements.Length()) {
    mSelect->AppendChild(*aOption, aError);
    return;
  }

  // Find the option they're talking about and replace it
  // hold a strong reference to follow COM rules.
  RefPtr<HTMLOptionElement> refChild = ItemAsOption(aIndex);
  if (!refChild) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsCOMPtr<nsINode> parent = refChild->GetParent();
  if (!parent) {
    return;
  }

  parent->ReplaceChild(*aOption, *refChild, aError);
}

int32_t
HTMLOptionsCollection::GetSelectedIndex(ErrorResult& aError)
{
  if (!mSelect) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return 0;
  }

  int32_t selectedIndex;
  aError = mSelect->GetSelectedIndex(&selectedIndex);
  return selectedIndex;
}

NS_IMETHODIMP
HTMLOptionsCollection::GetSelectedIndex(int32_t* aSelectedIndex)
{
  ErrorResult rv;
  *aSelectedIndex = GetSelectedIndex(rv);
  return rv.StealNSResult();
}

void
HTMLOptionsCollection::SetSelectedIndex(int32_t aSelectedIndex,
                                        ErrorResult& aError)
{
  if (!mSelect) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aError = mSelect->SetSelectedIndex(aSelectedIndex);
}

NS_IMETHODIMP
HTMLOptionsCollection::SetSelectedIndex(int32_t aSelectedIndex)
{
  ErrorResult rv;
  SetSelectedIndex(aSelectedIndex, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLOptionsCollection::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsISupports* item = GetElementAt(aIndex);
  if (!item) {
    *aReturn = nullptr;

    return NS_OK;
  }

  return CallQueryInterface(item, aReturn);
}

Element*
HTMLOptionsCollection::GetElementAt(uint32_t aIndex)
{
  return ItemAsOption(aIndex);
}

HTMLOptionElement*
HTMLOptionsCollection::NamedGetter(const nsAString& aName, bool& aFound)
{
  uint32_t count = mElements.Length();
  for (uint32_t i = 0; i < count; i++) {
    HTMLOptionElement* content = mElements.ElementAt(i);
    if (content &&
        (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, aName,
                              eCaseMatters) ||
         content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id, aName,
                              eCaseMatters))) {
      aFound = true;
      return content;
    }
  }

  aFound = false;
  return nullptr;
}

nsINode*
HTMLOptionsCollection::GetParentObject()
{
  return mSelect;
}

NS_IMETHODIMP
HTMLOptionsCollection::NamedItem(const nsAString& aName,
                                 nsIDOMNode** aReturn)
{
  NS_IF_ADDREF(*aReturn = GetNamedItem(aName));

  return NS_OK;
}

void
HTMLOptionsCollection::GetSupportedNames(nsTArray<nsString>& aNames)
{
  AutoTArray<nsIAtom*, 8> atoms;
  for (uint32_t i = 0; i < mElements.Length(); ++i) {
    HTMLOptionElement* content = mElements.ElementAt(i);
    if (content) {
      // Note: HasName means the names is exposed on the document,
      // which is false for options, so we don't check it here.
      const nsAttrValue* val = content->GetParsedAttr(nsGkAtoms::name);
      if (val && val->Type() == nsAttrValue::eAtom) {
        nsIAtom* name = val->GetAtomValue();
        if (!atoms.Contains(name)) {
          atoms.AppendElement(name);
        }
      }
      if (content->HasID()) {
        nsIAtom* id = content->GetID();
        if (!atoms.Contains(id)) {
          atoms.AppendElement(id);
        }
      }
    }
  }

  uint32_t atomsLen = atoms.Length();
  nsString* names = aNames.AppendElements(atomsLen);
  for (uint32_t i = 0; i < atomsLen; ++i) {
    atoms[i]->ToString(names[i]);
  }
}

NS_IMETHODIMP
HTMLOptionsCollection::GetSelect(nsIDOMHTMLSelectElement** aReturn)
{
  NS_IF_ADDREF(*aReturn = mSelect);
  return NS_OK;
}

NS_IMETHODIMP
HTMLOptionsCollection::Add(nsIDOMHTMLOptionElement* aOption,
                           nsIVariant* aBefore)
{
  if (!aOption) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!mSelect) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIDOMHTMLElement> elem = do_QueryInterface(aOption);
  return mSelect->Add(elem, aBefore);
}

void
HTMLOptionsCollection::Add(const HTMLOptionOrOptGroupElement& aElement,
                           const Nullable<HTMLElementOrLong>& aBefore,
                           ErrorResult& aError)
{
  if (!mSelect) {
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  mSelect->Add(aElement, aBefore, aError);
}

void
HTMLOptionsCollection::Remove(int32_t aIndex, ErrorResult& aError)
{
  if (!mSelect) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  uint32_t len = 0;
  mSelect->GetLength(&len);
  if (aIndex < 0 || (uint32_t)aIndex >= len)
    aIndex = 0;

  aError = mSelect->Remove(aIndex);
}

NS_IMETHODIMP
HTMLOptionsCollection::Remove(int32_t aIndex)
{
  ErrorResult rv;
  Remove(aIndex, rv);
  return rv.StealNSResult();
}

} // namespace dom
} // namespace mozilla
