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
class nsITextContent;
class nsIURL;
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

extern nsresult NS_NewAreaFrame(nsIFrame*& aNewFrame, PRUint32 aFlags);
extern nsresult NS_NewBRFrame(nsIFrame*& aNewFrame);
extern nsresult NS_NewBlockFrame(nsIFrame*& aNewFrame, PRUint32 aFlags);

// Flags for block/area frames
#define NS_BLOCK_SHRINK_WRAP     0x1
#define NS_BLOCK_NO_AUTO_MARGINS 0x2
#define NS_BLOCK_MARGIN_ROOT     0x4
#define NS_BLOCK_DOCUMENT_ROOT   0x8
#define NS_AREA_NO_SPACE_MGR     0x10

extern nsresult NS_NewCommentFrame(nsIFrame*& aFrameResult);
extern nsresult NS_NewHRFrame(nsIFrame*& aNewFrame);

// <frame> and <iframe> 
extern nsresult NS_NewHTMLFrameOuterFrame(nsIFrame*& aNewFrame);
// <frameset>
extern nsresult NS_NewHTMLFramesetFrame(nsIFrame*& aNewFrame);

extern nsresult NS_NewViewportFrame(nsIFrame*& aNewFrame);
extern nsresult NS_NewRootFrame(nsIFrame*& aNewFrame);
extern nsresult NS_NewImageFrame(nsIFrame*& aFrameResult);
extern nsresult NS_NewInlineFrame(nsIFrame*& aNewFrame);
extern nsresult NS_NewPositionedInlineFrame(nsIFrame*& aNewFrame);
extern nsresult NS_NewObjectFrame(nsIFrame*& aFrameResult);
extern nsresult NS_NewSpacerFrame(nsIFrame*& aResult);
extern nsresult NS_NewTextFrame(nsIFrame*& aResult);
extern nsresult NS_NewWBRFrame(nsIFrame*& aResult);
extern nsresult NS_NewScrollFrame(nsIFrame*& aResult);
extern nsresult NS_NewSimplePageSequenceFrame(nsIFrame*& aResult);
extern nsresult NS_NewPageFrame(nsIFrame*& aResult);

// forms
extern nsresult NS_NewFormFrame(nsIFrame*& aResult);
extern nsresult NS_NewButtonControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewImageControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewHTMLButtonControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewCheckboxControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewFieldSetFrame(nsIFrame*& aResult);
extern nsresult NS_NewFileControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewLabelFrame(nsIFrame*& aResult);
extern nsresult NS_NewLegendFrame(nsIFrame*& aResult);
extern nsresult NS_NewTextControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewRadioControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewSelectControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewListControlFrame(nsIFrame*& aResult);
extern nsresult NS_NewComboboxControlFrame(nsIFrame*& aResult);

// table frame factories

extern nsresult NS_NewTableOuterFrame(nsIFrame*& aResult);
extern nsresult NS_NewTableFrame(nsIFrame*& aResult);
extern nsresult NS_NewTableColFrame(nsIFrame*& aResult);
extern nsresult NS_NewTableColGroupFrame(nsIFrame*& aResult);
extern nsresult NS_NewTableRowFrame(nsIFrame*& aResult);
extern nsresult NS_NewTableRowGroupFrame(nsIFrame*& aResult);
extern nsresult NS_NewTableCellFrame(nsIFrame*& aResult);


// XXX passing aWebShell into this is wrong
extern nsresult NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                                      nsIDocument* aDoc,
                                      nsIURL* aURL,
                                      nsIWebShell* aWebShell);

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
