/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"
#include "DocAccessibleChild.h"
#include "DocAccessibleWrap.h"
#include "nsIDocShell.h"
#include "nsLayoutUtils.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsIPersistentProperties2.h"
#include "Pivot.h"
#include "SessionAccessibility.h"
#include "TraversalRule.h"
#include "mozilla/PresShell.h"
#include "mozilla/a11y/DocAccessiblePlatformExtChild.h"

using namespace mozilla;
using namespace mozilla::a11y;

const uint32_t kCacheRefreshInterval = 500;

#define UNIQUE_ID(acc)                             \
  !acc || (acc->IsDoc() && acc->AsDoc()->IPCDoc()) \
      ? 0                                          \
      : reinterpret_cast<uint64_t>(acc->UniqueID())

////////////////////////////////////////////////////////////////////////////////
// DocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

DocAccessibleWrap::DocAccessibleWrap(Document* aDocument, PresShell* aPresShell)
    : DocAccessible(aDocument, aPresShell) {
  if (aDocument->GetBrowsingContext()->IsTopContent()) {
    // The top-level content document gets this special ID.
    mID = kNoID;
  } else {
    mID = AcquireID();
  }
}

DocAccessibleWrap::~DocAccessibleWrap() {}

AccessibleWrap* DocAccessibleWrap::GetAccessibleByID(int32_t aID) const {
  if (AccessibleWrap* acc = mIDToAccessibleMap.Get(aID)) {
    return acc;
  }

  // If the ID is not in the hash table, check the IDs of the child docs.
  for (uint32_t i = 0; i < ChildDocumentCount(); i++) {
    auto childDoc = reinterpret_cast<AccessibleWrap*>(GetChildDocumentAt(i));
    if (childDoc->VirtualViewID() == aID) {
      return childDoc;
    }
  }

  return nullptr;
}

void DocAccessibleWrap::DoInitialUpdate() {
  DocAccessible::DoInitialUpdate();
  CacheViewport(true);
}

nsresult DocAccessibleWrap::HandleAccEvent(AccEvent* aEvent) {
  switch (aEvent->GetEventType()) {
    case nsIAccessibleEvent::EVENT_SCROLLING_END:
      CacheViewport(false);
      break;
    case nsIAccessibleEvent::EVENT_SCROLLING:
      UpdateFocusPathBounds();
      break;
    default:
      break;
  }

  return DocAccessible::HandleAccEvent(aEvent);
}

void DocAccessibleWrap::CacheViewportCallback(nsITimer* aTimer,
                                              void* aDocAccParam) {
  RefPtr<DocAccessibleWrap> docAcc(
      dont_AddRef(reinterpret_cast<DocAccessibleWrap*>(aDocAccParam)));
  if (!docAcc || docAcc->HasShutdown()) {
    return;
  }

  PresShell* presShell = docAcc->PresShellPtr();
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return;
  }

  nsTArray<nsIFrame*> frames;
  nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
  nsRect scrollPort = sf ? sf->GetScrollPortRect() : rootFrame->GetRect();

  nsLayoutUtils::GetFramesForArea(
      RelativeTo{presShell->GetRootFrame()}, scrollPort, frames,
      nsLayoutUtils::FrameForPointOption::OnlyVisible);
  AccessibleHashtable inViewAccs;
  for (size_t i = 0; i < frames.Length(); i++) {
    nsIContent* content = frames.ElementAt(i)->GetContent();
    if (!content) {
      continue;
    }

    Accessible* visibleAcc = docAcc->GetAccessibleOrContainer(content);
    if (!visibleAcc) {
      continue;
    }

    for (Accessible* acc = visibleAcc; acc && acc != docAcc->Parent();
         acc = acc->Parent()) {
      if (inViewAccs.Contains(acc->UniqueID())) {
        break;
      }
      inViewAccs.Put(acc->UniqueID(), RefPtr{acc});
    }
  }

  if (IPCAccessibilityActive()) {
    DocAccessibleChild* ipcDoc = docAcc->IPCDoc();
    nsTArray<BatchData> cacheData(inViewAccs.Count());
    for (auto iter = inViewAccs.Iter(); !iter.Done(); iter.Next()) {
      Accessible* accessible = iter.Data();
      nsAutoString name;
      accessible->Name(name);
      nsAutoString textValue;
      accessible->Value(textValue);
      nsAutoString nodeID;
      static_cast<AccessibleWrap*>(accessible)->WrapperDOMNodeID(nodeID);
      nsAutoString description;
      accessible->Description(description);

      cacheData.AppendElement(BatchData(
          accessible->Document()->IPCDoc(), UNIQUE_ID(accessible),
          accessible->State(), accessible->Bounds(), accessible->ActionCount(),
          name, textValue, nodeID, description, UnspecifiedNaN<double>(),
          UnspecifiedNaN<double>(), UnspecifiedNaN<double>(),
          UnspecifiedNaN<double>(), nsTArray<Attribute>()));
    }

    ipcDoc->SendBatch(eBatch_Viewport, cacheData);
  } else if (SessionAccessibility* sessionAcc =
                 SessionAccessibility::GetInstanceFor(docAcc)) {
    nsTArray<AccessibleWrap*> accessibles(inViewAccs.Count());
    for (auto iter = inViewAccs.Iter(); !iter.Done(); iter.Next()) {
      accessibles.AppendElement(
          static_cast<AccessibleWrap*>(iter.Data().get()));
    }

    sessionAcc->ReplaceViewportCache(accessibles);
  }

  if (docAcc->mCachePivotBoundaries) {
    a11y::Pivot pivot(docAcc);
    TraversalRule rule(java::SessionAccessibility::HTML_GRANULARITY_DEFAULT);
    Accessible* first = pivot.First(rule);
    Accessible* last = pivot.Last(rule);

    // If first/last are null, pass the root document as pivot boundary.
    if (IPCAccessibilityActive()) {
      DocAccessibleChild* ipcDoc = docAcc->IPCDoc();
      ipcDoc->GetPlatformExtension()->SendSetPivotBoundaries(
          first ? first->Document()->IPCDoc() : ipcDoc, UNIQUE_ID(first),
          last ? last->Document()->IPCDoc() : ipcDoc, UNIQUE_ID(last));
    } else if (SessionAccessibility* sessionAcc =
                   SessionAccessibility::GetInstanceFor(docAcc)) {
      sessionAcc->UpdateAccessibleFocusBoundaries(
          first ? static_cast<AccessibleWrap*>(first) : docAcc,
          last ? static_cast<AccessibleWrap*>(last) : docAcc);
    }

    docAcc->mCachePivotBoundaries = false;
  }

  if (docAcc->mCacheRefreshTimer) {
    docAcc->mCacheRefreshTimer = nullptr;
  }
}

void DocAccessibleWrap::CacheViewport(bool aCachePivotBoundaries) {
  mCachePivotBoundaries |= aCachePivotBoundaries;
  if (VirtualViewID() == kNoID && !mCacheRefreshTimer) {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mCacheRefreshTimer),
                                CacheViewportCallback, this,
                                kCacheRefreshInterval, nsITimer::TYPE_ONE_SHOT,
                                "a11y::DocAccessibleWrap::CacheViewport");
    if (mCacheRefreshTimer) {
      NS_ADDREF_THIS();  // Kung fu death grip
    }
  }
}

DocAccessibleWrap* DocAccessibleWrap::GetTopLevelContentDoc(
    AccessibleWrap* aAccessible) {
  DocAccessibleWrap* doc =
      static_cast<DocAccessibleWrap*>(aAccessible->Document());
  while (doc && doc->VirtualViewID() != kNoID) {
    doc = static_cast<DocAccessibleWrap*>(doc->ParentDocument());
  }

  return doc;
}

void DocAccessibleWrap::CacheFocusPath(AccessibleWrap* aAccessible) {
  mFocusPath.Clear();
  if (IPCAccessibilityActive()) {
    DocAccessibleChild* ipcDoc = IPCDoc();
    nsTArray<BatchData> cacheData;
    for (AccessibleWrap* acc = aAccessible; acc && acc != this->Parent();
         acc = static_cast<AccessibleWrap*>(acc->Parent())) {
      nsAutoString name;
      acc->Name(name);
      nsAutoString textValue;
      acc->Value(textValue);
      nsAutoString nodeID;
      acc->WrapperDOMNodeID(nodeID);
      nsAutoString description;
      acc->Description(description);
      nsCOMPtr<nsIPersistentProperties> props = acc->Attributes();
      nsTArray<Attribute> attributes;
      nsAccUtils::PersistentPropertiesToArray(props, &attributes);
      cacheData.AppendElement(
          BatchData(acc->Document()->IPCDoc(), UNIQUE_ID(acc), acc->State(),
                    acc->Bounds(), acc->ActionCount(), name, textValue, nodeID,
                    description, acc->CurValue(), acc->MinValue(),
                    acc->MaxValue(), acc->Step(), attributes));
      mFocusPath.Put(acc->UniqueID(), RefPtr{acc});
    }

    ipcDoc->SendBatch(eBatch_FocusPath, cacheData);
  } else if (SessionAccessibility* sessionAcc =
                 SessionAccessibility::GetInstanceFor(this)) {
    nsTArray<AccessibleWrap*> accessibles;
    for (AccessibleWrap* acc = aAccessible; acc && acc != this->Parent();
         acc = static_cast<AccessibleWrap*>(acc->Parent())) {
      accessibles.AppendElement(acc);
      mFocusPath.Put(acc->UniqueID(), RefPtr{acc});
    }

    sessionAcc->ReplaceFocusPathCache(accessibles);
  }
}

void DocAccessibleWrap::UpdateFocusPathBounds() {
  if (!mFocusPath.Count()) {
    return;
  }

  if (IPCAccessibilityActive()) {
    DocAccessibleChild* ipcDoc = IPCDoc();
    nsTArray<BatchData> boundsData(mFocusPath.Count());
    for (auto iter = mFocusPath.Iter(); !iter.Done(); iter.Next()) {
      Accessible* accessible = iter.Data();
      if (!accessible || accessible->IsDefunct()) {
        MOZ_ASSERT_UNREACHABLE("Focus path cached accessible is gone.");
        continue;
      }

      boundsData.AppendElement(
          BatchData(accessible->Document()->IPCDoc(), UNIQUE_ID(accessible), 0,
                    accessible->Bounds(), 0, nsString(), nsString(), nsString(),
                    nsString(), UnspecifiedNaN<double>(),
                    UnspecifiedNaN<double>(), UnspecifiedNaN<double>(),
                    UnspecifiedNaN<double>(), nsTArray<Attribute>()));
    }

    ipcDoc->SendBatch(eBatch_BoundsUpdate, boundsData);
  } else if (SessionAccessibility* sessionAcc =
                 SessionAccessibility::GetInstanceFor(this)) {
    nsTArray<AccessibleWrap*> accessibles(mFocusPath.Count());
    for (auto iter = mFocusPath.Iter(); !iter.Done(); iter.Next()) {
      Accessible* accessible = iter.Data();
      if (!accessible || accessible->IsDefunct()) {
        MOZ_ASSERT_UNREACHABLE("Focus path cached accessible is gone.");
        continue;
      }

      accessibles.AppendElement(static_cast<AccessibleWrap*>(accessible));
    }

    sessionAcc->UpdateCachedBounds(accessibles);
  }
}

#undef UNIQUE_ID