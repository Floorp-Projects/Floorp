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

#include "nsInputPassword.h"
#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"

#include "nsIStyleContext.h"
#include "nsIFontMetrics.h"
#include "nsWidgetsCID.h"
#include "nsITextWidget.h"

static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);

class nsInputPasswordFrame : public nsInputFrame {
public:
  nsInputPasswordFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
                   nsIFrame* aParentFrame);

  virtual void PreInitializeWidget(nsIPresContext* aPresContext, 
	                               nsSize& aBounds);

  virtual void InitializeWidget(nsIView *aView);

  virtual const nsIID GetCID();

  virtual const nsIID GetIID();

protected:
  virtual ~nsInputPasswordFrame();
  nsString mCacheValue;
};

nsInputPasswordFrame::nsInputPasswordFrame(nsIContent* aContent,
                                   PRInt32 aIndexInParent,
                                   nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsInputPasswordFrame::~nsInputPasswordFrame()
{
}

const nsIID
nsInputPasswordFrame::GetIID()
{
  static NS_DEFINE_IID(kTextIID, NS_ITEXTWIDGET_IID);
  return kTextIID;
}
  
const nsIID
nsInputPasswordFrame::GetCID()
{
  static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
  return kTextCID;
}

void 
nsInputPasswordFrame::PreInitializeWidget(nsIPresContext* aPresContext, 
                                          nsSize& aBounds)
{
  nsInputText* content = (nsInputText *)mContent;

  // get the value of the text
  if (nsnull != content->mValue) {
    mCacheValue = *content->mValue;
  } else {
    mCacheValue = "";
  }

  nsIStyleContext* styleContext = mStyleContext;
  nsStyleFont* styleFont = (nsStyleFont*)styleContext->GetData(kStyleFontSID);
  nsIFontMetrics* fm = aPresContext->GetMetricsFor(styleFont->mFont);

  float p2t = aPresContext->GetPixelsToTwips();

#define DEFAULT_SIZE 21
  PRInt32 size = content->mSize;
  if (size <= 0) {
    size = DEFAULT_SIZE;
  }

  // XXX 6 should come from widget: we tell widget what the font is,
  // tell it mSize, let it tell us it's width and height

  aBounds.width  = size * fm->GetWidth(' ') + nscoord(6 * p2t);
  aBounds.height = fm->GetHeight() + nscoord(6 * p2t);

  NS_RELEASE(fm);
}

void 
nsInputPasswordFrame::InitializeWidget(nsIView *aView)
{
  nsITextWidget* text;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&text)) {
    text->SetText(mCacheValue);
    NS_RELEASE(text);
  }
}

//----------------------------------------------------------------------
// nsInputPassword

nsInputPassword::nsInputPassword(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInputText(aTag, aManager)
{
}

nsInputPassword::~nsInputPassword()
{
}

nsIFrame* 
nsInputPassword::CreateFrame(nsIPresContext* aPresContext,
                         PRInt32 aIndexInParent,
                         nsIFrame* aParentFrame)
{
  nsIFrame* rv = new nsInputPasswordFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

NS_HTML nsresult
NS_NewHTMLInputPassword(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputPassword(aTag, aManager);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

