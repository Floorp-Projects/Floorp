/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS anonymous boxes */

/*
 * This file contains the list of nsIAtoms and their values for CSS
 * pseudo-element-ish things used internally for anonymous boxes.  It is
 * designed to be used as inline input to nsCSSAnonBoxes.cpp *only* through the
 * magic of C preprocessing.  All entries must be enclosed in the macros
 * CSS_ANON_BOX and CSS_NON_INHERITING_ANON_BOX which will have cruel and
 * unusual things done to it.  The entries should be kept in some sort of
 * logical order.
 *
 * The first argument to CSS_ANON_BOX/CSS_NON_INHERITING_ANON_BOX is the C++
 * identifier of the atom.
 *
 * The second argument is the string value of the atom.
 *
 * The third argument to CSS_ANON_BOX is whether the anonymous box skips parent-
 * based display fixups (such as blockification inside flex containers).  This
 * is implicitly true for CSS_NON_INHERITING_ANON_BOX.
 *
 * CSS_NON_INHERITING_ANON_BOX is used for anon boxes that never inherit style
 * from anything.  This means all their property values are the initial values
 * of those properties.
 */

// OUTPUT_CLASS=nsCSSAnonBoxes
// MACRO_NAME=CSS_ANON_BOX/CSS_NON_INHERITING_ANON_BOX

#ifndef CSS_NON_INHERITING_ANON_BOX
#  ifdef DEFINED_CSS_NON_INHERITING_ANON_BOX
#    error "Recursive includes of nsCSSAnonBoxList.h?"
#  endif /* DEFINED_CSS_NON_INHERITING_ANON_BOX */
#  define CSS_NON_INHERITING_ANON_BOX(name_, value_) CSS_ANON_BOX(name_, value_)
#  define DEFINED_CSS_NON_INHERITING_ANON_BOX
#endif /* CSS_NON_INHERITING_ANON_BOX */

// ::-moz-text, ::-moz-oof-placeholder, and ::-moz-first-letter-continuation are
// non-elements which no rule will match.
CSS_ANON_BOX(mozText, ":-moz-text")
// placeholder frames for out of flows.  Note that :-moz-placeholder is used for
// the pseudo-element that represents the placeholder text in <input
// placeholder="foo">, so we need a different string here.
CSS_NON_INHERITING_ANON_BOX(oofPlaceholder, ":-moz-oof-placeholder")
// nsFirstLetterFrames for content outside the ::first-letter.
CSS_ANON_BOX(firstLetterContinuation, ":-moz-first-letter-continuation")

CSS_ANON_BOX(mozBlockInsideInlineWrapper, ":-moz-block-inside-inline-wrapper")
CSS_ANON_BOX(mozMathMLAnonymousBlock, ":-moz-mathml-anonymous-block")
CSS_ANON_BOX(mozXULAnonymousBlock, ":-moz-xul-anonymous-block")

// Framesets
CSS_NON_INHERITING_ANON_BOX(horizontalFramesetBorder, ":-moz-hframeset-border")
CSS_NON_INHERITING_ANON_BOX(verticalFramesetBorder, ":-moz-vframeset-border")

CSS_ANON_BOX(mozLineFrame, ":-moz-line-frame")

CSS_ANON_BOX(buttonContent, ":-moz-button-content")
CSS_ANON_BOX(cellContent, ":-moz-cell-content")
CSS_ANON_BOX(dropDownList, ":-moz-dropdown-list")
CSS_ANON_BOX(fieldsetContent, ":-moz-fieldset-content")
CSS_NON_INHERITING_ANON_BOX(framesetBlank, ":-moz-frameset-blank")
CSS_ANON_BOX(mozDisplayComboboxControlFrame, ":-moz-display-comboboxcontrol-frame")
CSS_ANON_BOX(htmlCanvasContent, ":-moz-html-canvas-content")

CSS_ANON_BOX(inlineTable, ":-moz-inline-table")
CSS_ANON_BOX(table, ":-moz-table")
CSS_ANON_BOX(tableCell, ":-moz-table-cell")
CSS_ANON_BOX(tableColGroup, ":-moz-table-column-group")
CSS_ANON_BOX(tableCol, ":-moz-table-column")
CSS_ANON_BOX(tableWrapper, ":-moz-table-wrapper")
CSS_ANON_BOX(tableRowGroup, ":-moz-table-row-group")
CSS_ANON_BOX(tableRow, ":-moz-table-row")

CSS_ANON_BOX(canvas, ":-moz-canvas")
CSS_NON_INHERITING_ANON_BOX(pageBreak, ":-moz-pagebreak")
CSS_ANON_BOX(page, ":-moz-page")
CSS_ANON_BOX(pageContent, ":-moz-pagecontent")
CSS_ANON_BOX(pageSequence, ":-moz-page-sequence")
CSS_ANON_BOX(scrolledContent, ":-moz-scrolled-content")
CSS_ANON_BOX(scrolledCanvas, ":-moz-scrolled-canvas")
CSS_ANON_BOX(scrolledPageSequence, ":-moz-scrolled-page-sequence")
CSS_ANON_BOX(columnContent, ":-moz-column-content")
CSS_ANON_BOX(viewport, ":-moz-viewport")
CSS_ANON_BOX(viewportScroll, ":-moz-viewport-scroll")

// Inside a flex container, a contiguous run of text gets wrapped in
// an anonymous block, which is then treated as a flex item.
CSS_ANON_BOX(anonymousFlexItem, ":-moz-anonymous-flex-item")

// Inside a grid container, a contiguous run of text gets wrapped in
// an anonymous block, which is then treated as a grid item.
CSS_ANON_BOX(anonymousGridItem, ":-moz-anonymous-grid-item")

CSS_ANON_BOX(ruby, ":-moz-ruby")
CSS_ANON_BOX(rubyBase, ":-moz-ruby-base")
CSS_ANON_BOX(rubyBaseContainer, ":-moz-ruby-base-container")
CSS_ANON_BOX(rubyText, ":-moz-ruby-text")
CSS_ANON_BOX(rubyTextContainer, ":-moz-ruby-text-container")

#ifdef MOZ_XUL
CSS_ANON_BOX(mozTreeColumn, ":-moz-tree-column")
CSS_ANON_BOX(mozTreeRow, ":-moz-tree-row")
CSS_ANON_BOX(mozTreeSeparator, ":-moz-tree-separator")
CSS_ANON_BOX(mozTreeCell, ":-moz-tree-cell")
CSS_ANON_BOX(mozTreeIndentation, ":-moz-tree-indentation")
CSS_ANON_BOX(mozTreeLine, ":-moz-tree-line")
CSS_ANON_BOX(mozTreeTwisty, ":-moz-tree-twisty")
CSS_ANON_BOX(mozTreeImage, ":-moz-tree-image")
CSS_ANON_BOX(mozTreeCellText, ":-moz-tree-cell-text")
CSS_ANON_BOX(mozTreeCheckbox, ":-moz-tree-checkbox")
CSS_ANON_BOX(mozTreeProgressmeter, ":-moz-tree-progressmeter")
CSS_ANON_BOX(mozTreeDropFeedback, ":-moz-tree-drop-feedback")
#endif

CSS_ANON_BOX(mozSVGMarkerAnonChild, ":-moz-svg-marker-anon-child")
CSS_ANON_BOX(mozSVGOuterSVGAnonChild, ":-moz-svg-outer-svg-anon-child")
CSS_ANON_BOX(mozSVGForeignContent, ":-moz-svg-foreign-content")
CSS_ANON_BOX(mozSVGText, ":-moz-svg-text")

#ifdef DEFINED_CSS_NON_INHERITING_ANON_BOX
#  undef DEFINED_CSS_NON_INHERITING_ANON_BOX
#  undef CSS_NON_INHERITING_ANON_BOX
#endif /* DEFINED_CSS_NON_INHERITING_ANON_BOX */
