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

#include "nsFileControlFrame.h"
#include "nsFormFrame.h"
#include "nsButtonControlFrame.h"
#include "nsTextControlFrame.h"
#include "nsNativeTextControlFrame.h"   // XXX: remove when frame construction is done properly
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsDocument.h"
#include "nsIDOMMouseListener.h"
#include "nsIPresShell.h"
#include "nsIDOMHTMLInputElement.h"


static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID,     NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);

nsresult
NS_NewFileControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFileControlFrame* it = new nsFileControlFrame();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsFileControlFrame::nsFileControlFrame():
  mTextFrame(nsnull), 
  mFormFrame(nsnull),
  mTextContent(nsnull)
{
    //Shrink the area around it's contents
  SetFlags(NS_BLOCK_SHRINK_WRAP);
}

nsFileControlFrame::~nsFileControlFrame()
{
  NS_IF_RELEASE(mTextContent);
}

NS_IMETHODIMP
nsFileControlFrame::CreateAnonymousContent(nsISupportsArray& aChildList)
{
  
  // create text field
  nsIAtom* tag = NS_NewAtom("input");
  if (NS_OK == NS_NewHTMLInputElement(&mTextContent, tag)) {
    mTextContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, nsAutoString("text"), PR_FALSE);
    if (nsFormFrame::GetDisabled(this)) {
      nsCOMPtr<nsIDOMHTMLInputElement> textControl = do_QueryInterface(mTextContent);
      if (textControl) {
        textControl->SetDisabled(nsFormFrame::GetDisabled(this));
      }
    }
    aChildList.AppendElement(mTextContent);
  }

  // create browse button
  nsIHTMLContent* browse = nsnull;
  tag = NS_NewAtom("input");
  if (NS_OK == NS_NewHTMLInputElement(&browse, tag)) {
    browse->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, nsAutoString("button"), PR_FALSE);
    //browse->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, nsAutoString("browse..."), PR_FALSE);

    aChildList.AppendElement(browse);

    // get the reciever interface from the browser button's content node
    nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(browse));

    // we shouldn't have to unregister this listener because when
    // our frame goes away all these content node go away as well
    // because our frame is the only one who references them.
    reciever->AddEventListenerByIID(this, kIDOMMouseListenerIID);
  }

  return NS_OK;
}

nsresult
nsFileControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  } else if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  } else if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  } else  if (aIID.Equals(kIDOMMouseListenerIID)) {                                         
    *aInstancePtr = (void*)(nsIDOMMouseListener*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

PRBool
nsFileControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

void 
nsFileControlFrame::Reset()
{
  if (mTextFrame) {
    mTextFrame->Reset();
  }
}

NS_IMETHODIMP 
nsFileControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_INPUT_FILE;
  return NS_OK;
}

// XXX this should be removed when nsView exposes it
nsIWidget* GetWindowTemp(nsIView *aView);
nsIWidget*
nsFileControlFrame::GetWindowTemp(nsIView *aView)
{
  nsIWidget *window = nsnull;

  nsIView *ancestor = aView;
  while (nsnull != ancestor) {
    ancestor->GetWidget(window);
	  if (nsnull != window) {
	    return window;
	  }
	  ancestor->GetParent(ancestor);
  }
  return nsnull;
}


void 
nsFileControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  if (mTextFrame) {
    mTextFrame->SetFocus(aOn, aRepaint);
  }
}

void
nsFileControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_ANYWHERE,NS_PRESSHELL_SCROLL_ANYWHERE);

  }
}

/**
 * This is called when our browse button is clicked
 */
nsresult 
nsFileControlFrame::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsIView* textView;
  mTextFrame->GetView(&textView);
  if (nsnull == textView) {
    return NS_OK;
  }

  nsresult result = NS_OK;
  nsIView*   parentView;
  textView->GetParent(parentView);
  nsIWidget* parentWidget = GetWindowTemp(parentView);
 
  nsIFileWidget *fileWidget = nsnull;
  
  nsString title("File Upload");
  nsComponentManager::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget);
  
  if (fileWidget) {
	  nsString titles[] = {"all files"};
	  nsString filters[] = {"*.*"};
	  fileWidget->SetFilterList(1, titles, filters);

	  fileWidget->Create(parentWidget, title, eMode_load, nsnull, nsnull);
	  result = fileWidget->Show();

	  if (result) {
	    nsFileSpec fileSpec;
	    fileWidget->GetFile(fileSpec);
	    char* leafName = fileSpec.GetLeafName();
	    if (leafName) {
	      mTextFrame->SetProperty(nsHTMLAtoms::value,leafName);
	      nsCRT::free(leafName);
      }
	  }
	  NS_RELEASE(fileWidget);
  }
  NS_RELEASE(parentWidget);

  return NS_OK;
}


NS_IMETHODIMP nsFileControlFrame::Reflow(nsIPresContext&          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  if (mFormFrame == nsnull && eReflowReason_Initial == aReflowState.reason) {
    // add ourself as an nsIFormControlFrame
    nsFormFrame::AddFormControlFrame(aPresContext, *this);
    mTextFrame = GetTextControlFrame(this);
  }

  return nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

/*
NS_IMETHODIMP
nsFileControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  nsresult r = nsAreaFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // given that the CSS frame constuctor created all our frames. We need to find the text field
  // so we can get info from it.
  mTextFrame = GetTextControlFrame(this);
}
*/

/**
 * Given a start node. Find the given text node inside. We need to do this because the 
 * frame constuctor create the frame and its implementation. So we are given the text
 * node from the constructor and we find it in our tree.
 */
nsTextControlFrame*
nsFileControlFrame::GetTextControlFrame(nsIFrame* aStart)
{
  // find the text control frame.
  nsIFrame* childFrame = nsnull;
  aStart->FirstChild(nsnull, &childFrame);

  while (nsnull != childFrame) {    
    // see if the child is a text control
    nsCOMPtr<nsIContent> content;
    childFrame->GetContent(getter_AddRefs(content));
    nsIAtom* atom;
    if (content->GetTag(atom) == NS_OK && atom == nsHTMLAtoms::input) {
      nsString value;

      if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, value)) {
        if (value == "text") {
           return (nsTextControlFrame*)childFrame;      
        }
      }
    }

    // if not continue looking
    nsTextControlFrame* frame = GetTextControlFrame(childFrame);
    if (frame != nsnull)
       return frame;
     
    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
  }

  return nsnull;
}

PRIntn
nsFileControlFrame::GetSkipSides() const
{
  return 0;
}


NS_IMETHODIMP
nsFileControlFrame::GetName(nsString* aResult)
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
nsFileControlFrame::GetMaxNumValues()
{
  return 1;
}
  
PRBool
nsFileControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                   nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  // use our name and the text widgets value 
  aNames[0] = name;
  nsresult status = PR_FALSE;

  if (NS_SUCCEEDED(mTextFrame->GetProperty(nsHTMLAtoms::value, aValues[0]))) {
    aNumValues = 1;
    status = PR_TRUE;
  }

  return status;
}

NS_IMETHODIMP
nsFileControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aHint)
{
  // set the text control to readonly or not
  if (nsHTMLAtoms::disabled == aAttribute) {
    //nsAutoString val(nsFormFrame::GetDisabled(this) ? "true":"false");
    nsCOMPtr<nsIDOMHTMLInputElement> textControl = do_QueryInterface(mTextContent);
    if (textControl)
    {
      textControl->SetDisabled(nsFormFrame::GetDisabled(this));
    }
    //mTextContent->SetHTMLAttribute(nsHTMLAtoms::disabled, val, PR_TRUE);
  }

  return nsAreaFrame::AttributeChanged(aPresContext, aChild,  aAttribute, aHint);
}

NS_IMETHODIMP
nsFileControlFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  if (nsFormFrame::GetDisabled(this)) {
    *aFrame = this;
    return NS_OK;
  } else {
    return nsAreaFrame::GetFrameForPoint(aPoint, aFrame);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFileControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("FileControl", aResult);
}

NS_IMETHODIMP
nsFileControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

NS_IMETHODIMP
nsFileControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}
nscoord 
nsFileControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
   return 0;
}

nscoord 
nsFileControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

nsresult nsFileControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP nsFileControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::value == aName) {
    mTextFrame->SetTextControlFrameState(aValue);                                         
  }
  return NS_OK;
}      

NS_IMETHODIMP nsFileControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    mTextFrame->GetTextControlFrameState(aValue);
  }
 
  return NS_OK;
}




NS_METHOD
nsFileControlFrame::Paint(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
  return nsAreaFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}
