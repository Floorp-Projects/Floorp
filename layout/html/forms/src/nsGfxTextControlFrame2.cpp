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
 * Owner:
 * Michael F. Judge mjudge@netscape.com
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsGfxTextControlFrame2.h"
#include "nsIDocument.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsIServiceManager.h"
#include "nsIFrameSelection.h"
#include "nsIHTMLEditor.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsFormControlHelper.h"
#include "nsIDocumentEncoder.h"
#include "nsICaret.h"
#include "nsIDOMSelectionListener.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIEditorController.h"
#include "nsIElementFactory.h"
#include "nsIHTMLContent.h"
#include "nsFormFrame.h"
#include "nsIEditorIMESupport.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"



#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIComponentManager.h"


#define DEFAULT_COLUMN_WIDTH 20


static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);
static void RemoveNewlines(nsString &aString);

static void RemoveNewlines(nsString &aString)
{
  // strip CR/LF and null
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}




class nsTextAreaSelectionImpl : public nsSupportsWeakReference, public nsISelectionController, public nsIFrameSelection
{
public:
  NS_DECL_ISUPPORTS

  nsTextAreaSelectionImpl(nsIFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter);
  ~nsTextAreaSelectionImpl(){}

  
  //NSISELECTIONCONTROLLER INTERFACES
  NS_IMETHOD SetDisplaySelection(PRInt16 toggle);
  NS_IMETHOD GetDisplaySelection(PRInt16 *_retval);
  NS_IMETHOD GetSelection(PRInt16 type, nsIDOMSelection **_retval);
  NS_IMETHOD ScrollSelectionIntoView(PRInt16 type, PRInt16 region);
  NS_IMETHOD RepaintSelection(PRInt16 type);
  NS_IMETHOD RepaintSelection(nsIPresContext* aPresContext, SelectionType aSelectionType);
  NS_IMETHOD SetCaretEnabled(PRBool enabled);
  NS_IMETHOD SetCaretReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetCaretEnabled(PRBool *_retval);
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD LineMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD IntraLineMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD PageMove(PRBool aForward, PRBool aExtend){return NS_OK;}//*
  NS_IMETHOD CompleteScroll(PRBool aForward){return NS_OK;}//*
  NS_IMETHOD CompleteMove(PRBool aForward, PRBool aExtend){return NS_OK;}//*
  NS_IMETHOD ScrollPage(PRBool aForward){return NS_OK;}//*
  NS_IMETHOD ScrollLine(PRBool aForward){return NS_OK;}//*
  NS_IMETHOD ScrollHorizontal(PRBool aLeft){return NS_OK;}//*
  NS_IMETHOD SelectAll(void);

  //NSIFRAMSELECTION INTERFACES
  NS_IMETHOD Init(nsIFocusTracker *aTracker, nsIContent *aLimiter) ;
  NS_IMETHOD ShutDown() ;
  NS_IMETHOD HandleTextEvent(nsGUIEvent *aGuiEvent) ;
  NS_IMETHOD HandleKeyEvent(nsIPresContext* aPresContext, nsGUIEvent *aGuiEvent);
  NS_IMETHOD HandleClick(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset , 
                       PRBool aContinueSelection, PRBool aMultipleSelection, PRBool aHint); 
  NS_IMETHOD HandleDrag(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint);
  NS_IMETHOD HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRUint32 aTarget, nsMouseEvent *aMouseEvent);
  NS_IMETHOD StartAutoScrollTimer(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint, PRUint32 aDelay);
  NS_IMETHOD StopAutoScrollTimer();
  NS_IMETHOD EnableFrameNotification(PRBool aEnable);
  NS_IMETHOD LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails, PRBool aSlowCheck);
  NS_IMETHOD SetMouseDownState(PRBool aState);
  NS_IMETHOD GetMouseDownState(PRBool *aState);
  NS_IMETHOD GetTableCellSelection(PRBool *aState);
  NS_IMETHOD GetTableCellSelectionStyleColor(const nsStyleColor **aStyleColor);
  NS_IMETHOD GetFrameForNodeOffset(nsIContent *aNode, PRInt32 aOffset, nsIFrame **aReturnFrame, PRInt32 *aReturnOffset);

private:
  nsCOMPtr<nsIFrameSelection> mFrameSelection;
  nsCOMPtr<nsIContent>        mLimiter;
  nsWeakPtr mPresShellWeak;
};

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS3(nsTextAreaSelectionImpl, nsISelectionController, nsISupportsWeakReference, nsIFrameSelection)



// BEGIN nsTextAreaSelectionImpl

nsTextAreaSelectionImpl::nsTextAreaSelectionImpl(nsIFrameSelection *aSel, nsIPresShell *aShell, nsIContent *aLimiter)
{
  NS_INIT_REFCNT();
  if (aSel && aShell)
  {
    mFrameSelection = aSel;//we are the owner now!
    nsCOMPtr<nsIFocusTracker> tracker = do_QueryInterface(aShell);
    mLimiter = aLimiter;
    mFrameSelection->Init(tracker, mLimiter);
    mPresShellWeak = getter_AddRefs( NS_GetWeakReference(aShell) );
  }
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::SetDisplaySelection(PRInt16 aToggle)
{
  if (mFrameSelection)
    return mFrameSelection->SetDisplaySelection(aToggle);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::GetDisplaySelection(PRInt16 *aToggle)
{
  if (mFrameSelection)
    return mFrameSelection->GetDisplaySelection(aToggle);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::GetSelection(PRInt16 type, nsIDOMSelection **_retval)
{
  if (mFrameSelection)
    return mFrameSelection->GetSelection(type, _retval);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::ScrollSelectionIntoView(PRInt16 type, PRInt16 region)
{
  if (mFrameSelection)
    return mFrameSelection->ScrollSelectionIntoView(type, region);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::RepaintSelection(PRInt16 type)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
  if (presShell)
  {
    nsCOMPtr<nsIPresContext> context;
    if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(context))) && context)
    {
      return mFrameSelection->RepaintSelection(context, type);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::RepaintSelection(nsIPresContext* aPresContext, SelectionType aSelectionType)
{
  return RepaintSelection(aSelectionType);
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::SetCaretEnabled(PRBool enabled)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(result = shell->GetCaret(getter_AddRefs(caret))))
    {
      nsCOMPtr<nsIDOMSelection> domSel;
      if (NS_SUCCEEDED(result = mFrameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))))
      {
        return caret->SetCaretVisible(enabled, domSel);
      }
    }

  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::SetCaretReadOnly(PRBool aReadOnly)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsresult result;
  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShellWeak, &result);
  if (shell)
  {
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(result = shell->GetCaret(getter_AddRefs(caret))))
    {
      nsCOMPtr<nsIDOMSelection> domSel;
      if (NS_SUCCEEDED(result = mFrameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))))
      {
        return caret->SetCaretReadOnly(aReadOnly, domSel);
      }
    }

  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTextAreaSelectionImpl::GetCaretEnabled(PRBool *_retval)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShellWeak);
  if (selCon)
  {
    return selCon->GetCaretEnabled(_retval);//we can use presshells because there is only 1 caret
  }
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::CharacterMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->CharacterMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::WordMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->WordMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::LineMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->LineMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::IntraLineMove(PRBool aForward, PRBool aExtend)
{
  if (mFrameSelection)
    return mFrameSelection->IntraLineMove(aForward, aExtend);
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::SelectAll()
{
  if (mFrameSelection)
    return mFrameSelection->SelectAll();
  return NS_ERROR_NULL_POINTER;
}


//nsTextAreaSelectionImpl::FRAMESELECTIONAPIS

NS_IMETHODIMP
nsTextAreaSelectionImpl::Init(nsIFocusTracker *aTracker, nsIContent *aLimiter)
{
  return mFrameSelection->Init(aTracker, aLimiter);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::ShutDown()
{
  return mFrameSelection->ShutDown();
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::HandleTextEvent(nsGUIEvent *aGuiEvent)
{
  return mFrameSelection->HandleTextEvent(aGuiEvent);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::HandleKeyEvent(nsIPresContext* aPresContext, nsGUIEvent *aGuiEvent)
{
  return mFrameSelection->HandleKeyEvent(aPresContext, aGuiEvent);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::HandleClick(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset , 
                     PRBool aContinueSelection, PRBool aMultipleSelection, PRBool aHint)
{
  return mFrameSelection->HandleClick(aNewFocus, aContentOffset, aContentEndOffset , 
                     aContinueSelection, aMultipleSelection, aHint);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::HandleDrag(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint)
{
  return mFrameSelection->HandleDrag(aPresContext, aFrame, aPoint);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRUint32 aTarget, nsMouseEvent *aMouseEvent)
{
  return mFrameSelection->HandleTableSelection(aParentContent, aContentOffset, aTarget, aMouseEvent);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::StartAutoScrollTimer(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint, PRUint32 aDelay)
{
  return mFrameSelection->StartAutoScrollTimer(aPresContext, aFrame, aPoint, aDelay);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::StopAutoScrollTimer()
{
  return mFrameSelection->StopAutoScrollTimer();
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::EnableFrameNotification(PRBool aEnable)
{
  return mFrameSelection->EnableFrameNotification(aEnable);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                           SelectionDetails **aReturnDetails, PRBool aSlowCheck)
{
  return mFrameSelection->LookUpSelection(aContent, aContentOffset, aContentLength, aReturnDetails, aSlowCheck);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::SetMouseDownState(PRBool aState)
{
  return mFrameSelection->SetMouseDownState(aState);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::GetMouseDownState(PRBool *aState)
{
  return mFrameSelection->GetMouseDownState(aState);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::GetTableCellSelection(PRBool *aState)
{
  return mFrameSelection->GetTableCellSelection(aState);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::GetTableCellSelectionStyleColor(const nsStyleColor **aStyleColor)
{
  return mFrameSelection->GetTableCellSelectionStyleColor(aStyleColor);
}


NS_IMETHODIMP
nsTextAreaSelectionImpl::GetFrameForNodeOffset(nsIContent *aNode, PRInt32 aOffset, nsIFrame **aReturnFrame, PRInt32 *aReturnOffset)
{
  return mFrameSelection->GetFrameForNodeOffset(aNode, aOffset,aReturnFrame,aReturnOffset);
}




// END   nsTextAreaSelectionImpl



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

NS_IMPL_ADDREF_INHERITED(nsGfxTextControlFrame2, nsHTMLContainerFrame);
NS_IMPL_RELEASE_INHERITED(nsGfxTextControlFrame2, nsHTMLContainerFrame);
 

NS_IMETHODIMP
nsGfxTextControlFrame2::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIAnonymousContentCreator))) {
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIGfxTextControlFrame2))) {
    *aInstancePtr = (void*)(nsIGfxTextControlFrame2*) this;
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

nsGfxTextControlFrame2::nsGfxTextControlFrame2()
{
  mIsProcessing=PR_FALSE;
  mFormFrame = nsnull;
  mCachedState = nsnull;
}

nsGfxTextControlFrame2::~nsGfxTextControlFrame2()
{
  if (mFormFrame) {
    mFormFrame->RemoveFormControlFrame(*this);
    mFormFrame = nsnull;
  }

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

#define DIV_STRING "user-focus: none; overflow:scroll; border: 0px !important; padding: 0px; margin:0px"

NS_IMETHODIMP
nsGfxTextControlFrame2::CreateAnonymousContent(nsIPresContext* aPresContext,
                                           nsISupportsArray& aChildList)
{
//create editor
//create selection
//init editor with div.
//====

//get the presshell
  mState |= NS_FRAME_INDEPENDENT_SELECTION;

  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
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
  
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMElement> domElement;
  

  if (NS_FAILED(domdoc->CreateElement(NS_ConvertToString("div"),getter_AddRefs(domElement))) && domElement)
    content = do_QueryInterface(domElement);
  if (content)
  {
    content->SetAttribute(kNameSpaceID_None,nsHTMLAtoms::style, NS_ConvertToString(DIV_STRING), PR_FALSE);
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
    nsTextAreaSelectionImpl * textSelImpl = new nsTextAreaSelectionImpl(frameSel,shell,content);
    mSelCon =  do_QueryInterface((nsISupports *)(nsISelectionController *)textSelImpl);//this will addref it once
    mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
//get the flags 
    PRUint32 editorFlags = 0;
    if (IsPlainTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPlaintextMask;
    if (IsSingleLineTextControl())
      editorFlags |= nsIHTMLEditor::eEditorSingleLineMask;
    if (IsPasswordTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPasswordMask;

//initialize the editor
    mEditor->Init(domdoc, shell, content, mSelCon, editorFlags);

//initialize the controller for the editor
    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMNSHTMLInputElement>    inputElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIControllers> controllers;
    if (textAreaElement)
      textAreaElement->GetControllers(getter_AddRefs(controllers));
    else if (inputElement)
      inputElement->GetControllers(getter_AddRefs(controllers));
    else
      return rv = NS_ERROR_FAILURE;
    
    if (NS_SUCCEEDED(rv))
    {
      PRUint32 count;
      PRBool found = PR_FALSE;
      rv = controllers->GetControllerCount(&count);
      for (PRUint32 i = 0; i < count; i ++)
      {
        nsCOMPtr<nsIController> controller;
        rv = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(rv) && controller)
        {
          nsCOMPtr<nsIEditorController> editController = do_QueryInterface(controller);
          if (editController)
          {
            editController->SetCommandRefCon(mEditor);
            found = PR_TRUE;
          }
        }
      }
      if (!found)
        rv = NS_ERROR_FAILURE;
    }


//get the caret
    nsCOMPtr<nsICaret> caret;
    if (NS_SUCCEEDED(shell->GetCaret(getter_AddRefs(caret))) && caret)
    {
      nsCOMPtr<nsIDOMSelectionListener> listener = do_QueryInterface(caret);
      nsCOMPtr<nsIDOMSelection> domSelection;
      if (listener)
      {
        if (NS_SUCCEEDED(mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSelection))) && domSelection)
          domSelection->AddSelectionListener(listener);
      }
    }

  }
  return NS_OK;
}


#if 0
NS_IMETHODIMP nsGfxTextControlFrame2::Reflow(nsIPresContext*          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{

  // Figure out if we are doing Quirks or Standard
  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(&mode);

  nsMargin border;
  border.SizeTo(0, 0, 0, 0);
  nsMargin padding;
  padding.SizeTo(0, 0, 0, 0);
  // Get the CSS border
  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
  spacing->CalcBorderFor(this, border);
  spacing->CalcPaddingFor(this, padding);

  // calculate the the desired size for the text control
  // use the suggested size if it has been set
  nsresult rv = NS_OK;
  nsHTMLReflowState suggestedReflowState(aReflowState);
  if ((kSuggestedNotSet != mSuggestedWidth) || 
      (kSuggestedNotSet != mSuggestedHeight)) {
      // Honor the suggested width and/or height.
    if (kSuggestedNotSet != mSuggestedWidth) {
      suggestedReflowState.mComputedWidth = mSuggestedWidth;
      aDesiredSize.width = mSuggestedWidth;
    }

    if (kSuggestedNotSet != mSuggestedHeight) {
      suggestedReflowState.mComputedHeight = mSuggestedHeight;
      aDesiredSize.height = mSuggestedHeight;
    }
    rv = NS_OK;
  
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;

    aStatus = NS_FRAME_COMPLETE;
  } else {

    // this is the right way
    // Quirks mode will NOT obey CSS border and padding
    // GetDesiredSize calculates the size without CSS borders
    // the nsLeafFrame::Reflow will add in the borders
    if (eCompatibility_NavQuirks == mode) {
      rv = ReflowNavQuirks(aPresContext, aDesiredSize, aReflowState, aStatus, border, padding);
    } else {
      rv = ReflowStandard(aPresContext, aDesiredSize, aReflowState, aStatus, border, padding);
    }

    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
      if (aReflowState.mComputedWidth > aDesiredSize.width) {
        aDesiredSize.width = aReflowState.mComputedWidth;
      }
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
      if (aReflowState.mComputedHeight > aDesiredSize.height) {
        aDesiredSize.height = aReflowState.mComputedHeight;
      }
    }
    aStatus = NS_FRAME_COMPLETE;
  }

#endif

NS_IMETHODIMP nsGfxTextControlFrame2::Reflow(nsIPresContext*          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

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


  if (kidReflowState.mComputedWidth != NS_INTRINSICSIZE)
      kidReflowState.mComputedWidth -= (kidReflowState.mComputedBorderPadding.left + kidReflowState.mComputedBorderPadding.right);

  if (kidReflowState.mComputedHeight != NS_INTRINSICSIZE)
      kidReflowState.mComputedHeight -= (kidReflowState.mComputedBorderPadding.top + kidReflowState.mComputedBorderPadding.bottom);

  if (aReflowState.reason == eReflowReason_Incremental)
  {
    if (aReflowState.reflowCommand) {
      nsIFrame* incrementalChild = nsnull;
      aReflowState.reflowCommand->GetNext(incrementalChild);

      NS_ASSERTION(incrementalChild == child || !incrementalChild, "Child is not in our list!!");

      if (!incrementalChild) {
        nsIFrame* target;
        aReflowState.reflowCommand->GetTarget(target);
        NS_ASSERTION(target == this, "Not our target!");

        nsIReflowCommand::ReflowType  type;
        aReflowState.reflowCommand->GetType(type);
        switch (type) {
          case nsIReflowCommand::StyleChanged:
            kidReflowState.reason = eReflowReason_StyleChange;
            kidReflowState.reflowCommand = nsnull;
          break;
          case nsIReflowCommand::ReflowDirty:
            kidReflowState.reason = eReflowReason_Dirty;
            kidReflowState.reflowCommand = nsnull;
          break;
          default:
            NS_ERROR("Unknown incremental reflow type");
        }
      }
    }
  }

  nsresult rv = ReflowChild(child, aPresContext, aDesiredSize, kidReflowState, 0, 0, 0, aStatus);
 // Place and size the child.
  FinishReflowChild(child, aPresContext, aDesiredSize, aReflowState.mComputedBorderPadding.left,
                    aReflowState.mComputedBorderPadding.top, 0);

  aStatus = NS_FRAME_COMPLETE;

  return rv;
}
//#endif

PRIntn
nsGfxTextControlFrame2::GetSkipSides() const
{
  return 0;
}

//IMPLEMENTING NS_IFORMCONTROLFRAME
NS_IMETHODIMP
nsGfxTextControlFrame2::GetName(nsString* aResult)
{
  nsresult rv = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    rv = mContent->QueryInterface(NS_GET_IID(nsIHTMLContent),(void**)&formControl);
    if (NS_SUCCEEDED(rv) && formControl) {
      nsHTMLValue value;
      rv = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetType(PRInt32* aType) const
{
  nsresult rv = NS_FORM_NOTOK;
  if (mContent) {
    nsIFormControl* formControl = nsnull;
    rv = mContent->QueryInterface(NS_GET_IID(nsIFormControl), (void**)&formControl);
    if ((NS_OK == rv) && formControl) {
      rv = formControl->GetType(aType);
      NS_RELEASE(formControl);
    }
  }
  return rv;
}


void    nsGfxTextControlFrame2::SetFocus(PRBool aOn , PRBool aRepaint){}
void    nsGfxTextControlFrame2::ScrollIntoView(nsIPresContext* aPresContext){}
void    nsGfxTextControlFrame2::MouseClicked(nsIPresContext* aPresContext){}

void    nsGfxTextControlFrame2::Reset(nsIPresContext* aPresContext){}
PRInt32 nsGfxTextControlFrame2::GetMaxNumValues(){return 1;}/**/

PRBool  nsGfxTextControlFrame2::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames)
{
  return PR_FALSE;
}

nscoord 
nsGfxTextControlFrame2::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(0, aPixToTwip); 
}


//---------------------------------------------------------
nscoord 
nsGfxTextControlFrame2::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPresContext, aPixToTwip, aInnerWidth);
}


void 
nsGfxTextControlFrame2::SetFormFrame(nsFormFrame* aFormFrame) 
{ 
  mFormFrame = aFormFrame; 
}


//---------------------------------------------------------
PRBool
nsGfxTextControlFrame2::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

NS_IMETHODIMP 
nsGfxTextControlFrame2::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

nsresult 
nsGfxTextControlFrame2::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetFont(nsIPresContext* aPresContext, 
                            const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

NS_IMETHODIMP nsGfxTextControlFrame2::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue)
{
  if (!mIsProcessing)//some kind of lock.
  {
    mIsProcessing = PR_TRUE;
    
    if (nsHTMLAtoms::value == aName) 
    {
      if (mEditor) {
        mEditor->EnableUndo(PR_FALSE);    // wipe out undo info
      }
      SetTextControlFrameState(aValue);   // set new text value
      if (mEditor)  {
        mEditor->EnableUndo(PR_TRUE);     // fire up a new txn stack
      }
    }
    else if (nsHTMLAtoms::select == aName && mSelCon)
    {
      // select all the text
      mSelCon->SelectAll();
    }
    mIsProcessing = PR_FALSE;
  }
  return NS_OK;
}      

NS_IMETHODIMP nsGfxTextControlFrame2::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    GetTextControlFrameState(aValue);
  }
  return NS_OK;
}  



NS_IMETHODIMP
nsGfxTextControlFrame2::GetEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  *aEditor = mEditor;
  NS_IF_ADDREF(*aEditor);
  return NS_OK;
}



NS_IMETHODIMP
nsGfxTextControlFrame2::GetTextLength(PRInt32* aTextLength)
{
  NS_ENSURE_ARG_POINTER(aTextLength);
  nsString *str = GetCachedString();
  if (str)
  {
    *aTextLength = str->Length();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsGfxTextControlFrame2::SetSelectionStart(PRInt32 aSelectionStart)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsGfxTextControlFrame2::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetSelectionController(nsISelectionController **aSelCon)
{
  NS_ENSURE_ARG_POINTER(aSelCon);
  NS_IF_ADDREF(*aSelCon = mSelCon);
  return NS_OK;
}


/////END INTERFACE IMPLEMENTATIONS

////NSIFRAME
NS_IMETHODIMP
nsGfxTextControlFrame2::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent*     aChild,
                                        PRInt32         aNameSpaceID,
                                        nsIAtom*        aAttribute,
                                        PRInt32         aHint)
{
  if (!mEditor || !mSelCon) {return NS_ERROR_NOT_INITIALIZED;}
  nsresult rv = NS_OK;

  if (nsHTMLAtoms::value == aAttribute) 
  {
    if (mEditor)
    {
      nsString value;
      GetText(&value, PR_TRUE);           // get the initial value from the content attribute
      mEditor->EnableUndo(PR_FALSE);      // wipe out undo info
      SetTextControlFrameState(value);    // set new text value
      mEditor->EnableUndo(PR_TRUE);       // fire up a new txn stack
    }
    if (aHint != NS_STYLE_HINT_REFLOW)
      nsFormFrame::StyleChangeReflow(aPresContext, this);
  } 
  else if (nsHTMLAtoms::maxlength == aAttribute) 
  {
    PRInt32 maxLength;
    nsresult rv = GetMaxLength(&maxLength);
    
    nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
    if (htmlEditor)
    {
      if (NS_CONTENT_ATTR_NOT_THERE != rv) 
      {  // set the maxLength attribute
          htmlEditor->SetMaxTextLength(maxLength);
        // if maxLength>docLength, we need to truncate the doc content
      }
      else { // unset the maxLength attribute
          htmlEditor->SetMaxTextLength(-1);
      }
    }
  } 
  else if (mEditor && nsHTMLAtoms::readonly == aAttribute) 
  {
    nsresult rv = DoesAttributeExist(nsHTMLAtoms::readonly);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= nsIHTMLEditor::eEditorReadonlyMask;
      if (mSelCon)
        mSelCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIHTMLEditor::eEditorReadonlyMask);
      if (mSelCon)
        mSelCon->SetCaretEnabled(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (mEditor && nsHTMLAtoms::disabled == aAttribute) 
  {
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv) || !shell)
      return rv?rv:NS_ERROR_FAILURE;

    rv = DoesAttributeExist(nsHTMLAtoms::disabled);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= nsIHTMLEditor::eEditorDisabledMask;
      mSelCon->SetCaretEnabled(PR_FALSE);
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
    }
    else 
    { // unset readonly
      flags &= ~(nsIHTMLEditor::eEditorDisabledMask);
      mSelCon->SetCaretEnabled(PR_TRUE);
      mSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
    }    
    mEditor->SetFlags(flags);
  }
  else if ((nsHTMLAtoms::size == aAttribute ||
            nsHTMLAtoms::rows == aAttribute) && aHint != NS_STYLE_HINT_REFLOW) {
    nsFormFrame::StyleChangeReflow(aPresContext, this);
  }
  // Allow the base class to handle common attributes supported
  // by all form elements... 
  else {
    rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
  }

  return rv;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetText(nsString* aText, PRBool aInitialValue)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) 
  {
    if (PR_TRUE==aInitialValue)
    {
      rv = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
    }
    else
    {
      if (mEditor)
      {
        nsCOMPtr<nsIEditorIMESupport> imeSupport = do_QueryInterface(mEditor);
        if(imeSupport) 
            imeSupport->ForceCompositionEnd();
        nsString format; format.AssignWithConversion("text/plain");
        mEditor->OutputToString(*aText, format, 0);
      }
      // we've never built our editor, so the content attribute is the value
      else
      {
        rv = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
      }
    }
    RemoveNewlines(*aText);
  } 
  else 
  {
    nsIDOMHTMLTextAreaElement* textArea = nsnull;
    rv = mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLTextAreaElement), (void**)&textArea);
    if ((NS_OK == rv) && textArea) {
      if (PR_TRUE == aInitialValue) {
        rv = textArea->GetDefaultValue(*aText);
      }
      else {
        if(mEditor) {
          nsCOMPtr<nsIEditorIMESupport> imeSupport = do_QueryInterface(mEditor);
          if(imeSupport) 
            imeSupport->ForceCompositionEnd();
        }
        rv = textArea->GetValue(*aText);
      }
      NS_RELEASE(textArea);
    }
  }
  return rv;
}



///END NSIFRAME OVERLOADS
/////BEGIN PROTECTED METHODS

void nsGfxTextControlFrame2::RemoveNewlines(nsString &aString)
{
  // strip CR/LF and null
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    rv = content->GetHTMLAttribute(nsHTMLAtoms::maxlength, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return rv;
}

NS_IMETHODIMP
nsGfxTextControlFrame2::DoesAttributeExist(nsIAtom *aAtt)
{
  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) 
  {
    nsHTMLValue value;
    rv = content->GetHTMLAttribute(aAtt, value);
    NS_RELEASE(content);
  }
  return rv;
}


nsString *
nsGfxTextControlFrame2::GetCachedString()
{
  if (!mCachedState && mEditor)
  {
    mCachedState = new nsString;
    if (!mCachedState)
      return nsnull;
    GetTextControlFrameState(*mCachedState);  
  }
  return mCachedState;
}

void nsGfxTextControlFrame2::GetTextControlFrameState(nsString& aValue)
{
  aValue.SetLength(0);  // initialize out param
  
  if (mEditor) 
  {
    nsString format; format.AssignWithConversion("text/plain");
    PRUint32 flags = 0;

    if (PR_TRUE==IsPlainTextControl()) {
      flags |= nsIDocumentEncoder::OutputBodyOnly;   // OutputNoDoctype if head info needed
    }

    nsFormControlHelper::nsHTMLTextWrap wrapProp;
    nsresult rv = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    {
      if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard)
      {
        flags |= nsIDocumentEncoder::OutputFormatted;
      }
    }

    mEditor->OutputToString(aValue, format, flags);
  }
}     


// END IMPLEMENTING NS_IFORMCONTROLFRAME

void
nsGfxTextControlFrame2::SetTextControlFrameState(const nsString& aValue)
{
  if (mEditor) 
  {
    nsAutoString currentValue;
    nsAutoString format; format.AssignWithConversion("text/plain");
    nsresult rv = mEditor->OutputToString(currentValue, format, 0);
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
      rv = mEditor->GetDocument(getter_AddRefs(domDoc));
			if (NS_FAILED(rv)) return;
			if (!domDoc) return;

      rv = mEditor->SelectAll();
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
}


NS_IMETHODIMP
nsGfxTextControlFrame2::SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  /*nsIFrame *list = aChildList;
  nsFrameState  frameState;
  while (list)
  {
    list->GetFrameState(&frameState);
    frameState |= NS_FRAME_INDEPENDENT_SELECTION;
    list->SetFrameState(frameState);
    list->GetNextSibling(&list);
  }
  */
  nsresult rv = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (mEditor)
    mEditor->PostCreate();
  return rv;
}


PRInt32 
nsGfxTextControlFrame2::GetWidthInCharacters() const
{
  // see if there's a COL attribute, if so it wins
  nsCOMPtr<nsIHTMLContent> content;
  nsresult rv = mContent->QueryInterface(NS_GET_IID(nsIHTMLContent), getter_AddRefs(content));
  if (NS_SUCCEEDED(rv) && content)
  {
    nsHTMLValue resultValue;
    rv = content->GetHTMLAttribute(nsHTMLAtoms::cols, resultValue);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    {
      if (resultValue.GetUnit() == eHTMLUnit_Integer) 
      {
        return (resultValue.GetIntValue());
      }
    }
  }

  // otherwise, see if CSS has a width specified.  If so, work backwards to get the 
  // number of characters this width represents.
 
  
  // otherwise, the default is just returned.
  return DEFAULT_COLUMN_WIDTH;
}
