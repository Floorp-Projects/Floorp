/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// Force references to all of the symbols that we want exported from
// the dll that are located in the .lib files we link with

#include "nsString.h"
#include "nsIPresShell.h"
#include "nsIPrintContext.h"
#include "nsIPresContext.h"
#include "nsIStyleSet.h"
#include "nsIDocument.h"
#include "nsHTMLParts.h"
#include "nsINameSpaceManager.h"

void XXXNeverCalled()
{
  nsIPresShell* ps;
  NS_NewPresShell(&ps);
  nsIPresContext* cx;
  nsIPrintContext *px;
  NS_NewGalleyContext(&cx);
  NS_NewPrintPreviewContext(&cx);
  NS_NewPrintContext(&px);
  nsIStyleSet* ss;
  NS_NewStyleSet(&ss);
  nsIDocument* doc;
  NS_NewHTMLDocument(&doc);
  NS_NewImageDocument(&doc);
  nsIFrame* f;
  NS_NewTextFrame(ps, &f);
  NS_NewInlineFrame(ps, &f);
  NS_NewBRFrame(ps, &f);
  NS_NewWBRFrame(ps, &f);
  NS_NewHRFrame(ps, &f);
  NS_NewObjectFrame(ps, &f);
  NS_NewSpacerFrame(ps, &f);
  NS_NewHTMLFramesetFrame(ps, &f);
  NS_NewCanvasFrame(ps, &f);
  NS_NewScrollFrame(ps, &f);
  NS_NewSimplePageSequenceFrame(ps, &f);
  nsINameSpaceManager* nsm;
  NS_NewNameSpaceManager(&nsm);
  NS_CreateHTMLElement(nsnull, nsnull);
}
