/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// nsNativeScrollbarFrame
//
// A class that wraps a nsIWidget that is a native scrollbar
// and keeps the two in sync with the rest of gecko. Allows us to
// shim a native scrollbar into the GFX scroll-view mechanism.
// 

#ifndef nsNativeScrollbarFrame_h__
#define nsNativeScrollbarFrame_h__


#include "nsScrollbarFrame.h"
#include "nsIWidget.h"

class nsISupportsArray;
class nsIPresShell;
class nsPresContext;
class nsIContent;
class nsStyleContext;

nsresult NS_NewNativeScrollbarFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;


class nsNativeScrollbarFrame : public nsBoxFrame
{
public:
  nsNativeScrollbarFrame(nsIPresShell* aShell);
  virtual ~nsNativeScrollbarFrame ( ) ;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("NativeScrollbarFrame"), aResult);
  }
#endif

  NS_IMETHOD Init(nsPresContext* aPresContext, nsIContent* aContent,
                    nsIFrame* aParent, nsStyleContext* aContext, nsIFrame* aPrevInFlow);
           
  // nsIFrame overrides
  NS_IMETHOD AttributeChanged(nsPresContext* aPresContext, nsIContent* aChild,
                              PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                              PRInt32 aModType);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize);
                        
protected:
  
  void Hookup();

  nsresult FindScrollbar(nsIFrame* start, nsIFrame** outFrame, nsIContent** outContent);
  
  PRBool IsVertical() const { return mIsVertical; }
                  
private:

  PRPackedBool mIsVertical;
  PRPackedBool mScrollbarNeedsContent;
  nsCOMPtr<nsIWidget> mScrollbar;
  
}; // class nsNativeScrollbarFrame

#endif
