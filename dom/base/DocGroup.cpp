/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/Telemetry.h"
#include "nsIDocShell.h"
#include "nsDOMMutationObserver.h"

namespace mozilla {
namespace dom {

AutoTArray<RefPtr<DocGroup>, 2>* DocGroup::sPendingDocGroups = nullptr;

/* static */ nsresult
DocGroup::GetKey(nsIPrincipal* aPrincipal, nsACString& aKey)
{
  // Use GetBaseDomain() to handle things like file URIs, IP address URIs,
  // etc. correctly.
  nsresult rv = aPrincipal->GetBaseDomain(aKey);
  if (NS_FAILED(rv)) {
    // We don't really know what to do here.  But we should be conservative,
    // otherwise it would be possible to reorder two events incorrectly in the
    // future if we interrupt at the DocGroup level, so to be safe, use an
    // empty string to classify all such documents as belonging to the same
    // DocGroup.
    aKey.Truncate();
  }

  return rv;
}

void
DocGroup::RemoveDocument(nsIDocument* aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDocuments.Contains(aDocument));
  mDocuments.RemoveElement(aDocument);
}

DocGroup::DocGroup(TabGroup* aTabGroup, const nsACString& aKey)
  : mKey(aKey), mTabGroup(aTabGroup)
{
  // This method does not add itself to mTabGroup->mDocGroups as the caller does it for us.
}

DocGroup::~DocGroup()
{
  MOZ_ASSERT(mDocuments.IsEmpty());
  if (!NS_IsMainThread()) {
    nsIEventTarget* target = EventTargetFor(TaskCategory::Other);
    NS_ProxyRelease("DocGroup::mReactionsStack", target, mReactionsStack.forget());
  }

  mTabGroup->mDocGroups.RemoveEntry(mKey);
}

nsresult
DocGroup::Dispatch(TaskCategory aCategory,
                   already_AddRefed<nsIRunnable>&& aRunnable)
{
  return mTabGroup->Dispatch(aCategory, Move(aRunnable));
}

nsISerialEventTarget*
DocGroup::EventTargetFor(TaskCategory aCategory) const
{
  return mTabGroup->EventTargetFor(aCategory);
}

AbstractThread*
DocGroup::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return mTabGroup->AbstractMainThreadFor(aCategory);
}

bool*
DocGroup::GetValidAccessPtr()
{
  return mTabGroup->GetValidAccessPtr();
}

void
DocGroup::SignalSlotChange(const HTMLSlotElement* aSlot)
{
  if (mSignalSlotList.Contains(aSlot)) {
    return;
  }

  mSignalSlotList.AppendElement(const_cast<HTMLSlotElement*>(aSlot));

  if (!sPendingDocGroups) {
    // Queue a mutation observer compound microtask.
    nsDOMMutationObserver::QueueMutationObserverMicroTask();
    sPendingDocGroups = new AutoTArray<RefPtr<DocGroup>, 2>;
  }

  sPendingDocGroups->AppendElement(this);
}

}
}
