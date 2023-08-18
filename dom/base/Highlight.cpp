/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Highlight.h"
#include "HighlightRegistry.h"
#include "mozilla/StaticAnalysisFunctions.h"
#include "mozilla/dom/HighlightBinding.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"

#include "AbstractRange.h"
#include "Document.h"
#include "PresShell.h"
#include "Selection.h"

#include "nsFrameSelection.h"
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
                                       nsAtom& aHighlightName) {
  mHighlightRegistries.LookupOrInsert(&aHighlightRegistry)
      .Insert(&aHighlightName);
}

void Highlight::RemoveFromHighlightRegistry(
    HighlightRegistry& aHighlightRegistry, nsAtom& aHighlightName) {
  if (auto entry = mHighlightRegistries.Lookup(&aHighlightRegistry)) {
    auto& highlightNames = entry.Data();
    highlightNames.Remove(&aHighlightName);
    if (highlightNames.IsEmpty()) {
      entry.Remove();
    }
  }
}

already_AddRefed<Selection> Highlight::CreateHighlightSelection(
    nsAtom* aHighlightName, nsFrameSelection* aFrameSelection) {
  MOZ_ASSERT(aFrameSelection);
  MOZ_ASSERT(aFrameSelection->GetPresShell());
  RefPtr<Selection> selection =
      MakeRefPtr<Selection>(SelectionType::eHighlight, aFrameSelection);
  selection->SetHighlightSelectionData({aHighlightName, this});
  AutoFrameSelectionBatcher selectionBatcher(__FUNCTION__);
  for (const RefPtr<AbstractRange>& range : mRanges) {
    if (range->GetComposedDocOfContainers() ==
        aFrameSelection->GetPresShell()->GetDocument()) {
      // since this is run in a context guarded by a selection batcher,
      // no strong reference is needed to keep `range` alive.
      selection->AddHighlightRangeAndSelectFramesAndNotifyListeners(
          MOZ_KnownLive(*range));
    }
  }
  return selection.forget();
}

void Highlight::Add(AbstractRange& aRange, ErrorResult& aRv) {
  Highlight_Binding::SetlikeHelpers::Add(this, aRange, aRv);
  if (aRv.Failed()) {
    return;
  }
  if (!mRanges.Contains(&aRange)) {
    mRanges.AppendElement(&aRange);
    AutoFrameSelectionBatcher selectionBatcher(__FUNCTION__,
                                               mHighlightRegistries.Count());
    for (const RefPtr<HighlightRegistry>& registry :
         mHighlightRegistries.Keys()) {
      auto frameSelection = registry->GetFrameSelection();
      selectionBatcher.AddFrameSelection(frameSelection);
      // since this is run in a context guarded by a selection batcher,
      // no strong reference is needed to keep `registry` alive.
      MOZ_KnownLive(registry)->MaybeAddRangeToHighlightSelection(aRange, *this);
      if (aRv.Failed()) {
        return;
      }
    }
  }
}

void Highlight::Clear(ErrorResult& aRv) {
  Highlight_Binding::SetlikeHelpers::Clear(this, aRv);
  if (!aRv.Failed()) {
    mRanges.Clear();
    AutoFrameSelectionBatcher selectionBatcher(__FUNCTION__,
                                               mHighlightRegistries.Count());

    for (const RefPtr<HighlightRegistry>& registry :
         mHighlightRegistries.Keys()) {
      auto frameSelection = registry->GetFrameSelection();
      selectionBatcher.AddFrameSelection(frameSelection);
      // since this is run in a context guarded by a selection batcher,
      // no strong reference is needed to keep `registry` alive.
      MOZ_KnownLive(registry)->RemoveHighlightSelection(*this);
    }
  }
}

bool Highlight::Delete(AbstractRange& aRange, ErrorResult& aRv) {
  if (Highlight_Binding::SetlikeHelpers::Delete(this, aRange, aRv)) {
    mRanges.RemoveElement(&aRange);
    AutoFrameSelectionBatcher selectionBatcher(__FUNCTION__,
                                               mHighlightRegistries.Count());

    for (const RefPtr<HighlightRegistry>& registry :
         mHighlightRegistries.Keys()) {
      auto frameSelection = registry->GetFrameSelection();
      selectionBatcher.AddFrameSelection(frameSelection);
      // since this is run in a context guarded by a selection batcher,
      // no strong reference is needed to keep `registry` alive.
      MOZ_KnownLive(registry)->MaybeRemoveRangeFromHighlightSelection(aRange,
                                                                      *this);
    }
    return true;
  }
  return false;
}

JSObject* Highlight::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Highlight_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
