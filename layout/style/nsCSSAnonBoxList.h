/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

// Framesets
CSS_ANON_BOX(horizontalFramesetBorder, ":-moz-hframeset-border")
CSS_ANON_BOX(verticalFramesetBorder, ":-moz-vframeset-border")

CSS_ANON_BOX(mozLineFrame, ":-moz-line-frame")

CSS_ANON_BOX(buttonContent, ":-moz-button-content")
CSS_ANON_BOX(mozButtonLabel, ":-moz-buttonlabel")
CSS_ANON_BOX(cellContent, ":-moz-cell-content")
CSS_ANON_BOX(dropDownList, ":-moz-dropdown-list")
CSS_ANON_BOX(fieldsetContent, ":-moz-fieldset-content")
CSS_ANON_BOX(framesetBlank, ":-moz-frameset-blank")
CSS_ANON_BOX(mozDisplayComboboxControlFrame, ":-moz-display-comboboxcontrol-frame")
CSS_ANON_BOX(htmlCanvasContent, ":-moz-html-canvas-content")

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

// Inside a flex container, a contiguous run of non-replaced inline children
// gets wrapped in an anonymous block, which is then treated as a flex item.
CSS_ANON_BOX(anonymousFlexItem, ":-moz-anonymous-flex-item")

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

CSS_ANON_BOX(mozSVGForeignContent, ":-moz-svg-foreign-content")
