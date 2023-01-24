/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Highlight.h"
#include "HighlightRegistry.h"
#include "mozilla/dom/HighlightBinding.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"

#include "AbstractRange.h"
#include "Document.h"
#include "PresShell.h"
#include "Selection.h"

#include "nsPIDOMWindow.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Highlight, mRanges, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Highlight)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Highlight)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Highlight)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Highlight::Highlight(
    const Sequence<OwningNonNull<AbstractRange>>& aInitialRanges,
    nsPIDOMWindowInner* aWindow, ErrorResult& aRv)
    : mWindow(aWindow) {
  for (RefPtr<AbstractRange> range : aInitialRanges) {
    Add(*range, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

already_AddRefed<Highlight> Highlight::Constructor(
    const GlobalObject& aGlobal,
    const Sequence<OwningNonNull<AbstractRange>>& aInitialRanges,
    ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.ThrowUnknownError(
        "There is no window associated to "
        "this highlight object!");
    return nullptr;
  }

  RefPtr<Highlight> highlight = new Highlight(aInitialRanges, window, aRv);
  return aRv.Failed() ? nullptr : highlight.forget();
}

void Highlight::AddToHighlightRegistry(HighlightRegistry& aHighlightRegistry,
                                       const nsAtom& aHighlightName) {
  mHighlightRegistries.LookupOrInsert(&aHighlightRegistry)
      .Insert(&aHighlightName);
}

void Highlight::RemoveFromHighlightRegistry(
    HighlightRegistry& aHighlightRegistry, const nsAtom& aHighlightName) {
  if (auto entry = mHighlightRegistries.Lookup(&aHighlightRegistry)) {
    auto& highlightNames = entry.Data();
    highlightNames.Remove(&aHighlightName);
    if (highlightNames.IsEmpty()) {
      entry.Remove();
    }
  }
}

already_AddRefed<Selection> Highlight::CreateHighlightSelection(
    const nsAtom* aHighlightName, nsFrameSelection* aFrameSelection,
    ErrorResult& aRv) const {
  RefPtr<Selection> selection =
      MakeRefPtr<Selection>(SelectionType::eHighlight, aFrameSelection);
  selection->SetHighlightName(aHighlightName);

  for (auto const& range : mRanges) {
    // this is safe because `Highlight::Add()` ensures all ranges are
    // dynamic.
    RefPtr<nsRange> dynamicRange = range->AsDynamicRange();
    selection->AddRangeAndSelectFramesAndNotifyListeners(*dynamicRange, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
  }
  return selection.forget();
}

void Highlight::NotifyChangesToRegistries(ErrorResult& aRv) {
  for (RefPtr<HighlightRegistry> highlightRegistry :
       mHighlightRegistries.Keys()) {
    MOZ_ASSERT(highlightRegistry);
    highlightRegistry->HighlightPropertiesChanged(*this, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void Highlight::Add(AbstractRange& aRange, ErrorResult& aRv) {
  if (aRange.IsStaticRange()) {
    // TODO (jjaschke) Selection needs to be able to deal with StaticRanges
    // (Bug 1808565)
    aRv.ThrowUnknownError("Support for StaticRanges is not implemented yet!");
    return;
  }
  Highlight_Binding::SetlikeHelpers::Add(this, aRange, aRv);
  if (aRv.Failed()) {
    return;
  }
  if (!mRanges.Contains(&aRange)) {
    mRanges.AppendElement(&aRange);
    NotifyChangesToRegistries(aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void Highlight::Clear(ErrorResult& aRv) {
  Highlight_Binding::SetlikeHelpers::Clear(this, aRv);
  if (!aRv.Failed()) {
    mRanges.Clear();
    NotifyChangesToRegistries(aRv);
  }
}

void Highlight::Delete(AbstractRange& aRange, ErrorResult& aRv) {
  if (Highlight_Binding::SetlikeHelpers::Delete(this, aRange, aRv)) {
    mRanges.RemoveElement(&aRange);
    NotifyChangesToRegistries(aRv);
  }
}

JSObject* Highlight::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Highlight_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
