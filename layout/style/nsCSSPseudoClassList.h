/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-classes */

/*
 * This file contains the list of nsIAtoms and their values for CSS
 * pseudo-classes.  It is designed to be used as inline input to
 * nsCSSPseudoClasses.cpp *only* through the magic of C preprocessing.
 * All entries must be enclosed in the macros CSS_PSEUDO_CLASS,
 * CSS_STATE_DEPENDENT_PSEUDO_CLASS, or CSS_STATE_PSEUDO_CLASS which
 * will have cruel and unusual things done to them.  The entries should
 * be kept in some sort of logical order.  The common arguments to these
 * macros are:
 * name_  : The C++ identifier used for the atom (which will be a member
 *          of nsCSSPseudoClasses)
 * value_ : The pseudo-class as a string, including the initial colon,
 *          used as the string value of the atom.
 * flags_ : A bitfield containing flags defined in nsCSSPseudoClasses.h
 * pref_  : The name of the preference controlling whether the
 *          pseudo-class is recognized by the parser, or the empty
 *          string if it's unconditional.
 * CSS_STATE_PSEUDO_CLASS has an additional argument:
 * bit_   : The event state bit or bits that corresponds to the
 *          pseudo-class, i.e., causes it to match (only one bit
 *          required to match).
 * CSS_STATE_DEPENDENT_PSEUDO_CLASS has an additional argument:
 * bit_   : The event state bits that affect whether the pseudo-class
 *          matches.  Matching depends on a customized per-class
 *          algorithm which should be defined in SelectorMatches() in
 *          nsCSSRuleProcessor.cpp.
 *
 * If CSS_STATE_PSEUDO_CLASS is not defined, it'll be automatically
 * defined to CSS_STATE_DEPENDENT_PSEUDO_CLASS;
 * if CSS_STATE_DEPENDENT_PSEUDO_CLASS is not defined, it'll be
 * automatically defined to CSS_PSEUDO_CLASS.
 */

// OUTPUT_CLASS=nsCSSPseudoClasses
// MACRO_NAME=CSS_PSEUDO_CLASS

#ifdef DEFINED_CSS_STATE_DEPENDENT_PSEUDO_CLASS
#error "CSS_STATE_DEPENDENT_PSEUDO_CLASS shouldn't be defined"
#endif

#ifndef CSS_STATE_DEPENDENT_PSEUDO_CLASS
#define CSS_STATE_DEPENDENT_PSEUDO_CLASS(_name, _value, _flags, _pref, _bit)  \
  CSS_PSEUDO_CLASS(_name, _value, _flags, _pref)
#define DEFINED_CSS_STATE_DEPENDENT_PSEUDO_CLASS
#endif

#ifdef DEFINED_CSS_STATE_PSEUDO_CLASS
#error "CSS_STATE_PSEUDO_CLASS shouldn't be defined"
#endif

#ifndef CSS_STATE_PSEUDO_CLASS
#define CSS_STATE_PSEUDO_CLASS(_name, _value, _flags, _pref, _bit)      \
  CSS_STATE_DEPENDENT_PSEUDO_CLASS(_name, _value, _flags, _pref, _bit)
#define DEFINED_CSS_STATE_PSEUDO_CLASS
#endif

// The CSS_PSEUDO_CLASS entries should all come before the
// CSS_STATE_PSEUDO_CLASS entries.  The CSS_PSEUDO_CLASS entry order
// must be the same as the order of cases in SelectorMatches.  :not
// must be the last CSS_PSEUDO_CLASS.

CSS_PSEUDO_CLASS(empty, ":empty", 0, "")
CSS_PSEUDO_CLASS(mozOnlyWhitespace, ":-moz-only-whitespace", 0, "")
CSS_PSEUDO_CLASS(mozEmptyExceptChildrenWithLocalname, ":-moz-empty-except-children-with-localname", 0, "")
CSS_PSEUDO_CLASS(lang, ":lang", 0, "")
CSS_PSEUDO_CLASS(mozBoundElement, ":-moz-bound-element", 0, "")
CSS_PSEUDO_CLASS(root, ":root", 0, "")
CSS_PSEUDO_CLASS(any, ":-moz-any", 0, "")

CSS_PSEUDO_CLASS(firstChild, ":first-child", 0, "")
CSS_PSEUDO_CLASS(firstNode, ":-moz-first-node", 0, "")
CSS_PSEUDO_CLASS(lastChild, ":last-child", 0, "")
CSS_PSEUDO_CLASS(lastNode, ":-moz-last-node", 0, "")
CSS_PSEUDO_CLASS(onlyChild, ":only-child", 0, "")
CSS_PSEUDO_CLASS(firstOfType, ":first-of-type", 0, "")
CSS_PSEUDO_CLASS(lastOfType, ":last-of-type", 0, "")
CSS_PSEUDO_CLASS(onlyOfType, ":only-of-type", 0, "")
CSS_PSEUDO_CLASS(nthChild, ":nth-child", 0, "")
CSS_PSEUDO_CLASS(nthLastChild, ":nth-last-child", 0, "")
CSS_PSEUDO_CLASS(nthOfType, ":nth-of-type", 0, "")
CSS_PSEUDO_CLASS(nthLastOfType, ":nth-last-of-type", 0, "")

// Match nodes that are HTML but not XHTML
CSS_PSEUDO_CLASS(mozIsHTML, ":-moz-is-html", 0, "")

// Match all custom elements whose created callback has not yet been invoked
 CSS_STATE_PSEUDO_CLASS(unresolved, ":unresolved", 0, "", NS_EVENT_STATE_UNRESOLVED)

// Matches nodes that are in a native-anonymous subtree (i.e., nodes in
// a subtree of C++ anonymous content constructed by Gecko for its own
// purposes).
CSS_PSEUDO_CLASS(mozNativeAnonymous, ":-moz-native-anonymous",
                 CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS, "")

// Matches anything when the specified look-and-feel metric is set
CSS_PSEUDO_CLASS(mozSystemMetric, ":-moz-system-metric", 0, "")

// -moz-locale-dir(ltr) and -moz-locale-dir(rtl) may be used
// to match based on the locale's chrome direction
CSS_PSEUDO_CLASS(mozLocaleDir, ":-moz-locale-dir", 0, "")

// -moz-lwtheme may be used to match a document that has a lightweight theme
CSS_PSEUDO_CLASS(mozLWTheme, ":-moz-lwtheme", 0, "")

// -moz-lwtheme-brighttext matches a document that has a dark lightweight theme
CSS_PSEUDO_CLASS(mozLWThemeBrightText, ":-moz-lwtheme-brighttext", 0, "")

// -moz-lwtheme-darktext matches a document that has a bright lightweight theme
CSS_PSEUDO_CLASS(mozLWThemeDarkText, ":-moz-lwtheme-darktext", 0, "")

// Matches anything when the containing window is inactive
CSS_PSEUDO_CLASS(mozWindowInactive, ":-moz-window-inactive", 0, "")

// Matches any table elements that have a nonzero border attribute,
// according to HTML integer attribute parsing rules.
CSS_PSEUDO_CLASS(mozTableBorderNonzero, ":-moz-table-border-nonzero", 0, "")

// Matches HTML frame/iframe elements which are mozbrowser.
CSS_PSEUDO_CLASS(mozBrowserFrame, ":-moz-browser-frame",
                 CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "")

// Matches whatever the contextual reference elements are for the
// matching operation.
CSS_PSEUDO_CLASS(scope, ":scope", 0, "layout.css.scope-pseudo.enabled")

// :not needs to come at the end of the non-bit pseudo-class list, since
// it doesn't actually get directly matched on in SelectorMatches.
CSS_PSEUDO_CLASS(negation, ":not", 0, "")

// :dir(ltr) and :dir(rtl) match elements whose resolved
// directionality in the markup language is ltr or rtl respectively.
CSS_STATE_DEPENDENT_PSEUDO_CLASS(dir, ":dir", 0, "",
                                 NS_EVENT_STATE_LTR | NS_EVENT_STATE_RTL)
// prefix version is deprecated and will be removed per bug 1270406.
CSS_STATE_DEPENDENT_PSEUDO_CLASS(mozDir, ":-moz-dir", 0, "",
                                 NS_EVENT_STATE_LTR | NS_EVENT_STATE_RTL)

CSS_STATE_PSEUDO_CLASS(link, ":link", 0, "", NS_EVENT_STATE_UNVISITED)
// what matches :link or :visited
CSS_STATE_PSEUDO_CLASS(mozAnyLink, ":-moz-any-link", 0, "",
                       NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED)
CSS_STATE_PSEUDO_CLASS(anyLink, ":any-link", 0, "",
                       NS_EVENT_STATE_VISITED | NS_EVENT_STATE_UNVISITED)
CSS_STATE_PSEUDO_CLASS(visited, ":visited", 0, "", NS_EVENT_STATE_VISITED)

CSS_STATE_PSEUDO_CLASS(active, ":active", 0, "", NS_EVENT_STATE_ACTIVE)
CSS_STATE_PSEUDO_CLASS(checked, ":checked", 0, "", NS_EVENT_STATE_CHECKED)
CSS_STATE_PSEUDO_CLASS(disabled, ":disabled", 0, "", NS_EVENT_STATE_DISABLED)
CSS_STATE_PSEUDO_CLASS(enabled, ":enabled", 0, "", NS_EVENT_STATE_ENABLED)
CSS_STATE_PSEUDO_CLASS(focus, ":focus", 0, "", NS_EVENT_STATE_FOCUS)
CSS_STATE_PSEUDO_CLASS(hover, ":hover", 0, "", NS_EVENT_STATE_HOVER)
CSS_STATE_PSEUDO_CLASS(mozDragOver, ":-moz-drag-over", 0, "", NS_EVENT_STATE_DRAGOVER)
CSS_STATE_PSEUDO_CLASS(target, ":target", 0, "", NS_EVENT_STATE_URLTARGET)
CSS_STATE_PSEUDO_CLASS(indeterminate, ":indeterminate", 0, "",
                       NS_EVENT_STATE_INDETERMINATE)

CSS_STATE_PSEUDO_CLASS(mozDevtoolsHighlighted, ":-moz-devtools-highlighted", 0, "",
                       NS_EVENT_STATE_DEVTOOLS_HIGHLIGHTED)
CSS_STATE_PSEUDO_CLASS(mozStyleeditorTransitioning, ":-moz-styleeditor-transitioning", 0, "",
                       NS_EVENT_STATE_STYLEEDITOR_TRANSITIONING)

// Matches the element which is being displayed full-screen, and
// any containing frames.
CSS_STATE_PSEUDO_CLASS(fullscreen, ":fullscreen",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME,
                       "full-screen-api.unprefix.enabled",
                       NS_EVENT_STATE_FULL_SCREEN)
CSS_STATE_PSEUDO_CLASS(mozFullScreen, ":-moz-full-screen", 0, "", NS_EVENT_STATE_FULL_SCREEN)

// Matches if the element is focused and should show a focus ring
CSS_STATE_PSEUDO_CLASS(mozFocusRing, ":-moz-focusring", 0, "", NS_EVENT_STATE_FOCUSRING)

// Image, object, etc state pseudo-classes
CSS_STATE_PSEUDO_CLASS(mozBroken, ":-moz-broken", 0, "", NS_EVENT_STATE_BROKEN)
CSS_STATE_PSEUDO_CLASS(mozLoading, ":-moz-loading", 0, "", NS_EVENT_STATE_LOADING)

CSS_STATE_PSEUDO_CLASS(mozUserDisabled, ":-moz-user-disabled",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_USERDISABLED)
CSS_STATE_PSEUDO_CLASS(mozSuppressed, ":-moz-suppressed",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_SUPPRESSED)
CSS_STATE_PSEUDO_CLASS(mozTypeUnsupported, ":-moz-type-unsupported",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_TYPE_UNSUPPORTED)
CSS_STATE_PSEUDO_CLASS(mozTypeUnsupportedPlatform, ":-moz-type-unsupported-platform",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_TYPE_UNSUPPORTED_PLATFORM)
CSS_STATE_PSEUDO_CLASS(mozHandlerClickToPlay, ":-moz-handler-clicktoplay",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_TYPE_CLICK_TO_PLAY)
CSS_STATE_PSEUDO_CLASS(mozHandlerVulnerableUpdatable, ":-moz-handler-vulnerable-updatable",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_VULNERABLE_UPDATABLE)
CSS_STATE_PSEUDO_CLASS(mozHandlerVulnerableNoUpdate, ":-moz-handler-vulnerable-no-update",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_VULNERABLE_NO_UPDATE)
CSS_STATE_PSEUDO_CLASS(mozHandlerDisabled, ":-moz-handler-disabled",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_HANDLER_DISABLED)
CSS_STATE_PSEUDO_CLASS(mozHandlerBlocked, ":-moz-handler-blocked",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_HANDLER_BLOCKED)
CSS_STATE_PSEUDO_CLASS(mozHandlerCrashed, ":-moz-handler-crashed",
                       CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME, "",
                       NS_EVENT_STATE_HANDLER_CRASHED)

CSS_STATE_PSEUDO_CLASS(mozMathIncrementScriptLevel,
                       ":-moz-math-increment-script-level", 0, "",
                       NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL)

// CSS 3 UI
// http://www.w3.org/TR/2004/CR-css3-ui-20040511/#pseudo-classes
CSS_STATE_PSEUDO_CLASS(required, ":required", 0, "", NS_EVENT_STATE_REQUIRED)
CSS_STATE_PSEUDO_CLASS(optional, ":optional", 0, "", NS_EVENT_STATE_OPTIONAL)
CSS_STATE_PSEUDO_CLASS(valid, ":valid", 0, "", NS_EVENT_STATE_VALID)
CSS_STATE_PSEUDO_CLASS(invalid, ":invalid", 0, "", NS_EVENT_STATE_INVALID)
CSS_STATE_PSEUDO_CLASS(inRange, ":in-range", 0, "", NS_EVENT_STATE_INRANGE)
CSS_STATE_PSEUDO_CLASS(outOfRange, ":out-of-range", 0, "", NS_EVENT_STATE_OUTOFRANGE)
CSS_STATE_PSEUDO_CLASS(defaultPseudo, ":default", 0, "", NS_EVENT_STATE_DEFAULT)
CSS_STATE_PSEUDO_CLASS(placeholderShown, ":placeholder-shown", 0, "",
                       NS_EVENT_STATE_PLACEHOLDERSHOWN)
CSS_STATE_PSEUDO_CLASS(mozReadOnly, ":-moz-read-only", 0, "",
                       NS_EVENT_STATE_MOZ_READONLY)
CSS_STATE_PSEUDO_CLASS(mozReadWrite, ":-moz-read-write", 0, "",
                       NS_EVENT_STATE_MOZ_READWRITE)
CSS_STATE_PSEUDO_CLASS(mozSubmitInvalid, ":-moz-submit-invalid", 0, "",
                       NS_EVENT_STATE_MOZ_SUBMITINVALID)
CSS_STATE_PSEUDO_CLASS(mozUIInvalid, ":-moz-ui-invalid", 0, "",
                       NS_EVENT_STATE_MOZ_UI_INVALID)
CSS_STATE_PSEUDO_CLASS(mozUIValid, ":-moz-ui-valid", 0, "",
                       NS_EVENT_STATE_MOZ_UI_VALID)
CSS_STATE_PSEUDO_CLASS(mozMeterOptimum, ":-moz-meter-optimum", 0, "",
                       NS_EVENT_STATE_OPTIMUM)
CSS_STATE_PSEUDO_CLASS(mozMeterSubOptimum, ":-moz-meter-sub-optimum", 0, "",
                       NS_EVENT_STATE_SUB_OPTIMUM)
CSS_STATE_PSEUDO_CLASS(mozMeterSubSubOptimum, ":-moz-meter-sub-sub-optimum", 0, "",
                       NS_EVENT_STATE_SUB_SUB_OPTIMUM)

// Those values should be parsed but do nothing.
CSS_STATE_PSEUDO_CLASS(mozPlaceholder, ":-moz-placeholder", 0, "", NS_EVENT_STATE_IGNORE)

#ifdef DEFINED_CSS_STATE_PSEUDO_CLASS
#undef DEFINED_CSS_STATE_PSEUDO_CLASS
#undef CSS_STATE_PSEUDO_CLASS
#endif

#ifdef DEFINED_CSS_STATE_DEPENDENT_PSEUDO_CLASS
#undef DEFINED_CSS_STATE_DEPENDENT_PSEUDO_CLASS
#undef CSS_STATE_DEPENDENT_PSEUDO_CLASS
#endif
