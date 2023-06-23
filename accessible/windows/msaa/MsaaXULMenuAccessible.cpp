/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MsaaXULMenuAccessible.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "mozilla/dom/Element.h"
#include "mozilla/a11y/AccessibleWrap.h"
#include "LocalAccessible-inl.h"
#include "nsGkAtoms.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// MsaaXULMenuitemAccessible
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP
MsaaXULMenuitemAccessible::get_accKeyboardShortcut(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszKeyboardShortcut) {
  if (!pszKeyboardShortcut) return E_INVALIDARG;
  *pszKeyboardShortcut = nullptr;

  if (varChild.vt != VT_I4 || varChild.lVal != CHILDID_SELF) {
    return MsaaAccessible::get_accKeyboardShortcut(varChild,
                                                   pszKeyboardShortcut);
  }

  LocalAccessible* acc = LocalAcc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  KeyBinding keyBinding = acc->AccessKey();
  if (keyBinding.IsEmpty()) {
    return S_FALSE;
  }

  nsAutoString shortcut;
  keyBinding.ToString(shortcut);

  *pszKeyboardShortcut = ::SysAllocStringLen(shortcut.get(), shortcut.Length());
  return *pszKeyboardShortcut ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
MsaaXULMenuitemAccessible::get_accName(
    /* [optional][in] */ VARIANT varChild,
    /* [retval][out] */ BSTR __RPC_FAR* pszName) {
  if (varChild.vt != VT_I4 || varChild.lVal != CHILDID_SELF) {
    return MsaaAccessible::get_accName(varChild, pszName);
  }
  if (!pszName) {
    return E_INVALIDARG;
  }
  LocalAccessible* acc = LocalAcc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  *pszName = nullptr;
  nsAutoString name;
  acc->Name(name);
  if (name.IsVoid()) {
    return S_FALSE;
  }

  nsAutoString accel;
  if (dom::Element* el = acc->Elm()) {
    el->GetAttr(nsGkAtoms::acceltext, accel);
  }
  if (!accel.IsEmpty()) {
    name += u"\t"_ns + accel;
  }

  *pszName = ::SysAllocStringLen(name.get(), name.Length());
  if (!*pszName) {
    return E_OUTOFMEMORY;
  }
  return S_OK;
}
