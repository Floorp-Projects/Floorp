/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAllCollection.h"

#include "jsfriendapi.h"
#include "mozilla/dom/HTMLAllCollectionBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/Element.h"
#include "nsContentList.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

HTMLAllCollection::HTMLAllCollection(mozilla::dom::Document* aDocument)
    : mDocument(aDocument) {
  MOZ_ASSERT(mDocument);
}

HTMLAllCollection::~HTMLAllCollection() = default;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(HTMLAllCollection, mDocument, mCollection,
                                      mNamedMap)

NS_IMPL_CYCLE_COLLECTING_ADDREF(HTMLAllCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HTMLAllCollection)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLAllCollection)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsINode* HTMLAllCollection::GetParentObject() const { return mDocument; }

uint32_t HTMLAllCollection::Length() { return Collection()->Length(true); }

Element* HTMLAllCollection::Item(uint32_t aIndex) {
  nsIContent* item = Collection()->Item(aIndex);
  return item ? item->AsElement() : nullptr;
}

void HTMLAllCollection::Item(const Optional<nsAString>& aNameOrIndex,
                             Nullable<OwningHTMLCollectionOrElement>& aResult) {
  if (!aNameOrIndex.WasPassed()) {
    aResult.SetNull();
    return;
  }

  const nsAString& nameOrIndex = aNameOrIndex.Value();
  uint32_t indexVal;
  if (js::StringIsArrayIndex(nameOrIndex.BeginReading(), nameOrIndex.Length(),
                             &indexVal)) {
    Element* element = Item(indexVal);
    if (element) {
      aResult.SetValue().SetAsElement() = element;
    } else {
      aResult.SetNull();
    }
    return;
  }

  NamedItem(nameOrIndex, aResult);
}

nsContentList* HTMLAllCollection::Collection() {
  if (!mCollection) {
    Document* document = mDocument;
    mCollection = document->GetElementsByTagName(u"*"_ns);
    MOZ_ASSERT(mCollection);
  }
  return mCollection;
}

static bool IsAllNamedElement(nsIContent* aContent) {
  return aContent->IsAnyOfHTMLElements(
      nsGkAtoms::a, nsGkAtoms::button, nsGkAtoms::embed, nsGkAtoms::form,
      nsGkAtoms::iframe, nsGkAtoms::img, nsGkAtoms::input, nsGkAtoms::map,
      nsGkAtoms::meta, nsGkAtoms::object, nsGkAtoms::select,
      nsGkAtoms::textarea, nsGkAtoms::frame, nsGkAtoms::frameset);
}

static bool DocAllResultMatch(Element* aElement, int32_t aNamespaceID,
                              nsAtom* aAtom, void* aData) {
  if (aElement->GetID() == aAtom) {
    return true;
  }

  nsGenericHTMLElement* elm = nsGenericHTMLElement::FromNode(aElement);
  if (!elm) {
    return false;
  }

  if (!IsAllNamedElement(elm)) {
    return false;
  }

  const nsAttrValue* val = elm->GetParsedAttr(nsGkAtoms::name);
  return val && val->Type() == nsAttrValue::eAtom &&
         val->GetAtomValue() == aAtom;
}

nsContentList* HTMLAllCollection::GetDocumentAllList(const nsAString& aID) {
  return mNamedMap
      .LookupOrInsertWith(aID,
                          [this, &aID] {
                            RefPtr<nsAtom> id = NS_Atomize(aID);
                            return new nsContentList(mDocument,
                                                     DocAllResultMatch, nullptr,
                                                     nullptr, true, id);
                          })
      .get();
}

void HTMLAllCollection::NamedGetter(
    const nsAString& aID, bool& aFound,
    Nullable<OwningHTMLCollectionOrElement>& aResult) {
  if (aID.IsEmpty()) {
    aFound = false;
    aResult.SetNull();
    return;
  }

  nsContentList* docAllList = GetDocumentAllList(aID);
  if (!docAllList) {
    aFound = false;
    aResult.SetNull();
    return;
  }

  // Check if there are more than 1 entries. Do this by getting the second one
  // rather than the length since getting the length always requires walking
  // the entire document.
  if (docAllList->Item(1, true)) {
    aFound = true;
    aResult.SetValue().SetAsHTMLCollection() = docAllList;
    return;
  }

  // There's only 0 or 1 items. Return the first one or null.
  if (nsIContent* node = docAllList->Item(0, true)) {
    aFound = true;
    aResult.SetValue().SetAsElement() = node->AsElement();
    return;
  }

  aFound = false;
  aResult.SetNull();
}

void HTMLAllCollection::GetSupportedNames(nsTArray<nsString>& aNames) {
  // XXXbz this is very similar to nsContentList::GetSupportedNames,
  // but has to check IsAllNamedElement for the name case.
  AutoTArray<nsAtom*, 8> atoms;
  for (uint32_t i = 0; i < Length(); ++i) {
    nsIContent* content = Item(i);
    if (content->HasID()) {
      nsAtom* id = content->GetID();
      MOZ_ASSERT(id != nsGkAtoms::_empty, "Empty ids don't get atomized");
      if (!atoms.Contains(id)) {
        atoms.AppendElement(id);
      }
    }

    nsGenericHTMLElement* el = nsGenericHTMLElement::FromNode(content);
    if (el) {
      // Note: nsINode::HasName means the name is exposed on the document,
      // which is false for options, so we don't check it here.
      const nsAttrValue* val = el->GetParsedAttr(nsGkAtoms::name);
      if (val && val->Type() == nsAttrValue::eAtom &&
          IsAllNamedElement(content)) {
        nsAtom* name = val->GetAtomValue();
        MOZ_ASSERT(name != nsGkAtoms::_empty, "Empty names don't get atomized");
        if (!atoms.Contains(name)) {
          atoms.AppendElement(name);
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

JSObject* HTMLAllCollection::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return HTMLAllCollection_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
