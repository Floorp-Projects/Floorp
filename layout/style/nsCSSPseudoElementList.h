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

CSS_PSEUDO_ELEMENT(after, ":after", CSS_PSEUDO_ELEMENT_IS_CSS2 |
                                    CSS_PSEUDO_ELEMENT_IS_FLEX_OR_GRID_ITEM)
CSS_PSEUDO_ELEMENT(before, ":before", CSS_PSEUDO_ELEMENT_IS_CSS2 |
                                      CSS_PSEUDO_ELEMENT_IS_FLEX_OR_GRID_ITEM)

CSS_PSEUDO_ELEMENT(backdrop, ":backdrop", 0)

CSS_PSEUDO_ELEMENT(cue, ":cue", CSS_PSEUDO_ELEMENT_IS_JS_CREATED_NAC |
                                CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE)

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

CSS_PSEUDO_ELEMENT(mozMathAnonymous, ":-moz-math-anonymous", 0)

// HTML5 Forms pseudo elements
CSS_PSEUDO_ELEMENT(mozNumberWrapper, ":-moz-number-wrapper",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE |
                   CSS_PSEUDO_ELEMENT_UA_SHEET_ONLY)
CSS_PSEUDO_ELEMENT(mozNumberText, ":-moz-number-text",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE |
                   CSS_PSEUDO_ELEMENT_UA_SHEET_ONLY)
CSS_PSEUDO_ELEMENT(mozNumberSpinBox, ":-moz-number-spin-box",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE |
                   CSS_PSEUDO_ELEMENT_UA_SHEET_ONLY)
CSS_PSEUDO_ELEMENT(mozNumberSpinUp, ":-moz-number-spin-up",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE |
                   CSS_PSEUDO_ELEMENT_UA_SHEET_ONLY)
CSS_PSEUDO_ELEMENT(mozNumberSpinDown, ":-moz-number-spin-down",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE |
                   CSS_PSEUDO_ELEMENT_UA_SHEET_ONLY)
CSS_PSEUDO_ELEMENT(mozProgressBar, ":-moz-progress-bar",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
CSS_PSEUDO_ELEMENT(mozRangeTrack, ":-moz-range-track",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
CSS_PSEUDO_ELEMENT(mozRangeProgress, ":-moz-range-progress",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
CSS_PSEUDO_ELEMENT(mozRangeThumb, ":-moz-range-thumb",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
CSS_PSEUDO_ELEMENT(mozMeterBar, ":-moz-meter-bar",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
CSS_PSEUDO_ELEMENT(mozPlaceholder, ":-moz-placeholder",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
CSS_PSEUDO_ELEMENT(placeholder, ":placeholder",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
CSS_PSEUDO_ELEMENT(mozColorSwatch, ":-moz-color-swatch",
                   CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE |
                   CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE)
