/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMathMLTokenFrame_h___
#define nsMathMLTokenFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// Base class to handle token elements
//

class nsMathMLTokenFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  virtual nsIAtom* GetType() const;

  NS_IMETHOD
  Init(nsPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsStyleContext*  aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(nsPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD
  Place(nsPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD
  ReflowDirtyChild(nsIPresShell* aPresShell,
                   nsIFrame*     aChild);

  NS_IMETHOD
  AttributeChanged(nsPresContext* aPresContext,
                   nsIContent*     aChild,
                   PRInt32         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   PRInt32         aModType);
protected:
  nsMathMLTokenFrame();
  virtual ~nsMathMLTokenFrame();

  virtual PRIntn GetSkipSides() const { return 0; }

  // hook to perform MathML-specific actions depending on the tag
  virtual void
  ProcessTextData(nsPresContext* aPresContext);

  // helper to set the style of <mi> which has to be italic or normal
  // depending on its textual content
  void SetTextStyle(nsPresContext* aPresContext);

  // helper to set the quotes of <ms>
  void SetQuotes(nsPresContext* aPresContext);
};

#endif /* nsMathMLTokentFrame_h___ */
