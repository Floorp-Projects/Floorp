/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

/*
 * This file contains the list of nsIAtoms and their values for CSS
 * pseudo-elements.  It is designed to be used as inline input to
 * nsCSSPseudoElements.cpp *only* through the magic of C preprocessing.  All
 * entries must be enclosed either in the macro CSS_PSEUDO_ELEMENT;
 * these macros will have cruel and unusual things done to them.  The
 * entries should be kept in some sort of logical order.
 *
 * Code including this file MUST define CSS_PSEUDO_ELEMENT, which takes
 * three parameters:
 * name_  : The C++ identifier used for the atom (which will be a member
 *          of nsCSSPseudoElements)
 * value_ : The pseudo-element as a string, with single-colon syntax,
 *          used as the string value of the atom.
 * flags_ : A bitfield containing flags defined in nsCSSPseudoElements.h
 */

// OUTPUT_CLASS=nsCSSPseudoElements
// MACRO_NAME=CSS_PSEUDO_ELEMENT

CSS_PSEUDO_ELEMENT(after, ":after", CSS_PSEUDO_ELEMENT_IS_CSS2)
CSS_PSEUDO_ELEMENT(before, ":before", CSS_PSEUDO_ELEMENT_IS_CSS2)

CSS_PSEUDO_ELEMENT(firstLetter, ":first-letter",
                   CSS_PSEUDO_ELEMENT_IS_CSS2 |
                   CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS)
CSS_PSEUDO_ELEMENT(firstLine, ":first-line",
                   CSS_PSEUDO_ELEMENT_IS_CSS2 |
                   CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS)

CSS_PSEUDO_ELEMENT(mozSelection, ":-moz-selection",
                   CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS)

// XXXbz should we really allow random content to style these?  Maybe
// use our flags to prevent that?
CSS_PSEUDO_ELEMENT(mozFocusInner, ":-moz-focus-inner", 0)
CSS_PSEUDO_ELEMENT(mozFocusOuter, ":-moz-focus-outer", 0)

// XXXbz should we really allow random content to style these?  Maybe
// use our flags to prevent that?
CSS_PSEUDO_ELEMENT(mozListBullet, ":-moz-list-bullet", 0)
CSS_PSEUDO_ELEMENT(mozListNumber, ":-moz-list-number", 0)

CSS_PSEUDO_ELEMENT(mozMathStretchy, ":-moz-math-stretchy", 0)
CSS_PSEUDO_ELEMENT(mozMathAnonymous, ":-moz-math-anonymous", 0)

// HTML5 Forms pseudo elements
CSS_PSEUDO_ELEMENT(mozProgressBar, ":-moz-progress-bar", 0)

