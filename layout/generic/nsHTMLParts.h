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
extern nsresult NS_NewHTMLAnchor(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLApplet(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLArea(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLBR(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLBase(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLBaseFont(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLBody(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLButton(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLDList(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLDel(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLDiv(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLEmbed(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLFont(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLForm(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLFrame(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLFrameSet(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLHR(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLHead(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLHeading(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLHtml(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLIFrame(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLImage(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLInput(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLIns(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLIsIndex(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLLI(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLLabel(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLLayer(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLLegend(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLLink(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLMap(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLMenu(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLMeta(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLMod(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLOList(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLObject(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLOptGroup(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLOption(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLParagraph(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLParam(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLPre(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLQuote(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLScript(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLSelect(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLSpacer(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLStyle(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTableCaption(nsIHTMLContent** aResult,nsIAtom* aTag);
extern nsresult NS_NewHTMLTableCell(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTableCol(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTable(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTableRow(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTableSection(nsIHTMLContent** aResult,nsIAtom* aTag);
extern nsresult NS_NewHTMLTbody(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTextArea(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTfoot(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLThead(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLTitle(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLUList(nsIHTMLContent** aResult, nsIAtom* aTag);
extern nsresult NS_NewHTMLWBR(nsIHTMLContent** aResult, nsIAtom* aTag);

extern nsresult NS_NewHTMLComment(nsIHTMLContent** aResult, nsIAtom* aTag,
                                  const nsString& aComment);

// Factory methods for creating html layout objects

// Everything below this line is obsolete...
//----------------------------------------------------------------------
// XXX naming consistency puhleeze!

// XXX passing aWebShell into this is wrong
extern nsresult NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                                      nsIDocument* aDoc,
                                      nsIURL* aURL,
                                      nsIWebShell* aWebShell);

/**
 * Create a new content object for the given tag.
 * Returns NS_ERROR_NOT_AVAILABLE for an unknown/unhandled tag.
 * Returns some other error on error.
 * Returns NS_OK on success
 */
extern nsresult NS_CreateHTMLElement(nsIHTMLContent** aInstancePtrResult,
                                     const nsString& aTag);

// Create an html root part
extern nsresult
  NS_NewRootPart(nsIHTMLContent** aInstancePtrResult,
                 nsIDocument* aDocument);

// Head parts
extern nsresult
  NS_NewHTMLHead(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag);
extern nsresult
  NS_NewHTMLMeta(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag);
extern nsresult
  NS_NewHTMLTitle(nsIHTMLContent** aInstancePtrResult,
                  nsIAtom* aTag,
                  const nsString& aTitle);

// Create an html body part
extern nsresult
  NS_NewBodyPart(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag);

// Create a new generic html container (e.g. P, DIV, SPAN, B, I, etc).
// Not used for tables or framesets (see below).
extern nsresult
  NS_NewHTMLContainer(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag);
extern nsresult
  NS_NewHTMLContainer(nsIHTMLContent** aInstancePtrResult,
                      nsIArena* aArena, nsIAtom* aTag);

extern nsresult
  NS_NewHTMLBreak(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag);

extern nsresult
  NS_NewHTMLWordBreak(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag);

extern nsresult
  NS_NewHTMLSpacer(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag);

extern nsresult
  NS_NewHTMLBullet(nsIHTMLContent** aInstancePtrResult);

// Create a new text content object. The memory for the unicode string
// will be allocated from the heap. The object returned will support
// the nsIHTMLContent interface as well as the nsITextContent
// interface
extern nsresult
  NS_NewHTMLText(nsIHTMLContent** aInstancePtrResult,
                 const PRUnichar* us, PRInt32 uslen = 0);
extern nsresult
  NS_NewHTMLText(nsIHTMLContent** aInstancePtrResult,
                 nsIArena* aArena, const PRUnichar* us, PRInt32 uslen = 0);

// Create a new text content object. The memory for the unicode string
// is allocated by the caller. The caller is responsible for
// deallocating the memory (e.g. use an nsIArena). The object returned
// will support the nsIHTMLContent interface as well as the
// nsITextContent interface
extern nsresult
  NS_NewSharedHTMLText(nsIHTMLContent** aInstancePtrResult,
                       PRUnichar* us, PRInt32 uslen = 0);
extern nsresult
  NS_NewSharedHTMLText(nsIHTMLContent** aInstancePtrResult,
                       nsIArena* aArena, PRUnichar* us, PRInt32 uslen = 0);


/** Create a new table content object <TABLE> */
extern nsresult
NS_NewTablePart(nsIHTMLContent** aInstancePtrResult,
               nsIAtom* aTag);

/** Create a new table row group object <THEAD> <TBODY> <TFOOT> */
extern nsresult
NS_NewTableRowGroupPart(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag);

/** Create a new table row content object <TR> */
extern nsresult
NS_NewTableRowPart(nsIHTMLContent** aInstancePtrResult,
               nsIAtom* aTag);

/** Create a new table column group content object <COLGROUP> */
extern nsresult
NS_NewTableColGroupPart(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag);

/** Create a new table column content object <COL> */
extern nsresult
NS_NewTableColPart(nsIHTMLContent** aInstancePtrResult,
                   nsIAtom* aTag); 

/** Create a new table cell content object <TD> */
extern nsresult
NS_NewTableCellPart(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag);

/** Create a new table caption content object <CAPTION> */
extern nsresult
NS_NewTableCaptionPart(nsIHTMLContent** aInstancePtrResult,
                       nsIAtom* aTag);

extern nsresult
NS_NewHTMLEmbed(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag);

extern nsresult
NS_NewHTMLObject(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag);

extern nsresult
NS_NewHTMLImage(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag);

extern nsresult
NS_NewHTMLLayer(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag);

/** Create a new HTML reflow command */
extern nsresult
NS_NewHTMLReflowCommand(nsIReflowCommand**           aInstancePtrResult,
                        nsIFrame*                    aTargetFrame,
                        nsIReflowCommand::ReflowType aReflowType,
                        nsIFrame*                    aChildFrame = nsnull);

extern nsresult
NS_NewObjectFrame(nsIContent* aContent,
                  nsIFrame* aParentFrame,
                  nsIFrame*& aFrameResult);

extern nsresult
NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIWebShell* aWebShell);

extern nsresult
NS_NewHTMLFrame(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag, nsIWebShell* aWebShell);

extern nsresult
NS_NewHTMLFrameset(nsIHTMLContent** aInstancePtrResult,
                   nsIAtom* aTag, nsIWebShell* aWebShell);

#endif /* nsHTMLParts_h___ */
