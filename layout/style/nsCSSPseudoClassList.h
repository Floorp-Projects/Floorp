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

/* atom list for CSS pseudo-classes */

/*
 * This file contains the list of nsIAtoms and their values for CSS
 * pseudo-classes.  It is designed to be used as inline input to
 * nsCSSPseudoClasses.cpp *only* through the magic of C preprocessing.
 * All entries must be enclosed in the macros CSS_PSEUDO_CLASS or
 * CSS_STATE_PSEUDO_CLASS which will have cruel and unusual things
 * done to it.  The entries should be kept in some sort of logical
 * order.  The first argument to CSS_PSEUDO_CLASS is the C++
 * identifier of the atom.  The second argument is the string value of
 * the atom.  CSS_STATE_PSEUDO_CLASS also takes the name of the state
 * bits that the class corresponds to.  Only one of the bits needs to
 * match for the pseudo-class to match.  If CSS_STATE_PSEUDO_CLASS is
 * not defined, it'll be automatically defined to CSS_PSEUDO_CLASS.
 */

// OUTPUT_CLASS=nsCSSPseudoClasses
// MACRO_NAME=CSS_PSEUDO_CLASS

#ifdef DEFINED_CSS_STATE_PSEUDO_CLASS
#error "This shouldn't be defined"
#endif

#ifndef CSS_STATE_PSEUDO_CLASS
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _bit) \
  CSS_PSEUDO_CLASS(_name, _value)
#define DEFINED_CSS_STATE_PSEUDO_CLASS
#endif

// The CSS_PSEUDO_CLASS entries should all come before the
// CSS_STATE_PSEUDO_CLASS entries.  The CSS_PSEUDO_CLASS entry order
// must be the same as the order of cases in SelectorMatches.

CSS_PSEUDO_CLASS(empty, ":empty")
CSS_PSEUDO_CLASS(mozOnlyWhitespace, ":-moz-only-whitespace")
CSS_PSEUDO_CLASS(mozEmptyExceptChildrenWithLocalname, ":-moz-empty-except-children-with-localname")
CSS_PSEUDO_CLASS(lang, ":lang")
CSS_PSEUDO_CLASS(mozBoundElement, ":-moz-bound-element")
CSS_PSEUDO_CLASS(root, ":root")
CSS_PSEUDO_CLASS(any, ":-moz-any")

CSS_PSEUDO_CLASS(firstChild, ":first-child")
CSS_PSEUDO_CLASS(firstNode, ":-moz-first-node")
CSS_PSEUDO_CLASS(lastChild, ":last-child")
CSS_PSEUDO_CLASS(lastNode, ":-moz-last-node")
CSS_PSEUDO_CLASS(onlyChild, ":only-child")
CSS_PSEUDO_CLASS(firstOfType, ":first-of-type")
CSS_PSEUDO_CLASS(lastOfType, ":last-of-type")
CSS_PSEUDO_CLASS(onlyOfType, ":only-of-type")
CSS_PSEUDO_CLASS(nthChild, ":nth-child")
CSS_PSEUDO_CLASS(nthLastChild, ":nth-last-child")
CSS_PSEUDO_CLASS(nthOfType, ":nth-of-type")
CSS_PSEUDO_CLASS(nthLastOfType, ":nth-last-of-type")

CSS_PSEUDO_CLASS(mozHasHandlerRef, ":-moz-has-handlerref")

// Match nodes that are HTML but not XHTML
CSS_PSEUDO_CLASS(mozIsHTML, ":-moz-is-html")

// Matches anything when the specified look-and-feel metric is set
CSS_PSEUDO_CLASS(mozSystemMetric, ":-moz-system-metric")

// -moz-locale-dir(ltr) and -moz-locale-dir(rtl) may be used
// to match based on the locale's chrome direction
CSS_PSEUDO_CLASS(mozLocaleDir, ":-moz-locale-dir")

// -moz-lwtheme may be used to match a document that has a lightweight theme
CSS_PSEUDO_CLASS(mozLWTheme, ":-moz-lwtheme")

// -moz-lwtheme-brighttext matches a document that has a bright lightweight theme
CSS_PSEUDO_CLASS(mozLWThemeBrightText, ":-moz-lwtheme-brighttext")

// -moz-lwtheme-darktext matches a document that has a bright lightweight theme
CSS_PSEUDO_CLASS(mozLWThemeDarkText, ":-moz-lwtheme-darktext")

// Matches anything when the containing window is inactive
CSS_PSEUDO_CLASS(mozWindowInactive, ":-moz-window-inactive")

// :not needs to come at the end of the non-bit pseudo-class list, since
// it doesn't actually get directly matched on in SelectorMatches.
CSS_PSEUDO_CLASS(notPseudo, ":not")

CSS_STATE_PSEUDO_CLASS(link, ":link", NS_EVENT_STATE_UNVISITED)
// what matches :link or :visited
CSS_STATE_PSEUDO_CLASS(mozAnyLink, ":-moz-any-link",
                       NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED)
CSS_STATE_PSEUDO_CLASS(visited, ":visited", NS_EVENT_STATE_VISITED)

CSS_STATE_PSEUDO_CLASS(active, ":active", NS_EVENT_STATE_ACTIVE)
CSS_STATE_PSEUDO_CLASS(checked, ":checked", NS_EVENT_STATE_CHECKED)
CSS_STATE_PSEUDO_CLASS(disabled, ":disabled", NS_EVENT_STATE_DISABLED)
CSS_STATE_PSEUDO_CLASS(enabled, ":enabled", NS_EVENT_STATE_ENABLED)
CSS_STATE_PSEUDO_CLASS(focus, ":focus", NS_EVENT_STATE_FOCUS)
CSS_STATE_PSEUDO_CLASS(hover, ":hover", NS_EVENT_STATE_HOVER)
CSS_STATE_PSEUDO_CLASS(mozDragOver, ":-moz-drag-over", NS_EVENT_STATE_DRAGOVER)
CSS_STATE_PSEUDO_CLASS(target, ":target", NS_EVENT_STATE_URLTARGET)
CSS_STATE_PSEUDO_CLASS(indeterminate, ":indeterminate",
                       NS_EVENT_STATE_INDETERMINATE)

// Matches if the element is focused and should show a focus ring
CSS_STATE_PSEUDO_CLASS(mozFocusRing, ":-moz-focusring", NS_EVENT_STATE_FOCUSRING)

// Image, object, etc state pseudo-classes
CSS_STATE_PSEUDO_CLASS(mozBroken, ":-moz-broken", NS_EVENT_STATE_BROKEN)
CSS_STATE_PSEUDO_CLASS(mozUserDisabled, ":-moz-user-disabled",
                       NS_EVENT_STATE_USERDISABLED)
CSS_STATE_PSEUDO_CLASS(mozSuppressed, ":-moz-suppressed",
                       NS_EVENT_STATE_SUPPRESSED)
CSS_STATE_PSEUDO_CLASS(mozLoading, ":-moz-loading", NS_EVENT_STATE_LOADING)
CSS_STATE_PSEUDO_CLASS(mozTypeUnsupported, ":-moz-type-unsupported",
                       NS_EVENT_STATE_TYPE_UNSUPPORTED)
CSS_STATE_PSEUDO_CLASS(mozHandlerDisabled, ":-moz-handler-disabled",
                       NS_EVENT_STATE_HANDLER_DISABLED)
CSS_STATE_PSEUDO_CLASS(mozHandlerBlocked, ":-moz-handler-blocked",
                       NS_EVENT_STATE_HANDLER_BLOCKED)
CSS_STATE_PSEUDO_CLASS(mozHandlerCrashed, ":-moz-handler-crashed",
                       NS_EVENT_STATE_HANDLER_CRASHED)

#ifdef MOZ_MATHML
CSS_STATE_PSEUDO_CLASS(mozMathIncrementScriptLevel,
                       ":-moz-math-increment-script-level",
                       NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL)
#endif

// CSS 3 UI
// http://www.w3.org/TR/2004/CR-css3-ui-20040511/#pseudo-classes
CSS_STATE_PSEUDO_CLASS(required, ":required", NS_EVENT_STATE_REQUIRED)
CSS_STATE_PSEUDO_CLASS(optional, ":optional", NS_EVENT_STATE_OPTIONAL)
CSS_STATE_PSEUDO_CLASS(valid, ":valid", NS_EVENT_STATE_VALID)
CSS_STATE_PSEUDO_CLASS(invalid, ":invalid", NS_EVENT_STATE_INVALID)
CSS_STATE_PSEUDO_CLASS(inRange, ":in-range", NS_EVENT_STATE_INRANGE)
CSS_STATE_PSEUDO_CLASS(outOfRange, ":out-of-range", NS_EVENT_STATE_OUTOFRANGE)
CSS_STATE_PSEUDO_CLASS(defaultPseudo, ":default", NS_EVENT_STATE_DEFAULT)
CSS_STATE_PSEUDO_CLASS(mozReadOnly, ":-moz-read-only",
                       NS_EVENT_STATE_MOZ_READONLY)
CSS_STATE_PSEUDO_CLASS(mozReadWrite, ":-moz-read-write",
                       NS_EVENT_STATE_MOZ_READWRITE)
CSS_STATE_PSEUDO_CLASS(mozPlaceholder, ":-moz-placeholder",
                       NS_EVENT_STATE_MOZ_PLACEHOLDER)
CSS_STATE_PSEUDO_CLASS(mozSubmitInvalid, ":-moz-submit-invalid",
                       NS_EVENT_STATE_MOZ_SUBMITINVALID)

#ifdef DEFINED_CSS_STATE_PSEUDO_CLASS
#undef DEFINED_CSS_STATE_PSEUDO_CLASS
#undef CSS_STATE_PSEUDO_CLASS
#endif
