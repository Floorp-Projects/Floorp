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
#include "nsBodyFrame.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"

class BodyPart : public nsHTMLContainer {
public:
  BodyPart(nsIAtom* aTag);

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                nsIFrame* aParentFrame);

protected:
  virtual ~BodyPart();
};

BodyPart::BodyPart(nsIAtom* aTag)
  : nsHTMLContainer(aTag)
{
}

BodyPart::~BodyPart()
{
}

nsrefcnt BodyPart::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt BodyPart::Release(void)
{
  if (--mRefCnt == 0) {
    delete this;
    return 0;
  }
  return mRefCnt;
}

nsIFrame* BodyPart::CreateFrame(nsIPresContext* aPresContext,
                                nsIFrame* aParentFrame)
{
  nsIFrame* rv;
  nsresult status = nsBodyFrame::NewFrame(&rv, this, aParentFrame);
  return rv;
}

nsresult
NS_NewBodyPart(nsIHTMLContent** aInstancePtrResult,
               nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new BodyPart(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
