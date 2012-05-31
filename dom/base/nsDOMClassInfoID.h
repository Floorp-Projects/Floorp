/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#undef DOMCI_CASTABLE_INTERFACE
#define DOMCI_CASTABLE_INTERFACES(_extra)                                     \
DOMCI_CASTABLE_INTERFACE(nsINode, nsINode, 0, _extra)                         \
DOMCI_CASTABLE_INTERFACE(nsIContent, nsIContent, 1, _extra)                   \
DOMCI_CASTABLE_INTERFACE(nsIDocument, nsIDocument, 2, _extra)                 \
DOMCI_CASTABLE_INTERFACE(nsINodeList, nsINodeList, 3, _extra)                 \
DOMCI_CASTABLE_INTERFACE(nsICSSDeclaration, nsICSSDeclaration, 4, _extra)     \
DOMCI_CASTABLE_INTERFACE(nsDocument, nsIDocument, 5, _extra)                  \
DOMCI_CASTABLE_INTERFACE(nsGenericHTMLElement, nsGenericHTMLElement, 6,       \
                         _extra)                                              \
DOMCI_CASTABLE_INTERFACE(nsHTMLDocument, nsIDocument, 7, _extra)              \
DOMCI_CASTABLE_INTERFACE(nsStyledElement, nsStyledElement, 8, _extra)         \
DOMCI_CASTABLE_INTERFACE(nsSVGStylableElement, nsIContent, 9, _extra)         \
DOMCI_CASTABLE_INTERFACE(nsIDOMWebGLRenderingContext,                         \
                         nsIDOMWebGLRenderingContext, 10, _extra)             \
DOMCI_CASTABLE_INTERFACE(nsIWebGLUniformLocation,                             \
                         nsIWebGLUniformLocation, 11, _extra)                 \
DOMCI_CASTABLE_INTERFACE(nsIDOMImageData, nsIDOMImageData, 12, _extra)
 
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
