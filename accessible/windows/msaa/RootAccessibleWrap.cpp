/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "Compatibility.h"
#include "nsCoreUtils.h"
#include "nsWinUtils.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Constructor/destructor

RootAccessibleWrap::RootAccessibleWrap(nsIDocument* aDocument,
                                       nsIPresShell* aPresShell)
  : RootAccessible(aDocument, aPresShell)
  , mOuter(&mInternalUnknown)
{
}

RootAccessibleWrap::~RootAccessibleWrap()
{
}

////////////////////////////////////////////////////////////////////////////////
// Aggregated IUnknown
HRESULT
RootAccessibleWrap::InternalQueryInterface(REFIID aIid, void** aOutInterface)
{
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  // InternalQueryInterface should always return its internal unknown
  // when queried for IID_IUnknown...
  if (aIid == IID_IUnknown) {
    RefPtr<IUnknown> punk(&mInternalUnknown);
    punk.forget(aOutInterface);
    return S_OK;
  }

  // ...Otherwise we pass through to the base COM implementation of
  // QueryInterface which is provided by DocAccessibleWrap.
  return DocAccessibleWrap::QueryInterface(aIid, aOutInterface);
}

ULONG
RootAccessibleWrap::InternalAddRef()
{
  return DocAccessible::AddRef();
}

ULONG
RootAccessibleWrap::InternalRelease()
{
  return DocAccessible::Release();
}

already_AddRefed<IUnknown>
RootAccessibleWrap::Aggregate(IUnknown* aOuter)
{
  MOZ_ASSERT(mOuter && (mOuter == &mInternalUnknown || mOuter == aOuter || !aOuter));
  if (!aOuter) {
    // If there is no aOuter then we should always set mOuter to
    // mInternalUnknown. This is standard COM aggregation stuff.
    mOuter = &mInternalUnknown;
    return nullptr;
  }

  mOuter = aOuter;
  return GetInternalUnknown();
}

already_AddRefed<IUnknown>
RootAccessibleWrap::GetInternalUnknown()
{
  RefPtr<IUnknown> result(&mInternalUnknown);
  return result.forget();
}

////////////////////////////////////////////////////////////////////////////////
// RootAccessible

void
RootAccessibleWrap::DocumentActivated(DocAccessible* aDocument)
{
  if (Compatibility::IsDolphin() &&
      nsCoreUtils::IsTabDocument(aDocument->DocumentNode())) {
    uint32_t count = mChildDocuments.Length();
    for (uint32_t idx = 0; idx < count; idx++) {
      DocAccessible* childDoc = mChildDocuments[idx];
      HWND childDocHWND = static_cast<HWND>(childDoc->GetNativeWindow());
      if (childDoc != aDocument)
        nsWinUtils::HideNativeWindow(childDocHWND);
      else
        nsWinUtils::ShowNativeWindow(childDocHWND);
    }
  }
}
