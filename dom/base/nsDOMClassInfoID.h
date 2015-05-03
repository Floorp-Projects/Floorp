/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines enum values for all of the DOM objects which have
 * an entry in nsDOMClassInfo.
 */

#ifndef nsDOMClassInfoID_h__
#define nsDOMClassInfoID_h__

#include "nsIXPCScriptable.h"

#define DOMCI_CLASS(_dom_class)                                               \
  eDOMClassInfo_##_dom_class##_id,

enum nsDOMClassInfoID {

#include "nsDOMClassInfoClasses.h"

  // This one better be the last one in this list
  eDOMClassInfoIDCount
};

#undef DOMCI_CLASS

/**
 * nsIClassInfo helper macros
 */

#ifdef MOZILLA_INTERNAL_API

class nsIClassInfo;
class nsXPCClassInfo;

extern nsIClassInfo*
NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID);

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(_class)                          \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                                \
      aIID.Equals(NS_GET_IID(nsXPCClassInfo))) {                              \
    foundInterface = NS_GetDOMClassInfoInstance(eDOMClassInfo_##_class##_id); \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nullptr;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(_class, condition)   \
  if ((condition) &&                                                          \
      (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                               \
       aIID.Equals(NS_GET_IID(nsXPCClassInfo)))) {                            \
    foundInterface = NS_GetDOMClassInfoInstance(eDOMClassInfo_##_class##_id); \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nullptr;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#else

// See nsIDOMClassInfo.h

#endif // MOZILLA_INTERNAL_API

#endif // nsDOMClassInfoID_h__
