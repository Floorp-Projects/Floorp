/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 */

#ifndef nsMathMLContainerFrame_h___
#define nsMathMLContainerFrame_h___

#include "nsCOMPtr.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsInlineFrame.h"
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#include "nsMathMLChar.h"
#include "nsIMathMLFrame.h"
#include "nsMathMLParts.h"

/*
 * Base class for MathML container frames. It acts like an inferred 
 * mrow. By default, this frame uses its Reflow() method to lay its 
 * children horizontally and ensure that their baselines are aligned.
 * 
 * This frame is a *math-aware frame* in the sense that given the markup
 * <tag>base arguments</tag>, the method InsertScriptLevelStyleContext()
 * can be used to render the 'base' in normal-font, and the 'arguments'
 * in small-font. This is a common functionality to tags like msub, msup, 
 * msubsup, mover, munder, munderover, mmultiscripts. All are  derived 
 * from this base class. They use SetInitialChildList() to trigger
 * InsertScriptLevelStyleContext() for the very first time as soon as all
 * their children are known. However, each of these tags has its own Reflow()
 * method to lay its children as appropriate, thus overriding the default 
 * Reflow() method in this base class.
 * 
 * Other tags like mi that do not have 'arguments' can be derived from
 * this base class as well. The class caters for empty arguments.
 * Notice that mi implements its own SetInitialChildList() method 
 * to switch to a normal-font (rather than italics) if its text content 
 * is not a single character (as per the MathML REC).
 * 
 */

class nsMathMLContainerFrame : public nsHTMLContainerFrame, public nsIMathMLFrame {
public:

  // nsIMathMLFrame methods  

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_IMETHOD Stretch(nsIPresContext&    aPresContext,
                     nsStretchDirection aStretchDirection,
                     nsCharMetrics&     aContainerSize,
                     nsCharMetrics&     aDesiredStretchSize);
  NS_IMETHOD
  GetPresentationData(PRInt32* aScriptLevel, 
                      PRBool*  aDisplayStyle);

  NS_IMETHOD
  UpdatePresentationData(PRInt32 aScriptLevelIncrement, 
                         PRBool  aDisplayStyle);

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32 aIndex, 
                                    PRInt32 aScriptLevelIncrement,
                                    PRBool  aDisplayStyle);

  // nsHTMLContainerFrame methods
 
  NS_IMETHOD
  Init(nsIPresContext&  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext& aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList);

  NS_IMETHOD
  Reflow(nsIPresContext&          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  // helper method for altering the style contexts of subscript/superscript elements

  NS_IMETHOD
  InsertScriptLevelStyleContext(nsIPresContext& aPresContext);

  // helper methods for processing empty MathML frames (with whitespace only)

  static PRBool
  IsOnlyWhitespace(nsIFrame* aFrame);
  
  static void
  ReflowEmptyChild(nsIPresContext& aPresContext,
                   nsIFrame*       aFrame);

protected:

  PRInt32 mScriptLevel;  // Relevant to nested frames within: msub, msup, msubsup, munder,
                         // mover, munderover, mmultiscripts, mfrac, mroot, mtable.

  PRBool mDisplayStyle;  // displaystyle="false" is intended to slightly alter how the
                         // rendering is done in inline mode.

  virtual PRIntn GetSkipSides() const { return 0; }
};

#endif /* nsMathMLContainerFrame_h___ */
