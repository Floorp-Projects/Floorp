/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* factory functions for rendering object classes */

#ifndef nsHTMLParts_h___
#define nsHTMLParts_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsIFrame.h"
class nsIAtom;
class nsNodeInfoManager;
class nsIContent;
class nsIContentIterator;
class nsIDocument;
class nsIFrame;
class nsIHTMLContentSink;
class nsIFragmentContentSink;
class nsStyleContext;
class nsIURI;
class nsString;
class nsIPresShell;
class nsIChannel;
class nsTableColFrame;

// These are all the block specific frame bits, they are copied from
// the prev-in-flow to a newly created next-in-flow, except for the
// NS_BLOCK_FLAGS_NON_INHERITED_MASK bits below.
#define NS_BLOCK_FLAGS_MASK (NS_BLOCK_MARGIN_ROOT              | \
                             NS_BLOCK_FLOAT_MGR                | \
                             NS_BLOCK_CLIP_PAGINATED_OVERFLOW  | \
                             NS_BLOCK_HAS_FIRST_LETTER_STYLE   | \
                             NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET | \
                             NS_BLOCK_HAS_FIRST_LETTER_CHILD   | \
                             NS_BLOCK_FRAME_HAS_INSIDE_BULLET)

// This is the subset of NS_BLOCK_FLAGS_MASK that is NOT inherited
// by default.  They should only be set on the first-in-flow.
// See nsBlockFrame::Init.
#define NS_BLOCK_FLAGS_NON_INHERITED_MASK                        \
                            (NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET | \
                             NS_BLOCK_HAS_FIRST_LETTER_CHILD   | \
                             NS_BLOCK_FRAME_HAS_INSIDE_BULLET)

// Factory methods for creating html layout objects

// Create a frame that supports "display: block" layout behavior
class nsBlockFrame;
nsBlockFrame*
NS_NewBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsFrameState aFlags = nsFrameState(0));

// Special Generated Content Node. It contains text taken from an
// attribute of its *grandparent* content node. 
nsresult
NS_NewAttributeContent(nsNodeInfoManager *aNodeInfoManager,
                       int32_t aNameSpaceID, nsIAtom* aAttrName,
                       nsIContent** aResult);

// Create a basic area frame but the GetFrameForPoint is overridden to always
// return the option frame 
// By default, area frames will extend
// their height to cover any children that "stick out".
nsContainerFrame*
NS_NewSelectsAreaFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsFrameState aFlags);

// Create a block formatting context blockframe
inline nsBlockFrame* NS_NewBlockFormattingContext(nsIPresShell* aPresShell,
                                                  nsStyleContext* aStyleContext)
{
  return NS_NewBlockFrame(aPresShell, aStyleContext,
                          NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT);
}

nsIFrame*
NS_NewBRFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewCommentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

// <frame> and <iframe> 
nsIFrame*
NS_NewSubDocumentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
// <frameset>
nsIFrame*
NS_NewHTMLFramesetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class ViewportFrame;
ViewportFrame*
NS_NewViewportFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsCanvasFrame;
nsCanvasFrame*
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsInlineFrame;
nsInlineFrame*
NS_NewInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewEmptyFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
inline nsIFrame*
NS_NewWBRFrame(nsIPresShell* aPresShell, nsStyleContext* aContext) {
  return NS_NewEmptyFrame(aPresShell, aContext);
}

nsContainerFrame*
NS_NewColumnSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsFrameState aStateFlags);

class nsSimplePageSequenceFrame;
nsSimplePageSequenceFrame*
NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsPageFrame;
nsPageFrame*
NS_NewPageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsPageContentFrame;
nsPageContentFrame*
NS_NewPageContentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsFirstLetterFrame;
nsFirstLetterFrame*
NS_NewFirstLetterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsFirstLineFrame;
nsFirstLineFrame*
NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

// forms
nsContainerFrame*
NS_NewGfxButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNativeButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewImageControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewHTMLButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNativeCheckboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewColorControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewLegendFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNativeTextControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTextControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewGfxAutoTextControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewGfxRadioControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNativeRadioControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNativeSelectControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewListControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsFrameState aFlags);
nsIFrame*
NS_NewProgressFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewMeterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewRangeFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNumberControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

// Table frame factories
class nsTableOuterFrame;
nsTableOuterFrame*
NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsTableFrame;
nsTableFrame*
NS_NewTableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsTableCaptionFrame;
nsTableCaptionFrame*
NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsTableColFrame*
NS_NewTableColFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsTableColGroupFrame;
nsTableColGroupFrame*
NS_NewTableColGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsTableRowFrame;
nsTableRowFrame*
NS_NewTableRowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsTableRowGroupFrame;
nsTableRowGroupFrame*
NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
class nsTableCellFrame;
nsTableCellFrame*
NS_NewTableCellFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsBorderCollapse);

nsresult
NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                      nsIDocument* aDoc, nsIURI* aURL,
                      nsISupports* aContainer, // e.g. docshell
                      nsIChannel* aChannel);
nsresult
NS_NewHTMLFragmentContentSink(nsIFragmentContentSink** aInstancePtrResult);
nsresult
NS_NewHTMLFragmentContentSink2(nsIFragmentContentSink** aInstancePtrResult);

#endif /* nsHTMLParts_h___ */
