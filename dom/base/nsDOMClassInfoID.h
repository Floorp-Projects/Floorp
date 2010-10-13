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
DOMCI_CASTABLE_INTERFACE(nsINode, nsINode, 0, _extra)                         \
DOMCI_CASTABLE_INTERFACE(nsIContent, nsIContent, 1, _extra)                   \
DOMCI_CASTABLE_INTERFACE(nsIDocument, nsIDocument, 2, _extra)                 \
DOMCI_CASTABLE_INTERFACE(nsINodeList, nsINodeList, 3, _extra)                 \
DOMCI_CASTABLE_INTERFACE(nsICSSDeclaration, nsICSSDeclaration, 4, _extra)     \
DOMCI_CASTABLE_INTERFACE(nsGenericTextNode, nsGenericTextNode, 5, _extra)     \
DOMCI_CASTABLE_INTERFACE(nsDocument, nsIDocument, 6, _extra)                  \
DOMCI_CASTABLE_INTERFACE(nsGenericHTMLElement, nsGenericHTMLElement, 7,       \
                         _extra)                                              \
DOMCI_CASTABLE_INTERFACE(nsHTMLDocument, nsIDocument, 8, _extra)              \
DOMCI_CASTABLE_INTERFACE(nsStyledElement, nsStyledElement, 9, _extra)         \
DOMCI_CASTABLE_INTERFACE(nsSVGStylableElement, nsIContent, 10, _extra)

// Make sure all classes mentioned in DOMCI_CASTABLE_INTERFACES
// have been declared.
#define DOMCI_CASTABLE_INTERFACE(_interface, _u1, _u2, _u3) class _interface;
DOMCI_CASTABLE_INTERFACES(unused)
#undef DOMCI_CASTABLE_INTERFACE

#ifdef _IMPL_NS_LAYOUT

#define DOMCI_CLASS(_dom_class)                                               \
  extern const PRUint32 kDOMClassInfo_##_dom_class##_interfaces;

#include "nsDOMClassInfoClasses.h"

#undef DOMCI_CLASS

/**
 * Provide a general "does class C implement interface I" predicate.
 * This is not as sophisticated as e.g. boost's is_base_of template,
 * but it does the job adequately for our purposes.
 */

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2) || \
    _MSC_FULL_VER >= 140050215

/* Use a compiler intrinsic if one is available. */

#define DOMCI_CASTABLE_TO(_interface, _class) __is_base_of(_interface, _class)

#else

/* The generic version of this predicate relies on the overload resolution
 * rules.  If |_class| inherits from |_interface|, the |_interface*|
 * overload of DOMCI_CastableTo<_interface>::p() will be chosen, otherwise
 * the |void*| overload will be chosen.  There is no definition of these
 * functions; we determine which overload was selected by inspecting the
 * size of the return type.
 */

template <typename Interface> struct DOMCI_CastableTo {
  struct false_type { int x[1]; };
  struct true_type { int x[2]; };
  static false_type p(void*);
  static true_type p(Interface*);
};

#define DOMCI_CASTABLE_TO(_interface, _class)                                 \
  (sizeof(DOMCI_CastableTo<_interface>::p(static_cast<_class*>(0))) ==        \
   sizeof(DOMCI_CastableTo<_interface>::true_type))

#endif

/**
 * Here we calculate the bitmap for a given class.
 */
#define DOMCI_CASTABLE_INTERFACE(_interface, _base, _bit, _class)             \
  (DOMCI_CASTABLE_TO(_interface, _class) ? 1 << _bit : 0) +

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

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(_class, condition)   \
  if ((condition) &&                                                          \
      (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                               \
       aIID.Equals(NS_GET_IID(nsXPCClassInfo)))) {                            \
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
