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




class nsTextAreaSelectionImpl : public nsSupportsWeakReference, public nsISelectionController
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
  NS_IMETHOD SetCaretEnabled(PRBool enabled);
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
private:
  nsCOMPtr<nsIFrameSelection> mFrameSelection;
  nsCOMPtr<nsIContent>        mLimiter;
  nsWeakPtr mPresShellWeak;
};

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsTextAreaSelectionImpl, nsISelectionController, nsISupportsWeakReference)



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
nsTextAreaSelectionImpl::SetCaretEnabled(PRBool enabled)
{
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsISelectionController> selCon = do_QueryReferent(mPresShellWeak);
  if (selCon)
  {
    return selCon->SetCaretEnabled(enabled);//we can use presshells because there is only 1 caret
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

//get the presshell
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv) || !shell)
      return rv?rv:NS_ERROR_FAILURE;
//create selection
    nsCOMPtr<nsIFrameSelection> frameSel;
    rv = nsComponentManager::CreateInstance(kFrameSelectionCID, nsnull,
                                                   NS_GET_IID(nsIFrameSelection),
                                                   getter_AddRefs(frameSel));
//create selection controller
    nsTextAreaSelectionImpl * textSelImpl = new nsTextAreaSelectionImpl(frameSel,shell,content);
    mSelCon =  do_QueryInterface((nsISupports *)(nsISelectionController *)textSelImpl);//this will addref it once

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
      kidReflowState.mComputedWidth -= kidReflowState.mComputedBorderPadding.left + kidReflowState.mComputedBorderPadding.right;

  if (kidReflowState.mComputedHeight != NS_INTRINSICSIZE)
      kidReflowState.mComputedHeight -= kidReflowState.mComputedBorderPadding.top + kidReflowState.mComputedBorderPadding.bottom;

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

  return rv;
}
//#endif

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
    result = mContent->QueryInterface(NS_GET_IID(nsIFormControl), (void**)&formControl);
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


NS_IMETHODIMP
nsGfxTextControlFrame2::SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  nsIFrame *list = aChildList;
  nsFrameState  frameState;
  while (list)
  {
    list->GetFrameState(&frameState);
    frameState |= NS_FRAME_INDEPENDENT_SELECTION;
    list->SetFrameState(frameState);
    list->GetNextSibling(&list);
  }
  nsresult result = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (mEditor)
    mEditor->PostCreate();
  return result;
}


NS_IMETHODIMP
nsGfxTextControlFrame2::GetSelectionController(nsIPresContext *aPresContext, nsISelectionController **aSelCon)
{
  if (!aSelCon)
    return NS_ERROR_INVALID_ARG;
  NS_IF_ADDREF(*aSelCon = mSelCon);
  return NS_OK;
}

//-----------------
nsresult 
nsGfxTextControlFrame2::GetColRowSizeAttr(nsIFormControlFrame*  aFrame,
                                         nsIAtom *     aColSizeAttr,
                                         nsHTMLValue & aColSize,
                                         nsresult &    aColStatus,
                                         nsIAtom *     aRowSizeAttr,
                                         nsHTMLValue & aRowSize,
                                         nsresult &    aRowStatus)
{
  nsIContent* iContent = nsnull;
  aFrame->GetFormContent((nsIContent*&) iContent);
  if (!iContent) {
    return NS_ERROR_FAILURE;
  }
  nsIHTMLContent* hContent = nsnull;
  nsresult result = iContent->QueryInterface(kIHTMLContentIID, (void**)&hContent);
  if ((NS_OK != result) || !hContent) {
    NS_RELEASE(iContent);
    return NS_ERROR_FAILURE;
  }

  aColStatus = NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aColSizeAttr) {
    aColStatus = hContent->GetHTMLAttribute(aColSizeAttr, aColSize);
  }

  aRowStatus= NS_CONTENT_ATTR_NOT_THERE;
  if (nsnull != aRowSizeAttr) {
    aRowStatus = hContent->GetHTMLAttribute(aRowSizeAttr, aRowSize);
  }

  NS_RELEASE(hContent);
  NS_RELEASE(iContent);
  
  return NS_OK;
}


PRInt32 
nsGfxTextControlFrame2::GetWidthInCharacters() const
{
  // see if there's a COL attribute, if so it wins
  nsCOMPtr<nsIHTMLContent> content;
  nsresult result = mContent->QueryInterface(NS_GET_IID(nsIHTMLContent), getter_AddRefs(content));
  if (NS_SUCCEEDED(result) && content)
  {
    nsHTMLValue resultValue;
    result = content->GetHTMLAttribute(nsHTMLAtoms::cols, resultValue);
    if (NS_CONTENT_ATTR_NOT_THERE != result) 
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
