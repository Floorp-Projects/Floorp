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

#include "nsGfxTextControlFrame2.h"
#include "nsFormFrame.h"
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
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsDocument.h"
#include "nsIDOMMouseListener.h"
#include "nsIPresShell.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIComponentManager.h"
#include "nsIElementFactory.h"
#include "nsIServiceManager.h"

#include "nsIFrameSelection.h"
#include "nsIHTMLEditor.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsFormControlHelper.h"
#include "nsIDocumentEncoder.h"


static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);
static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static void RemoveNewlines(nsString &aString);

static void RemoveNewlines(nsString &aString)
{
  // strip CR/LF and null
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}




class nsTextAreaSelectionImpl : public nsISelectionController
{
public:
  NS_DECL_ISUPPORTS

  nsTextAreaSelectionImpl(nsIFrameSelection *aSel){  NS_INIT_REFCNT(); mFrameSelection = aSel;}
  ~nsTextAreaSelectionImpl(){}

  
  //NSISELECTIONCONTROLLER INTERFACES
  NS_IMETHOD SetDisplaySelection(PRInt16 toggle){return NS_OK;}
  NS_IMETHOD GetDisplaySelection(PRInt16 *_retval){return NS_OK;}
  NS_IMETHOD GetSelection(PRInt16 type, nsIDOMSelection **_retval){return NS_OK;}
  NS_IMETHOD ScrollSelectionIntoView(PRInt16 type, PRInt16 region){return NS_OK;}
  NS_IMETHOD RepaintSelection(PRInt16 type){return NS_OK;}
  NS_IMETHOD SetCaretEnabled(PRBool enabled){return NS_OK;}
  NS_IMETHOD GetCaretEnabled(PRBool *_retval){return NS_OK;}
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend){return NS_OK;}
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend){return NS_OK;}
  NS_IMETHOD LineMove(PRBool aForward, PRBool aExtend){return NS_OK;}
  NS_IMETHOD IntraLineMove(PRBool aForward, PRBool aExtend){return NS_OK;}
  NS_IMETHOD PageMove(PRBool aForward, PRBool aExtend){return NS_OK;}
  NS_IMETHOD CompleteScroll(PRBool aForward){return NS_OK;}
  NS_IMETHOD CompleteMove(PRBool aForward, PRBool aExtend){return NS_OK;}
  NS_IMETHOD ScrollPage(PRBool aForward){return NS_OK;}
  NS_IMETHOD ScrollLine(PRBool aForward){return NS_OK;}
  NS_IMETHOD ScrollHorizontal(PRBool aLeft){return NS_OK;}
  NS_IMETHOD SelectAll(void){return NS_OK;}
private:
  nsCOMPtr<nsIFrameSelection> mFrameSelection;
};

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsTextAreaSelectionImpl, nsISelectionController)


nsresult
NS_NewGfxTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxTextControlFrame2* it = new (aPresShell) nsGfxTextControlFrame2();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE_INHERITED1(nsGfxTextControlFrame2, nsHTMLContainerFrame,nsIAnonymousContentCreator);
NS_IMPL_ADDREF_INHERITED(nsGfxTextControlFrame2, nsHTMLContainerFrame);
NS_IMPL_RELEASE_INHERITED(nsGfxTextControlFrame2, nsHTMLContainerFrame);
 

nsGfxTextControlFrame2::nsGfxTextControlFrame2()
{
}

nsGfxTextControlFrame2::~nsGfxTextControlFrame2()
{
}


// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame2::IsSingleLineTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT==type) || (NS_FORM_INPUT_PASSWORD==type)) {
    return PR_TRUE;
  }
  return PR_FALSE; 
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame2::IsPlainTextControl() const
{
  // need to check HTML attribute of mContent and/or CSS.
  return PR_TRUE;
}

PRBool nsGfxTextControlFrame2::IsPasswordTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_PASSWORD==type) {
    return PR_TRUE;
  }
  return PR_FALSE;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::CreateFrameFor(nsIPresContext*   aPresContext,
                               nsIContent *      aContent,
                               nsIFrame**        aFrame)
{
  aContent = nsnull;
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsGfxTextControlFrame2::CreateAnonymousContent(nsIPresContext* aPresContext,
                                           nsISupportsArray& aChildList)
{
//create editor
//create selection
//init editor with div.

  nsCAutoString progID = NS_ELEMENT_FACTORY_PROGID_PREFIX;
  progID += "http://www.w3.org/TR/REC-html40";
  nsresult rv;
  NS_WITH_SERVICE(nsIElementFactory, elementFactory, progID, &rv);
  if (!elementFactory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content;
  elementFactory->CreateInstanceByTag(NS_ConvertToString("div"), getter_AddRefs(content));
  if (content)
  {
    aChildList.AppendElement(content);

//make the editor
    rv = nsComponentManager::CreateInstance(kHTMLEditorCID,
                                                nsnull,
                                                NS_GET_IID(nsIEditor), getter_AddRefs(mEditor));
    if (NS_FAILED(rv))
      return rv;
    if (!mEditor) 
      return NS_ERROR_OUT_OF_MEMORY;
//create selection
    nsCOMPtr<nsIFrameSelection> frameSel;
    rv = nsComponentManager::CreateInstance(kFrameSelectionCID, nsnull,
                                                   NS_GET_IID(nsIFrameSelection),
                                                   getter_AddRefs(frameSel));
//create selection controller
    nsTextAreaSelectionImpl * textSelImpl = new nsTextAreaSelectionImpl(frameSel);
    mSelCon =  do_QueryInterface((nsISupports *)textSelImpl);//this will addref it once

//get the presshell
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv) || !shell)
      return rv?rv:NS_ERROR_FAILURE;
//get the document
    nsCOMPtr<nsIDocument> doc;
    rv = shell->GetDocument(getter_AddRefs(doc));
    if (NS_FAILED(rv) || !doc)
      return rv?rv:NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMDocument> domdoc = do_QueryInterface(doc, &rv);
    if (NS_FAILED(rv) || !domdoc)
      return rv?rv:NS_ERROR_FAILURE;
//get the flags
    PRUint32 editorFlags = 0;
    if (IsPlainTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPlaintextMask;
    if (IsSingleLineTextControl())
      editorFlags |= nsIHTMLEditor::eEditorSingleLineMask;
    if (IsPasswordTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPasswordMask;

//initialize the editor
    mEditor->Init(domdoc, shell, mSelCon, 0);
  }
  return NS_OK;
}

NS_IMETHODIMP nsGfxTextControlFrame2::Reflow(nsIPresContext*          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  // assuming 1 child
  nsIFrame* child = mFrames.FirstChild();
  //mFrames.FirstChild(aPresContext,nsnull,&child);
  if (!child)
    return nsHTMLContainerFrame::Reflow(aPresContext,aDesiredSize,aReflowState,aStatus);
  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, child,
                                   availSize);

  kidReflowState.mComputedWidth = aReflowState.mComputedWidth;
  kidReflowState.mComputedHeight = aReflowState.mComputedHeight;

  if (aReflowState.reason == eReflowReason_Incremental)
  {
    if (aReflowState.reflowCommand) {
       nsIFrame* incrementalChild = nsnull;
       aReflowState.reflowCommand->GetNext(incrementalChild);
       NS_ASSERTION(incrementalChild == child, "Child is not in our list!!");
    }
  }

  nsresult rv = ReflowChild(child, aPresContext, aDesiredSize, kidReflowState, 0, 0, 0, aStatus);

  return rv;
}


PRIntn
nsGfxTextControlFrame2::GetSkipSides() const
{
  return 0;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetType(PRInt32* aType) const
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIFormControl* formControl = nsnull;
    result = mContent->QueryInterface(kIFormControlIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      result = formControl->GetType(aType);
      NS_RELEASE(formControl);
    }
  }
  return result;
}


void
nsGfxTextControlFrame2::SetTextControlFrameState(const nsString& aValue)
{
  if (mEditor) 
  {
    nsAutoString currentValue;
    nsAutoString format; format.AssignWithConversion("text/plain");
    nsresult result = mEditor->OutputToString(currentValue, format, 0);
    if (PR_TRUE==IsSingleLineTextControl()) {
      RemoveNewlines(currentValue); 
    }
    if (PR_FALSE==currentValue.Equals(aValue))  // this is necessary to avoid infinite recursion
    {
      // \r is an illegal character in the dom, but people use them,
      // so convert windows and mac platform linebreaks to \n:
      // Unfortunately aValue is declared const, so we have to copy
      // in order to do this substitution.
      currentValue.Assign(aValue);
      nsFormControlHelper::PlatformToDOMLineBreaks(currentValue);

      nsCOMPtr<nsIDOMDocument>domDoc;
      result = mEditor->GetDocument(getter_AddRefs(domDoc));
			if (NS_FAILED(result)) return;
			if (!domDoc) return;

      result = mEditor->SelectAll();
      nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
			if (!htmlEditor) return;

			// get the flags, remove readonly and disabled, set the value, restore flags
			PRUint32 flags, savedFlags;
			mEditor->GetFlags(&savedFlags);
			flags = savedFlags;
			flags &= ~(nsIHTMLEditor::eEditorDisabledMask);
			flags &= ~(nsIHTMLEditor::eEditorReadonlyMask);
			mEditor->SetFlags(flags);
      mEditor->SelectAll();
      mEditor->DeleteSelection(nsIEditor::eNone);
      htmlEditor->InsertText(currentValue);
      mEditor->SetFlags(savedFlags);
    }
  }
  else {/*
    mCachedState = aValue;
    if (mDisplayContent)
    {
      const PRUnichar *text = mCachedStat.>GetUnicode();
      PRInt32 len = mCachedStateLength();
      mDisplayContent->SetText(text, len, PR_TRUE);
      if (mContent)
      {
        nsIDocument *doc;
        mContent->GetDocument(doc);
        if (doc) 
        {
          doc->ContentChanged(mContent, nsnull);
          NS_RELEASE(doc);
        }
      }
    }*/
  }
}


//XXX: this needs to be fixed for HTML output
void nsGfxTextControlFrame2::GetTextControlFrameState(nsString& aValue)
{
  aValue.SetLength(0);  // initialize out param
  
  if (mEditor ) 
  {
    nsString format; format.AssignWithConversion("text/plain");
    PRUint32 flags = 0;

    if (PR_TRUE==IsPlainTextControl()) {
      flags |= nsIDocumentEncoder::OutputBodyOnly;   // OutputNoDoctype if head info needed
    }

    nsFormControlHelper::nsHTMLTextWrap wrapProp;
    nsresult result = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
    if (NS_CONTENT_ATTR_NOT_THERE != result) 
    {
      if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard)
      {
        flags |= nsIDocumentEncoder::OutputFormatted;
      }
    }

    mEditor->OutputToString(aValue, format, flags);
  }
  // otherwise, just return our text attribute
  else {
/*    if (mCachedState) {
      aValue = *mCachedState;
    } else {
      GetText(&aValue, PR_TRUE);
    }*/
  }
}     
