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
 * The Original Code is atom lists for CSS pseudos.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* atom list for CSS pseudo-elements */

#ifndef nsCSSPseudoElements_h___
#define nsCSSPseudoElements_h___

#include "nsIAtom.h"

// Is this pseudo-element a CSS2 pseudo-element that can be specified
// with the single colon syntax (in addition to the double-colon syntax,
// which can be used for all pseudo-elements)?
#define CSS_PSEUDO_ELEMENT_IS_CSS2                (1<<0)
// Is this pseudo-element a pseudo-element that can contain other
// elements?
// (Currently pseudo-elements are either leaves of the tree (relative to
// real elements) or they contain other elements in a non-tree-like
// manner (i.e., like incorrectly-nested start and end tags).  It's
// possible that in the future there might be container pseudo-elements
// that form a properly nested tree structure.  If that happens, we
// should probably split this flag into two.)
#define CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS      (1<<1)

// Empty class derived from nsIAtom so that function signatures can
// require an atom from this atom list.
class nsICSSPseudoElement : public nsIAtom {};

class nsCSSPseudoElements {
public:

  static void AddRefAtoms();

  static PRBool IsPseudoElement(nsIAtom *aAtom);

  static PRBool IsCSS2PseudoElement(nsIAtom *aAtom);

  static PRBool PseudoElementContainsElements(nsIAtom *aAtom) {
    return PseudoElementHasFlags(aAtom, CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS);
  }

#define CSS_PSEUDO_ELEMENT(_name, _value, _flags) \
  static nsICSSPseudoElement* _name;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

private:
  static PRUint32 FlagsForPseudoElement(nsIAtom *aAtom);

  // Does the given pseudo-element have all of the flags given?
  static PRBool PseudoElementHasFlags(nsIAtom *aAtom, PRUint32 aFlags)
  {
    return (FlagsForPseudoElement(aAtom) & aFlags) == aFlags;
  }
};

#endif /* nsCSSPseudoElements_h___ */
