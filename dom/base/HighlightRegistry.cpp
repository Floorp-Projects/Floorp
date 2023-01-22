/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HighlightRegistry.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/CompactPair.h"

#include "Document.h"
#include "Highlight.h"
#include "mozilla/dom/HighlightBinding.h"
#include "PresShell.h"

#include "nsAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "nsFrameSelection.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(HighlightRegistry)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HighlightRegistry)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  for (auto const& iter : tmp->mHighlightsOrdered) {
    iter.second()->RemoveFromHighlightRegistry(*tmp, *iter.first());
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHighlightsOrdered)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HighlightRegistry)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  for (size_t i = 0; i < tmp->mHighlightsOrdered.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHighlightsOrdered[i].second())
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(HighlightRegistry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HighlightRegistry)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HighlightRegistry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

HighlightRegistry::HighlightRegistry(Document* aDocument)
    : mDocument(aDocument) {}

HighlightRegistry::~HighlightRegistry() {
  for (auto const& iter : mHighlightsOrdered) {
    iter.second()->RemoveFromHighlightRegistry(*this, *iter.first());
  }
}

JSObject* HighlightRegistry::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return HighlightRegistry_Binding::Wrap(aCx, this, aGivenProto);
}

void HighlightRegistry::HighlightPropertiesChanged(Highlight& aHighlight,
                                                   ErrorResult& aRv) {
  RefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
  if (!frameSelection) {
    return;
  }
  for (auto const& iter : mHighlightsOrdered) {
    if (iter.second() != &aHighlight) {
      continue;
    }

    RefPtr<const nsAtom> highlightName = iter.first();
    frameSelection->RemoveHighlightSelection(highlightName);
    frameSelection->AddHighlightSelection(highlightName, aHighlight, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void HighlightRegistry::AddHighlightSelectionsToFrameSelection(
    ErrorResult& aRv) {
  if (mHighlightsOrdered.IsEmpty()) {
    return;
  }
  RefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
  if (!frameSelection) {
    return;
  }
  for (auto const& iter : mHighlightsOrdered) {
    RefPtr<const nsAtom> highlightName = iter.first();
    RefPtr<Highlight> highlight = iter.second();
    frameSelection->AddHighlightSelection(highlightName, *highlight, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void HighlightRegistry::Set(const nsAString& aKey, Highlight& aValue,
                            ErrorResult& aRv) {
  HighlightRegistry_Binding::MaplikeHelpers::Set(this, aKey, aValue, aRv);
  if (aRv.Failed()) {
    return;
  }
  RefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
  RefPtr<nsAtom> highlightNameAtom = NS_AtomizeMainThread(aKey);
  auto foundIter =
      std::find_if(mHighlightsOrdered.begin(), mHighlightsOrdered.end(),
                   [&highlightNameAtom](auto const& aElm) {
                     return aElm.first() == highlightNameAtom;
                   });
  if (foundIter != mHighlightsOrdered.end()) {
    foundIter->second()->RemoveFromHighlightRegistry(*this, *highlightNameAtom);
    if (frameSelection) {
      frameSelection->RemoveHighlightSelection(highlightNameAtom);
    }
    foundIter->second() = &aValue;
  } else {
    mHighlightsOrdered.AppendElement(
        CompactPair<RefPtr<const nsAtom>, RefPtr<Highlight>>(highlightNameAtom,
                                                             &aValue));
  }
  aValue.AddToHighlightRegistry(*this, *highlightNameAtom);
  if (frameSelection) {
    frameSelection->AddHighlightSelection(highlightNameAtom, aValue, aRv);
  }
}

void HighlightRegistry::Clear(ErrorResult& aRv) {
  HighlightRegistry_Binding::MaplikeHelpers::Clear(this, aRv);
  if (aRv.Failed()) {
    return;
  }
  auto frameSelection = GetFrameSelection();
  for (auto const& iter : mHighlightsOrdered) {
    const auto& highlightName = iter.first();
    const auto& highlight = iter.second();
    highlight->RemoveFromHighlightRegistry(*this, *highlightName);
    if (frameSelection) {
      frameSelection->RemoveHighlightSelection(highlightName);
    }
  }
  mHighlightsOrdered.Clear();
}

void HighlightRegistry::Delete(const nsAString& aKey, ErrorResult& aRv) {
  if (!HighlightRegistry_Binding::MaplikeHelpers::Delete(this, aKey, aRv)) {
    return;
  }
  RefPtr<nsAtom> highlightNameAtom = NS_AtomizeMainThread(aKey);
  auto foundIter =
      std::find_if(mHighlightsOrdered.cbegin(), mHighlightsOrdered.cend(),
                   [&highlightNameAtom](auto const& aElm) {
                     return aElm.first() == highlightNameAtom;
                   });
  if (foundIter == mHighlightsOrdered.cend()) {
    return;
  }
  RefPtr<Highlight> highlight = foundIter->second();
  mHighlightsOrdered.RemoveElementAt(foundIter);

  if (auto frameSelection = GetFrameSelection()) {
    frameSelection->RemoveHighlightSelection(highlightNameAtom);
  }
  highlight->RemoveFromHighlightRegistry(*this, *highlightNameAtom);
}

RefPtr<nsFrameSelection> HighlightRegistry::GetFrameSelection() {
  return RefPtr<nsFrameSelection>(
      mDocument->GetPresShell() ? mDocument->GetPresShell()->FrameSelection()
                                : nullptr);
}

}  // namespace mozilla::dom
