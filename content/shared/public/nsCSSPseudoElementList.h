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

/*
 * This file contains the list of nsIAtoms and their values for CSS
 * pseudo-elements.  It is designed to be used as inline input to
 * nsCSSPseudoElements.cpp *only* through the magic of C preprocessing.  All
 * entries must be enclosed either in the macro CSS_PSEUDO_ELEMENT or in the
 * macro CSS2_PSEUDO_ELEMENT; these macros will have cruel and unusual things
 * done to them.  The difference between the two macros is that the
 * pseudo-elements enclosed in CSS2_PSEUDO_ELEMENT are the pseudo-elements
 * which are allowed to have only a single ':' in stylesheets.  The entries
 * should be kept in some sort of logical order.  The first argument to all the
 * macros is the C++ identifier of the atom.  The second argument is the string
 * value of the atom.
 *
 * Code including this file MUST define CSS_PSEUDO_ELEMENT.  If desired, it can
 * also define CSS2_PSEUDO_ELEMENT to perform special handling of the CSS2
 * pseudo-elements.
 */

// OUTPUT_CLASS=nsCSSPseudoElements
// MACRO_NAME=CSS_PSEUDO_ELEMENT

#ifndef CSS2_PSEUDO_ELEMENT
#define DEFINED_CSS2_PSEUDO_ELEMENT
#define CSS2_PSEUDO_ELEMENT(name_, value_) CSS_PSEUDO_ELEMENT(name_, value_)
#endif // CSS2_PSEUDO_ELEMENT

CSS2_PSEUDO_ELEMENT(after, ":after")
CSS2_PSEUDO_ELEMENT(before, ":before")

CSS2_PSEUDO_ELEMENT(firstLetter, ":first-letter")
CSS2_PSEUDO_ELEMENT(firstLine, ":first-line")

CSS_PSEUDO_ELEMENT(mozSelection, ":-moz-selection")

CSS_PSEUDO_ELEMENT(mozFocusInner, ":-moz-focus-inner")
CSS_PSEUDO_ELEMENT(mozFocusOuter, ":-moz-focus-outer")

CSS_PSEUDO_ELEMENT(mozListBullet, ":-moz-list-bullet")
CSS_PSEUDO_ELEMENT(mozListNumber, ":-moz-list-number")

CSS_PSEUDO_ELEMENT(horizontalFramesetBorder, ":-moz-hframeset-border")
CSS_PSEUDO_ELEMENT(verticalFramesetBorder, ":-moz-vframeset-border")

#ifdef DEFINED_CSS2_PSEUDO_ELEMENT
#undef CSS2_PSEUDO_ELEMENT
#endif
