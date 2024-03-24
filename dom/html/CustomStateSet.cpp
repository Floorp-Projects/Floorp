/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CustomStateSet.h"
#include "mozilla/dom/ElementInternalsBinding.h"
#include "mozilla/dom/HTMLElement.h"
#include "mozilla/dom/Document.h"
#include "mozilla/PresShell.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CustomStateSet, mTarget);

NS_IMPL_CYCLE_COLLECTING_ADDREF(CustomStateSet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CustomStateSet)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CustomStateSet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

CustomStateSet::CustomStateSet(HTMLElement* aTarget) : mTarget(aTarget) {}

// WebIDL interface
nsISupports* CustomStateSet::GetParentObject() const {
  return ToSupports(mTarget);
}

JSObject* CustomStateSet::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return CustomStateSet_Binding::Wrap(aCx, this, aGivenProto);
}

void CustomStateSet::Clear(ErrorResult& aRv) {
  CustomStateSet_Binding::SetlikeHelpers::Clear(this, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsTArray<RefPtr<nsAtom>>& states = mTarget->EnsureCustomStates();
  Document* doc = mTarget->GetComposedDoc();
  PresShell* presShell = doc ? doc->GetPresShell() : nullptr;
  if (presShell) {
    presShell->CustomStatesWillChange(*mTarget);
    // Iterate over each state to ensure each one is invalidated.
    while (!states.IsEmpty()) {
      RefPtr<nsAtom> atom = states.PopLastElement();
      presShell->CustomStateChanged(*mTarget, atom);
    }
    return;
  }

  states.Clear();
}

bool CustomStateSet::Delete(const nsAString& aState, ErrorResult& aRv) {
  CustomStateSet_Binding::SetlikeHelpers::Delete(this, aState, aRv);
  if (aRv.Failed()) {
    return false;
  }

  RefPtr<nsAtom> atom = NS_AtomizeMainThread(aState);
  Document* doc = mTarget->GetComposedDoc();
  PresShell* presShell = doc ? doc->GetPresShell() : nullptr;
  if (presShell) {
    presShell->CustomStatesWillChange(*mTarget);
  }

  bool deleted = mTarget->EnsureCustomStates().RemoveElement(atom);

  if (presShell) {
    presShell->CustomStateChanged(*mTarget, atom);
  }
  return deleted;
}

void CustomStateSet::Add(const nsAString& aState, ErrorResult& aRv) {
  CustomStateSet_Binding::SetlikeHelpers::Add(this, aState, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsTArray<RefPtr<nsAtom>>& states = mTarget->EnsureCustomStates();
  RefPtr<nsAtom> atom = NS_AtomizeMainThread(aState);
  if (states.Contains(atom)) {
    return;
  }

  Document* doc = mTarget->GetComposedDoc();
  PresShell* presShell = doc ? doc->GetPresShell() : nullptr;
  if (presShell) {
    presShell->CustomStatesWillChange(*mTarget);
  }

  states.AppendElement(atom);

  if (presShell) {
    presShell->CustomStateChanged(*mTarget, atom);
  }
}

}  // namespace mozilla::dom
