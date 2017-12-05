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
  // This check will never work with e10s enabled, in other words, as of
  // Firefox 57.
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

STDMETHODIMP
RootAccessibleWrap::accNavigate(
      /* [in] */ long navDir,
      /* [optional][in] */ VARIANT varStart,
      /* [retval][out] */ VARIANT __RPC_FAR *pvarEndUpAt)
{
  // Special handling for NAVRELATION_EMBEDS.
  // When we only have a single process, this can be handled the same way as
  // any other relation.
  // However, for multi process, the normal relation mechanism doesn't work
  // because it can't handle remote objects.
  if (navDir != NAVRELATION_EMBEDS ||
      varStart.vt != VT_I4  || varStart.lVal != CHILDID_SELF) {
    // We only handle EMBEDS on the root here.
    // Forward to the base implementation.
    return DocAccessibleWrap::accNavigate(navDir, varStart, pvarEndUpAt);
  }

  if (!pvarEndUpAt) {
    return E_INVALIDARG;
  }
  if (IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  Accessible* target = nullptr;
  // Get the document in the active tab.
  ProxyAccessible* docProxy = GetPrimaryRemoteTopLevelContentDoc();
  if (docProxy) {
    target = WrapperFor(docProxy);
  } else {
    // The base implementation could handle this, but we may as well
    // just handle it here.
    Relation rel = RelationByType(RelationType::EMBEDS);
    target = rel.Next();
  }

  if (!target) {
    return E_FAIL;
  }

  VariantInit(pvarEndUpAt);
  pvarEndUpAt->pdispVal = NativeAccessible(target);
  pvarEndUpAt->vt = VT_DISPATCH;
  return S_OK;
}

STDMETHODIMP
RootAccessibleWrap::get_accFocus(
      /* [retval][out] */ VARIANT __RPC_FAR *pvarChild)
{
  HRESULT hr = DocAccessibleWrap::get_accFocus(pvarChild);
  if (FAILED(hr) || pvarChild->vt != VT_EMPTY) {
    // We got a definite result (either failure or an accessible).
    return hr;
  }

  // The base implementation reported no focus.
  // Focus might be in a remote document.
  // (The base implementation can't handle this.)
  // Get the document in the active tab.
  ProxyAccessible* docProxy = GetPrimaryRemoteTopLevelContentDoc();
  if (!docProxy) {
    return hr;
  }
  Accessible* docAcc = WrapperFor(docProxy);
  if (!docAcc) {
    return E_FAIL;
  }
  RefPtr<IDispatch> docDisp = NativeAccessible(docAcc);
  if (!docDisp) {
    return E_FAIL;
  }
  RefPtr<IAccessible> docIa;
  hr = docDisp->QueryInterface(IID_IAccessible, (void**)getter_AddRefs(docIa));
  MOZ_ASSERT(SUCCEEDED(hr));
  MOZ_ASSERT(docIa);

  // Ask this document for its focused descendant.
  // We return this as is to the client except for CHILDID_SELF (see below).
  hr = docIa->get_accFocus(pvarChild);
  if (FAILED(hr)) {
    return hr;
  }

  if (pvarChild->vt == VT_I4 && pvarChild->lVal == CHILDID_SELF) {
    // The document itself has focus.
    // We're handling a call to accFocus on the root accessible,
    // so replace CHILDID_SELF with the document accessible.
    pvarChild->vt = VT_DISPATCH;
    docDisp.forget(&pvarChild->pdispVal);
  }

  return S_OK;
}
