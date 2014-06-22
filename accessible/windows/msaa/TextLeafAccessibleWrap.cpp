/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextLeafAccessibleWrap.h"

#include "sdnTextAccessible.h"
#include "Statistics.h"

using namespace mozilla::a11y;

IMPL_IUNKNOWN_QUERY_HEAD(TextLeafAccessibleWrap)
  if (aIID == IID_ISimpleDOMText) {
    statistics::ISimpleDOMUsed();
    *aInstancePtr = static_cast<ISimpleDOMText*>(new sdnTextAccessible(this));
    static_cast<IUnknown*>(*aInstancePtr)->AddRef();
    return S_OK;
  }
IMPL_IUNKNOWN_QUERY_TAIL_INHERITED(AccessibleWrap)
