/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "nsComboboxControlFrame.h"
#include "nsFormFrame.h"
#include "nsFormControlFrame.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMElement.h"
#include "nsListControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsIDOMHTMLCollection.h" 
#include "nsIDOMHTMLSelectElement.h" 
#include "nsIDOMHTMLOptionElement.h" 
#include "nsIPresShell.h"
#include "nsISupportsArray.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsIScrollableView.h"

#include "nsIEventStateManager.h"
// Get onChange to target Select not Option
#include "nsIDOMNode.h"
#include "nsIPrivateDOMEvent.h"

static NS_DEFINE_IID(kIFormControlFrameIID,      NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIComboboxControlFrameIID,  NS_ICOMBOBOXCONTROLFRAME_IID);
static NS_DEFINE_IID(kIListControlFrameIID,      NS_ILISTCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID,      NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIFrameIID,                 NS_IFRAME_IID);
static NS_DEFINE_IID(kIAnonymousContentCreatorIID, NS_IANONYMOUS_CONTENT_CREATOR_IID);
static NS_DEFINE_IID(kIDOMNodeIID,               NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID,       NS_IPRIVATEDOMEVENT_IID);

// Drop down list event management.
// The combo box uses the following strategy for managing the
// drop-down list.
// If the combo box or it's arrow button is clicked on the drop-down list is displayed
// If mouse exit's the combo box with the drop-down list displayed the drop-down list
// is asked to capture events
// The drop-down list will capture all events including mouse down and up and will always
// return with ListWasSelected method call regardless of whether an item in the list was
// actually selected.
// The ListWasSelected code will turn off mouse-capture for the drop-down list.
// The drop-down list does not explicitly set capture when it is in the drop-down mode.


//XXX: This is temporary. It simulates psuedo states by using a attribute selector on 

const char * kMozDropdownActive = "-moz-dropdown-active";

nsresult
NS_NewComboboxControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsComboboxControlFrame* it = new nsComboboxControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsComboboxControlFrame::nsComboboxControlFrame()
  : nsAreaFrame() 
{
  mPresContext                 = nsnull;
  mFormFrame                   = nsnull;       
  mListControlFrame            = nsnull;
  mTextStr                     = "";
  mDisplayContent              = nsnull;
  mButtonContent               = nsnull;
  mDroppedDown                 = PR_FALSE;
  mDisplayFrame                = nsnull;
  mButtonFrame                 = nsnull;
  mDropdownFrame               = nsnull;
  mIgnoreFocus                 = PR_FALSE;
  mSelectedIndex               = -1;

   //Shrink the area around it's contents
  SetFlags(NS_BLOCK_SHRINK_WRAP);
}

//--------------------------------------------------------------
nsComboboxControlFrame::~nsComboboxControlFrame()
{
  // get the reciever interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mButtonContent));
  reciever->RemoveEventListenerByIID((nsIDOMMouseListener *)this, kIDOMMouseListenerIID);

  nsCOMPtr<nsIDOMEventReceiver> displayReciever(do_QueryInterface(mDisplayContent));
  displayReciever->RemoveEventListenerByIID((nsIDOMMouseListener *)this, kIDOMMouseListenerIID);
  //selectReciever->RemoveEventListenerByIID((nsIDOMFocusListener *)this, nsCOMTypeInfo<nsIDOMFocusListener>::GetIID());

  mFormFrame = nsnull;
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mDisplayContent);
  NS_IF_RELEASE(mButtonContent);
}

//--------------------------------------------------------------
nsresult
nsComboboxControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIComboboxControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIComboboxControlFrame*) this);
    return NS_OK;
  } else if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  } else if (aIID.Equals(kIDOMMouseListenerIID)) {                                         
    *aInstancePtr = (void*)(nsIDOMMouseListener*) this;                                        
    NS_ADDREF_THIS();
    return NS_OK;                                                        
  } else if (aIID.Equals(nsCOMTypeInfo<nsIDOMFocusListener>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMFocusListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    return NS_OK;   
  } else if (aIID.Equals(nsCOMTypeInfo<nsISelectControlFrame>::GetIID())) {
    *aInstancePtr = (void *)((nsISelectControlFrame*)this);
    return NS_OK;
  }

  return nsAreaFrame::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP
nsComboboxControlFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
   // Need to hold on the pres context because it is used later in methods
   // which don't have it passed in.
  mPresContext = &aPresContext;
  NS_ADDREF(mPresContext);
  return nsAreaFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

//--------------------------------------------------------------
PRBool
nsComboboxControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}


// Initialize the text string in the combobox using either the current
// selection in the list box or the first item item in the list box.

void 
nsComboboxControlFrame::InitTextStr(PRBool aUpdate)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
 
   // Reset the list, so we can ask it for it's selected item.
  fcFrame->Reset();

   // Update the selected text string
  mListControlFrame->GetSelectedItem(mTextStr);

  if (mTextStr == "") {
     // No selection so use the first item in the list box
 
    if ((NS_OK == result) && (nsnull != fcFrame)) {
      // Find out if there are any options in the list to select
      PRInt32 length = 0;
      mListControlFrame->GetNumberOfOptions(&length);
      if (length > 0) {
         // Set listbox selection to first item in the list box
        fcFrame->SetProperty(nsHTMLAtoms::selectedindex, "0");
        mSelectedIndex = 0;
         // Get the listbox selection as a string
        mListControlFrame->GetSelectedItem(mTextStr);
      }
    }
  }

   // Update the display by setting the value attribute
  mDisplayContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, mTextStr, aUpdate);
  //nsIFrame* buttonFrame = GetDisplayFrame(aPresContext);
  //nsFormControlHelper::ForceDrawFrame(buttonFrame);
}

//--------------------------------------------------------------

// Reset the combo box back to it original state.

void 
nsComboboxControlFrame::Reset()
{
   // Reset the dropdown list to its original state
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_OK == result) && (nsnull != fcFrame)) {
    fcFrame->Reset(); 
  }
 
    // Update the combobox using the text string returned from the dropdown list
  InitTextStr(PR_TRUE);
}

void 
nsComboboxControlFrame::PostCreateWidget(nsIPresContext* aPresContext,
                                       nscoord& aWidth,
                                       nscoord& aHeight)
{
  Reset();
}

//--------------------------------------------------------------
NS_IMETHODIMP 
nsComboboxControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_SELECT;
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult rv;
  rv = GetContent(&content);
  aContent = content;
  return rv;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return 0;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return 0;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
   return 0;
}

//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}



void 
nsComboboxControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  //XXX: TODO Implement focus for combobox. 
}

void
nsComboboxControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_ANYWHERE,NS_PRESSHELL_SCROLL_ANYWHERE);

  }
}


void
nsComboboxControlFrame::ShowPopup(PRBool aShowPopup)
{
  //XXX: This is temporary. It simulates a psuedo dropdown states by using a attribute selector 
  // This will not be need if the event state supports active states for use with the dropdown list
  // Currently, the event state manager will reset the active state to the content which has focus
  // which causes the active state to be removed from the event state manager immediately after it
  // it the select is set to the active state.
  
  nsCOMPtr<nsIAtom> activeAtom ( dont_QueryInterface(NS_NewAtom(kMozDropdownActive)));
  if (PR_TRUE == aShowPopup) {
    mContent->SetAttribute(kNameSpaceID_None, activeAtom, "", PR_TRUE);
  } else {
    mContent->UnsetAttribute(kNameSpaceID_None, activeAtom, PR_TRUE);
  }
}

// Show the dropdown list

void 
nsComboboxControlFrame::ShowList(nsIPresContext* aPresContext, PRBool aShowList)
{
  if (PR_TRUE == aShowList) {
    ShowPopup(PR_TRUE);
    mDroppedDown = PR_TRUE;
     // The listcontrol frame will call back to the nsComboboxControlFrame's ListWasSelected
     // which will stop the capture.
    mListControlFrame->AboutToDropDown();
    mListControlFrame->CaptureMouseEvents(PR_TRUE);
  } else {
    ShowPopup(PR_FALSE);
    mDroppedDown = PR_FALSE;
  }
}


//-------------------------------------------------------------
// this is in response to the MouseClick from the containing browse button
// XXX: TODO still need to get filters from accept attribute
void 
nsComboboxControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
   //ToggleList(aPresContext);
}


nsresult
nsComboboxControlFrame::ReflowComboChildFrame(nsIFrame* aFrame, 
                                         nsIPresContext&  aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus,
                                         nscoord                  aAvailableWidth,
                                         nscoord                  aAvailableHeight)
{
   // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
  nsSize availSize(aAvailableWidth, aAvailableHeight);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, aFrame,
                                   availSize);
  kidReflowState.mComputedWidth = aAvailableWidth;
  kidReflowState.mComputedHeight = aAvailableHeight;
      
   // Reflow child
  nsresult rv = ReflowChild(aFrame, aPresContext, aDesiredSize, kidReflowState, aStatus);
 
   // Set the child's width and height to it's desired size
  nsRect rect;
  aFrame->GetRect(rect);
  rect.width = aDesiredSize.width;
  rect.height = aDesiredSize.height;
  aFrame->SetRect(rect);
  return rv;
}

// Suggest a size for the child frame. 
// Only frames which implement the nsIFormControlFrame interface and
// honor the SetSuggestedSize method will be placed and sized correctly.

void 
nsComboboxControlFrame::SetChildFrameSize(nsIFrame* aFrame, nscoord aWidth, nscoord aHeight) 
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
    fcFrame->SetSuggestedSize(aWidth, aHeight); 
  }
}

nsresult 
nsComboboxControlFrame::GetPrimaryComboFrame(nsIPresContext& aPresContext, nsIContent* aContent, nsIFrame** aFrame)
{
  nsresult rv = NS_OK;
   // Get the primary frame from the presentation shell.
  nsCOMPtr<nsIPresShell> presShell;
  rv = aPresContext.GetShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv) && presShell) {
    presShell->GetPrimaryFrameFor(aContent, aFrame);
  }
  return rv;
}

nsIFrame*
nsComboboxControlFrame::GetButtonFrame(nsIPresContext& aPresContext)
{
  if (mButtonFrame == nsnull) {
    NS_ASSERTION(mButtonContent != nsnull, "nsComboboxControlFrame mButtonContent is null");
    GetPrimaryComboFrame(aPresContext, mButtonContent, &mButtonFrame);
  }

  return mButtonFrame;
}

nsIFrame* 
nsComboboxControlFrame::GetDisplayFrame(nsIPresContext& aPresContext)
{
  if (mDisplayFrame == nsnull) {
    NS_ASSERTION(mDisplayContent != nsnull, "nsComboboxControlFrame mDisplayContent is null");
    GetPrimaryComboFrame(aPresContext, mDisplayContent, &mDisplayFrame);
  }

  return mDisplayFrame;
}

nsIFrame* 
nsComboboxControlFrame::GetDropdownFrame()
{
  return mDropdownFrame;
}


nsresult 
nsComboboxControlFrame::GetScreenHeight(nsIPresContext& aPresContext,
                                        nscoord& aHeight)
{
  aHeight = 0;
  nsIDeviceContext* context;
  aPresContext.GetDeviceContext( &context );
	if ( nsnull != context )
	{
		PRInt32 height;
    PRInt32 width;
		context->GetDeviceSurfaceDimensions(width, height);
		float devUnits;
 		context->GetDevUnitsToAppUnits(devUnits);
		aHeight = NSToIntRound(float( height) / devUnits );
		NS_RELEASE( context );
		return NS_OK;
	}

  return NS_ERROR_FAILURE;
}

int counter = 0;

nsresult 
nsComboboxControlFrame::PositionDropdown(nsIPresContext& aPresContext, 
                                         nscoord aHeight, 
                                         nsRect aAbsoluteTwipsRect, 
                                         nsRect aAbsolutePixelRect)
{
   // Position the dropdown list. It is positioned below the display frame if there is enough
   // room on the screen to display the entire list. Otherwise it is placed above the display
   // frame.

   // Note: As first glance, it appears that you could simply get the absolute bounding box for the
   // dropdown list by first getting it's view, then getting the view's nsIWidget, then asking the nsIWidget
   // for it's AbsoluteBounds. The problem with this approach, is that the dropdown lists y location can
   // change based on whether the dropdown is placed below or above the display frame.
   // The approach, taken here is to get use the absolute position of the display frame and use it's location
   // to determine if the dropdown will go offscreen.

   // Use the height calculated for the area frame so it includes both
   // the display and button heights.
  nsresult rv = NS_OK;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nscoord dropdownYOffset = aHeight;
// XXX: Enable this code to debug popping up above the display frame, rather than below it
  nsRect dropdownRect;
  dropdownFrame->GetRect(dropdownRect);

  nscoord screenHeightInPixels = 0;
  if (NS_SUCCEEDED(GetScreenHeight(aPresContext, screenHeightInPixels))) {
     // Get the height of the dropdown list in pixels.
     float t2p;
     aPresContext.GetTwipsToPixels(&t2p);
     nscoord absoluteDropDownHeight = NSTwipsToIntPixels(dropdownRect.height, t2p);
    
      // Check to see if the drop-down list will go offscreen
    if (NS_SUCCEEDED(rv) && ((aAbsolutePixelRect.y + aAbsolutePixelRect.height + absoluteDropDownHeight) > screenHeightInPixels)) {
      // move the dropdown list up
      dropdownYOffset = - (dropdownRect.height);
    }
  } 
 
  dropdownRect.x = 0;
  dropdownRect.y = dropdownYOffset; 
  nsRect currentRect;
  dropdownFrame->GetRect(currentRect);
  //if (currentRect != dropdownRect) {
    dropdownFrame->SetRect(dropdownRect);
#ifdef DEBUG_rods
    printf("%d Position Dropdown at: %d %d %d %d\n", counter++, dropdownRect.x, dropdownRect.y, dropdownRect.width, dropdownRect.height);
#endif
  //}

  return rv;
}


// Calculate a frame's position in screen coordinates
nsresult
nsComboboxControlFrame::GetAbsoluteFramePosition(nsIPresContext& aPresContext,
                                                 nsIFrame *aFrame, 
                                                 nsRect& aAbsoluteTwipsRect, 
                                                 nsRect& aAbsolutePixelRect)
{
  //XXX: This code needs to take the view's offset into account when calculating
  //the absolute coordinate of the frame.
  nsresult rv = NS_OK;
 
  aFrame->GetRect(aAbsoluteTwipsRect);
 
    // Get conversions between twips and pixels
  float t2p;
  float p2t;
  aPresContext.GetTwipsToPixels(&t2p);
  aPresContext.GetPixelsToTwips(&p2t);
  
   // Add in frame's offset from it it's containing view
  nsIView *containingView = nsnull;
  nsPoint offset;
  rv = aFrame->GetOffsetFromView(offset, &containingView);
  if (NS_SUCCEEDED(rv) && (nsnull != containingView)) {
    aAbsoluteTwipsRect.x += offset.x;
    aAbsoluteTwipsRect.y += offset.y;

    nsPoint viewOffset;
    containingView->GetPosition(&viewOffset.x, &viewOffset.y);
    nsIView * parent;
    containingView->GetParent(parent);
    while (nsnull != parent) {
      nsPoint po;
      parent->GetPosition(&po.x, &po.y);
      viewOffset.x += po.x;
      viewOffset.y += po.y;
      nsIScrollableView * scrollView;
      if (NS_OK == containingView->QueryInterface(nsIScrollableView::GetIID(), (void **)&scrollView)) {
        nscoord x;
        nscoord y;
        scrollView->GetScrollPosition(x, y);
        viewOffset.x -= x;
        viewOffset.y -= y;
      }
      nsIWidget * widget;
      parent->GetWidget(widget);
      if (nsnull != widget) {
        // Add in the absolute offset of the widget.
        nsRect absBounds;
        nsRect lc;
        widget->WidgetToScreen(lc, absBounds);
        // Convert widget coordinates to twips   
        aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
        aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
        NS_RELEASE(widget);
        break;
      }
      parent->GetParent(parent);
    }
    aAbsoluteTwipsRect.x += viewOffset.x;
    aAbsoluteTwipsRect.y += viewOffset.y;

     // Addin the containing view's offset form it's containing widget
    /*nsIWidget* widget = nsnull;
    nscoord widgetx = 0;
    nscoord widgety = 0;
    rv = containingView->GetOffsetFromWidget(&widgetx, &widgety, widget);
    if (NS_SUCCEEDED(rv) && (nsnull != widget)) {
      aAbsoluteTwipsRect.x += widgetx;
      aAbsoluteTwipsRect.y += widgety;
  
       // Add in the absolute offset of the widget.
      nsRect absBounds;
 //XXX: Remove this     widget->GetAbsoluteBounds(absBounds);

      // Convert widget coordinates to twips   
      aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
      aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
      NS_RELEASE(widget);
    }*/
  }

   // convert to pixel coordinates
  if (NS_SUCCEEDED(rv)) {
   aAbsolutePixelRect.x = NSTwipsToIntPixels(aAbsoluteTwipsRect.x, t2p);
   aAbsolutePixelRect.y = NSTwipsToIntPixels(aAbsoluteTwipsRect.y, t2p);
   aAbsolutePixelRect.width = NSTwipsToIntPixels(aAbsoluteTwipsRect.width, t2p);
   aAbsolutePixelRect.height = NSTwipsToIntPixels(aAbsoluteTwipsRect.height, t2p);
  }

  return rv;
}

static int myCounter = 0;
NS_IMETHODIMP 
nsComboboxControlFrame::Reflow(nsIPresContext&          aPresContext, 
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState, 
                               nsReflowStatus&          aStatus)
{
#ifdef DEBUG_rods
  printf("nsComboboxControlFrame::Reflow %d   Reason: ", myCounter++);
  switch (aReflowState.reason) {
    case eReflowReason_Initial:printf("eReflowReason_Initial\n");break;
    case eReflowReason_Incremental:printf("eReflowReason_Incremental\n");break;
    case eReflowReason_Resize:printf("eReflowReason_Resize\n");break;
    case eReflowReason_StyleChange:printf("eReflowReason_StyleChange\n");break;
  }
#endif

  nsresult rv = NS_OK;
  nsIFrame* buttonFrame = GetButtonFrame(aPresContext);
  nsIFrame* displayFrame = GetDisplayFrame(aPresContext);
  nsIFrame* dropdownFrame = GetDropdownFrame();

  // Don't try to do any special sizing and positioning unless all of the frames
  // have been created.
  if ((nsnull == displayFrame) ||
     (nsnull == buttonFrame) ||
     (nsnull == dropdownFrame)) 
  {
     // Since combobox frames are missing just do a normal area frame reflow
    return nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  // size of each part of the combo box
  nsRect displayRect;
  nsRect buttonRect;
  nsRect dropdownRect;

 
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

   // Get the current sizes of the combo box child frames
  displayFrame->GetRect(displayRect);
  buttonFrame->GetRect(buttonRect);
  dropdownFrame->GetRect(dropdownRect);

  nsHTMLReflowState firstPassState(aReflowState);

    // Only reflow the display and button if they are the target of 
    // the incremental reflow, unless they change size. If they do
    // then everything needs to be reflowed.
  if (eReflowReason_Incremental == firstPassState.reason) {
    nsIFrame* targetFrame;
    firstPassState.reflowCommand->GetTarget(targetFrame);
    if ((targetFrame == buttonFrame) || (targetFrame == displayFrame)) {
      nsRect oldDisplayRect = displayRect;
      nsRect oldButtonRect = buttonRect;
      rv = nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
      displayFrame->GetRect(displayRect);
      buttonFrame->GetRect(buttonRect);
      if ((oldDisplayRect == displayRect) && (oldButtonRect == buttonRect)) {
        // Reposition the popup.
        //nsRect absoluteTwips;
        //nsRect absolutePixels;
        //GetAbsoluteFramePosition(aPresContext, displayFrame,  absoluteTwips, absolutePixels);
        //PositionDropdown(aPresContext, displayRect.height, absoluteTwips, absolutePixels);
        return rv;
      }
    }
    nsIReflowCommand::ReflowType type;
    aReflowState.reflowCommand->GetType(type);
    firstPassState.reason = eReflowReason_StyleChange;
    firstPassState.reflowCommand = nsnull;
  }


  //Set the desired size for the button and display frame
  if (NS_UNCONSTRAINEDSIZE == firstPassState.mComputedWidth) {
    // A width has not been specified for the select so size the display area to
    // match the width of the longest item in the drop-down list. The dropdown
    // list has already been reflowed and sized to shrink around its contents above.

     // Reflow the dropdown shrink-wrapped.
    nsHTMLReflowMetrics  dropdownDesiredSize(aDesiredSize);
    ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);  
    nsSize size;
    PRInt32 length = 0;
    mListControlFrame->GetNumberOfOptions(&length);
    /*if (0 == length) {
      nsCOMPtr<nsIPresShell> presShell;
      rv = mPresContext->GetShell(getter_AddRefs(presShell));
      if (NS_SUCCEEDED(rv) && presShell) {
        nsCOMPtr<nsIRenderingContext> renderContext;
        rv = presShell->CreateRenderingContext(this, getter_AddRefs(renderContext));
        if (NS_SUCCEEDED(rv) && renderContext) {
          rv = presShell->CreateRenderingContext(this, getter_AddRefs(renderContext));
          nsFormControlHelper::GetTextSize(*mPresContext, this, nsAutoString("XX"), size, renderContext);
          dropdownRect.width  = size.width + dropdownDesiredSize.width;
          dropdownRect.height = size.height + dropdownDesiredSize.height;
        }
      }

    } else {*/
      dropdownFrame->GetRect(dropdownRect);
      // Get maximum size and height of a option in the dropdown
      mListControlFrame->GetMaximumSize(size);
    //}

     // Set width of display to match width of the drop down 
    SetChildFrameSize(displayFrame, dropdownRect.width, size.height);

    // Size the button to be the same height as the displayFrame
    SetChildFrameSize(buttonFrame, size.height, size.height);

     // Reflow display + button
    nsAreaFrame::Reflow(aPresContext, aDesiredSize, firstPassState, aStatus);
    displayFrame->GetRect(displayRect);
    buttonFrame->GetRect(buttonRect);

    // Reflow the dropdown list to match the width of the display + button
    ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, aDesiredSize.width, NS_UNCONSTRAINEDSIZE);
    dropdownFrame->GetRect(dropdownRect);  
    

  } else {
    // A width has been specified for the select.
    // Make the display frame's width + button frame width = the width specified.
   
    nsHTMLReflowMetrics  dropdownDesiredSize(aDesiredSize);
    ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); 
     // Get unconstrained size of the dropdown list.
    nsSize size;
    mListControlFrame->GetMaximumSize(size);

      // Size the button to be the same height as the displayFrame
    SetChildFrameSize(buttonFrame, size.height, size.height);

    // Compute display width
    // Since the button's width is the same as its height
    // we subtract size.height (the width)
    nscoord displayWidth = firstPassState.mComputedWidth - size.height;

     // Set the displayFrame to match the displayWidth computed above
    SetChildFrameSize(displayFrame, displayWidth, size.height);

      // Reflow again with the width of the display frame set.
    nsAreaFrame::Reflow(aPresContext, aDesiredSize, firstPassState, aStatus);

     // Reflow the dropdown list to match the width of the display + button
    ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, aDesiredSize.width, NS_UNCONSTRAINEDSIZE);
    
  }
 
  nsRect absoluteTwips;
  nsRect absolutePixels;
  GetAbsoluteFramePosition(aPresContext, displayFrame,  absoluteTwips, absolutePixels);
  PositionDropdown(aPresContext, aDesiredSize.height, absoluteTwips, absolutePixels);

  return rv;

}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetName(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      nsHTMLValue value;
      result = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}

PRInt32 
nsComboboxControlFrame::GetMaxNumValues()
{
  return 1;
}
  
/*XXX-REMOVE
PRBool
nsComboboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                   nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  // use our name and the text widgets value 
  aNames[0] = name;
  aValues[0] = mTextStr;
  aNumValues = 1;
  nsresult status = PR_TRUE;
  return status;
}
*/

PRBool
nsComboboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->GetNamesValues(aMaxNumValues, aNumValues, aValues, aNames);
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  if (nsFormFrame::GetDisabled(this)) {
    *aFrame = this;
    return NS_OK;
  } else {
    return nsHTMLContainerFrame::GetFrameForPoint(aPoint, aFrame);
  }
  return NS_OK;
}


//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ComboboxControl", aResult);
}



//----------------------------------------------------------------------
// nsIDOMMouseListener
//----------------------------------------------------------------------
nsresult
nsComboboxControlFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  /*if (nsFormFrame::GetDisabled(this)) {
    return NS_OK;
  }
  nsRect absoluteTwips;
  nsRect absolutePixels;
  nsIFrame * displayFrame = GetDisplayFrame(*mPresContext);
  nsRect displayRect;
   // Get the current sizes of the combo box child frames
  displayFrame->GetRect(displayRect);
  GetAbsoluteFramePosition(*mPresContext, displayFrame,  absoluteTwips, absolutePixels);
  PositionDropdown(*mPresContext, displayRect.height, absoluteTwips, absolutePixels);

  ToggleList(mPresContext);
  //mIgnoreMouseUp = PR_TRUE;
*/
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIComboboxControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::ShowDropDown(PRBool aDoDropDown) 
{ 
  if (nsFormFrame::GetDisabled(this)) {
    return NS_OK;
  }

  if (!mDroppedDown && aDoDropDown) {
    //nsRect absoluteTwips;
    //nsRect absolutePixels;
    //nsIFrame * displayFrame = GetDisplayFrame(*mPresContext);
    //nsRect displayRect;
     // Get the current sizes of the combo box child frames
    //displayFrame->GetRect(displayRect);
    //GetAbsoluteFramePosition(*mPresContext, displayFrame,  absoluteTwips, absolutePixels);
    //PositionDropdown(*mPresContext, displayRect.height, absoluteTwips, absolutePixels);
    if (mListControlFrame) {
      mListControlFrame->SyncViewWithFrame();
    }
    ToggleList(mPresContext);
    return NS_OK;
  } else if (mDroppedDown && !aDoDropDown) {
    ToggleList(mPresContext);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetDropDown(nsIFrame* aDropDownFrame)
{
  mDropdownFrame = aDropDownFrame;
 
  if (NS_OK != mDropdownFrame->QueryInterface(kIListControlFrameIID, (void**)&mListControlFrame)) {
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetDropDown(nsIFrame** aDropDownFrame) 
{
  if (nsnull == aDropDownFrame) {
    return NS_ERROR_FAILURE;
  }

  *aDropDownFrame = mDropdownFrame;
 
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::ListWasSelected(nsIPresContext* aPresContext)
{
  if (aPresContext == nsnull) {
    aPresContext = mPresContext;
  }
  ShowList(aPresContext, PR_FALSE);
  mListControlFrame->CaptureMouseEvents(PR_FALSE);

  PRInt32 indx;
  mListControlFrame->GetSelectedIndex(&indx);

  UpdateSelection(PR_TRUE, PR_FALSE, indx);

  return NS_OK;
}
// Toggle dropdown list.

NS_IMETHODIMP 
nsComboboxControlFrame::ToggleList(nsIPresContext* aPresContext)
{
  if (PR_TRUE == mDroppedDown) {
    ShowList(aPresContext, PR_FALSE);
  } else {
    ShowList(aPresContext, PR_TRUE);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsComboboxControlFrame::UpdateSelection(PRBool aDoDispatchEvent, PRBool aForceUpdate, PRInt32 aNewIndex)
{
  if (nsnull != mListControlFrame) {
     // Check to see if the selection changed
    if (mSelectedIndex != aNewIndex || aForceUpdate) {
      nsAutoString newTextStr;
      mListControlFrame->GetSelectedItem(newTextStr);
      //XXX:TODO look at the ordinal position of the selected content in the listbox to tell
      // if the selection has changed, rather than looking at the text string.
      // There may be more than one item in the dropdown list with the same label. 
      if (newTextStr != mTextStr) {
        mTextStr = newTextStr;
        SelectionChanged(aDoDispatchEvent);
      }
    } 
    mSelectedIndex = aNewIndex;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::AbsolutelyPositionDropDown()
{
  nsRect absoluteTwips;
  nsRect absolutePixels;
  nsIFrame* displayFrame = GetDisplayFrame(*mPresContext);
  nsRect rect;
  displayFrame->GetRect(rect);
  GetAbsoluteFramePosition(*mPresContext, displayFrame,  absoluteTwips, absolutePixels);
  PositionDropdown(*mPresContext, rect.height, absoluteTwips, absolutePixels);
  return NS_OK;
}

///////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsComboboxControlFrame::SelectionChanged(PRBool aDoDispatchEvent)
{
  if (nsnull != mDisplayContent) {
    mDisplayContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, mTextStr, PR_TRUE);
    nsIFrame* displayFrame = GetDisplayFrame(*mPresContext);
    nsFormControlHelper::ForceDrawFrame(displayFrame);

    // Send reflow command because the new text maybe larger
    nsIReflowCommand* cmd;
    nsresult          rv;
    rv = NS_NewHTMLReflowCommand(&cmd, displayFrame, nsIReflowCommand::ContentChanged);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIPresShell> shell;
      rv = mPresContext->GetShell(getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv) && shell) {
        shell->AppendReflowCommand(cmd);
        NS_RELEASE(cmd);
      }
    }
  }

  if (aDoDispatchEvent) {
     // Dispatch the NS_FORM_CHANGE event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.widget = nsnull;
    event.message = NS_FORM_CHANGE;
    event.flags = NS_EVENT_FLAG_NONE;

     // Here we create our own DOM event and set the target to the Select
     // We'll pass this DOM event in, in hopes that the target is used.
    nsIDOMEvent* DOMEvent = nsnull;
    nsresult res = NS_NewDOMUIEvent(&DOMEvent, *mPresContext, &event);
    if (NS_SUCCEEDED(res) && DOMEvent && mContent) {
      nsIDOMNode* node = nsnull;
      res = mContent->QueryInterface(kIDOMNodeIID, (void**)&node);
      if (NS_SUCCEEDED(res) && node) {
	nsIPrivateDOMEvent* pDOMEvent = nsnull;
        res = DOMEvent->QueryInterface(kIPrivateDOMEventIID, (void**)&pDOMEvent);
        if (NS_SUCCEEDED(res) && pDOMEvent) {
          pDOMEvent->SetTarget(node);
	  NS_RELEASE(pDOMEvent);

           // Have the content handle the event.
          mContent->HandleDOMEvent(*mPresContext, &event, &DOMEvent, NS_EVENT_FLAG_BUBBLE, status); 
        }
	NS_RELEASE(node);
      }
      NS_RELEASE(DOMEvent);
    }
     
     // Now have the frame handle the event
    nsIFrame* frame = nsnull;
    nsIFrame* dropdownFrame = GetDropdownFrame();
    nsresult result = dropdownFrame->QueryInterface(kIFrameIID, (void**)&frame);
    if ((NS_SUCCEEDED(result)) && (nsnull != frame)) {
      frame->HandleEvent(*mPresContext, &event, status);
    }
  }
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISelectControlFrame
//----------------------------------------------------------------------

NS_IMETHODIMP
nsComboboxControlFrame::AddOption(PRInt32 aIndex)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(nsCOMTypeInfo<nsISelectControlFrame>::GetIID(), 
                                              (void**)&listFrame);
  if (NS_SUCCEEDED(rv) && nsnull != listFrame) {
    return listFrame->AddOption(aIndex);
  }
  return rv;
}
  

NS_IMETHODIMP
nsComboboxControlFrame::RemoveOption(PRInt32 aIndex)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(nsCOMTypeInfo<nsISelectControlFrame>::GetIID(), 
                                              (void**)&listFrame);
  if (NS_SUCCEEDED(rv) && nsnull != listFrame) {
    return listFrame->RemoveOption(aIndex);
  }
  return rv;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetOptionSelected(PRInt32 aIndex, PRBool aValue)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(nsCOMTypeInfo<nsISelectControlFrame>::GetIID(), 
                                              (void**)&listFrame);
  if (NS_SUCCEEDED(rv) && nsnull != listFrame) {
    return listFrame->SetOptionSelected(aIndex, aValue);
  }
  return rv;
}

NS_IMETHODIMP 
nsComboboxControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus&  aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }
  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  return NS_OK;
}


nsresult 
nsComboboxControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP 
nsComboboxControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->SetProperty(aName, aValue);
  }
  return result;
}

NS_IMETHODIMP 
nsComboboxControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->GetProperty(aName, aValue);
  }
  return result;
}


NS_IMETHODIMP
nsComboboxControlFrame::CreateAnonymousContent(nsISupportsArray& aChildList)
{
  // The frames used to display the combo box and the button used to popup the dropdown list
  // are created through anonymous content. The dropdown list is not created through anonymous
  // content because it's frame is initialized specifically for the drop-down case and it is placed
  // a special list referenced through NS_COMBO_FRAME_POPUP_LIST_INDEX to keep separate from the
  // layout of the display and button. 
  //
  // Note: The value attribute of the display content is set when an item is selected in the dropdown list.
  // If the content specified below does not honor the value attribute than nothing will be displayed.
  // In addition, if the frame created by content below for does not implement the nsIFormControlFrame 
  // interface and honor the SetSuggestedSize method the placement and size of the display area will not
  // match what is normally desired for a combobox.


  // For now the content that is created corresponds to two input buttons. It would be better to create the
  // tag as something other than input, but then there isn't any way to create a button frame since it
  // isn't possible to set the display type in CSS2 to create a button frame.

    // create content used for display
  //nsIAtom* tag = NS_NewAtom("mozcombodisplay");
  nsIAtom* tag = NS_NewAtom("input");
  NS_NewHTMLInputElement(&mDisplayContent, tag);
  NS_ADDREF(mDisplayContent);
  mDisplayContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, nsAutoString("button"), PR_FALSE);

    //XXX: Do not use nsHTMLAtoms::id use nsHTMLAtoms::kClass instead. There will end up being multiple
    //ids set to the same value which is illegal.
  mDisplayContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, nsAutoString("-moz-display"), PR_FALSE);
  // This is 
  mDisplayContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, nsAutoString("X"), PR_TRUE);
  aChildList.AppendElement(mDisplayContent);

  // create button which drops the list down
  tag = NS_NewAtom("input");
  NS_NewHTMLInputElement(&mButtonContent, tag);
  NS_ADDREF(mButtonContent);
  mButtonContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, nsAutoString("button"), PR_FALSE);
  aChildList.AppendElement(mButtonContent);

  // get the reciever interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mButtonContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  reciever->AddEventListenerByIID((nsIDOMMouseListener *)this, kIDOMMouseListenerIID);
  //reciever->AddEventListenerByIID((nsIDOMFocusListener *)this, nsCOMTypeInfo<nsIDOMFocusListener>::GetIID());

  // get the reciever interface from the browser button's content node
  nsCOMPtr<nsIDOMEventReceiver> displayReciever(do_QueryInterface(mDisplayContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  displayReciever->AddEventListenerByIID((nsIDOMMouseListener *)this, kIDOMMouseListenerIID);
  //displayReciever->AddEventListenerByIID((nsIDOMFocusListener *)this, nsCOMTypeInfo<nsIDOMFocusListener>::GetIID());

  // get the reciever interface from the select's content
  nsCOMPtr<nsIDOMEventReceiver> selectReciever(do_QueryInterface(mContent));

  // we shouldn't have to unregister this listener because when
  // our frame goes away all these content node go away as well
  // because our frame is the only one who references them.
  //selectReciever->AddEventListenerByIID((nsIDOMFocusListener *)this, nsCOMTypeInfo<nsIDOMFocusListener>::GetIID());

  return NS_OK;
}

NS_IMETHODIMP 
nsComboboxControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}



NS_IMETHODIMP
nsComboboxControlFrame::Destroy(nsIPresContext& aPresContext)
{
   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames(aPresContext);
  return nsAreaFrame::Destroy(aPresContext);
}


NS_IMETHODIMP
nsComboboxControlFrame::FirstChild(nsIAtom*   aListName,
                                      nsIFrame** aFirstChild) const
{
  if (nsLayoutAtoms::popupList == aListName) {
    *aFirstChild = mPopupFrames.FirstChild();
  } else {
    nsAreaFrame::FirstChild(aListName, aFirstChild);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;
  if (nsLayoutAtoms::popupList == aListName) {
    mPopupFrames.SetFrames(aChildList);
  } else {
    rv = nsAreaFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    InitTextStr(PR_FALSE);
  }
  return rv;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const
{
   // Maintain a seperate child list for the dropdown list (i.e. popup listbox)
   // This is necessary because we don't want the listbox to be included in the layout
   // of the combox's children because it would take up space, when it is suppose to
   // be floating above the display.
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex <= NS_AREA_FRAME_ABSOLUTE_LIST_INDEX) {
    return nsAreaFrame::GetAdditionalChildListName(aIndex, aListName);
  }
  
  *aListName = nsnull;
  if (NS_COMBO_FRAME_POPUP_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::popupList;
    NS_ADDREF(*aListName);
  }
  return NS_OK;
}

PRIntn
nsComboboxControlFrame::GetSkipSides() const
{    
    // Don't skip any sides during border rendering
  return 0;
}


//----------------------------------------------------------------------
// nsIDOMFocusListener
//----------------------------------------------------------------------
nsresult
nsComboboxControlFrame::Focus(nsIDOMEvent* aEvent)
{
#if 0
  printf("nsComboboxControlFrame::Focus ");

  nsCOMPtr<nsIDOMNode> node;
  aEvent->GetTarget(getter_AddRefs(node));
  nsCOMPtr<nsIDOMNode> curNode;
  aEvent->GetCurrentNode(getter_AddRefs(curNode));
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));

  if (content == mContent) {
    printf("Combobox\n");
  } else if (content == mDisplayContent) {
    printf("Display\n");
    return NS_OK;
  } else if (content == mButtonContent) {
    printf("Button\n");
  }

  if (mIgnoreFocus) {
    printf(" mIgnoreFocus is TRUE\n");
    return NS_OK;
  }

  nsIEventStateManager *stateManager;
  if (NS_OK == mPresContext->GetEventStateManager(&stateManager)) {
    printf("Before ------------------------\n");
    mIgnoreFocus = PR_TRUE;
    //stateManager->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
    stateManager->SetContentState(mDisplayContent, NS_EVENT_STATE_FOCUS);
    //stateManager->SetContentState(mDisplayContent, NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);
    mIgnoreFocus = PR_FALSE;
    printf("After ------------------------\n");
    NS_RELEASE(stateManager);
  }
#endif
  return NS_OK;
}

nsresult
nsComboboxControlFrame::Blur(nsIDOMEvent* aEvent)
{
#if 0
  printf("nsComboboxControlFrame::Blur ");
  nsCOMPtr<nsIDOMNode> node;
  aEvent->GetTarget(getter_AddRefs(node));
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));

  if (content == mContent) {
    printf("Combobox\n");
  } else if (content == mDisplayContent) {
    printf("Display\n");
  } else if (content == mButtonContent) {
    printf("Button\n");
  }
#endif
  return NS_OK;
}


