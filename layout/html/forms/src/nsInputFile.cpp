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

#include "nsInputFile.h"
#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"

class nsInputFileFrame : public nsInputFrame {
public:
  nsInputFileFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

protected:
  virtual ~nsInputFileFrame();
};

nsInputFileFrame::nsInputFileFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsInputFileFrame::~nsInputFileFrame()
{
}


void 
nsInputFileFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
}

//----------------------------------------------------------------------
// nsInputFile

nsInputFile::nsInputFile(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager) 
{
}

nsInputFile::~nsInputFile()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}

nsresult
nsInputFile::CreateFrame(nsIPresContext* aPresContext,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult)
{
  nsIFrame* frame = new nsInputFileFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void nsInputFile::GetType(nsString& aResult) const
{
  aResult = "file";
}


nsresult
NS_NewHTMLInputFile(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputFile(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

