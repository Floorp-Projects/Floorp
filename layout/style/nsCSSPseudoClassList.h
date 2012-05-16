/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

// Matches any table elements that have a nonzero border attribute,
// according to HTML integer attribute parsing rules.
CSS_PSEUDO_CLASS(mozTableBorderNonzero, ":-moz-table-border-nonzero")

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

// Matches the element which is being displayed full-screen, and
// any containing frames.
CSS_STATE_PSEUDO_CLASS(mozFullScreen, ":-moz-full-screen", NS_EVENT_STATE_FULL_SCREEN)

// Matches any element which is an ancestor of the DOM full-screen element,
// or an ancestor of a containing frame of the full-screen element.
CSS_STATE_PSEUDO_CLASS(mozFullScreenAncestor, ":-moz-full-screen-ancestor", NS_EVENT_STATE_FULL_SCREEN_ANCESTOR)

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
CSS_STATE_PSEUDO_CLASS(mozHandlerClickToPlay, ":-moz-handler-clicktoplay",
                       NS_EVENT_STATE_TYPE_CLICK_TO_PLAY)
CSS_STATE_PSEUDO_CLASS(mozHandlerDisabled, ":-moz-handler-disabled",
                       NS_EVENT_STATE_HANDLER_DISABLED)
CSS_STATE_PSEUDO_CLASS(mozHandlerBlocked, ":-moz-handler-blocked",
                       NS_EVENT_STATE_HANDLER_BLOCKED)
CSS_STATE_PSEUDO_CLASS(mozHandlerCrashed, ":-moz-handler-crashed",
                       NS_EVENT_STATE_HANDLER_CRASHED)

CSS_STATE_PSEUDO_CLASS(mozMathIncrementScriptLevel,
                       ":-moz-math-increment-script-level",
                       NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL)

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
CSS_STATE_PSEUDO_CLASS(mozUIInvalid, ":-moz-ui-invalid",
                       NS_EVENT_STATE_MOZ_UI_INVALID)
CSS_STATE_PSEUDO_CLASS(mozUIValid, ":-moz-ui-valid",
                       NS_EVENT_STATE_MOZ_UI_VALID)
CSS_STATE_PSEUDO_CLASS(mozMeterOptimum, ":-moz-meter-optimum",
                       NS_EVENT_STATE_OPTIMUM)
CSS_STATE_PSEUDO_CLASS(mozMeterSubOptimum, ":-moz-meter-sub-optimum",
                       NS_EVENT_STATE_SUB_OPTIMUM)
CSS_STATE_PSEUDO_CLASS(mozMeterSubSubOptimum, ":-moz-meter-sub-sub-optimum",
                       NS_EVENT_STATE_SUB_SUB_OPTIMUM)

#ifdef DEFINED_CSS_STATE_PSEUDO_CLASS
#undef DEFINED_CSS_STATE_PSEUDO_CLASS
#undef CSS_STATE_PSEUDO_CLASS
#endif
