/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uiaRawElmProvider.h"

#include "AccAttributes.h"
#include "AccessibleWrap.h"
#include "ARIAMap.h"
#include "LocalAccessible-inl.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "MsaaAccessible.h"
#include "nsTextEquivUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// uiaRawElmProvider
////////////////////////////////////////////////////////////////////////////////

Accessible* uiaRawElmProvider::Acc() {
  return static_cast<MsaaAccessible*>(this)->Acc();
}

// IUnknown

// Because uiaRawElmProvider inherits multiple COM interfaces (and thus multiple
// IUnknowns), we need to explicitly implement AddRef and Release to make
// our QueryInterface implementation (IMPL_IUNKNOWN2) happy.
ULONG STDMETHODCALLTYPE uiaRawElmProvider::AddRef() {
  return static_cast<MsaaAccessible*>(this)->AddRef();
}

ULONG STDMETHODCALLTYPE uiaRawElmProvider::Release() {
  return static_cast<MsaaAccessible*>(this)->Release();
}

IMPL_IUNKNOWN2(uiaRawElmProvider, IAccessibleEx, IRawElementProviderSimple)

////////////////////////////////////////////////////////////////////////////////
// IAccessibleEx

STDMETHODIMP
uiaRawElmProvider::GetObjectForChild(
    long aIdChild, __RPC__deref_out_opt IAccessibleEx** aAccEx) {
  if (!aAccEx) return E_INVALIDARG;

  *aAccEx = nullptr;

  return Acc() ? S_OK : CO_E_OBJNOTCONNECTED;
}

STDMETHODIMP
uiaRawElmProvider::GetIAccessiblePair(__RPC__deref_out_opt IAccessible** aAcc,
                                      __RPC__out long* aIdChild) {
  if (!aAcc || !aIdChild) return E_INVALIDARG;

  *aAcc = nullptr;
  *aIdChild = 0;

  if (!Acc()) {
    return CO_E_OBJNOTCONNECTED;
  }

  *aIdChild = CHILDID_SELF;
  RefPtr<IAccessible> copy = static_cast<MsaaAccessible*>(this);
  copy.forget(aAcc);

  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::GetRuntimeId(__RPC__deref_out_opt SAFEARRAY** aRuntimeIds) {
  if (!aRuntimeIds) return E_INVALIDARG;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  int ids[] = {UiaAppendRuntimeId, MsaaAccessible::GetChildIDFor(acc)};
  *aRuntimeIds = SafeArrayCreateVector(VT_I4, 0, 2);
  if (!*aRuntimeIds) return E_OUTOFMEMORY;

  for (LONG i = 0; i < (LONG)ArrayLength(ids); i++)
    SafeArrayPutElement(*aRuntimeIds, &i, (void*)&(ids[i]));

  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::ConvertReturnedElement(
    __RPC__in_opt IRawElementProviderSimple* aRawElmProvider,
    __RPC__deref_out_opt IAccessibleEx** aAccEx) {
  if (!aRawElmProvider || !aAccEx) return E_INVALIDARG;

  *aAccEx = nullptr;

  void* instancePtr = nullptr;
  HRESULT hr = aRawElmProvider->QueryInterface(IID_IAccessibleEx, &instancePtr);
  if (SUCCEEDED(hr)) *aAccEx = static_cast<IAccessibleEx*>(instancePtr);

  return hr;
}

////////////////////////////////////////////////////////////////////////////////
// IRawElementProviderSimple

STDMETHODIMP
uiaRawElmProvider::get_ProviderOptions(
    __RPC__out enum ProviderOptions* aOptions) {
  if (!aOptions) return E_INVALIDARG;

  *aOptions = ProviderOptions_ServerSideProvider |
              ProviderOptions_UseComThreading |
              ProviderOptions_HasNativeIAccessible;
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::GetPatternProvider(
    PATTERNID aPatternId, __RPC__deref_out_opt IUnknown** aPatternProvider) {
  if (!aPatternProvider) return E_INVALIDARG;

  *aPatternProvider = nullptr;
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::GetPropertyValue(PROPERTYID aPropertyId,
                                    __RPC__out VARIANT* aPropertyValue) {
  if (!aPropertyValue) return E_INVALIDARG;

  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  LocalAccessible* localAcc = acc->AsLocal();

  aPropertyValue->vt = VT_EMPTY;

  switch (aPropertyId) {
    // Accelerator Key / shortcut.
    case UIA_AcceleratorKeyPropertyId: {
      if (!localAcc) {
        // KeyboardShortcut is only currently relevant for LocalAccessible.
        break;
      }
      nsAutoString keyString;

      localAcc->KeyboardShortcut().ToString(keyString);

      if (!keyString.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(keyString.get());
        return S_OK;
      }

      break;
    }

    // Access Key / mneumonic.
    case UIA_AccessKeyPropertyId: {
      nsAutoString keyString;

      acc->AccessKey().ToString(keyString);

      if (!keyString.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(keyString.get());
        return S_OK;
      }

      break;
    }

    // ARIA Role / shortcut
    case UIA_AriaRolePropertyId: {
      nsAutoString xmlRoles;

      RefPtr<AccAttributes> attributes = acc->Attributes();
      attributes->GetAttribute(nsGkAtoms::xmlroles, xmlRoles);

      if (!xmlRoles.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(xmlRoles.get());
        return S_OK;
      }

      break;
    }

    // ARIA Properties
    case UIA_AriaPropertiesPropertyId: {
      if (!localAcc) {
        // XXX Implement a unified version of this. We don't cache explicit
        // values for many ARIA attributes in RemoteAccessible; e.g. we use the
        // checked state rather than caching aria-checked:true. Thus, a unified
        // implementation will need to work with State(), etc.
        break;
      }
      nsAutoString ariaProperties;

      aria::AttrIterator attribIter(localAcc->GetContent());
      while (attribIter.Next()) {
        nsAutoString attribName, attribValue;
        nsAutoString value;
        attribIter.AttrName()->ToString(attribName);
        attribIter.AttrValue(attribValue);
        if (StringBeginsWith(attribName, u"aria-"_ns)) {
          // Found 'aria-'
          attribName.ReplaceLiteral(0, 5, u"");
        }

        ariaProperties.Append(attribName);
        ariaProperties.Append('=');
        ariaProperties.Append(attribValue);
        ariaProperties.Append(';');
      }

      if (!ariaProperties.IsEmpty()) {
        // remove last delimiter:
        ariaProperties.Truncate(ariaProperties.Length() - 1);
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(ariaProperties.get());
        return S_OK;
      }

      break;
    }

    case UIA_IsControlElementPropertyId:
    case UIA_IsContentElementPropertyId:
      aPropertyValue->vt = VT_BOOL;
      aPropertyValue->boolVal = IsControl() ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
  }

  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::get_HostRawElementProvider(
    __RPC__deref_out_opt IRawElementProviderSimple** aRawElmProvider) {
  if (!aRawElmProvider) return E_INVALIDARG;

  // This method is not used with IAccessibleEx implementations.
  *aRawElmProvider = nullptr;
  return S_OK;
}

// Private methods

bool uiaRawElmProvider::IsControl() {
  // UIA provides multiple views of the tree: raw, control and content. The
  // control and content views should only contain elements which a user cares
  // about when navigating.
  Accessible* acc = Acc();
  MOZ_ASSERT(acc);
  if (acc->IsTextLeaf()) {
    // If an ancestor control allows the name to be generated from content, do
    // not expose this text leaf as a control. Otherwise, the user will see the
    // text twice: once as the label of the control and once for the text leaf.
    for (Accessible* ancestor = acc->Parent(); ancestor && !ancestor->IsDoc();
         ancestor = ancestor->Parent()) {
      if (nsTextEquivUtils::HasNameRule(ancestor, eNameFromSubtreeRule)) {
        return false;
      }
    }
    return true;
  }

  if (acc->HasNumericValue() || acc->ActionCount() > 0) {
    return true;
  }
  uint64_t state = acc->State();
  if (state & states::FOCUSABLE) {
    return true;
  }
  if (state & states::EDITABLE) {
    Accessible* parent = acc->Parent();
    if (parent && !(parent->State() & states::EDITABLE)) {
      // This is the root of a rich editable control.
      return true;
    }
  }

  // Don't treat generic or text containers as controls unless they have a name
  // or description.
  switch (acc->Role()) {
    case roles::EMPHASIS:
    case roles::MARK:
    case roles::PARAGRAPH:
    case roles::SECTION:
    case roles::STRONG:
    case roles::SUBSCRIPT:
    case roles::SUPERSCRIPT:
    case roles::TEXT:
    case roles::TEXT_CONTAINER: {
      if (!acc->NameIsEmpty()) {
        return true;
      }
      nsAutoString text;
      acc->Description(text);
      if (!text.IsEmpty()) {
        return true;
      }
      return false;
    }
    default:
      break;
  }

  return true;
}
