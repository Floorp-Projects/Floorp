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
#include "nsXIFConverter.h"

#define nsHTMLTitleSuper nsHTMLTagContent

class nsHTMLTitle : public nsHTMLTitleSuper {
public:
  nsHTMLTitle(nsIAtom* aTag, const nsString& aTitle);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  virtual void List(FILE* out, PRInt32 aIndent) const;


  virtual void BeginConvertToXIF(nsXIFConverter& aConverter) const;
  virtual void ConvertContentToXIF(nsXIFConverter& aConverter) const;
  virtual void FinishConvertToXIF(nsXIFConverter& aConverter) const;

protected:
  virtual ~nsHTMLTitle();
  nsString mTitle;
};

nsHTMLTitle::nsHTMLTitle(nsIAtom* aTag, const nsString& aTitle)
  : nsHTMLTitleSuper(aTag), mTitle(aTitle)
{
}

nsHTMLTitle::~nsHTMLTitle()
{
}

nsresult
nsHTMLTitle::CreateFrame(nsIPresContext*  aPresContext,
                         nsIFrame*        aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*&       aResult)
{
  nsIFrame* frame;
  nsFrame::NewFrame(&frame, this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void
nsHTMLTitle::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsIAtom* tag = GetTag();
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }

  ListAttributes(out);

  fprintf(out, " RefCount=%d<", mRefCnt);
  fputs(mTitle, out);
  fputs(">\n", out);
}

nsresult
NS_NewHTMLTitle(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag, const nsString& aTitle)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTitle(aTag, aTitle);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

/**
 * Translate the content object into the (XIF) XML Interchange Format
 * XIF is an intermediate form of the content model, the buffer
 * will then be parsed into any number of formats including HTML, TXT, etc.
 * These methods must be called in the following order:
   
      BeginConvertToXIF
      ConvertContentToXIF
      EndConvertToXIF
 */

void nsHTMLTitle::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {
    nsAutoString name;
    mTag->ToString(name);
    aConverter.BeginContainer(name);
  }

}

void nsHTMLTitle::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  aConverter.AddContent(mTitle);
}

void nsHTMLTitle::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  if (nsnull != mTag)
  {  
    nsAutoString name;
    mTag->ToString(name);
    aConverter.EndContainer(name);
  }
}
