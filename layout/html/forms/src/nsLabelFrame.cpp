/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsInlineFrame.h"

#include "nsIPresShell.h"
#include "nsFormControlFrame.h"
#include "nsIDOMHTMLAnchorElement.h"

typedef nsInlineFrame nsLabelFrameSuper;

class nsLabelFrame : public nsLabelFrameSuper
{
public:
  nsLabelFrame();
  virtual ~nsLabelFrame();

  NS_IMETHOD Destroy(nsIPresContext *aPresContext);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame** aFrame);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("Label"), aResult);
  }
#endif

};

nsresult
NS_NewLabelFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsLabelFrame* it = new (aPresShell) nsLabelFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // set the state flags (if any are provided)
  nsFrameState state;
  it->GetFrameState( &state );
  state |= aStateFlags;
  it->SetFrameState( state );

  *aNewFrame = it;
  return NS_OK;
}

nsLabelFrame::nsLabelFrame()
  : nsLabelFrameSuper()
{
}

nsLabelFrame::~nsLabelFrame()
{
}


NS_IMETHODIMP
nsLabelFrame::Destroy(nsIPresContext *aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(aPresContext,
                                    NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  return nsLabelFrameSuper::Destroy(aPresContext);
}

NS_IMETHODIMP 
nsLabelFrame::Reflow(nsIPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  if (eReflowReason_Initial == aReflowState.reason)
    nsFormControlFrame::RegUnRegAccessKey(aPresContext,
                                     NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
  return nsLabelFrameSuper::Reflow(aPresContext, aDesiredSize,
                                   aReflowState, aStatus);
}

NS_IMETHODIMP
nsLabelFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                               const nsPoint& aPoint,
                               nsFramePaintLayer aWhichLayer,
                               nsIFrame** aFrame)
{
  nsresult rv = nsLabelFrameSuper::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  if (rv == NS_OK) {
    nsCOMPtr<nsIFormControlFrame> controlFrame = do_QueryInterface(*aFrame);
    if (!controlFrame) {
      // if the hit frame isn't a form control then
      // check to see if it is an anchor

      // XXX It could be something else that should get the event.  Perhaps
      // this is better handled by event bubbling?

      nsIFrame * parent;
      (*aFrame)->GetParent(&parent);
      while (parent != this && parent != nsnull) {
        nsCOMPtr<nsIContent> content;
        parent->GetContent(getter_AddRefs(content));
        nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement(do_QueryInterface(content));
        if (anchorElement) {
          nsIStyleContext *psc;
          parent->GetStyleContext(&psc);
          if (psc) {
            const nsStyleVisibility* vis = 
             (const nsStyleVisibility*)psc->GetStyleData(eStyleStruct_Visibility);
      
            if (vis->IsVisible()) {
              *aFrame = parent;
              return NS_OK;
            }
          }
        }
        parent->GetParent(&parent);
      }
      const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      
      if (vis->IsVisible()) {
        *aFrame = this;
        return NS_OK;
      }
    }
  }

  return rv;
}
