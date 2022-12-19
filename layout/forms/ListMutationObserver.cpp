/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ListMutationObserver.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "nsIFrame.h"

namespace mozilla {
NS_IMPL_ISUPPORTS(ListMutationObserver, nsIMutationObserver)

ListMutationObserver::~ListMutationObserver() = default;

void ListMutationObserver::Attach(bool aRepaint) {
  nsAutoString id;
  if (InputElement().GetAttr(nsGkAtoms::list_, id)) {
    Unlink();
    RefPtr<nsAtom> idAtom = NS_AtomizeMainThread(id);
    ResetWithID(InputElement(), idAtom);
    AddObserverIfNeeded();
  }
  if (aRepaint) {
    mOwningElementFrame->InvalidateFrame();
  }
}

void ListMutationObserver::AddObserverIfNeeded() {
  if (auto* list = get()) {
    if (list->IsHTMLElement(nsGkAtoms::datalist)) {
      list->AddMutationObserver(this);
    }
  }
}

void ListMutationObserver::RemoveObserverIfNeeded(dom::Element* aList) {
  if (aList && aList->IsHTMLElement(nsGkAtoms::datalist)) {
    aList->RemoveMutationObserver(this);
  }
}

void ListMutationObserver::Detach() {
  RemoveObserverIfNeeded();
  Unlink();
}

dom::HTMLInputElement& ListMutationObserver::InputElement() const {
  MOZ_ASSERT(mOwningElementFrame->GetContent()->IsHTMLElement(nsGkAtoms::input),
             "bad cast");
  return *static_cast<dom::HTMLInputElement*>(
      mOwningElementFrame->GetContent());
}

void ListMutationObserver::AttributeChanged(dom::Element* aElement,
                                            int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType,
                                            const nsAttrValue* aOldValue) {
  if (aAttribute == nsGkAtoms::value && aNameSpaceID == kNameSpaceID_None &&
      aElement->IsHTMLElement(nsGkAtoms::option)) {
    mOwningElementFrame->InvalidateFrame();
  }
}

void ListMutationObserver::CharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  mOwningElementFrame->InvalidateFrame();
}

void ListMutationObserver::ContentAppended(nsIContent* aFirstNewContent) {
  mOwningElementFrame->InvalidateFrame();
}

void ListMutationObserver::ContentInserted(nsIContent* aChild) {
  mOwningElementFrame->InvalidateFrame();
}

void ListMutationObserver::ContentRemoved(nsIContent* aChild,
                                          nsIContent* aPreviousSibling) {
  mOwningElementFrame->InvalidateFrame();
}

void ListMutationObserver::ElementChanged(dom::Element* aFrom,
                                          dom::Element* aTo) {
  IDTracker::ElementChanged(aFrom, aTo);
  RemoveObserverIfNeeded(aFrom);
  AddObserverIfNeeded();
  mOwningElementFrame->InvalidateFrame();
}

}  // namespace mozilla
