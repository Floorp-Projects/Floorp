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
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLForms.h"
#include "nsIFileWidget.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsRepository.h"
#include "nsIView.h"
#include "nsHTMLParts.h"

PRInt32 nsInputFileFrame::gSpacing = 40;
nsString* nsInputFile::gFILE_TYPE = new nsString("file");

nsresult
NS_NewInputFileFrame(nsIContent* aContent,
                     nsIFrame*   aParent,
                     nsIFrame*&  aResult)
{
  aResult = new nsInputFileFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsInputFileFrame::nsInputFileFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsHTMLContainerFrame(aContent, aParentFrame)
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
    ancestor->GetWidget(window);
	  if (nsnull != window) {
	    return window;
	  }
	  ancestor->GetParent(ancestor);
  }
  return nsnull;
}

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);

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
  nsIView*   parentView;
  textView->GetParent(parentView);
  nsIWidget* parentWidget = GetWindowTemp(parentView);
 
  nsIFileWidget *fileWidget;

  nsString title("FileWidget Title <here> mode = save");
  nsRepository::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget);
  
  nsString titles[] = {"all files"};
  nsString filters[] = {"*.*"};
  fileWidget->SetFilterList(1, titles, filters);

  fileWidget->Create(parentWidget, title, eMode_load, nsnull, nsnull);
  PRUint32 result = fileWidget->Show();

  if (result) {
    PRUint32 size;
    nsString fileName;
    fileWidget->GetFile(fileName);
    ((nsITextWidget *)textWidget)->SetText(fileName,size);
  }
  NS_RELEASE(fileWidget);
  NS_RELEASE(parentWidget);
  NS_RELEASE(textWidget);
}


NS_IMETHODIMP nsInputFileFrame::Reflow(nsIPresContext&      aPresContext, 
                                       nsReflowMetrics&     aDesiredSize,
                                       const nsReflowState& aReflowState, 
                                       nsReflowStatus&      aStatus)
{
  nsInputFile* content = (nsInputFile*)mContent;
  nsIFrame* childFrame;

  if (nsnull == mFirstChild) {
    // XXX This code should move to Init(), someday when the frame construction
    // changes are all done and Init() is always getting called...
    nsInputText* textField = content->GetTextField();
    nsInput* browseButton = content->GetBrowseButton();
    if ((nsnull != textField) && (nsnull != browseButton))  {
      NS_NewInputTextFrame(textField, this, childFrame);
      childFrame->SetStyleContext(&aPresContext, mStyleContext);
      mFirstChild = childFrame;
      NS_NewInputButtonFrame(browseButton, this, childFrame);
      childFrame->SetStyleContext(&aPresContext, mStyleContext);
      mFirstChild->SetNextSibling(childFrame);
      mChildCount = 2;
    }
    NS_IF_RELEASE(textField);
    NS_IF_RELEASE(browseButton);
  }

  nsSize maxSize = aReflowState.maxSize;
  nsReflowMetrics desiredSize = aDesiredSize;
  aDesiredSize.width = gSpacing; 
  aDesiredSize.height = 0;
  childFrame = mFirstChild;
  nsPoint offset(0,0);
  while (nsnull != childFrame) {  // reflow, place, size the children
    nsReflowState   reflowState(childFrame, aReflowState, maxSize);
    childFrame->WillReflow(aPresContext);
    nsresult result = childFrame->Reflow(aPresContext, desiredSize, reflowState, aStatus);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
    nsRect rect(offset.x, offset.y, desiredSize.width, desiredSize.height);
    childFrame->SetRect(rect);
    maxSize.width  -= desiredSize.width;
    aDesiredSize.width  += desiredSize.width; 
    aDesiredSize.height = desiredSize.height;
    childFrame->GetNextSibling(childFrame);
    offset.x += desiredSize.width + gSpacing;
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

PRIntn
nsInputFileFrame::GetSkipSides() const
{
  return 0;
}


//----------------------------------------------------------------------
// nsInputFile

nsInputFile::nsInputFile(nsIAtom* aTag, nsIFormManager* aManager)
  : nsInput(aTag, aManager) 
{
  mTextField    = nsnull;
  mBrowseButton = nsnull;
}

nsInputFile::~nsInputFile()
{
  NS_IF_RELEASE(mTextField);
  NS_IF_RELEASE(mBrowseButton);
}

nsInputText* nsInputFile::GetTextField() 
{
  NS_IF_ADDREF(mTextField);
  return mTextField;
}

void nsInputFile::SetTextField(nsInputText* aTextField)
{
  NS_IF_RELEASE(mTextField);
  NS_IF_ADDREF(aTextField);
  mTextField = aTextField;
}

nsInput* nsInputFile::GetBrowseButton() 
{
  NS_IF_ADDREF(mBrowseButton);
  return mBrowseButton;
}

void nsInputFile::SetBrowseButton(nsInput* aBrowseButton)
{
  NS_IF_RELEASE(mBrowseButton);
  NS_IF_ADDREF(aBrowseButton);
  mBrowseButton = aBrowseButton;
}

void nsInputFile::GetType(nsString& aResult) const
{
  aResult = "file";
}

PRInt32 
nsInputFile::GetMaxNumValues()
{
  return 1;
}
  
PRBool
nsInputFile::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                            nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
    return PR_FALSE;
  }
  // use our name and the text widgets value 
  aNames[0] = *mName;
  nsIWidget*      widget = mTextField->GetWidget();
  nsITextWidget*  textWidget = nsnull;
    

  if (widget != nsnull && NS_OK == widget->QueryInterface(kITextWidgetIID,(void**)textWidget))
  {
    PRUint32 actualSize;
    textWidget->GetText(aValues[0], 0, actualSize);
    aNumValues = 1;
    NS_RELEASE(textWidget);
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsInputFile::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify)
{
  // get the text and set its relevant attributes 
  if ((aAttribute == nsHTMLAtoms::size) || (aAttribute == nsHTMLAtoms::maxlength) || 
      (aAttribute == nsHTMLAtoms::value) || 
      (aAttribute == nsHTMLAtoms::disabled) || (aAttribute == nsHTMLAtoms::readonly)) 
  {
    mTextField->SetAttribute(aAttribute, aValue, aNotify);
  }
 
  return nsInputFileSuper::SetAttribute(aAttribute, aValue, aNotify);
}

nsresult
NS_NewHTMLInputFile(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsInputFile* inputFile = new nsInputFile(aTag, aManager);
  if (nsnull == inputFile) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsInputText* textField;
  // pass in a null form manager, since this text field is not part of the content model
  nsresult status = NS_NewHTMLInputText((nsIHTMLContent**)&textField, aTag, nsnull);
  if (NS_OK != status) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  inputFile->SetTextField(textField);
  NS_RELEASE(textField);

  nsInput* browseButton;
  // pass in a null form manager, since this browse button is not part of the content model
  status = NS_NewHTMLInputBrowse((nsIHTMLContent**)&browseButton, aTag, nsnull);
  if (NS_OK != status) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  inputFile->SetBrowseButton(browseButton);
  NS_RELEASE(browseButton);

  return inputFile->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

