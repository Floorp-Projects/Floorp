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
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsFrame.h"
#include "nsHTMLIIDs.h"

// XXX temporary
nsresult NS_NewAppletFrame(nsIFrame** aFrameResult, nsIContent* aContent,
                           nsIFrame* aParentFrame)
{
  nsFrame::NewFrame(aFrameResult, aContent, aParentFrame);
  if (nsnull == *aFrameResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

#define nsHTMLAppletSuper nsHTMLContainer

class nsHTMLApplet : public nsHTMLAppletSuper {
public:
  nsHTMLApplet(nsIAtom* aTag);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

protected:
  virtual ~nsHTMLApplet();
  nsString mApplet;
};

nsHTMLApplet::nsHTMLApplet(nsIAtom* aTag)
  : nsHTMLAppletSuper(aTag)
{
}

nsHTMLApplet::~nsHTMLApplet()
{
}

nsresult
nsHTMLApplet::CreateFrame(nsIPresContext*  aPresContext,
                          nsIFrame*        aParentFrame,
                          nsIStyleContext* aStyleContext,
                          nsIFrame*&       aResult)
{
  nsIFrame* frame;
  nsresult rv = NS_NewAppletFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

nsresult
NS_NewHTMLApplet(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLApplet(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
