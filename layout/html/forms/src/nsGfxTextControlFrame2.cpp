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
#include "nsIElementFactory.h"
#include "nsIServiceManager.h"
#include "nsIFrameSelection.h"


static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);





class nsTextAreaSelectionImpl : public nsISelectionController
{
public:
  nsTextAreaSelectionImpl(nsIFrameSelection *aSel){mFrameSelection = aSel;}
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


nsresult
NS_NewGfxTextControlFrame2(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
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


NS_IMETHOD
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
    if (NS_FAILED(result))
      return result;
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
    nsCOMPtr<nsIDOMDocuemtn> domdoc = do_QueryInterface(doc, &rv);
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

