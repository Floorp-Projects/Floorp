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

#include "nsInput.h"
#include "nsInputFrame.h"
#include "nsInputFile.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLForms.h"
#include "nsInlineFrame.h"
#include "nsIFileWidget.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsRepository.h"
#include "nsIView.h"

PRInt32 nsInputFileFrame::gSpacing = 40;

nsInputFileFrame::nsInputFileFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsInlineFrame(aContent, aParentFrame)
{
}

nsInputFileFrame::~nsInputFileFrame()
{
}

// XXX this should be removed when nsView exposes it
nsIWidget*
GetWindowTemp(nsIView *aView)
{
  nsIWidget *window = nsnull;

  nsIView *ancestor = aView;
  while (nsnull != ancestor) {
	  if (nsnull != (window = ancestor->GetWidget())) {
	    return window;
	  }
	  ancestor = ancestor->GetParent();
  }
  return nsnull;
}

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);

// this is in response to the MouseClick from the containing browse button
// XXX still need to get filters from accept attribute
void nsInputFileFrame::MouseClicked(nsIPresContext* aPresContext)
{
  nsInputFrame* textFrame = (nsInputFrame *)mFirstChild;
  nsITextWidget* textWidget;
  nsIView* textView;
  textFrame->GetView(textView);
  if (nsnull == textView) {
    return;
  }
  if (NS_OK != textFrame->GetWidget(textView, (nsIWidget **)&textWidget)) {
    return;
  }
  nsIWidget* parentWidget = GetWindowTemp(textView->GetParent());
 
  nsIFileWidget *fileWidget;

  nsString title("FileWidget Title <here> mode = save");
  NSRepository::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget);
  
  nsString titles[] = {"all files"};
  nsString filters[] = {"*.*"};
  fileWidget->SetFilterList(1, titles, filters);

  fileWidget->Create(parentWidget, title, eMode_load, nsnull, nsnull);
  PRUint32 result = fileWidget->Show();

  if (result) {
    nsString fileName;
    fileWidget->GetFile(fileName);
    ((nsITextWidget *)textWidget)->SetText(fileName);
  }
  NS_RELEASE(fileWidget);
  NS_RELEASE(parentWidget);
  NS_RELEASE(textView);
  NS_RELEASE(textWidget);
}

NS_IMETHODIMP
nsInputFileFrame::MoveTo(nscoord aX, nscoord aY)
{
  if ( ((aX == 0) && (aY == 0)) || (aX != mRect.x) || (aY != mRect.y)) {
    nsIFrame* childFrame = mFirstChild;
    nscoord x = aX;
    nscoord y = aY;
    while (nsnull != childFrame) {
      nsresult result = childFrame->MoveTo(x, y);
      nsSize childSize;
      ((nsInputFrame *)childFrame)->GetWidgetSize(childSize);
      x = x + childSize.width + gSpacing;
      childFrame->GetNextSibling(childFrame);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInputFileFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;

  // Let the view know the correct size
  nsIView* view = nsnull;
  GetView(view);
  if (nsnull != view) {
    view->SetDimensions(aWidth, aHeight);
    NS_RELEASE(view);
  }
  return NS_OK;
}

NS_IMETHODIMP nsInputFileFrame::Reflow(nsIPresContext*      aCX, 
                                       nsReflowMetrics&     aDesiredSize,
                                       const nsReflowState& aReflowState, 
                                       nsReflowStatus&      aStatus)
{
  nsIFrame* childFrame;
  PRInt32 numChildren;
  ChildCount(numChildren);  
  if (0 == numChildren) { // create the children frames 
    nsInputFile* content = (nsInputFile *)mContent;
    numChildren = content->ChildCount();
    NS_ASSERTION(2 == numChildren, "nsInputFile must contain 2 children");
    nsInput* childContent;
    if (nsnull == mStyleContext) {
      GetStyleContext(aCX, mStyleContext);
    }
    for (int i = 0; i < numChildren; i++) {
      childContent = (nsInput *)content->ChildAt(i);
      // Use this style context for the children. They will not modify it.
      // XXX When IStyleContext provides an api for cloning/inheriting, it could be used instead.
      childContent->CreateFrame(aCX, this, mStyleContext, childFrame);
      if (0 == i) {
        mFirstChild = childFrame;
        SetFirstContentOffset(mFirstChild);
      }
      else {
        mFirstChild->SetNextSibling(childFrame);
        SetLastContentOffset(childFrame);
      }
      NS_RELEASE(childContent);
      mChildCount++;
    }
  }

  nsSize maxSize = aReflowState.maxSize;
  nsReflowMetrics desiredSize = aDesiredSize;
  aDesiredSize.width = gSpacing; 
  aDesiredSize.height = 0;
  childFrame = mFirstChild;
  while (nsnull != childFrame) {
    nsReflowState   reflowState(aReflowState, maxSize);
    nsresult result = childFrame->Reflow(aCX, desiredSize, reflowState, aStatus);
    // XXX check aStatus ??
    if (NS_OK != result) {
      break;
    }
    maxSize.width  -= desiredSize.width;
    aDesiredSize.width  += desiredSize.width; 
    aDesiredSize.height = desiredSize.height;
    childFrame->GetNextSibling(childFrame);
  }

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}



//----------------------------------------------------------------------
// nsInputFile

nsInputFile::nsInputFile(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager) 
{
}

nsInputFile::~nsInputFile()
{
}

nsresult
nsInputFile::CreateFrame(nsIPresContext* aPresContext,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult)
{
  nsIFrame* frame = new nsInputFileFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void nsInputFile::GetType(nsString& aResult) const
{
  aResult = "file";
}


void nsInputFile::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  // get the text and set its relevant attributes 
  nsInput* text = (nsInput *)ChildAt(0);
  if ((aAttribute == nsHTMLAtoms::size) || (aAttribute == nsHTMLAtoms::maxlength) || 
      (aAttribute == nsHTMLAtoms::value) || 
      (aAttribute == nsHTMLAtoms::disabled) || (aAttribute == nsHTMLAtoms::readonly)) 
  {
    text->SetAttribute(aAttribute, aValue);
  }
  NS_RELEASE(text);
 
  nsInputFileSuper::SetAttribute(aAttribute, aValue); 
}

nsresult
NS_NewHTMLInputFile(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* inputFile = new nsInputFile(aTag, aManager);
  if (nsnull == inputFile) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIHTMLContent* child;
  nsresult status = NS_NewHTMLInputText(&child, aTag, aManager);
  if (NS_OK != status) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  inputFile->AppendChild(child);
  status = NS_NewHTMLInputBrowse(&child, aTag, aManager);
  if (NS_OK != status) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  inputFile->AppendChild(child);

  return inputFile->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

