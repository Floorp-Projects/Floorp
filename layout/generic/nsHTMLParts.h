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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#ifndef nsHTMLParts_h___
#define nsHTMLParts_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsReflowType.h"
class nsHTMLReflowCommand;
class nsIAtom;
class nsINodeInfo;
class nsIContent;
class nsIContentIterator;
class nsIDocument;
class nsIFrame;
class nsIHTMLContent;
class nsIHTMLContentSink;
class nsIFragmentContentSink;
class nsPresContext;
class nsITextContent;
class nsIURI;
class nsString;
class nsIWebShell;
class nsIPresShell;
class nsIChannel;

/**
 * Additional frame-state bits used by nsBlockFrame
 * See the meanings at http://www.mozilla.org/newlayout/doc/block-and-line.html
 */
#define NS_BLOCK_SHRINK_WRAP                0x00100000
#define NS_BLOCK_NO_AUTO_MARGINS            0x00200000
#define NS_BLOCK_MARGIN_ROOT                0x00400000
#define NS_BLOCK_SPACE_MGR                  0x00800000
#define NS_BLOCK_HAS_FIRST_LETTER_STYLE     0x20000000
#define NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET   0x40000000
// These are the bits that get inherited from a block frame to its
// next-in-flows and are not private to blocks
#define NS_BLOCK_FLAGS_MASK                 0xF0F00000

// Factory method for creating a content iterator for generated
// content
nsresult
NS_NewFrameContentIterator(nsPresContext*      aPresContext,
                           nsIFrame*            aFrame,
                           nsIContentIterator** aIterator);

// Factory methods for creating html layout objects

// These are variations on AreaFrame with slightly different layout
// policies.

// Create a frame that supports "display: block" layout behavior
nsresult
NS_NewBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame,
                 PRUint32 aFlags = 0);

// Special Generated Content Frame
nsresult
NS_NewAttributeContent(nsIContent* aContent, PRInt32 aNameSpaceID,
                       nsIAtom* aAttrName, nsIContent** aResult);

// Create a basic area frame but the GetFrameForPoint is overridden to always
// return the option frame 
// By default, area frames will extend
// their height to cover any children that "stick out".
nsresult
NS_NewSelectsAreaFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame,
                       PRUint32 aFlags);

// Create a basic area frame.
nsresult
NS_NewAreaFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame,
                PRUint32 aFlags);

// These AreaFrame's shrink wrap around their contents
inline nsresult
NS_NewTableCellInnerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame) {
  return NS_NewBlockFrame(aPresShell, aNewFrame,
                          NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame is the document root, a margin root, and the
// initial containing block for absolutely positioned elements
inline nsresult
NS_NewDocumentElementFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aPresShell, aNewFrame, NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame is a margin root, but does not shrink wrap
inline nsresult
NS_NewAbsoluteItemWrapperFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aPresShell, aNewFrame, NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame shrink wraps
inline nsresult
NS_NewFloatingItemWrapperFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aPresShell, aNewFrame, NS_BLOCK_SPACE_MGR|NS_BLOCK_SHRINK_WRAP|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame doesn't use its own space manager and
// doesn't shrink wrap.
inline nsresult
NS_NewRelativeItemWrapperFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aPresShell, aNewFrame, 0);
}

nsresult
NS_NewBRFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewCommentFrame(nsIPresShell* aPresShell, nsIFrame** aFrameResult);

// <frame> and <iframe> 
nsresult
NS_NewSubDocumentFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
// <frameset>
nsresult
NS_NewHTMLFramesetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewViewportFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewImageFrame(nsIPresShell* aPresShell, nsIFrame** aFrameResult);
nsresult
NS_NewInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewPositionedInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewObjectFrame(nsIPresShell* aPresShell, nsIFrame** aFrameResult);
nsresult
NS_NewSpacerFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewTextFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewEmptyFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
inline nsresult
NS_NewWBRFrame(nsIPresShell* aPresShell, nsIFrame** aResult) {
  return NS_NewEmptyFrame(aPresShell, aResult);
}

nsresult
NS_NewColumnSetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags );

nsresult
NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewPageFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewPageContentFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewFirstLetterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

// forms
nsresult
NS_NewGfxButtonControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewNativeButtonControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewImageControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewHTMLButtonControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewNativeCheckboxControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsIFrame** aResult, PRUint32 aFlags);
nsresult
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewLegendFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewNativeTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewGfxAutoTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
nsresult
NS_NewGfxRadioControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewNativeRadioControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewNativeSelectControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewListControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsIFrame** aResult, PRUint32 aFlags);
nsresult
NS_NewIsIndexFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

// Table frame factories
nsresult
NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewTableFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

nsresult
NS_NewTableColFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewTableColGroupFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewTableRowFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsIFrame** aResult);
nsresult
NS_NewTableCellFrame(nsIPresShell* aPresShell, PRBool aIsBorderCollapse, nsIFrame** aResult);

nsresult
NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                      nsIDocument* aDoc, nsIURI* aURL,
                      nsISupports* aContainer, // e.g. docshell
                      nsIChannel* aChannel);
nsresult
NS_NewHTMLFragmentContentSink(nsIFragmentContentSink** aInstancePtrResult);
nsresult
NS_NewHTMLFragmentContentSink2(nsIFragmentContentSink** aInstancePtrResult);

/** Create a new HTML reflow command */
nsresult
NS_NewHTMLReflowCommand(nsHTMLReflowCommand** aInstancePtrResult,
                        nsIFrame*             aTargetFrame,
                        nsReflowType          aReflowType,
                        nsIFrame*             aChildFrame = nsnull,
                        nsIAtom*              aAttribute = nsnull);

#endif /* nsHTMLParts_h___ */
