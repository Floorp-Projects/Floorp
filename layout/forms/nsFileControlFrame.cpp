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
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIFileWidget.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"

// XXX make this pixels
#define CONTROL_SPACING 40  

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

nsresult
NS_NewFileControlFrame(nsIFrame*& aResult)
{
  aResult = new nsFileControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsFileControlFrame::nsFileControlFrame()
  : nsHTMLContainerFrame()
{
  mTextFrame   = nsnull;
  mBrowseFrame = nsnull;
  mFormFrame   = nsnull;
}

nsresult
nsFileControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
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

// this is in response to the MouseClick from the containing browse button
// XXX still need to get filters from accept attribute
void nsFileControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
  nsIView* textView;
  mTextFrame->GetView(&textView);
  if (nsnull == textView) {
    return;
  }
  nsIWidget* widget;
  mTextFrame->GetWidget(&widget);
  if (!widget) {
    return;
  }
 
  nsITextWidget* textWidget;
  nsresult result = widget->QueryInterface(kITextWidgetIID, (void**)&textWidget);
  if (NS_OK != result) {
    NS_RELEASE(widget);
    return;
  }
  
  nsIView*   parentView;
  textView->GetParent(parentView);
  nsIWidget* parentWidget = GetWindowTemp(parentView);
 
  nsIFileWidget *fileWidget;

  nsString title("File Upload");
  nsComponentManager::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget);
  
  nsString titles[] = {"all files"};
  nsString filters[] = {"*.*"};
  fileWidget->SetFilterList(1, titles, filters);

  fileWidget->Create(parentWidget, title, eMode_load, nsnull, nsnull);
  result = fileWidget->Show();

  if (result) {
    PRUint32 size;
    nsString fileName;
    fileWidget->GetFile(fileName);
    textWidget->SetText(fileName,size);
  }
  NS_RELEASE(fileWidget);
  NS_RELEASE(parentWidget);
  NS_RELEASE(textWidget);
  NS_RELEASE(widget);
}


NS_IMETHODIMP nsFileControlFrame::Reflow(nsIPresContext&          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  PRInt32 numChildren = mFrames.GetLength();
  
  nsIFrame* childFrame;
  if (0 == numChildren) {
    // XXX This code should move to Init(), someday when the frame construction
    // changes are all done and Init() is always getting called...
    PRBool disabled = nsFormFrame::GetDisabled(this);
    nsIHTMLContent* text = nsnull;
    nsIAtom* tag = NS_NewAtom("text");
    NS_NewHTMLInputElement(&text, tag);
    text->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, nsAutoString("text"), PR_FALSE);
    if (disabled) {
      text->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::disabled, nsAutoString("1"), PR_FALSE);  // XXX this should use an "empty" bool value
    }
    NS_NewTextControlFrame(childFrame);

     //XXX: This style should be cached, rather than resolved each time.
     // Get pseudo style for the text field
    nsCOMPtr<nsIStyleContext> textFieldStyleContext;
    nsresult rv = aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::fileTextStylePseudo,
                                                       mStyleContext, PR_FALSE,
                                                       getter_AddRefs(textFieldStyleContext));

    if (NS_SUCCEEDED(rv)) {
        // Found the pseudo style for the text field
      childFrame->Init(aPresContext, text, this, textFieldStyleContext, nsnull);
    } else {
        // Can't find pseduo style so use the style set for the file updload element
      childFrame->Init(aPresContext, mContent, this, mStyleContext, nsnull);
    }
    mTextFrame = (nsTextControlFrame*)childFrame;
    mFrames.SetFrames(childFrame);

    nsIHTMLContent* browse = nsnull;
    tag = NS_NewAtom("browse");
    NS_NewHTMLInputElement(&browse, tag);
    browse->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, nsAutoString("browse"), PR_FALSE);
    if (disabled) {
      browse->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::disabled, nsAutoString("1"), PR_FALSE);  // XXX should be "empty"
    }
    NS_NewButtonControlFrame(childFrame);
    ((nsButtonControlFrame*)childFrame)->SetMouseListener((nsIFormControlFrame*)this);
    mBrowseFrame = (nsButtonControlFrame*)childFrame;

     //XXX: This style should be cached, rather than resolved each time.
     // Get pseudo style for the button
    nsCOMPtr<nsIStyleContext> buttonStyleContext;
    rv = aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::fileButtonStylePseudo,
                                                       mStyleContext, PR_FALSE,
                                                       getter_AddRefs(buttonStyleContext));
    if (NS_SUCCEEDED(rv)) {
       // Found pseduo style for the button
      childFrame->Init(aPresContext, browse, this, buttonStyleContext, nsnull);
    } else {
       // Can't find pseudo style for the button so use the style set for the file upload element
      childFrame->Init(aPresContext, mContent, this, mStyleContext, nsnull);
    }

    mFrames.FirstChild()->SetNextSibling(childFrame);

    NS_RELEASE(text);
    NS_RELEASE(browse);
  }

  nsSize maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
  nsHTMLReflowMetrics desiredSize = aDesiredSize;
  aDesiredSize.width = CONTROL_SPACING; 
  aDesiredSize.height = 0;
  childFrame = mFrames.FirstChild();
  nsPoint offset(0,0);
  while (nsnull != childFrame) {  // reflow, place, size the children
    nsHTMLReflowState   reflowState(aPresContext, aReflowState, childFrame,
                                    maxSize);
    nsIHTMLReflow*      htmlReflow;

    if (NS_OK == childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->WillReflow(aPresContext);
      htmlReflow->Reflow(aPresContext, desiredSize, reflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
      nsRect rect(offset.x, offset.y, desiredSize.width, desiredSize.height);
      childFrame->SetRect(rect);
      maxSize.width  -= desiredSize.width;
      aDesiredSize.width  += desiredSize.width; 
      aDesiredSize.height = desiredSize.height;
      childFrame->GetNextSibling(&childFrame);
      offset.x += desiredSize.width + CONTROL_SPACING;
    }
  }

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
//XXX    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
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
  nsIWidget*  widget;
  nsITextWidget* textWidget;
  mTextFrame->GetWidget(&widget);
  if (widget && (NS_OK == widget->QueryInterface(kITextWidgetIID, (void**)&textWidget))) {
    PRUint32 actualSize;
    textWidget->GetText(aValues[0], 0, actualSize);
    aNumValues = 1;
    NS_RELEASE(textWidget);
    status = PR_TRUE;
  }
  NS_IF_RELEASE(widget);
  return status;
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

PRBool 
nsFileControlFrame::HasWidget()
{
  PRBool hasWidget = PR_FALSE;
  nsIWidget* widget;
  mTextFrame->GetWidget(&widget);
  if (widget) {
    NS_RELEASE(widget);
    hasWidget = PR_TRUE;
  } 
  return(hasWidget);
}




NS_METHOD
nsFileControlFrame::Paint(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
#ifndef XP_MAC
// XXX: This is temporary until we can find out whats going wrong on the MAC
// where widget's are not being created in presentation shell 0. 

  // Since the file control has a mTextFrame which does not live in
  // the content model it is necessary to get the current text value
  // from the nsFileControlFrame through the content model, then
  // explicitly ask the test frame to draw it.  The mTextFrame's Paint
  // method will still be called, but it will not render the current
  // contents because it will not be possible for the content
  // associated with the mTextFrame to get a handle to it frame in the
  // presentation shell 0.

   // Only paint if it doesn't have a widget.
  if (HasWidget())
    return NS_OK;

  nsAutoString browse("Browse...");
  nsRect rect;
  mBrowseFrame->GetRect(rect);
  mBrowseFrame->PaintButton(aPresContext, aRenderingContext, aDirtyRect,
                            browse, rect);

  mTextFrame->PaintTextControlBackground(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    nsString text;
    if (NS_SUCCEEDED(nsFormControlHelper::GetInputElementValue(mContent, &text, PR_FALSE))) {
      nsRect rect;
      mTextFrame->GetRect(rect);
      mTextFrame->PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, text, mStyleContext, rect);
    }
  }
#endif
  return NS_OK;
}
