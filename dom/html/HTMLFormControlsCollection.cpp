/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLFormControlsCollection.h"

#include "mozilla/FlushType.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLFormControlsCollectionBinding.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "nsGenericHTMLElement.h"  // nsGenericHTMLFormElement
#include "nsQueryObject.h"
#include "nsIFormControl.h"
#include "RadioNodeList.h"
#include "jsfriendapi.h"

namespace mozilla::dom {

/* static */
bool HTMLFormControlsCollection::ShouldBeInElements(
    nsIFormControl* aFormControl) {
  // For backwards compatibility (with 4.x and IE) we must not add
  // <input type=image> elements to the list of form controls in a
  // form.

  switch (aFormControl->ControlType()) {
    case FormControlType::ButtonButton:
    case FormControlType::ButtonReset:
    case FormControlType::ButtonSubmit:
    case FormControlType::InputButton:
    case FormControlType::InputCheckbox:
    case FormControlType::InputColor:
    case FormControlType::InputEmail:
    case FormControlType::InputFile:
    case FormControlType::InputHidden:
    case FormControlType::InputReset:
    case FormControlType::InputPassword:
    case FormControlType::InputRadio:
    case FormControlType::InputSearch:
    case FormControlType::InputSubmit:
    case FormControlType::InputText:
    case FormControlType::InputTel:
    case FormControlType::InputUrl:
    case FormControlType::InputNumber:
    case FormControlType::InputRange:
    case FormControlType::InputDate:
    case FormControlType::InputTime:
    case FormControlType::InputMonth:
    case FormControlType::InputWeek:
    case FormControlType::InputDatetimeLocal:
    case FormControlType::Select:
    case FormControlType::Textarea:
    case FormControlType::Fieldset:
    case FormControlType::Object:
    case FormControlType::Output:
    case FormControlType::FormAssociatedCustomElement:
      return true;

    // These form control types are not supposed to end up in the
    // form.elements array
    // XXXbz maybe we should just return aType != InputImage or something
    // instead of the big switch?
    case FormControlType::InputImage:
      break;
  }
  return false;
}

HTMLFormControlsCollection::HTMLFormControlsCollection(HTMLFormElement* aForm)
    : mForm(aForm)
      // Initialize the elements list to have an initial capacity
      // of 8 to reduce allocations on small forms.
      ,
      mElements(8),
      mNameLookupTable(HTMLFormElement::FORM_CONTROL_LIST_HASHTABLE_LENGTH) {}

HTMLFormControlsCollection::~HTMLFormControlsCollection() {
  mForm = nullptr;
  Clear();
}

void HTMLFormControlsCollection::DropFormReference() {
  mForm = nullptr;
  Clear();
}

void HTMLFormControlsCollection::Clear() {
  // Null out childrens' pointer to me.  No refcounting here
  for (int32_t i = mElements.Length() - 1; i >= 0; i--) {
    nsCOMPtr<nsIFormControl> formControl = do_QueryObject(mElements[i]);
    MOZ_ASSERT(formControl);
    formControl->ClearForm(false, false);
  }
  mElements.Clear();

  for (int32_t i = mNotInElements.Length() - 1; i >= 0; i--) {
    nsCOMPtr<nsIFormControl> formControl = do_QueryObject(mNotInElements[i]);
    MOZ_ASSERT(formControl);
    formControl->ClearForm(false, false);
  }
  mNotInElements.Clear();

  mNameLookupTable.Clear();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLFormControlsCollection)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HTMLFormControlsCollection)
  // Note: We intentionally don't set tmp->mForm to nullptr here, since doing
  // so may result in crashes because of inconsistent null-checking after the
  // object gets unlinked.
  tmp->Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HTMLFormControlsCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNameLookupTable)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(HTMLFormControlsCollection)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// XPConnect interface list for HTMLFormControlsCollection
NS_INTERFACE_TABLE_HEAD(HTMLFormControlsCollection)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(HTMLFormControlsCollection, nsIHTMLCollection)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(HTMLFormControlsCollection)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(HTMLFormControlsCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HTMLFormControlsCollection)

// nsIHTMLCollection interface

uint32_t HTMLFormControlsCollection::Length() { return mElements.Length(); }

nsISupports* HTMLFormControlsCollection::NamedItemInternal(
    const nsAString& aName) {
  return mNameLookupTable.GetWeak(aName);
}

nsresult HTMLFormControlsCollection::AddElementToTable(
    nsGenericHTMLFormElement* aChild, const nsAString& aName) {
  nsCOMPtr<nsIFormControl> formControl = do_QueryObject(aChild);
  MOZ_ASSERT(formControl);
  if (!ShouldBeInElements(formControl)) {
    return NS_OK;
  }

  return mForm->AddElementToTableInternal(mNameLookupTable, aChild, aName);
}

nsresult HTMLFormControlsCollection::IndexOfContent(nsIContent* aContent,
                                                    int32_t* aIndex) {
  // Note -- not a DOM method; callers should handle flushing themselves

  NS_ENSURE_ARG_POINTER(aIndex);

  *aIndex = mElements.IndexOf(aContent);

  return NS_OK;
}

nsresult HTMLFormControlsCollection::RemoveElementFromTable(
    nsGenericHTMLFormElement* aChild, const nsAString& aName) {
  nsCOMPtr<nsIFormControl> formControl = do_QueryObject(aChild);
  MOZ_ASSERT(formControl);
  if (!ShouldBeInElements(formControl)) {
    return NS_OK;
  }

  return mForm->RemoveElementFromTableInternal(mNameLookupTable, aChild, aName);
}

nsresult HTMLFormControlsCollection::GetSortedControls(
    nsTArray<RefPtr<nsGenericHTMLFormElement>>& aControls) const {
#ifdef DEBUG
  HTMLFormElement::AssertDocumentOrder(mElements, mForm);
  HTMLFormElement::AssertDocumentOrder(mNotInElements, mForm);
#endif

  aControls.Clear();

  // Merge the elements list and the not in elements list. Both lists are
  // already sorted.
  uint32_t elementsLen = mElements.Length();
  uint32_t notInElementsLen = mNotInElements.Length();
  aControls.SetCapacity(elementsLen + notInElementsLen);

  uint32_t elementsIdx = 0;
  uint32_t notInElementsIdx = 0;

  while (elementsIdx < elementsLen || notInElementsIdx < notInElementsLen) {
    // Check whether we're done with mElements
    if (elementsIdx == elementsLen) {
      NS_ASSERTION(notInElementsIdx < notInElementsLen,
                   "Should have remaining not-in-elements");
      // Append the remaining mNotInElements elements
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      aControls.AppendElements(mNotInElements.Elements() + notInElementsIdx,
                               notInElementsLen - notInElementsIdx);
      break;
    }
    // Check whether we're done with mNotInElements
    if (notInElementsIdx == notInElementsLen) {
      NS_ASSERTION(elementsIdx < elementsLen,
                   "Should have remaining in-elements");
      // Append the remaining mElements elements
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      aControls.AppendElements(mElements.Elements() + elementsIdx,
                               elementsLen - elementsIdx);
      break;
    }
    // Both lists have elements left.
    NS_ASSERTION(mElements[elementsIdx] && mNotInElements[notInElementsIdx],
                 "Should have remaining elements");
    // Determine which of the two elements should be ordered
    // first and add it to the end of the list.
    nsGenericHTMLFormElement* elementToAdd;
    if (nsContentUtils::CompareTreePosition(mElements[elementsIdx],
                                            mNotInElements[notInElementsIdx],
                                            mForm) < 0) {
      elementToAdd = mElements[elementsIdx];
      ++elementsIdx;
    } else {
      elementToAdd = mNotInElements[notInElementsIdx];
      ++notInElementsIdx;
    }
    // Add the first element to the list.
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    aControls.AppendElement(elementToAdd);
  }

  NS_ASSERTION(aControls.Length() == elementsLen + notInElementsLen,
               "Not all form controls were added to the sorted list");
#ifdef DEBUG
  HTMLFormElement::AssertDocumentOrder(aControls, mForm);
#endif

  return NS_OK;
}

Element* HTMLFormControlsCollection::GetElementAt(uint32_t aIndex) {
  return mElements.SafeElementAt(aIndex, nullptr);
}

/* virtual */
nsINode* HTMLFormControlsCollection::GetParentObject() { return mForm; }

/* virtual */
Element* HTMLFormControlsCollection::GetFirstNamedElement(
    const nsAString& aName, bool& aFound) {
  Nullable<OwningRadioNodeListOrElement> maybeResult;
  NamedGetter(aName, aFound, maybeResult);
  if (!aFound) {
    return nullptr;
  }
  MOZ_ASSERT(!maybeResult.IsNull());
  const OwningRadioNodeListOrElement& result = maybeResult.Value();
  if (result.IsElement()) {
    return result.GetAsElement().get();
  }
  if (result.IsRadioNodeList()) {
    RadioNodeList& nodelist = result.GetAsRadioNodeList();
    return nodelist.Item(0)->AsElement();
  }
  MOZ_ASSERT_UNREACHABLE("Should only have Elements and NodeLists here.");
  return nullptr;
}

void HTMLFormControlsCollection::NamedGetter(
    const nsAString& aName, bool& aFound,
    Nullable<OwningRadioNodeListOrElement>& aResult) {
  nsISupports* item = NamedItemInternal(aName);
  if (!item) {
    aFound = false;
    return;
  }
  aFound = true;
  if (nsCOMPtr<Element> element = do_QueryInterface(item)) {
    aResult.SetValue().SetAsElement() = element;
    return;
  }
  if (nsCOMPtr<RadioNodeList> nodelist = do_QueryInterface(item)) {
    aResult.SetValue().SetAsRadioNodeList() = nodelist;
    return;
  }
  MOZ_ASSERT_UNREACHABLE("Should only have Elements and NodeLists here.");
}

void HTMLFormControlsCollection::GetSupportedNames(nsTArray<nsString>& aNames) {
  // Just enumerate mNameLookupTable.  This won't guarantee order, but
  // that's OK, because the HTML5 spec doesn't define an order for
  // this enumeration.
  AppendToArray(aNames, mNameLookupTable.Keys());
}

/* virtual */
JSObject* HTMLFormControlsCollection::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return HTMLFormControlsCollection_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
