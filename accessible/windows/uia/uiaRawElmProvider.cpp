/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uiaRawElmProvider.h"

#include <comdef.h>
#include <uiautomationcoreapi.h>

#include "AccAttributes.h"
#include "AccessibleWrap.h"
#include "ARIAMap.h"
#include "LocalAccessible-inl.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "MsaaAccessible.h"
#include "MsaaRootAccessible.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsTextEquivUtils.h"
#include "RootAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// uiaRawElmProvider
////////////////////////////////////////////////////////////////////////////////

Accessible* uiaRawElmProvider::Acc() const {
  return static_cast<const MsaaAccessible*>(this)->Acc();
}

/* static */
void uiaRawElmProvider::RaiseUiaEventForGeckoEvent(Accessible* aAcc,
                                                   uint32_t aGeckoEvent) {
  if (!StaticPrefs::accessibility_uia_enable()) {
    return;
  }
  auto* uia = MsaaAccessible::GetFrom(aAcc);
  if (!uia) {
    return;
  }
  PROPERTYID property = 0;
  switch (aGeckoEvent) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      ::UiaRaiseAutomationEvent(uia, UIA_AutomationFocusChangedEventId);
      return;
    case nsIAccessibleEvent::EVENT_NAME_CHANGE:
      property = UIA_NamePropertyId;
      break;
  }
  // Don't pointlessly query the property value if no UIA clients are listening.
  if (property && ::UiaClientsAreListening()) {
    // We can't get the old value. Thankfully, clients don't seem to need it.
    _variant_t oldVal;
    _variant_t newVal;
    uia->GetPropertyValue(property, &newVal);
    ::UiaRaiseAutomationPropertyChangedEvent(uia, property, oldVal, newVal);
  }
}

// IUnknown

STDMETHODIMP
uiaRawElmProvider::QueryInterface(REFIID aIid, void** aInterface) {
  *aInterface = nullptr;
  if (aIid == IID_IAccessibleEx) {
    *aInterface = static_cast<IAccessibleEx*>(this);
  } else if (aIid == IID_IRawElementProviderSimple) {
    *aInterface = static_cast<IRawElementProviderSimple*>(this);
  } else if (aIid == IID_IRawElementProviderFragment) {
    *aInterface = static_cast<IRawElementProviderFragment*>(this);
  } else {
    return E_NOINTERFACE;
  }
  MOZ_ASSERT(*aInterface);
  static_cast<MsaaAccessible*>(this)->AddRef();
  return S_OK;
}

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

  *aOptions = static_cast<enum ProviderOptions>(
      ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading |
      ProviderOptions_HasNativeIAccessible);
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

    case UIA_AutomationIdPropertyId: {
      nsAutoString id;
      acc->DOMNodeID(id);
      if (!id.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(id.get());
        return S_OK;
      }
      break;
    }

    case UIA_ControlTypePropertyId:
      aPropertyValue->vt = VT_I4;
      aPropertyValue->lVal = GetControlType();
      break;

    case UIA_HasKeyboardFocusPropertyId:
      aPropertyValue->vt = VT_BOOL;
      aPropertyValue->boolVal = VARIANT_FALSE;
      if (auto* focusMgr = FocusMgr()) {
        if (focusMgr->IsFocused(acc)) {
          aPropertyValue->boolVal = VARIANT_TRUE;
        }
      }
      return S_OK;

    case UIA_IsContentElementPropertyId:
    case UIA_IsControlElementPropertyId:
      aPropertyValue->vt = VT_BOOL;
      aPropertyValue->boolVal = IsControl() ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;

    case UIA_IsKeyboardFocusablePropertyId:
      aPropertyValue->vt = VT_BOOL;
      aPropertyValue->boolVal =
          (acc->State() & states::FOCUSABLE) ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;

    case UIA_NamePropertyId: {
      nsAutoString name;
      acc->Name(name);
      if (!name.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(name.get());
        return S_OK;
      }
      break;
    }
  }

  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::get_HostRawElementProvider(
    __RPC__deref_out_opt IRawElementProviderSimple** aRawElmProvider) {
  if (!aRawElmProvider) return E_INVALIDARG;
  *aRawElmProvider = nullptr;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  if (acc->IsRoot()) {
    HWND hwnd = MsaaAccessible::GetHWNDFor(acc);
    return UiaHostProviderFromHwnd(hwnd, aRawElmProvider);
  }
  return S_OK;
}

// IRawElementProviderFragment

STDMETHODIMP
uiaRawElmProvider::Navigate(
    enum NavigateDirection aDirection,
    __RPC__deref_out_opt IRawElementProviderFragment** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  Accessible* target = nullptr;
  switch (aDirection) {
    case NavigateDirection_Parent:
      if (!acc->IsRoot()) {
        target = acc->Parent();
      }
      break;
    case NavigateDirection_NextSibling:
      if (!acc->IsRoot()) {
        target = acc->NextSibling();
      }
      break;
    case NavigateDirection_PreviousSibling:
      if (!acc->IsRoot()) {
        target = acc->PrevSibling();
      }
      break;
    case NavigateDirection_FirstChild:
      if (!nsAccUtils::MustPrune(acc)) {
        target = acc->FirstChild();
      }
      break;
    case NavigateDirection_LastChild:
      if (!nsAccUtils::MustPrune(acc)) {
        target = acc->LastChild();
      }
      break;
    default:
      return E_INVALIDARG;
  }
  RefPtr<IRawElementProviderFragment> fragment =
      MsaaAccessible::GetFrom(target);
  fragment.forget(aRetVal);
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::get_BoundingRectangle(__RPC__out struct UiaRect* aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  LayoutDeviceIntRect rect = acc->Bounds();
  aRetVal->left = rect.X();
  aRetVal->top = rect.Y();
  aRetVal->width = rect.Width();
  aRetVal->height = rect.Height();
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::GetEmbeddedFragmentRoots(
    __RPC__deref_out_opt SAFEARRAY** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::SetFocus() {
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  acc->TakeFocus();
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::get_FragmentRoot(
    __RPC__deref_out_opt IRawElementProviderFragmentRoot** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  LocalAccessible* localAcc = acc->AsLocal();
  if (!localAcc) {
    localAcc = acc->AsRemote()->OuterDocOfRemoteBrowser();
    if (!localAcc) {
      return CO_E_OBJNOTCONNECTED;
    }
  }
  MsaaAccessible* msaa = MsaaAccessible::GetFrom(localAcc->RootAccessible());
  RefPtr<IRawElementProviderFragmentRoot> fragRoot =
      static_cast<MsaaRootAccessible*>(msaa);
  fragRoot.forget(aRetVal);
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

long uiaRawElmProvider::GetControlType() const {
  Accessible* acc = Acc();
  MOZ_ASSERT(acc);
#define ROLE(_geckoRole, stringRole, ariaRole, atkRole, macRole, macSubrole, \
             msaaRole, ia2Role, androidClass, iosIsElement, uiaControlType,  \
             nameRule)                                                       \
  case roles::_geckoRole:                                                    \
    return uiaControlType;                                                   \
    break;
  switch (acc->Role()) {
#include "RoleMap.h"
  }
#undef ROLE
  MOZ_CRASH("Unknown role.");
  return 0;
}
