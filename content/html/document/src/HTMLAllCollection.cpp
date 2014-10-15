/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAllCollection.h"

#include "mozilla/dom/HTMLAllCollectionBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsHTMLDocument.h"

namespace mozilla {
namespace dom {

HTMLAllCollection::HTMLAllCollection(nsHTMLDocument* aDocument)
  : mDocument(aDocument)
{
  MOZ_ASSERT(mDocument);
}

HTMLAllCollection::~HTMLAllCollection()
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(HTMLAllCollection,
                                      mDocument,
                                      mCollection,
                                      mNamedMap)

NS_IMPL_CYCLE_COLLECTING_ADDREF(HTMLAllCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HTMLAllCollection)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLAllCollection)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsINode*
HTMLAllCollection::GetParentObject() const
{
  return mDocument;
}

uint32_t
HTMLAllCollection::Length()
{
  return Collection()->Length(true);
}

nsIContent*
HTMLAllCollection::Item(uint32_t aIndex)
{
  return Collection()->Item(aIndex);
}

nsContentList*
HTMLAllCollection::Collection()
{
  if (!mCollection) {
    nsIDocument* document = mDocument;
    mCollection = document->GetElementsByTagName(NS_LITERAL_STRING("*"));
    MOZ_ASSERT(mCollection);
  }
  return mCollection;
}

static bool
IsAllNamedElement(nsIContent* aContent)
{
  nsIAtom* tag = aContent->Tag();
  return
    tag == nsGkAtoms::a ||
    tag == nsGkAtoms::applet ||
    tag == nsGkAtoms::button ||
    tag == nsGkAtoms::embed ||
    tag == nsGkAtoms::form ||
    tag == nsGkAtoms::iframe ||
    tag == nsGkAtoms::img ||
    tag == nsGkAtoms::input ||
    tag == nsGkAtoms::map ||
    tag == nsGkAtoms::meta ||
    tag == nsGkAtoms::object ||
    tag == nsGkAtoms::select ||
    tag == nsGkAtoms::textarea ||
    tag == nsGkAtoms::frame ||
    tag == nsGkAtoms::frameset;
}

static bool
DocAllResultMatch(nsIContent* aContent, int32_t aNamespaceID, nsIAtom* aAtom,
                  void* aData)
{
  if (aContent->GetID() == aAtom) {
    return true;
  }

  nsGenericHTMLElement* elm = nsGenericHTMLElement::FromContent(aContent);
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

nsContentList*
HTMLAllCollection::GetDocumentAllList(const nsAString& aID)
{
  if (nsContentList* docAllList = mNamedMap.GetWeak(aID)) {
    return docAllList;
  }

  Element* root = mDocument->GetRootElement();
  if (!root) {
    return nullptr;
  }

  nsCOMPtr<nsIAtom> id = do_GetAtom(aID);
  nsRefPtr<nsContentList> docAllList =
    new nsContentList(root, DocAllResultMatch, nullptr, nullptr, true, id);
  mNamedMap.Put(aID, docAllList);
  return docAllList;
}

void
HTMLAllCollection::NamedGetter(const nsAString& aID,
                               bool& aFound,
                               Nullable<OwningNodeOrHTMLCollection>& aResult)
{
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
    aResult.SetValue().SetAsNode() = node;
    return;
  }

  aFound = false;
  aResult.SetNull();
}

void
HTMLAllCollection::GetSupportedNames(unsigned aFlags, nsTArray<nsString>& aNames)
{
  if (!(aFlags & JSITER_HIDDEN)) {
    return;
  }

  // XXXbz this is very similar to nsContentList::GetSupportedNames,
  // but has to check IsAllNamedElement for the name case.
  nsAutoTArray<nsIAtom*, 8> atoms;
  for (uint32_t i = 0; i < Length(); ++i) {
    nsIContent *content = Item(i);
    if (content->HasID()) {
      nsIAtom* id = content->GetID();
      MOZ_ASSERT(id != nsGkAtoms::_empty,
                 "Empty ids don't get atomized");
      if (!atoms.Contains(id)) {
        atoms.AppendElement(id);
      }
    }

    nsGenericHTMLElement* el = nsGenericHTMLElement::FromContent(content);
    if (el) {
      // Note: nsINode::HasName means the name is exposed on the document,
      // which is false for options, so we don't check it here.
      const nsAttrValue* val = el->GetParsedAttr(nsGkAtoms::name);
      if (val && val->Type() == nsAttrValue::eAtom &&
          IsAllNamedElement(content)) {
        nsIAtom* name = val->GetAtomValue();
        MOZ_ASSERT(name != nsGkAtoms::_empty,
                   "Empty names don't get atomized");
        if (!atoms.Contains(name)) {
          atoms.AppendElement(name);
        }
      }
    }
  }

  aNames.SetCapacity(atoms.Length());
  for (uint32_t i = 0; i < atoms.Length(); ++i) {
    aNames.AppendElement(nsDependentAtomString(atoms[i]));
  }
}


JSObject*
HTMLAllCollection::WrapObject(JSContext* aCx)
{
  return HTMLAllCollectionBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
