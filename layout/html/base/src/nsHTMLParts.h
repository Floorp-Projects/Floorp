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
class nsIWebWidget;

// XXX naming consistency puhleeze!

extern nsresult NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                                      nsIDocument* aDoc,
                                      nsIURL* aURL,
                                      nsIWebWidget* aWebWidget);

// Create an html root part
extern nsresult
  NS_NewRootPart(nsIHTMLContent** aInstancePtrResult,
                 nsIDocument* aDocument);
extern nsresult
  NS_NewHTMLHead(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag);

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

/** Create a new horizontal rule content object (e.g <HR>.) */
extern nsresult
NS_NewHRulePart(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag);

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
NS_NewHTMLImage(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag);

/** Create a new HTML reflow command */
extern nsresult
NS_NewHTMLReflowCommand(nsIReflowCommand**           aInstancePtrResult,
                        nsIFrame*                    aTargetFrame,
                        nsIReflowCommand::ReflowType aReflowType,
                        nsIFrame*                    aChildFrame = nsnull);

#endif /* nsHTMLParts_h___ */
