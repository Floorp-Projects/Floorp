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

#ifndef nsGfxButtonControlFrame_h___
#define nsGfxButtonControlFrame_h___

#include "nsFormControlFrame.h"
#include "nsHTMLButtonControlFrame.h"
#include "nsCOMPtr.h"
#include "nsIAnonymousContentCreator.h"
#include "nsITextContent.h"

// Class which implements the input[type=button, reset, submit] and
// browse button for input[type=file].
// The label for button is specified through generated content
// in the ua.css file.

class nsGfxButtonControlFrame : public nsHTMLButtonControlFrame,
                                public nsIAnonymousContentCreator
{
public:
  nsGfxButtonControlFrame();

     //nsIFrame
  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  virtual const nsIID& GetCID();
  virtual const nsIID& GetIID();

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

 
   // nsFormControlFrame
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif
  PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

 
  // nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsIPresContext* aPresContext,
                                    nsISupportsArray& aChildList);
  NS_IMETHOD SetDocumentForAnonymousContent(nsIDocument* aDocument,
                                            PRBool aDeep,
                                            PRBool aCompileEventHandlers);
  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                            nsIContent *      aContent,
                            nsIFrame**        aFrame);

protected:
  NS_IMETHOD AddComputedBorderPaddingToDesiredSize(nsHTMLReflowMetrics& aDesiredSize,
                                                   const nsHTMLReflowState& aSuggestedReflowState);
  NS_IMETHOD DoNavQuirksReflow(nsIPresContext*          aPresContext, 
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState, 
                               nsReflowStatus&          aStatus);
  NS_IMETHOD GetDefaultLabel(nsString& aLabel);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);


  virtual PRBool IsReset(PRInt32 type);
  virtual PRBool IsSubmit(PRInt32 type);
  virtual PRBool IsBrowse(PRInt32 type); // Browse button of file input
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  nscoord mSuggestedWidth;
  nscoord mSuggestedHeight;
  nsCOMPtr<nsITextContent> mTextContent;
};


#endif

