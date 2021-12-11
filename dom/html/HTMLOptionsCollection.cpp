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
#include "mozilla/MappedDeclarations.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLOptionsCollectionBinding.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "nsContentCreatorFunctions.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/Document.h"
#include "nsIFormControlFrame.h"
#include "nsLayoutUtils.h"
#include "nsMappedAttributes.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleConsts.h"
#include "jsfriendapi.h"

namespace mozilla::dom {

HTMLOptionsCollection::HTMLOptionsCollection(HTMLSelectElement* aSelect)
    : mSelect(aSelect) {}

nsresult HTMLOptionsCollection::GetOptionIndex(Element* aOption,
                                               int32_t aStartIndex,
                                               bool aForward, int32_t* aIndex) {
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(HTMLOptionsCollection, mElements, mSelect)

// nsISupports

// QueryInterface implementation for HTMLOptionsCollection
NS_INTERFACE_TABLE_HEAD(HTMLOptionsCollection)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(HTMLOptionsCollection, nsIHTMLCollection)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(HTMLOptionsCollection)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(HTMLOptionsCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HTMLOptionsCollection)

JSObject* HTMLOptionsCollection::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return HTMLOptionsCollection_Binding::Wrap(aCx, this, aGivenProto);
}

uint32_t HTMLOptionsCollection::Length() { return mElements.Length(); }

void HTMLOptionsCollection::SetLength(uint32_t aLength, ErrorResult& aError) {
  mSelect->SetLength(aLength, aError);
}

void HTMLOptionsCollection::IndexedSetter(uint32_t aIndex,
                                          HTMLOptionElement* aOption,
                                          ErrorResult& aError) {
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
    SetLength(aIndex, aError);
    ENSURE_SUCCESS_VOID(aError);
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

int32_t HTMLOptionsCollection::SelectedIndex() {
  return mSelect->SelectedIndex();
}

void HTMLOptionsCollection::SetSelectedIndex(int32_t aSelectedIndex) {
  mSelect->SetSelectedIndex(aSelectedIndex);
}

Element* HTMLOptionsCollection::GetElementAt(uint32_t aIndex) {
  return ItemAsOption(aIndex);
}

HTMLOptionElement* HTMLOptionsCollection::NamedGetter(const nsAString& aName,
                                                      bool& aFound) {
  uint32_t count = mElements.Length();
  for (uint32_t i = 0; i < count; i++) {
    HTMLOptionElement* content = mElements.ElementAt(i);
    if (content && (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                         aName, eCaseMatters) ||
                    content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
                                         aName, eCaseMatters))) {
      aFound = true;
      return content;
    }
  }

  aFound = false;
  return nullptr;
}

nsINode* HTMLOptionsCollection::GetParentObject() { return mSelect; }

DocGroup* HTMLOptionsCollection::GetDocGroup() const {
  return mSelect ? mSelect->GetDocGroup() : nullptr;
}

void HTMLOptionsCollection::GetSupportedNames(nsTArray<nsString>& aNames) {
  AutoTArray<nsAtom*, 8> atoms;
  for (uint32_t i = 0; i < mElements.Length(); ++i) {
    HTMLOptionElement* content = mElements.ElementAt(i);
    if (content) {
      // Note: HasName means the names is exposed on the document,
      // which is false for options, so we don't check it here.
      const nsAttrValue* val = content->GetParsedAttr(nsGkAtoms::name);
      if (val && val->Type() == nsAttrValue::eAtom) {
        nsAtom* name = val->GetAtomValue();
        if (!atoms.Contains(name)) {
          atoms.AppendElement(name);
        }
      }
      if (content->HasID()) {
        nsAtom* id = content->GetID();
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

void HTMLOptionsCollection::Add(const HTMLOptionOrOptGroupElement& aElement,
                                const Nullable<HTMLElementOrLong>& aBefore,
                                ErrorResult& aError) {
  mSelect->Add(aElement, aBefore, aError);
}

void HTMLOptionsCollection::Remove(int32_t aIndex) { mSelect->Remove(aIndex); }

}  // namespace mozilla::dom
