/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* list of user agent style sheets that nsLayoutStylesheetCache manages */

/*
 * STYLE_SHEET(identifier_, url_, shared_)
 *
 * identifier_
 *   An identifier for the style sheet, suitable for use as an enum class value.
 *
 * url_
 *   The URL of the style sheet.
 *
 * shared_
 *   A boolean indicating whether the sheet can be safely placed in shared
 *   memory.
 */

STYLE_SHEET(ContentEditable, "resource://gre/res/contenteditable.css", true)
STYLE_SHEET(CounterStyles, "resource://gre-resources/counterstyles.css", true)
STYLE_SHEET(DesignMode, "resource://gre/res/designmode.css", true)
STYLE_SHEET(Forms, "resource://gre-resources/forms.css", true)
STYLE_SHEET(HTML, "resource://gre-resources/html.css", true)
STYLE_SHEET(MathML, "resource://gre-resources/mathml.css", true)
STYLE_SHEET(MinimalXUL, "chrome://global/content/minimal-xul.css", true)
STYLE_SHEET(NoFrames, "resource://gre-resources/noframes.css", true)
STYLE_SHEET(NoScript, "resource://gre-resources/noscript.css", true)
STYLE_SHEET(PluginProblem, "resource://gre-resources/pluginproblem.css", true)
STYLE_SHEET(Quirk, "resource://gre-resources/quirk.css", true)
STYLE_SHEET(Scrollbars, "chrome://global/skin/scrollbars.css", true)
STYLE_SHEET(SVG, "resource://gre/res/svg.css", true)
STYLE_SHEET(UA, "resource://gre-resources/ua.css", true)
STYLE_SHEET(XUL, "chrome://global/content/xul.css", false)
