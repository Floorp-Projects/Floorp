/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This file defines enum values for all of the DOM objects which have
 * an entry in nsDOMClassInfo.
 */

#ifndef nsDOMClassInfoID_h__
#define nsDOMClassInfoID_h__

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

/**
 * DOMCI_CASTABLE_INTERFACES contains the list of interfaces that we have a bit
 * for in nsDOMClassInfo's mInterfacesBitmap. To use it you need to define
 * DOMCI_CASTABLE_INTERFACE(interface, bit, extra) and then call
 * DOMCI_CASTABLE_INTERFACES(extra). For every interface there will be one
 * call to DOMCI_CASTABLE_INTERFACE with the bit that it corresponds to and
 * the extra argument that was passed in to DOMCI_CASTABLE_INTERFACES.
 *
 * WARNING: Be very careful when adding interfaces to this list. Every object
 *          that implements one of these interfaces must be directly castable
 *          to that interface from the *canonical* nsISupports!
 */
#define DOMCI_CASTABLE_INTERFACES(_extra)                                     \
DOMCI_CASTABLE_INTERFACE(nsINode, 0, _extra)                                  \
DOMCI_CASTABLE_INTERFACE(nsIContent, 1, _extra)                               \
DOMCI_CASTABLE_INTERFACE(nsIDocument, 2, _extra)                              \
DOMCI_CASTABLE_INTERFACE(nsINodeList, 3, _extra)                              \
DOMCI_CASTABLE_INTERFACE(nsICSSDeclaration, 4, _extra)


#ifdef _IMPL_NS_LAYOUT

#define DOMCI_CLASS(_dom_class)                                               \
  extern const PRUint32 kDOMClassInfo_##_dom_class##_interfaces;

#include "nsDOMClassInfoClasses.h"

#undef DOMCI_CLASS

/**
 * We define two functions for every interface in DOMCI_CASTABLE_INTERFACES.
 * One takes a void* and one takes an nsIFoo*. These are used to compute the
 * bitmap for a given class. If the class doesn't implement the interface then
 * the void* variant will be called and we'll return 0, otherwise the nsIFoo*
 * variant will be called and we'll return (1 << bit).
 */
#define DOMCI_CASTABLE_INTERFACE(_interface, _bit, _extra)                    \
class _interface;                                                             \
inline PRUint32 Implements_##_interface(_interface *foo)                      \
  { return 1 << _bit; }                                                       \
inline PRUint32 Implements_##_interface(void *foo)                            \
  { return 0; }

DOMCI_CASTABLE_INTERFACES()

#undef DOMCI_CASTABLE_INTERFACE

/**
 * Here we calculate the bitmap for a given class. We'll call the functions
 * defined above with (_class*)nsnull. If the class implements an interface,
 * that function will return (1 << bit), if it doesn't the function returns 0.
 * We just make the sum of all the values returned from the functions to
 * generate the bitmap.
 */
#define DOMCI_CASTABLE_INTERFACE(_interface, _bit, _class)                    \
  Implements_##_interface((_class*)nsnull) +

#define DOMCI_DATA(_dom_class, _class)                                        \
const PRUint32 kDOMClassInfo_##_dom_class##_interfaces =                      \
  DOMCI_CASTABLE_INTERFACES(_class)                                           \
  0;

class nsIClassInfo;
class nsXPCClassInfo;

extern nsIClassInfo*
NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID);

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(_class)                          \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                                \
      aIID.Equals(NS_GET_IID(nsXPCClassInfo))) {                              \
    foundInterface = NS_GetDOMClassInfoInstance(eDOMClassInfo_##_class##_id); \
    if (!foundInterface) {                                                    \
      *aInstancePtr = nsnull;                                                 \
      return NS_ERROR_OUT_OF_MEMORY;                                          \
    }                                                                         \
  } else

#else

// See nsIDOMClassInfo.h

#endif // _IMPL_NS_LAYOUT

#endif // nsDOMClassInfoID_h__
