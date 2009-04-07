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

/* atom list for CSS anonymous boxes */

/*
 * This file contains the list of nsIAtoms and their values for CSS
 * pseudo-element-ish things used internally for anonymous boxes.  It is
 * designed to be used as inline input to nsCSSAnonBoxes.cpp *only*
 * through the magic of C preprocessing.  All entries must be enclosed
 * in the macro CSS_ANON_BOX which will have cruel and unusual things
 * done to it.  The entries should be kept in some sort of logical
 * order.  The first argument to CSS_ANON_BOX is the C++ identifier of
 * the atom.  The second argument is the string value of the atom.
 */

// OUTPUT_CLASS=nsCSSAnonBoxes
// MACRO_NAME=CSS_ANON_BOX

CSS_ANON_BOX(mozNonElement, ":-moz-non-element")

CSS_ANON_BOX(mozAnonymousBlock, ":-moz-anonymous-block")
CSS_ANON_BOX(mozAnonymousPositionedBlock, ":-moz-anonymous-positioned-block")
CSS_ANON_BOX(mozMathMLAnonymousBlock, ":-moz-mathml-anonymous-block")
CSS_ANON_BOX(mozXULAnonymousBlock, ":-moz-xul-anonymous-block")

CSS_ANON_BOX(mozLineFrame, ":-moz-line-frame")

CSS_ANON_BOX(buttonContent, ":-moz-button-content")
CSS_ANON_BOX(mozButtonLabel, ":-moz-buttonlabel")
CSS_ANON_BOX(cellContent, ":-moz-cell-content")
CSS_ANON_BOX(dropDownList, ":-moz-dropdown-list")
CSS_ANON_BOX(fieldsetContent, ":-moz-fieldset-content")
CSS_ANON_BOX(framesetBlank, ":-moz-frameset-blank")
CSS_ANON_BOX(mozDisplayComboboxControlFrame, ":-moz-display-comboboxcontrol-frame")

CSS_ANON_BOX(inlineTable, ":-moz-inline-table")
CSS_ANON_BOX(table, ":-moz-table")
CSS_ANON_BOX(tableCell, ":-moz-table-cell")
CSS_ANON_BOX(tableColGroup, ":-moz-table-column-group")
CSS_ANON_BOX(tableCol, ":-moz-table-column")
CSS_ANON_BOX(tableOuter, ":-moz-table-outer")
CSS_ANON_BOX(tableRowGroup, ":-moz-table-row-group")
CSS_ANON_BOX(tableRow, ":-moz-table-row")

CSS_ANON_BOX(canvas, ":-moz-canvas")
CSS_ANON_BOX(pageBreak, ":-moz-pagebreak")
CSS_ANON_BOX(page, ":-moz-page")
CSS_ANON_BOX(pageContent, ":-moz-pagecontent")
CSS_ANON_BOX(pageSequence, ":-moz-page-sequence")
CSS_ANON_BOX(scrolledContent, ":-moz-scrolled-content")
CSS_ANON_BOX(scrolledCanvas, ":-moz-scrolled-canvas")
CSS_ANON_BOX(scrolledPageSequence, ":-moz-scrolled-page-sequence")
CSS_ANON_BOX(columnContent, ":-moz-column-content")
CSS_ANON_BOX(viewport, ":-moz-viewport")
CSS_ANON_BOX(viewportScroll, ":-moz-viewport-scroll")

#ifdef MOZ_XUL
CSS_ANON_BOX(moztreecolumn, ":-moz-tree-column")
CSS_ANON_BOX(moztreerow, ":-moz-tree-row")
CSS_ANON_BOX(moztreeseparator, ":-moz-tree-separator")
CSS_ANON_BOX(moztreecell, ":-moz-tree-cell")
CSS_ANON_BOX(moztreeindentation, ":-moz-tree-indentation")
CSS_ANON_BOX(moztreeline, ":-moz-tree-line")
CSS_ANON_BOX(moztreetwisty, ":-moz-tree-twisty")
CSS_ANON_BOX(moztreeimage, ":-moz-tree-image")
CSS_ANON_BOX(moztreecelltext, ":-moz-tree-cell-text")
CSS_ANON_BOX(moztreecheckbox, ":-moz-tree-checkbox")
CSS_ANON_BOX(moztreeprogressmeter, ":-moz-tree-progressmeter")
CSS_ANON_BOX(moztreedropfeedback, ":-moz-tree-drop-feedback")
#endif

#ifdef MOZ_MATHML
CSS_ANON_BOX(mozMathStretchy, ":-moz-math-stretchy")
CSS_ANON_BOX(mozMathAnonymous, ":-moz-math-anonymous")
CSS_ANON_BOX(mozMathInline, ":-moz-math-inline")
#endif

#ifdef MOZ_SVG
CSS_ANON_BOX(mozSVGForeignContent, ":-moz-svg-foreign-content")
#endif
