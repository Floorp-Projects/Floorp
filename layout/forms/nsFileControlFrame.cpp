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

#include "nsFileControlFrame.h"
#include "nsFormFrame.h"
#include "nsGfxTextControlFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIPresState.h"
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
#include "nsIStatefulFrame.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID,     NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);

nsresult
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFileControlFrame* it = new (aPresShell) nsFileControlFrame();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsFileControlFrame::nsFileControlFrame():
  mTextFrame(nsnull), 
  mFormFrame(nsnull),
  mTextContent(nsnull),
  mCachedState(nsnull)
{
    //Shrink the area around it's contents
  SetFlags(NS_BLOCK_SHRINK_WRAP);
}

nsFileControlFrame::~nsFileControlFrame()
{
  NS_IF_RELEASE(mTextContent);
  if (mCachedState) {
    delete mCachedState;
    mCachedState = nsnull;
  }
  if (mFormFrame) {
    mFormFrame->RemoveFormControlFrame(*this);
    mFormFrame = nsnull;
  }
}

NS_IMETHODIMP
nsFileControlFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                           nsISupportsArray& aChildList)
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
  NS_IF_RELEASE(browse);

  nsString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::size, value)) {
    mTextContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::size, value, PR_FALSE);
  }

  return NS_OK;
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsFileControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  } else if (aIID.Equals(kIAnonymousContentCreatorIID)) {
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;
    return NS_OK;
  } else if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  } else  if (aIID.Equals(kIDOMMouseListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*) this;
    return NS_OK;
  } else  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
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
nsFileControlFrame::Reset(nsIPresContext* aPresContext)
{
  if (mTextFrame) {
    mTextFrame->Reset(aPresContext);
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
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);

  }
}

/**
 * This is called when our browse button is clicked
 */
nsresult 
nsFileControlFrame::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsIView* textView;
  mTextFrame->GetView(mPresContext, &textView);
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
      nsAutoString  pathName;
      fileSpec.GetNativePathString(pathName);
      mTextFrame->SetProperty(mPresContext, nsHTMLAtoms::value, pathName);
    }
    NS_RELEASE(fileWidget);
  }
  NS_RELEASE(parentWidget);

  return NS_OK;
}


NS_IMETHODIMP nsFileControlFrame::Reflow(nsIPresContext*          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  if (mFormFrame == nsnull && eReflowReason_Initial == aReflowState.reason) {
    // add ourself as an nsIFormControlFrame
    nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
    mTextFrame = GetTextControlFrame(aPresContext, this);
    if (!mTextFrame) return NS_ERROR_UNEXPECTED;
    if (mCachedState) {
      mTextFrame->SetProperty(aPresContext, nsHTMLAtoms::value, *mCachedState);
      delete mCachedState;
      mCachedState = nsnull;
    }
  }

  return nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

/*
NS_IMETHODIMP
nsFileControlFrame::SetInitialChildList(nsIPresContext* aPresContext,
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
nsGfxTextControlFrame*
nsFileControlFrame::GetTextControlFrame(nsIPresContext* aPresContext, nsIFrame* aStart)
{
  nsGfxTextControlFrame* result = nsnull;

  // find the text control frame.
  nsIFrame* childFrame = nsnull;
  aStart->FirstChild(aPresContext, nsnull, &childFrame);

  while (childFrame) {
    // see if the child is a text control
    nsCOMPtr<nsIContent> content;
    nsresult res = childFrame->GetContent(getter_AddRefs(content));
    if (NS_SUCCEEDED(res) && content) {
      nsCOMPtr<nsIAtom> atom;
      res = content->GetTag(*getter_AddRefs(atom));
      if (NS_SUCCEEDED(res) && atom) {
        if (atom.get() == nsHTMLAtoms::input) {

          // It's an input, is it a text input?
          nsAutoString value;
          if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, value)) {
            if (value.EqualsIgnoreCase("text")) {
              result = (nsGfxTextControlFrame*)childFrame;      
            }
          }
        }
      }
    }

    // if not continue looking
    nsGfxTextControlFrame* frame = GetTextControlFrame(aPresContext, childFrame);
    if (frame)
       result = frame;
     
    res = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(res == NS_OK,"failed to get next child");
  }

  return result;
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
                                       PRInt32         aNameSpaceID,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aHint)
{
  // set the text control to readonly or not
  if (nsHTMLAtoms::disabled == aAttribute) {
    nsCOMPtr<nsIDOMHTMLInputElement> textControl = do_QueryInterface(mTextContent);
    if (textControl)
    {
      textControl->SetDisabled(nsFormFrame::GetDisabled(this));
    }
  } else if (nsHTMLAtoms::size == aAttribute) {
    nsString value;
    if (nsnull != mTextContent && NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::size, value)) {
      mTextContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::size, value, PR_TRUE);
      if (aHint != NS_STYLE_HINT_REFLOW) {
        nsFormFrame::StyleChangeReflow(aPresContext, this);
      }
    }
  }

  return nsAreaFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
}

NS_IMETHODIMP
nsFileControlFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint,
                                     nsIFrame** aFrame)
{
  if (nsFormFrame::GetDisabled(this)) {
    *aFrame = this;
    return NS_OK;
  } else {
    return nsAreaFrame::GetFrameForPoint(aPresContext, aPoint, aFrame);
  }
  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsFileControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("FileControl", aResult);
}
#endif

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
nsFileControlFrame::GetFont(nsIPresContext* aPresContext, 
                            const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}
nscoord 
nsFileControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerHeight) const
{
   return 0;
}

nscoord 
nsFileControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
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


NS_IMETHODIMP nsFileControlFrame::SetProperty(nsIPresContext* aPresContext,
                                              nsIAtom* aName,
                                              const nsString& aValue)
{
  nsresult rv = NS_OK;
  if (nsHTMLAtoms::value == aName) {
    if (mTextFrame) {
      mTextFrame->SetTextControlFrameState(aValue);                                         
    } else {
      if (mCachedState) delete mCachedState;
      mCachedState = new nsString(aValue);
      if (!mCachedState) rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return rv;
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
nsFileControlFrame::Paint(nsIPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
  return nsAreaFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsFileControlFrame::GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType)
{
  *aStateType = nsIStatefulFrame::eFileType;
  return NS_OK;
}

NS_IMETHODIMP
nsFileControlFrame::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  // Construct a pres state.
  NS_NewPresState(aState); // The addref happens here.
  
  // This string will hold a single item, whether or not we're checked.
  nsAutoString stateString;
  GetProperty(nsHTMLAtoms::value, stateString);
  (*aState)->SetStateProperty("checked", stateString);

  return NS_OK;
}

NS_IMETHODIMP
nsFileControlFrame::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  nsAutoString string;
  aState->GetStateProperty("checked", string);
  SetProperty(aPresContext, nsHTMLAtoms::value, string);
  return NS_OK;

}
