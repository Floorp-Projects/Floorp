/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsHTMLParts_h___
#define nsHTMLParts_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsIReflowCommand.h"
class nsIArena;
class nsIAtom;
class nsIContent;
class nsIDocument;
class nsIHTMLContent;
class nsIHTMLContentSink;
class nsIHTMLFragmentContentSink;
class nsITextContent;
class nsIURI;
class nsString;
class nsIWebShell;

// Factory methods for creating html content objects
// XXX argument order is wrong (out parameter should be last)
extern nsresult
NS_NewHTMLAnchorElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLAppletElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLAreaElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLBRElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLBaseElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLBaseFontElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLBodyElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLButtonElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLDListElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLDelElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLDirectoryElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLDivElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLEmbedElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLFieldSetElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLFontElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLFormElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLFrameElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLFrameSetElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLHRElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLHeadElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLHeadingElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLHtmlElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLIFrameElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLImageElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLInputElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLInsElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLIsIndexElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLLIElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLLabelElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLLayerElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLLegendElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLLinkElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLMapElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLMenuElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLMetaElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLModElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLOListElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLObjectElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLOptGroupElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLOptionElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLParagraphElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLParamElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLPreElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLQuoteElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLScriptElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLSelectElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLSpacerElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLSpanElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLStyleElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTableCaptionElement(nsIHTMLContent** aResult,nsIAtom* aTag);

extern nsresult
NS_NewHTMLTableCellElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTableColElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTableColGroupElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTableElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTableRowElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTableSectionElement(nsIHTMLContent** aResult,nsIAtom* aTag);

extern nsresult
NS_NewHTMLTbodyElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTextAreaElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTfootElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTheadElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLTitleElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLUListElement(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult
NS_NewHTMLWBRElement(nsIHTMLContent** aResult, nsIAtom* aTag);

/**
 * Create a new content object for the given tag.
 * Returns NS_ERROR_NOT_AVAILABLE for an unknown/unhandled tag.
 * Returns some other error on error.
 * Returns NS_OK on success
 */
PR_EXTERN(nsresult)
NS_CreateHTMLElement(nsIHTMLContent** aResult,
                     const nsString& aTag);

// Factory methods for creating html layout objects

// These are variations on AreaFrame with slightly different layout
// policies.

// Flags for block/area frames
#define NS_BLOCK_SHRINK_WRAP     0x1
#define NS_BLOCK_NO_AUTO_MARGINS 0x2
#define NS_BLOCK_MARGIN_ROOT     0x4
#define NS_BLOCK_DOCUMENT_ROOT   0x8
#define NS_AREA_NO_SPACE_MGR     0x10
#define NS_AREA_WRAP_SIZE        0x20

// Create a basic area frame. By default, area frames will extend
// their height to cover any children that "stick out".
extern nsresult NS_NewAreaFrame(nsIFrame** aNewFrame,
                                PRUint32 aFlags = NS_AREA_WRAP_SIZE);

// These AreaFrame's shrink wrap around their contents
inline nsresult NS_NewTableCellInnerFrame(nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aNewFrame, NS_AREA_WRAP_SIZE);
}
inline nsresult NS_NewTableCaptionFrame(nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aNewFrame, NS_AREA_WRAP_SIZE);
}

// This type of AreaFrame is the document root and is a margin root for
// margin collapsing.
inline nsresult NS_NewDocumentElementFrame(nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aNewFrame, NS_BLOCK_DOCUMENT_ROOT|NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame is a margin root, but does not shrink wrap
inline nsresult NS_NewAbsoluteItemWrapperFrame(nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aNewFrame, NS_BLOCK_MARGIN_ROOT);
}

// This type of AreaFrame shrink wraps
inline nsresult NS_NewFloatingItemWrapperFrame(nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aNewFrame, NS_AREA_WRAP_SIZE|NS_BLOCK_SHRINK_WRAP);
}

// This type of AreaFrame doesn't use its own space manager and
// doesn't shrink wrap.
inline nsresult NS_NewRelativeItemWrapperFrame(nsIFrame** aNewFrame) {
  return NS_NewAreaFrame(aNewFrame, NS_AREA_NO_SPACE_MGR);
}

extern nsresult NS_NewBRFrame(nsIFrame** aNewFrame);

// Create a frame that supports "display: block" layout behavior
extern nsresult NS_NewBlockFrame(nsIFrame** aNewFrame);

extern nsresult NS_NewCommentFrame(nsIFrame** aFrameResult);
extern nsresult NS_NewHRFrame(nsIFrame** aNewFrame);

// <frame> and <iframe> 
extern nsresult NS_NewHTMLFrameOuterFrame(nsIFrame** aNewFrame);
// <frameset>
extern nsresult NS_NewHTMLFramesetFrame(nsIFrame** aNewFrame);

extern nsresult NS_NewViewportFrame(nsIFrame** aNewFrame);
extern nsresult NS_NewRootFrame(nsIFrame** aNewFrame);
extern nsresult NS_NewImageFrame(nsIFrame** aFrameResult);
extern nsresult NS_NewInlineFrame(nsIFrame** aNewFrame);
extern nsresult NS_NewPositionedInlineFrame(nsIFrame** aNewFrame);
extern nsresult NS_NewObjectFrame(nsIFrame** aFrameResult);
extern nsresult NS_NewSpacerFrame(nsIFrame** aResult);
extern nsresult NS_NewTextFrame(nsIFrame** aResult);
extern nsresult NS_NewEmptyFrame(nsIFrame** aResult);
inline nsresult NS_NewWBRFrame(nsIFrame** aResult) {
  return NS_NewEmptyFrame(aResult);
}
extern nsresult NS_NewScrollFrame(nsIFrame** aResult);
extern nsresult NS_NewSimplePageSequenceFrame(nsIFrame** aResult);
extern nsresult NS_NewPageFrame(nsIFrame** aResult);
extern nsresult NS_NewFirstLetterFrame(nsIFrame** aNewFrame);
extern nsresult NS_NewFirstLineFrame(nsIFrame** aNewFrame);

// forms
extern nsresult NS_NewFormFrame(nsIFrame** aResult);
extern nsresult NS_NewGfxButtonControlFrame(nsIFrame** aResult);
extern nsresult NS_NewNativeButtonControlFrame(nsIFrame** aResult);
extern nsresult NS_NewImageControlFrame(nsIFrame** aResult);
extern nsresult NS_NewHTMLButtonControlFrame(nsIFrame** aResult);
extern nsresult NS_NewGfxCheckboxControlFrame(nsIFrame** aResult);
extern nsresult NS_NewNativeCheckboxControlFrame(nsIFrame** aResult);
extern nsresult NS_NewFieldSetFrame(nsIFrame** aResult);
extern nsresult NS_NewFileControlFrame(nsIFrame** aResult);
extern nsresult NS_NewLabelFrame(nsIFrame** aResult);
extern nsresult NS_NewLegendFrame(nsIFrame** aResult);
extern nsresult NS_NewNativeTextControlFrame(nsIFrame** aNewFrame);
extern nsresult NS_NewGfxTextControlFrame(nsIFrame** aNewFrame);
extern nsresult NS_NewGfxRadioControlFrame(nsIFrame** aResult);
extern nsresult NS_NewNativeRadioControlFrame(nsIFrame** aResult);
extern nsresult NS_NewNativeSelectControlFrame(nsIFrame** aResult);
extern nsresult NS_NewListControlFrame(nsIFrame** aResult);
extern nsresult NS_NewComboboxControlFrame(nsIFrame** aResult);

// Table frame factories
extern nsresult NS_NewTableOuterFrame(nsIFrame** aResult);
extern nsresult NS_NewTableFrame(nsIFrame** aResult);
extern nsresult NS_NewTableColFrame(nsIFrame** aResult);
extern nsresult NS_NewTableColGroupFrame(nsIFrame** aResult);
extern nsresult NS_NewTableRowFrame(nsIFrame** aResult);
extern nsresult NS_NewTableRowGroupFrame(nsIFrame** aResult);
extern nsresult NS_NewTableCellFrame(nsIFrame** aResult);

// XXX passing aWebShell into this is wrong
extern nsresult NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                                      nsIDocument* aDoc,
                                      nsIURI* aURL,
                                      nsIWebShell* aWebShell);
extern nsresult NS_NewHTMLFragmentContentSink(nsIHTMLFragmentContentSink** aInstancePtrResult);

/** Create a new HTML reflow command */
extern nsresult
NS_NewHTMLReflowCommand(nsIReflowCommand**           aInstancePtrResult,
                        nsIFrame*                    aTargetFrame,
                        nsIReflowCommand::ReflowType aReflowType,
                        nsIFrame*                    aChildFrame = nsnull,
                        nsIAtom*                     aAttribute = nsnull);

/** Create a new HTML 'FrameInserted' reflow command */
extern nsresult
NS_NewHTMLReflowCommand(nsIReflowCommand** aInstancePtrResult,
                        nsIFrame*          aTargetFrame,
                        nsIFrame*          aChildFrame,
                        nsIFrame*          aPrevSiblingFrame);

#endif /* nsHTMLParts_h___ */
