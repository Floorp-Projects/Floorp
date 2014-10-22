/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleGeneric.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports and cycle collection

NS_IMPL_CYCLE_COLLECTION_0(xpcAccessibleGeneric)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(xpcAccessibleGeneric)
  NS_INTERFACE_MAP_ENTRY(nsIAccessible)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleSelectable,
                                     mSupportedIfaces & eSelectable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleValue,
                                     mSupportedIfaces & eValue)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleHyperLink,
                                     mSupportedIfaces & eHyperLink)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessible)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(xpcAccessibleGeneric)
NS_IMPL_CYCLE_COLLECTING_RELEASE(xpcAccessibleGeneric)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

Accessible*
xpcAccessibleGeneric::ToInternalAccessible() const
{
  return mIntl;
}

////////////////////////////////////////////////////////////////////////////////
// xpcAccessibleGeneric

void
xpcAccessibleGeneric::Shutdown()
{
  mIntl = nullptr;
}
