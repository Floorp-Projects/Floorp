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


#include "nsNativeScrollbarFrame.h"
#include "nsGkAtoms.h"
#include "nsBoxLayoutState.h"
#include "nsComponentManagerUtils.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsINativeScrollbar.h"
#include "nsIScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsWidgetsCID.h"
#include "nsINameSpaceManager.h"

//
// NS_NewNativeScrollbarFrame
//
// Creates a new scrollbar frame and returns it
//
nsIFrame*
NS_NewNativeScrollbarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsNativeScrollbarFrame (aPresShell, aContext);
} // NS_NewNativeScrollbarFrame


//
// QueryInterface
//
NS_IMETHODIMP
nsNativeScrollbarFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (!aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIScrollbarMediator))) {
    *aInstancePtr = (void*) ((nsIScrollbarMediator*) this);
    return NS_OK;
  }
  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);
}

//
// Init
//
// Pass along to our parent, but also create the native widget that we wrap. 
//
NS_IMETHODIMP
nsNativeScrollbarFrame::Init(nsIContent*     aContent,
                             nsIFrame*       aParent,
                             nsIFrame*       aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // create a view for this frame and then associate the view with the native
  // scrollbar widget. The net result of this is that the view will automatically
  // be resized and moved for us when things reflow, and the widget will follow
  // suit. We don't have to lift a finger!
  static NS_DEFINE_IID(kScrollbarCID,  NS_NATIVESCROLLBAR_CID);
  if ( NS_SUCCEEDED(CreateViewForFrame(GetPresContext(), this, GetStyleContext(), PR_TRUE)) ) {
    nsIView* myView = GetView();
    if ( myView ) {
      nsWidgetInitData widgetData;
      if ( NS_SUCCEEDED(myView->CreateWidget(kScrollbarCID, &widgetData, nsnull)) ) {
        mScrollbar = myView->GetWidget();
        if (mScrollbar) {
          mScrollbar->Show(PR_TRUE);
          mScrollbar->Enable(PR_TRUE);

          // defer telling the scrollbar about the mediator and the content
          // node until its first reflow since not everything has been set
          // by this point.
          mScrollbarNeedsContent = PR_TRUE;
        } else {
          NS_WARNING("Couldn't create native scrollbar!");
          return NS_ERROR_FAILURE;
        }
      }
    }
  }
  
  return rv;
}

void
nsNativeScrollbarFrame::Destroy()
{
  nsCOMPtr<nsINativeScrollbar> scrollbar(do_QueryInterface(mScrollbar));
  if (scrollbar) {
    // frame is going away, unhook the native scrollbar from
    // the content node just to be safe about lifetime issues
    scrollbar->SetContent(nsnull, nsnull, nsnull);
  }

  nsBoxFrame::Destroy();
}

//
// FindParts
//
// Walk up the parent frame tree and find the content node of the frame
// with the tag "scrollbar". This is the content node that the GFX Scroll Frame
// is watching for attribute changes. We return the associated frame and
// any mediator.
//
nsNativeScrollbarFrame::Parts
nsNativeScrollbarFrame::FindParts()
{
  nsIFrame* f;
  for (f = GetParent(); f; f = f->GetParent()) {
    nsIContent* currContent = f->GetContent();

    if (currContent && currContent->Tag() == nsGkAtoms::scrollbar) {
      nsIScrollbarFrame* sb;
      CallQueryInterface(f, &sb);
      if (sb)
        return Parts(f, sb, sb->GetScrollbarMediator());
    }
  }

  return Parts(nsnull, nsnull, nsnull);
}

NS_IMETHODIMP
nsNativeScrollbarFrame::Reflow(nsPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  // nsGfxScrollFrame may have told us to shrink to nothing. If so, make sure our
  // desired size agrees.
  if (aReflowState.availableWidth == 0) {
    aDesiredSize.width = 0;
  }
  if (aReflowState.availableHeight == 0) {
    aDesiredSize.height = 0;
  }

  return NS_OK;
}

//
// AttributeChanged
//
// We inherit changes to certain attributes from the parent's content node. These
// occur when gecko changes the values of the scrollbar or scrolls the content area
// by some means other than our scrollbar (keyboard, scrollwheel, etc). Update
// our native scrollbar with the correct values.
//
NS_IMETHODIMP
nsNativeScrollbarFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  
  if (  aAttribute == nsGkAtoms::curpos ||
        aAttribute == nsGkAtoms::maxpos || 
        aAttribute == nsGkAtoms::pageincrement ||
        aAttribute == nsGkAtoms::increment ) {
    nsAutoString valueStr;
    mContent->GetAttr(aNameSpaceID, aAttribute, valueStr);
    
    PRInt32 error;
    PRInt32 value = valueStr.ToInteger(&error);
    if (value < 0)
      value = 1;          // just be safe and sanity check, scrollbar expects unsigned

    nsCOMPtr<nsINativeScrollbar> scrollbar(do_QueryInterface(mScrollbar));
    if (scrollbar) {
      if (aAttribute == nsGkAtoms::maxpos) {
        // bounds check it
        PRUint32 maxValue = (PRUint32)value;
        PRUint32 current;
        scrollbar->GetPosition(&current);
        if (current > maxValue)
        {
          PRInt32 oldPosition = (PRInt32)current;
          PRInt32 curPosition = maxValue;
        
          Parts parts = FindParts();
          if (parts.mMediator) {
            parts.mMediator->PositionChanged(parts.mIScrollbarFrame, oldPosition, /* inout */ curPosition);
          }

          nsAutoString currentStr;
          currentStr.AppendInt(curPosition);
          parts.mScrollbarFrame->GetContent()->
            SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, currentStr, PR_TRUE);
        }
      }
      
      if ( aAttribute == nsGkAtoms::curpos )
        scrollbar->SetPosition(value);
      else if ( aAttribute == nsGkAtoms::maxpos )
        scrollbar->SetMaxRange(value);
      else if ( aAttribute == nsGkAtoms::pageincrement )   // poorly named, actually the height of the visible view area
        scrollbar->SetViewSize(value);
     else if ( aAttribute == nsGkAtoms::increment )
        scrollbar->SetLineIncrement(value);
    }
  }

  return rv;
}


//
// GetPrefSize
//
// Ask our native widget what dimensions it wants to be, convert them
// back to twips, and tell gecko.
//
nsSize
nsNativeScrollbarFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize size(0,0);
  DISPLAY_PREF_SIZE(this, size);

  PRInt32 narrowDimension = 0;
  nsCOMPtr<nsINativeScrollbar> native ( do_QueryInterface(mScrollbar) );
  if ( !native ) return size;
  native->GetNarrowSize(&narrowDimension);

  if ( IsVertical() )
    size.width = aState.PresContext()->DevPixelsToAppUnits(narrowDimension);
  else
    size.height = aState.PresContext()->DevPixelsToAppUnits(narrowDimension);

  // By now, we have both the content node for the scrollbar and the associated
  // scrollbar mediator (for outliner, if applicable). Hook up the scrollbar to
  // gecko
  Hookup();

  return size;
}


//
// Hookup
//
// Connect our widget to the content node and/or scrolling mediator. This needs
// to be called late enough in the game where everything is ready. Calling it too
// early can lead to situations where the mediator hasn't yet been hooked up to the
// scrollbar frame
//
void
nsNativeScrollbarFrame::Hookup()
{
  if (!mScrollbarNeedsContent)
    return;

  nsCOMPtr<nsINativeScrollbar> scrollbar(do_QueryInterface(mScrollbar));
  if (!scrollbar) {
    NS_WARNING("Native scrollbar widget doesn't implement nsINativeScrollbar");
    return;
  }

  Parts parts = FindParts();
  if (!parts.mScrollbarFrame) {
    // Nothing to do here
    return;
  }
  
  // We can't just pass 'mediator' to the widget, because 'mediator' might go away.
  // So pass a pointer to us. When we go away, we can tell the widget.
  nsIContent* scrollbarContent = parts.mScrollbarFrame->GetContent();
  scrollbar->SetContent(scrollbarContent,
                        parts.mIScrollbarFrame, parts.mMediator ? this : nsnull);
  mScrollbarNeedsContent = PR_FALSE;

  if (!scrollbarContent)
    return;

  // Check to see if the curpos attribute is already present on the content
  // node. If so, notify the scrollbar.

  nsAutoString value;
  scrollbarContent->GetAttr(kNameSpaceID_None, nsGkAtoms::curpos, value);

  PRInt32 error;
  PRUint32 curpos = value.ToInteger(&error);
  if (!curpos || error)
    return;

  scrollbar->SetPosition(curpos);
}

NS_IMETHODIMP
nsNativeScrollbarFrame::PositionChanged(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32& aNewIndex)
{
  Parts parts = FindParts();
  if (!parts.mMediator)
    return NS_OK;
  return parts.mMediator->PositionChanged(aScrollbar, aOldIndex, aNewIndex);
}

NS_IMETHODIMP
nsNativeScrollbarFrame::ScrollbarButtonPressed(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  Parts parts = FindParts();
  if (!parts.mMediator)
    return NS_OK;
  return parts.mMediator->ScrollbarButtonPressed(aScrollbar, aOldIndex, aNewIndex);
}

NS_IMETHODIMP
nsNativeScrollbarFrame::VisibilityChanged(nsISupports* aScrollbar, PRBool aVisible)
{
  Parts parts = FindParts();
  if (!parts.mMediator)
    return NS_OK;
  return parts.mMediator->VisibilityChanged(aScrollbar, aVisible);
}
