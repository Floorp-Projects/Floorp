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

//
// Mike Pinkerton
// Netscape Communications
//
// nsToolboxFrame is a layout object that contains one or more toolbar frames
// (specified as children in the DOM). These toolbars are laid out one on top
// of the other, and can be of varying heights but are all of the same width
// (the full width of the toolbox). Each toolbar is associated with a "grippy"
// which can be used to either collapse a particular toolbar or as a handle to
// pick up and move a toolbar to a new position within the toolbox. When toolbars
// are collapsed, it's grippy is moved to the bottom of the box and laid on
// its side. Clicking again on the grippy will reinstate the toolbar to its previous
// position in the toolbox.
//
// As mentioned above, the toolbox expects its toolbars to be its children in
// the DOM. The exact structure of the children is documented on:
//   http://www.mozilla.org/xpfe/DMWSpecNew.html
//

#ifndef nsToolBoxFrame_h___
#define nsToolBoxFrame_h___

#include "nsIDOMDragListener.h"
#include "nsHTMLContainerFrame.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsXULAtoms.h"
#include "nsCOMPtr.h"
#include "nsBoxFrame.h"

class nsToolboxFrame : public nsBoxFrame
{
public:
  friend nsresult NS_NewToolboxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow); 

/*BEGIN implementations of dragevent handler interface*/
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
  virtual nsresult DragEnter(nsIDOMEvent* aDragEvent);
  virtual nsresult DragOver(nsIDOMEvent* aDragEvent);
  virtual nsresult DragExit(nsIDOMEvent* aDragEvent);
  virtual nsresult DragDrop(nsIDOMEvent* aDragEvent);
  virtual nsresult DragGesture(nsIDOMEvent* aDragEvent) { return NS_OK; } 
/*END implementations of dragevent handler interface*/

  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      aResult = "Toolbox";
      return NS_OK;
  }

protected:
  
  nsToolboxFrame(nsIPresShell* aShell);
  virtual ~nsToolboxFrame();

  class DragListenerDelegate : public nsIDOMDragListener
  {
  protected:
    nsToolboxFrame* mFrame;

  public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

  
    // nsIDOMEventListener interface
    virtual nsresult HandleEvent(nsIDOMEvent* aEvent)
    {
      return mFrame ? mFrame->HandleEvent(aEvent) : NS_OK;
    }

    virtual nsresult DragGesture(nsIDOMEvent* aEvent)
    {
      return mFrame ? mFrame->DragGesture(aEvent) : NS_OK;
    }

    // nsIDOMDragListener interface
    virtual nsresult DragEnter(nsIDOMEvent* aMouseEvent)
    {
      return mFrame ? mFrame->DragEnter(aMouseEvent) : NS_OK;
    }

    virtual nsresult DragOver(nsIDOMEvent* aMouseEvent)
    {
      return mFrame ? mFrame->DragOver(aMouseEvent) : NS_OK;
    }

    virtual nsresult DragExit(nsIDOMEvent* aMouseEvent)
    {
      return mFrame ? mFrame->DragExit(aMouseEvent) : NS_OK;
    }

    virtual nsresult DragDrop(nsIDOMEvent* aMouseEvent)
    {
      return mFrame ? mFrame->DragDrop(aMouseEvent) : NS_OK;
    }

    // Implementation methods
    DragListenerDelegate(nsToolboxFrame* aFrame) : mFrame(aFrame)
    {
      NS_INIT_REFCNT();
    }

    virtual ~DragListenerDelegate() {}

    void NotifyFrameDestroyed() { mFrame = nsnull; }
  };
  DragListenerDelegate* mDragListenerDelegate;
  
    // pass-by-value not allowed for a toolbox because it corresponds 1-to-1
    // with an element in the UI.
  nsToolboxFrame ( const nsToolboxFrame& aFrame ) ;	            // DO NOT IMPLEMENT
  nsToolboxFrame& operator= ( const nsToolboxFrame& aFrame ) ;  // DO NOT IMPLEMENT
}; // class nsToolboxFrame

#endif
