/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsFrame.h"
#include "nsHTMLIIDs.h"

#define nsHTMLEmbedSuper nsHTMLContainer

class nsHTMLEmbed : public nsHTMLEmbedSuper {
public:
  nsHTMLEmbed(nsIAtom* aTag);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

protected:
  virtual ~nsHTMLEmbed();
};

nsHTMLEmbed::nsHTMLEmbed(nsIAtom* aTag)
  : nsHTMLEmbedSuper(aTag)
{
}

nsHTMLEmbed::~nsHTMLEmbed()
{
}

nsresult
nsHTMLEmbed::CreateFrame(nsIPresContext*  aPresContext,
                         nsIFrame*        aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*&       aResult)
{
  nsIFrame* frame;
  nsresult rv = NS_NewObjectFrame(frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

nsresult
NS_NewHTMLEmbed(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLEmbed(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
