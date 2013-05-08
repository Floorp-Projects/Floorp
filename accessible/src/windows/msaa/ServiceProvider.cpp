/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceProvider.h"

#include "ApplicationAccessibleWrap.h"
#include "Compatibility.h"
#include "DocAccessible.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "uiaRawElmProvider.h"

#include "mozilla/Preferences.h"
#include "nsIDocShell.h"

#include "ISimpleDOMNode_i.c"

namespace mozilla {
namespace a11y {

IMPL_IUNKNOWN_QUERY_HEAD(ServiceProvider)
  IMPL_IUNKNOWN_QUERY_IFACE(IServiceProvider)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mAccessible)


////////////////////////////////////////////////////////////////////////////////
// IServiceProvider

STDMETHODIMP
ServiceProvider::QueryService(REFGUID aGuidService, REFIID aIID,
                              void** aInstancePtr)
{
  if (!aInstancePtr)
    return E_INVALIDARG;

  *aInstancePtr = nullptr;

  // UIA IAccessibleEx
  if (aGuidService == IID_IAccessibleEx &&
      Preferences::GetBool("accessibility.uia.enable")) {
    uiaRawElmProvider* accEx = new uiaRawElmProvider(mAccessible);
    HRESULT hr = accEx->QueryInterface(aIID, aInstancePtr);
    if (FAILED(hr))
      delete accEx;

    return hr;
  }

  // Provide a special service ID for getting the accessible for the browser tab
  // document that contains this accessible object. If this accessible object
  // is not inside a browser tab then the service fails with E_NOINTERFACE.
  // A use case for this is for screen readers that need to switch context or
  // 'virtual buffer' when focus moves from one browser tab area to another.
  static const GUID SID_IAccessibleContentDocument =
    { 0xa5d8e1f3,0x3571,0x4d8f,{0x95,0x21,0x07,0xed,0x28,0xfb,0x07,0x2e} };
  if (aGuidService == SID_IAccessibleContentDocument) {
    if (aIID != IID_IAccessible)
      return E_NOINTERFACE;

    nsCOMPtr<nsIDocShell> docShell =
      nsCoreUtils::GetDocShellFor(mAccessible->GetNode());
    if (!docShell)
      return E_UNEXPECTED;

    // Walk up the parent chain without crossing the boundary at which item
    // types change, preventing us from walking up out of tab content.
    nsCOMPtr<nsIDocShellTreeItem> root;
    docShell->GetSameTypeRootTreeItem(getter_AddRefs(root));
    if (!root)
      return E_UNEXPECTED;


    // If the item type is typeContent, we assume we are in browser tab content.
    // Note this includes content such as about:addons, for consistency.
    int32_t itemType;
    root->GetItemType(&itemType);
    if (itemType != nsIDocShellTreeItem::typeContent)
      return E_NOINTERFACE;

    // Make sure this is a document.
    DocAccessible* docAcc = nsAccUtils::GetDocAccessibleFor(root);
    if (!docAcc)
      return E_UNEXPECTED;

    *aInstancePtr = static_cast<IAccessible*>(docAcc);

    (reinterpret_cast<IUnknown*>(*aInstancePtr))->AddRef();
    return S_OK;
  }

  // Can get to IAccessibleApplication from any node via QS
  if (aGuidService == IID_IAccessibleApplication ||
      (Compatibility::IsJAWS() && aIID == IID_IAccessibleApplication)) {
    ApplicationAccessibleWrap* applicationAcc =
      static_cast<ApplicationAccessibleWrap*>(ApplicationAcc());
    if (!applicationAcc)
      return E_NOINTERFACE;

    return applicationAcc->QueryInterface(aIID, aInstancePtr);
  }

  static const GUID IID_SimpleDOMDeprecated =
    { 0x0c539790,0x12e4,0x11cf,{0xb6,0x61,0x00,0xaa,0x00,0x4c,0xd6,0xd8} };
  if (aGuidService == IID_ISimpleDOMNode ||
      aGuidService == IID_SimpleDOMDeprecated ||
      aGuidService == IID_IAccessible ||  aGuidService == IID_IAccessible2)
    return mAccessible->QueryInterface(aIID, aInstancePtr);

  return E_INVALIDARG;
}

} // namespace a11y
} // namespace mozilla
