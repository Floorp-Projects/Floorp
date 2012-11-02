/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _a11yGeneric_H_
#define _a11yGeneric_H_

#include "nsThreadUtils.h"

// What we want is: NS_INTERFACE_MAP_ENTRY(self) for static IID accessors,
// but some of our classes have an ambiguous base class of nsISupports which
// prevents this from working (the default macro converts it to nsISupports,
// then addrefs it, then returns it). Therefore, we expand the macro here and
// change it so that it works. Yuck.
#define NS_INTERFACE_MAP_STATIC_AMBIGUOUS(_class)                              \
  if (aIID.Equals(NS_GET_IID(_class))) {                                       \
    NS_ADDREF(this);                                                           \
    *aInstancePtr = this;                                                      \
    return NS_OK;                                                              \
  } else

#endif
