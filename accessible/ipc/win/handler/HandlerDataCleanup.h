/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HandlerDataCleanup_h
#define mozilla_a11y_HandlerDataCleanup_h

#include <oleauto.h>

namespace mozilla {
namespace a11y {

inline void ReleaseStaticIA2DataInterfaces(StaticIA2Data& aData) {
  // Only interfaces of the proxied object wrapped by this handler should be
  // released here, never other objects!
  // For example, if StaticIA2Data were to include accParent in future,
  // that must not be released here.
  if (aData.mIA2) {
    aData.mIA2->Release();
  }
  if (aData.mIAHypertext) {
    aData.mIAHypertext->Release();
  }
  if (aData.mIAHyperlink) {
    aData.mIAHyperlink->Release();
  }
  if (aData.mIATable) {
    aData.mIATable->Release();
  }
  if (aData.mIATable2) {
    aData.mIATable2->Release();
  }
  if (aData.mIATableCell) {
    aData.mIATableCell->Release();
  }
}

/**
 * Pass true for aMarshaledByCom  if this struct was directly marshaled as an
 * out parameter of a COM method, currently only IGeckoBackChannel::Refresh.
 */
inline void CleanupDynamicIA2Data(DynamicIA2Data& aData,
                                  bool aMarshaledByCom = false) {
  // If freeing generic memory returned to the client, you *must* use freeMem,
  // not CoTaskMemFree!
  auto freeMem = [aMarshaledByCom](void* aMem) {
    if (aMarshaledByCom) {
      ::CoTaskMemFree(aMem);
    } else {
      ::midl_user_free(aMem);
    }
  };

  ::VariantClear(&aData.mRole);
  if (aData.mKeyboardShortcut) {
    ::SysFreeString(aData.mKeyboardShortcut);
  }
  if (aData.mName) {
    ::SysFreeString(aData.mName);
  }
  if (aData.mDescription) {
    ::SysFreeString(aData.mDescription);
  }
  if (aData.mDefaultAction) {
    ::SysFreeString(aData.mDefaultAction);
  }
  if (aData.mValue) {
    ::SysFreeString(aData.mValue);
  }
  if (aData.mAttributes) {
    ::SysFreeString(aData.mAttributes);
  }
  if (aData.mIA2Locale.language) {
    ::SysFreeString(aData.mIA2Locale.language);
  }
  if (aData.mIA2Locale.country) {
    ::SysFreeString(aData.mIA2Locale.country);
  }
  if (aData.mIA2Locale.variant) {
    ::SysFreeString(aData.mIA2Locale.variant);
  }
  if (aData.mRowHeaderCellIds) {
    freeMem(aData.mRowHeaderCellIds);
  }
  if (aData.mColumnHeaderCellIds) {
    freeMem(aData.mColumnHeaderCellIds);
  }
}

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_HandlerDataCleanup_h
