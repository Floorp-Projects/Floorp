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

/* factory functions for rendering object classes */

#ifndef nsHTMLParts_h___
#define nsHTMLParts_h___

#include "nscore.h"
#include "nsISupports.h"
class nsIAtom;
class nsNodeInfoManager;
class nsIContent;
class nsIContentIterator;
class nsIDocument;
class nsIFrame;
class nsIHTMLContentSink;
class nsIFragmentContentSink;
class nsPresContext;
class nsStyleContext;
class nsIURI;
class nsString;
class nsIPresShell;
class nsIChannel;

/**
 * Additional frame-state bits used by nsBlockFrame
 * See the meanings at http://www.mozilla.org/newlayout/doc/block-and-line.html
 */
#define NS_BLOCK_NO_AUTO_MARGINS            0x00200000
#define NS_BLOCK_MARGIN_ROOT                0x00400000
#define NS_BLOCK_SPACE_MGR                  0x00800000
#define NS_BLOCK_HAS_FIRST_LETTER_STYLE     0x20000000
#define NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET   0x40000000
// These are the bits that get inherited from a block frame to its
// next-in-flows and are not private to blocks
#define NS_BLOCK_FLAGS_MASK                 0xF0F00000

// Factory methods for creating html layout objects

// These are variations on AreaFrame with slightly different layout
// policies.

// Create a frame that supports "display: block" layout behavior
nsIFrame*
NS_NewBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags = 0);

// Special Generated Content Frame
nsresult
NS_NewAttributeContent(nsNodeInfoManager *aNodeInfoManager,
                       PRInt32 aNameSpaceID, nsIAtom* aAttrName,
                       nsIContent** aResult);

// Create a basic area frame but the GetFrameForPoint is overridden to always
// return the option frame 
// By default, area frames will extend
// their height to cover any children that "stick out".
nsIFrame*
NS_NewSelectsAreaFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);

// Create a basic area frame.
nsIFrame*
NS_NewAreaFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);

// These AreaFrame's shrink wrap around their contents
inline nsIFrame*
NS_NewTableCellInnerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext) {
  return NS_NewBlockFrame(aPresShell, aContext, NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame is the document root, a margin root, and the
// initial containing block for absolutely positioned elements
inline nsIFrame*
NS_NewDocumentElementFrame(nsIPresShell* aPresShell, nsStyleContext* aContext) {
  return NS_NewAreaFrame(aPresShell, aContext, NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame is a margin root, but does not shrink wrap
inline nsIFrame*
NS_NewAbsoluteItemWrapperFrame(nsIPresShell* aPresShell, nsStyleContext* aContext) {
  return NS_NewAreaFrame(aPresShell, aContext, NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame shrink wraps
inline nsIFrame*
NS_NewFloatingItemWrapperFrame(nsIPresShell* aPresShell, nsStyleContext* aContext) {
  return NS_NewAreaFrame(aPresShell, aContext,
    NS_BLOCK_SPACE_MGR|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame doesn't use its own space manager and
// doesn't shrink wrap.
inline nsIFrame*
NS_NewRelativeItemWrapperFrame(nsIPresShell* aPresShell, nsStyleContext* aContext) {
  return NS_NewAreaFrame(aPresShell, aContext, 0);
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

nsIFrame*
NS_NewViewportFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewPositionedInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSpacerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
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

nsIFrame*
NS_NewColumnSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aStateFlags);

nsIFrame*
NS_NewSimplePageSequenceFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewPageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewPageContentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewFirstLetterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

// forms
nsIFrame*
NS_NewGfxButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNativeButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewImageControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewHTMLButtonControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewNativeCheckboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
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
nsIFrame*
NS_NewListControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);
nsIFrame*
NS_NewIsIndexFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

// Table frame factories
nsIFrame*
NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTableColFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTableColGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTableRowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewTableCellFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsBorderCollapse);

nsresult
NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                      nsIDocument* aDoc, nsIURI* aURL,
                      nsISupports* aContainer, // e.g. docshell
                      nsIChannel* aChannel);
nsresult
NS_NewHTMLFragmentContentSink(nsIFragmentContentSink** aInstancePtrResult);
nsresult
NS_NewHTMLFragmentContentSink2(nsIFragmentContentSink** aInstancePtrResult);

// This strips all but a whitelist of elements and attributes defined
// in nsContentSink.h
nsresult
NS_NewHTMLParanoidFragmentSink(nsIFragmentContentSink** aInstancePtrResult);
void
NS_HTMLParanoidFragmentSinkShutdown();
#endif /* nsHTMLParts_h___ */
