/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleGeneric.h"

#include "xpcAccessibleDocument.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_INTERFACE_MAP_BEGIN(xpcAccessibleGeneric)
  NS_INTERFACE_MAP_ENTRY(nsIAccessible)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleSelectable,
                                     mSupportedIfaces & eSelectable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleValue,
                                     mSupportedIfaces & eValue)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleHyperLink,
                                     mSupportedIfaces & eHyperLink)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessible)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(xpcAccessibleGeneric)
NS_IMPL_RELEASE(xpcAccessibleGeneric)

xpcAccessibleGeneric::~xpcAccessibleGeneric() {
  if (!mIntl) {
    return;
  }

  xpcAccessibleDocument* xpcDoc = nullptr;
  if (mIntl->IsLocal()) {
    LocalAccessible* acc = mIntl->AsLocal();
    if (!acc->IsDoc() && !acc->IsApplication()) {
      xpcDoc = GetAccService()->GetXPCDocument(acc->Document());
      xpcDoc->NotifyOfShutdown(acc);
    }
  } else {
    RemoteAccessible* proxy = mIntl->AsRemote();
    if (!proxy->IsDoc()) {
      xpcDoc = GetAccService()->GetXPCDocument(proxy->Document());
      xpcDoc->NotifyOfShutdown(proxy);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

LocalAccessible* xpcAccessibleGeneric::ToInternalAccessible() const {
  return mIntl->AsLocal();
}

////////////////////////////////////////////////////////////////////////////////
// xpcAccessibleGeneric

void xpcAccessibleGeneric::Shutdown() { mIntl = nullptr; }
