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
#  define CSS_NON_INHERITING_ANON_BOX(name_, value_) CSS_ANON_BOX(name_, value_, true)
#  define DEFINED_CSS_NON_INHERITING_ANON_BOX
#endif /* CSS_NON_INHERITING_ANON_BOX */

// ::-moz-text, ::-moz-oof-placeholder, and ::-moz-first-letter-continuation are
// non-elements which no rule will match.
CSS_ANON_BOX(mozText, ":-moz-text", false)
// placeholder frames for out of flows.  Note that :-moz-placeholder is used for
// the pseudo-element that represents the placeholder text in <input
// placeholder="foo">, so we need a different string here.
CSS_NON_INHERITING_ANON_BOX(oofPlaceholder, ":-moz-oof-placeholder")
// nsFirstLetterFrames for content outside the ::first-letter.
CSS_ANON_BOX(firstLetterContinuation, ":-moz-first-letter-continuation", false)

CSS_ANON_BOX(mozBlockInsideInlineWrapper, ":-moz-block-inside-inline-wrapper", false)
CSS_ANON_BOX(mozMathMLAnonymousBlock, ":-moz-mathml-anonymous-block", false)
CSS_ANON_BOX(mozXULAnonymousBlock, ":-moz-xul-anonymous-block", false)

// Framesets
CSS_NON_INHERITING_ANON_BOX(horizontalFramesetBorder, ":-moz-hframeset-border")
CSS_NON_INHERITING_ANON_BOX(verticalFramesetBorder, ":-moz-vframeset-border")

CSS_ANON_BOX(mozLineFrame, ":-moz-line-frame", false)

CSS_ANON_BOX(buttonContent, ":-moz-button-content", false)
CSS_ANON_BOX(cellContent, ":-moz-cell-content", false)
CSS_ANON_BOX(dropDownList, ":-moz-dropdown-list", false)
CSS_ANON_BOX(fieldsetContent, ":-moz-fieldset-content", false)
CSS_NON_INHERITING_ANON_BOX(framesetBlank, ":-moz-frameset-blank")
CSS_ANON_BOX(mozDisplayComboboxControlFrame, ":-moz-display-comboboxcontrol-frame", true)
CSS_ANON_BOX(htmlCanvasContent, ":-moz-html-canvas-content", false)

CSS_ANON_BOX(inlineTable, ":-moz-inline-table", false)
CSS_ANON_BOX(table, ":-moz-table", false)
CSS_ANON_BOX(tableCell, ":-moz-table-cell", false)
CSS_ANON_BOX(tableColGroup, ":-moz-table-column-group", false)
CSS_ANON_BOX(tableCol, ":-moz-table-column", false)
CSS_ANON_BOX(tableWrapper, ":-moz-table-wrapper", false)
CSS_ANON_BOX(tableRowGroup, ":-moz-table-row-group", false)
CSS_ANON_BOX(tableRow, ":-moz-table-row", false)

CSS_ANON_BOX(canvas, ":-moz-canvas", false)
CSS_NON_INHERITING_ANON_BOX(pageBreak, ":-moz-pagebreak")
CSS_ANON_BOX(page, ":-moz-page", false)
CSS_ANON_BOX(pageContent, ":-moz-pagecontent", false)
CSS_ANON_BOX(pageSequence, ":-moz-page-sequence", false)
CSS_ANON_BOX(scrolledContent, ":-moz-scrolled-content", false)
CSS_ANON_BOX(scrolledCanvas, ":-moz-scrolled-canvas", false)
CSS_ANON_BOX(scrolledPageSequence, ":-moz-scrolled-page-sequence", false)
CSS_ANON_BOX(columnContent, ":-moz-column-content", false)
CSS_ANON_BOX(viewport, ":-moz-viewport", false)
CSS_ANON_BOX(viewportScroll, ":-moz-viewport-scroll", false)

// Inside a flex container, a contiguous run of text gets wrapped in
// an anonymous block, which is then treated as a flex item.
CSS_ANON_BOX(anonymousFlexItem, ":-moz-anonymous-flex-item", false)

// Inside a grid container, a contiguous run of text gets wrapped in
// an anonymous block, which is then treated as a grid item.
CSS_ANON_BOX(anonymousGridItem, ":-moz-anonymous-grid-item", false)

CSS_ANON_BOX(ruby, ":-moz-ruby", false)
CSS_ANON_BOX(rubyBase, ":-moz-ruby-base", false)
CSS_ANON_BOX(rubyBaseContainer, ":-moz-ruby-base-container", false)
CSS_ANON_BOX(rubyText, ":-moz-ruby-text", false)
CSS_ANON_BOX(rubyTextContainer, ":-moz-ruby-text-container", false)

#ifdef MOZ_XUL
CSS_ANON_BOX(moztreecolumn, ":-moz-tree-column", false)
CSS_ANON_BOX(moztreerow, ":-moz-tree-row", false)
CSS_ANON_BOX(moztreeseparator, ":-moz-tree-separator", false)
CSS_ANON_BOX(moztreecell, ":-moz-tree-cell", false)
CSS_ANON_BOX(moztreeindentation, ":-moz-tree-indentation", false)
CSS_ANON_BOX(moztreeline, ":-moz-tree-line", false)
CSS_ANON_BOX(moztreetwisty, ":-moz-tree-twisty", false)
CSS_ANON_BOX(moztreeimage, ":-moz-tree-image", false)
CSS_ANON_BOX(moztreecelltext, ":-moz-tree-cell-text", false)
CSS_ANON_BOX(moztreecheckbox, ":-moz-tree-checkbox", false)
CSS_ANON_BOX(moztreeprogressmeter, ":-moz-tree-progressmeter", false)
CSS_ANON_BOX(moztreedropfeedback, ":-moz-tree-drop-feedback", false)
#endif

CSS_ANON_BOX(mozSVGMarkerAnonChild, ":-moz-svg-marker-anon-child", false)
CSS_ANON_BOX(mozSVGOuterSVGAnonChild, ":-moz-svg-outer-svg-anon-child", false)
CSS_ANON_BOX(mozSVGForeignContent, ":-moz-svg-foreign-content", false)
CSS_ANON_BOX(mozSVGText, ":-moz-svg-text", false)

#ifdef DEFINED_CSS_NON_INHERITING_ANON_BOX
#  undef DEFINED_CSS_NON_INHERITING_ANON_BOX
#  undef CSS_NON_INHERITING_ANON_BOX
#endif /* DEFINED_CSS_NON_INHERITING_ANON_BOX */
