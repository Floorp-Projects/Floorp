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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsXULLeafFrame_h___
#define nsXULLeafFrame_h___

#include "nsLeafFrame.h"
#include "nsIBox.h"

class nsAccessKeyInfo;

class nsXULLeafFrame : public nsLeafFrame, public nsIBox
{
public:

  friend nsresult NS_NewXULLeafFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  // nsIBox frame interface
  NS_IMETHOD GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize);
  NS_IMETHOD InvalidateCache(nsIFrame* aChild);
  NS_IMETHOD SetDebug(nsIPresContext* aPresContext, PRBool aDebug) { return NS_OK; }

  NS_DECL_ISUPPORTS


  NS_IMETHOD GetFrameName(nsString& aResult) const;

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
 
    NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);


protected:

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);


  nsXULLeafFrame() {}

private:

}; // class nsXULLeafFrame

#endif /* nsXULLeafFrame_h___ */
