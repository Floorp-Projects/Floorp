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

#ifndef nsFileControlFrame_h___
#define nsFileControlFrame_h___

#include "nsAreaFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIStatefulFrame.h"
#include "nsIEditor.h"


class nsIPresState;
class nsGfxTextControlFrame;
class nsFormFrame;
class nsISupportsArray;
class nsIHTMLContent;
class nsIEditor;
class nsISelectionController;





class nsGfxTextControlFrame2 : public nsHTMLContainerFrame,
                           public nsIAnonymousContentCreator
{
public:
  nsGfxTextControlFrame2();
  virtual ~nsGfxTextControlFrame2();

  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
    aResult.AssignWithConversion("nsGfxControlFrame2");
    return NS_OK;
  }
#endif

  // from nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsIPresContext* aPresContext,
                                    nsISupportsArray& aChildList);
  NS_DECL_ISUPPORTS_INHERITED
protected:
  virtual PRIntn GetSkipSides() const;

//helper methods
  virtual PRBool IsSingleLineTextControl() const;
  virtual PRBool IsPlainTextControl() const;
  virtual PRBool IsPasswordTextControl() const;

  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                               nsIContent *      aContent,
                               nsIFrame**        aFrame);
private:
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsISelectionController> mSelCon;
};

#endif


