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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#ifndef ENDER_LITE

#include "nsCOMPtr.h"
#include "nsGfxTextControlFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsString.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsFont.h"
#include "nsDOMEvent.h"
#include "nsIFormControl.h"
#include "nsFormFrame.h"
#include "nsIFrameManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScrollbar.h"
#include "nsIScrollableFrame.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIWebShell.h"

#include "nsCSSRendering.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"

#include "nsIBaseWindow.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDocumentLoader.h"
#include "nsINameSpaceManager.h"
#include "nsIPref.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIDocumentViewer.h"
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"

#include "nsIHTMLEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsIDocumentEncoder.h"
#include "nsIEditorMailSupport.h"
#include "nsITransactionManager.h"
#include "nsEditorCID.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"
#include "nsStyleUtil.h"
#include "nsLinebreakConverter.h"
#include "nsRange.h"

// for anonymous content and frames
#include "nsHTMLParts.h"
#include "nsITextContent.h"

//for editor controllers 
#include "nsIEditorController.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsINodeInfo.h"

#include "nsLayoutAtoms.h"

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kTextCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENTLOADEROBSERVER_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);

static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);

#define EMPTY_DOCUMENT "about:blank"
#define PASSWORD_REPLACEMENT_CHAR '*'

//#define NOISY
const nscoord kSuggestedNotSet = -1;

/*================== global functions =========================*/
static nsIWidget * GetDeepestWidget(nsIView * aView)
{

  PRInt32 count;
  aView->GetChildCount(count);
  if (0 != count) {
    for (PRInt32 i=0;i<count;i++) {
      nsIView * child;
      aView->GetChild(i, child);
      nsIWidget * widget = GetDeepestWidget(child);
      if (widget) {
        return widget;
      } else {
        aView->GetWidget(widget);
        if (widget) {
          nsCOMPtr<nsIScrollbar> scrollbar(do_QueryInterface(widget));
          if (scrollbar) {
            NS_RELEASE(widget);
          } else {
            return widget;
          }
        }
      }
    }
  }

  return nsnull;
}

static void GetWidgetForView(nsIView *aView, nsIWidget *&aWidget)
{
  aWidget = nsnull;
  nsIView *view = aView;
  while (!aWidget && view)
  {
    view->GetWidget(aWidget);
    if (!aWidget)
      view->GetParent(view);
  }
}

nsresult
NS_NewGfxTextControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  *aNewFrame = new (aPresShell) nsGfxTextControlFrame;
  if (nsnull == aNewFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsGfxTextControlFrame::nsGfxTextControlFrame()
: mTempObserver(0), mDocObserver(0),
  mCreatingViewer(PR_FALSE),
  mNotifyOnInput(PR_FALSE),
  mIsProcessing(PR_FALSE),
  mNeedsStyleInit(PR_TRUE),
  mDidSetFocus(PR_FALSE),
  mGotSelectionState(PR_FALSE),
  mSelectionWasCollapsed(PR_FALSE),
  mIsInput(PR_TRUE),
  mFramePresContext(nsnull),
  mCachedState(nsnull),
  mWeakReferent(this),
  mFrameConstructor(nsnull),
  mDisplayFrame(nsnull),
  mPassThroughMouseEvents(eUninitialized)
{
#ifdef DEBUG
  mDebugTotalReflows=0;
  mDebugResizeReflows=0;
  mDebugResizeUnconstrained=0;
  mDebugResizeReflowsThatChangedMySize=0;
  mDebugReflowsThatMovedSubdoc=0;
  mDebugTotalPaints=0;
  mDebugPaintsSinceLastReflow=0;
#endif
}

nsGfxTextControlFrame::~nsGfxTextControlFrame()
{
  nsresult result;
  if (mDisplayFrame) {
    mFrameConstructor->RemoveMappingsForFrameSubtree(mFramePresContext,
                                                     mDisplayFrame, nsnull);
    mDisplayFrame->Destroy(mFramePresContext);
  }
  if (mTempObserver)
  {
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
    if (webShell) {
      nsCOMPtr<nsIDocumentLoader> docLoader;
      webShell->GetDocumentLoader(*getter_AddRefs(docLoader));
      if (docLoader) {
        docLoader->RemoveObserver(mTempObserver);
      }
    }
    mTempObserver->SetFrame(nsnull);
    NS_RELEASE(mTempObserver);
  }

  // remove the focus listener from the display content, if any
  if (mDisplayContent && mFocusListenerForDisplayContent)
  {
    nsCOMPtr<nsIDOMEventReceiver> er;
    result = mDisplayContent->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(er));
    if (NS_SUCCEEDED(result) && er) 
    {
      er->RemoveEventListenerByIID(mFocusListenerForDisplayContent, NS_GET_IID(nsIDOMFocusListener));
    }
    mFocusListenerForDisplayContent->SetFrame(nsnull);
    NS_RELEASE(mFocusListenerForDisplayContent);
  }

  // remove the drag listener from the frame's content node, if any
  if (mContent && mListenerForContent)
  {
    // checking errors below does not good, I'm in a void method
    // I only check the minimum required to be sure it all works right
    nsCOMPtr<nsIDOMEventReceiver> contentER;
    result = mContent->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(contentER));
    if (NS_SUCCEEDED(result) && contentER) 
    {
      nsCOMPtr<nsIDOMDragListener> dragListenerForContent;
      mListenerForContent->QueryInterface(NS_GET_IID(nsIDOMDragListener), getter_AddRefs(dragListenerForContent));
      if (dragListenerForContent)
      {
        contentER->RemoveEventListenerByIID(dragListenerForContent, NS_GET_IID(nsIDOMDragListener));
      }
    }
    mListenerForContent->SetFrame(nsnull);
    NS_RELEASE(mListenerForContent);
  }  

  if (mEventListener)
  {
    if (mEditor)
    {
      // remove selection listener
      nsCOMPtr<nsISelection> selection;
      result = mEditor->GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(result) && selection) 
      {
        nsCOMPtr<nsISelectionListener> selListener = do_QueryInterface(mEventListener);
        if (selListener)
          selection->RemoveSelectionListener(selListener); 
      }
      
      // remove the txnListener
      nsCOMPtr<nsITransactionManager> txnMgr;
      result = mEditor->GetTransactionManager(getter_AddRefs(txnMgr));
      if (NS_SUCCEEDED(result) && txnMgr) 
      {
        nsCOMPtr<nsITransactionListener> txnListener = do_QueryInterface(mEventListener);
        if (txnListener)
          txnMgr->RemoveListener(txnListener); 
      }
      
      // remove all other listeners from embedded document
      nsCOMPtr<nsIDOMDocument>domDoc;
      result = mEditor->GetDocument(getter_AddRefs(domDoc));
      if (NS_SUCCEEDED(result) && domDoc)
      {
        nsCOMPtr<nsIDOMEventReceiver> er;
        result = domDoc->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(er));
        if (NS_SUCCEEDED(result) && er) 
        {
          // remove key listener
          nsCOMPtr<nsIDOMKeyListener>keyListener;
          keyListener = do_QueryInterface(mEventListener);
          if (keyListener)
            er->RemoveEventListenerByIID(keyListener, NS_GET_IID(nsIDOMKeyListener));
          // remove mouse listener
          nsCOMPtr<nsIDOMMouseListener>mouseListener;
          mouseListener = do_QueryInterface(mEventListener);
          if (mouseListener)
            er->RemoveEventListenerByIID(mouseListener, NS_GET_IID(nsIDOMMouseListener));
          // remove focus listener
          nsCOMPtr<nsIDOMFocusListener>focusListener;
          focusListener = do_QueryInterface(mEventListener);
          if (focusListener)
            er->RemoveEventListenerByIID(focusListener, NS_GET_IID(nsIDOMFocusListener));
        }
      }
    }
  }

  result = NS_OK;
  // if there is a controller, remove the editor from it
  if (mEditor)
  {
    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> textAreaElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMNSHTMLInputElement>    inputElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIControllers> controllers;
    if (textAreaElement)
      result = textAreaElement->GetControllers(getter_AddRefs(controllers));
    else if (inputElement)
      result = inputElement->GetControllers(getter_AddRefs(controllers));
    else
      result = NS_ERROR_FAILURE;
    if (NS_SUCCEEDED(result))
    {
      PRUint32 count;
      PRBool found = PR_FALSE;
      result = controllers->GetControllerCount(&count);
      NS_ASSERTION((NS_SUCCEEDED(result)), "bad result in gfx text control destructor");
      for (PRUint32 i = 0; i < count; i ++)
      {
        nsCOMPtr<nsIController> controller;
        result = controllers->GetControllerAt(i, getter_AddRefs(controller));
        if (NS_SUCCEEDED(result) && controller)
        {
          nsCOMPtr<nsIEditorController> editController = do_QueryInterface(controller);
          if (editController)
          {
            editController->SetCommandRefCon(nsnull);
            found = PR_TRUE;
          }
        }
      }
    }
  }

  mEditor = 0;  // editor must be destroyed before the docshell!
   nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
   if(docShellWin)
      docShellWin->Destroy();
   docShellWin = nsnull;
   mDocShell = nsnull; // This is where it was released before.  Not sure if
                        // there is ordering depending on it.
  if (mDocObserver)
  {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDoc);
    if (doc)
    {
      doc->RemoveObserver(mDocObserver);
    }
    mDocObserver->SetFrame(nsnull);
    NS_RELEASE(mDocObserver);
  }

  if (mCachedState) {
    delete mCachedState;
    mCachedState = nsnull;
  }
#ifdef DEBUG
#if NOISY
  printf("gfxTC: %p reflow stats at destructor:\n\ttotal\tresize\tunconst\tchanged\tmoved\n", this);
  printf("\t%d\t%d\t%d\t%d\t%d\n", 
          mDebugTotalReflows, mDebugResizeReflows, 
          mDebugResizeUnconstrained,
          mDebugResizeReflowsThatChangedMySize, mDebugReflowsThatMovedSubdoc);
#endif
#endif
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsGfxTextControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;

  } else  if (aIID.Equals(NS_GET_IID(nsIGfxTextControlFrame))) {
    *aInstancePtr = (void*)(nsIGfxTextControlFrame*) this;
    return NS_OK;
    
  } else  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;                                                        
  }
  
  return nsFormControlFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsGfxTextControlFrame::Init(nsIPresContext*  aPresContext,
                            nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIStyleContext* aContext,
                            nsIFrame*        aPrevInFlow)
{
  mFramePresContext = aPresContext;
  nsresult rv = nsFormControlFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  if (NS_FAILED(rv)) return rv;
  
  // we are an input (hence single line)?
  NS_ASSERTION(mContent, "Should have content here");  
  nsCOMPtr<nsIDOMHTMLInputElement> contentAsInput = do_QueryInterface(mContent);
  mIsInput = (contentAsInput.get() != nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  *aEditor = mEditor;
  NS_IF_ADDREF(*aEditor);
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = NS_NewAtom("textControlFrame");
  return NS_OK;
}


NS_IMETHODIMP
nsGfxTextControlFrame::GetDocShell(nsIDocShell **aDocShell)
{
  NS_ENSURE_ARG_POINTER(aDocShell);

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::SetInnerFocus()
{
  SetFocus();
  return NS_OK;
}


NS_IMETHODIMP
nsGfxTextControlFrame::GetFirstTextNode(nsIDOMCharacterData* *aFirstTextNode)
{
  *aFirstTextNode = nsnull;
  
  // get the doc
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDoc);
  if (!htmlDoc) return NS_ERROR_FAILURE;
    
  nsCOMPtr<nsIDOMHTMLElement> bodyElement;
  htmlDoc->GetBody(getter_AddRefs(bodyElement));
  if (!bodyElement) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(bodyElement);
  if (!bodyNode) return NS_ERROR_FAILURE;
  
  // for a text widget, the text of the document is in a single
  // text node under the body. Let's make sure that's true.
  nsCOMPtr<nsIDOMNodeList> childNodesList;
  bodyNode->GetChildNodes(getter_AddRefs(childNodesList));
  if (!childNodesList)
  {
    NS_WARNING("Subdoc has no text node list");
    return NS_ERROR_FAILURE;
  }

  PRUint32 numChildNodes = 0;
  childNodesList->GetLength(&numChildNodes);
#if 0
  // editor can leave empty text node droppings around after deleting
  // all the text in a widget, so disable this check for now.
  if (numChildNodes != 1)
  {
    NS_WARNING("Found zero or several child nodes in a text widget doc!");
    return NS_ERROR_FAILURE;
  }
#endif

  nsCOMPtr<nsIDOMNode> firstChild;
  nsresult rv = bodyNode->GetFirstChild(getter_AddRefs(firstChild));
  if (NS_FAILED(rv)) return rv;
  if (!firstChild) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMCharacterData> charDataNode = do_QueryInterface(firstChild, &rv);
  if (NS_FAILED(rv)) return rv;
  
  NS_ADDREF(*aFirstTextNode = charDataNode);
  return NS_OK;
}


nsresult
nsGfxTextControlFrame::SelectAllContents()
{
  nsresult rv;
  
  if (IsSingleLineInput())
  {
    rv = SetSelectionRange(0, eSelectToEnd);
  }
  else
  {
    // we have to select all
    rv = EnsureEditor();
    if (NS_FAILED(rv)) return rv;
    
    NS_ASSERTION(mEditor, "Should have an editor here");    
    rv = mEditor->SelectAll();
  }

  return rv;
}


nsresult
nsGfxTextControlFrame::SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  NS_ASSERTION(IsSingleLineInput(), "Should only call this on a single line input");
  NS_ASSERTION(mEditor, "Should have an editor here");
  NS_ASSERTION(mDoc, "Should have an editor here");

  nsCOMPtr<nsIDOMCharacterData> firstTextNode;
  nsresult rv = GetFirstTextNode(getter_AddRefs(firstTextNode));
  if (NS_FAILED(rv) || !firstTextNode)
  {
    // probably an empty document. not an error
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMNode> firstNode = do_QueryInterface(firstTextNode, &rv);
  if (!firstNode) return rv;
  
  // constrain the selection to this node
  PRUint32 nodeLengthU;
  firstTextNode->GetLength(&nodeLengthU);
  PRInt32 nodeLength = (PRInt32)nodeLengthU;
    
  nsCOMPtr<nsISelection> selection;
  mEditor->GetSelection(getter_AddRefs(selection));  
  if (!selection) return NS_ERROR_FAILURE;

  // are we setting both start and end?
  if (aSelStart != eIgnoreSelect && aSelEnd != eIgnoreSelect)
  {
    if (aSelStart == eSelectToEnd || aSelStart > nodeLength)
      aSelStart = nodeLength;
    if (aSelStart < 0)
      aSelStart = 0;

    if (aSelEnd == eSelectToEnd || aSelEnd > nodeLength)
      aSelEnd = nodeLength;
    if (aSelEnd < 0)
      aSelEnd = 0;

    // remove existing ranges
    selection->ClearSelection();  

    nsCOMPtr<nsIDOMRange> selectionRange;
    NS_NewRange(getter_AddRefs(selectionRange));
    if (!selectionRange) return NS_ERROR_OUT_OF_MEMORY;
    
    selectionRange->SetStart(firstTextNode, aSelStart);
    selectionRange->SetEnd(firstTextNode, aSelEnd);
    
    selection->AddRange(selectionRange);
  }
  else    // we're setting either start or end but not both
  {
    // does a range exist?
    nsCOMPtr<nsIDOMRange> firstRange;
    selection->GetRangeAt(0, getter_AddRefs(firstRange));
    PRBool mustAdd = PR_FALSE;
    PRInt32 selStart = 0, selEnd = 0;

    if (firstRange)
    {
     firstRange->GetStartOffset(&selStart);
     firstRange->GetEndOffset(&selEnd);
    }
    else
    {
      // no range. Make a new one.
      NS_NewRange(getter_AddRefs(firstRange));
      if (!firstRange) return NS_ERROR_OUT_OF_MEMORY;
      mustAdd = PR_TRUE;
    }
    
    if (aSelStart == eSelectToEnd)
      selStart = nodeLength;
    else if (aSelStart != eIgnoreSelect)
      selStart = aSelStart;

    if (aSelEnd == eSelectToEnd)
      selEnd = nodeLength;
    else if (aSelEnd != eIgnoreSelect)
      selEnd = aSelEnd;
    
    // swap them
    if (selEnd < selStart)
    {
      PRInt32 temp = selStart;
      selStart = selEnd;
      selEnd = temp;
    }
    
    firstRange->SetStart(firstTextNode, selStart);
    firstRange->SetEnd(firstTextNode, selEnd);
    if (mustAdd)  
      selection->AddRange(firstRange);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::SetSelectionRange(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  if (!IsSingleLineInput()) return NS_ERROR_NOT_IMPLEMENTED;
  
  // make sure we have an editor
  nsresult rv = EnsureEditor();
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(mEditor, "Should have an editor here");
  NS_ASSERTION(mDoc, "Should have an editor here");
  
  return SetSelectionEndPoints(aSelStart, aSelEnd);
}


NS_IMETHODIMP
nsGfxTextControlFrame::SetSelectionStart(PRInt32 aSelectionStart)
{
  if (!IsSingleLineInput()) return NS_ERROR_NOT_IMPLEMENTED;

  // make sure we have an editor
  nsresult rv = EnsureEditor();
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(mEditor, "Should have an editor here");
  NS_ASSERTION(mDoc, "Should have an editor here");
  
  return SetSelectionEndPoints(aSelectionStart, eIgnoreSelect);
}

NS_IMETHODIMP
nsGfxTextControlFrame::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  if (!IsSingleLineInput()) return NS_ERROR_NOT_IMPLEMENTED;

  // make sure we have an editor
  nsresult rv = EnsureEditor();
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(mEditor, "Should have an editor here");
  NS_ASSERTION(mDoc, "Should have an editor here");
  
  return SetSelectionEndPoints(eIgnoreSelect, aSelectionEnd);
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetTextLength(PRInt32* aTextLength)
{
  if (!IsSingleLineInput()) return NS_ERROR_NOT_IMPLEMENTED;

  NS_ENSURE_ARG_POINTER(aTextLength);  
  *aTextLength = 0;
  
  nsresult rv = EnsureEditor();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMCharacterData> firstTextNode;
  rv = GetFirstTextNode(getter_AddRefs(firstTextNode));
  if (NS_FAILED(rv) || !firstTextNode)
  {
    // just an empty field. not an error
    return NS_OK;
  }

  PRUint32 nodeLengthU;
  firstTextNode->GetLength(&nodeLengthU);
  *aTextLength = (PRInt32)nodeLengthU;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd)
{
  if (!IsSingleLineInput()) return NS_ERROR_NOT_IMPLEMENTED;

  NS_ENSURE_ARG_POINTER((aSelectionStart && aSelectionEnd));

  nsresult rv = EnsureEditor();
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(mEditor, "Should have an editor here");
  NS_ASSERTION(mDoc, "Should have an editor here");
  
  nsCOMPtr<nsISelection> selection;
  mEditor->GetSelection(getter_AddRefs(selection));  
  if (!selection) return NS_ERROR_FAILURE;

  // we should have only zero or one range
  PRInt32 numRanges = 0;
  selection->GetRangeCount(&numRanges);
  if (numRanges > 1)
  {
    NS_ASSERTION(0, "Found more than on range in GetSelectionRange");
  }
  
  if (numRanges == 0)
  {
    *aSelectionStart = 0;
    *aSelectionEnd = 0;
  }
  else
  {
    nsCOMPtr<nsIDOMRange> firstRange;
    selection->GetRangeAt(0, getter_AddRefs(firstRange));
    if (!firstRange) return NS_ERROR_FAILURE;
#if 0    
    // make sure this range is pointing at the first child
    nsCOMPtr<nsIDOMCharacterData> firstTextNode;
    rv = GetFirstTextNode(getter_AddRefs(firstTextNode));
    nsCOMPtr<nsIDOMNode> firstNode = do_QueryInterface(firstTextNode);
    if (!firstNode)
    {
      *aSelectionStart = 0;
      *aSelectionEnd = 0;
      return NS_OK;
    }
    
    nsCOMPtr<nsIDOMNode> rangeStartNode;
    firstRange->GetStartParent(getter_AddRefs(rangeStartNode));   // maybe we should compare the end too?
    if (rangeStartNode != firstNode) return NS_ERROR_FAILURE;
#endif    
    firstRange->GetStartOffset(aSelectionStart);
    firstRange->GetEndOffset(aSelectionEnd);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::EnsureEditor()
{
  nsresult rv = NS_OK;
  if (!mDocShell)
  {
    rv = CreateSubDoc(nsnull);
    if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsGfxTextControlFrame::CreateEditor()
{
  nsresult result = NS_OK;

  mDocShell       = nsnull;
  mCreatingViewer = PR_FALSE;

  // create the stream observer
  mTempObserver   = new EnderTempObserver();
  if (!mTempObserver) { return NS_ERROR_OUT_OF_MEMORY; }
  mTempObserver->SetFrame(this);
  NS_ADDREF(mTempObserver);

  // create the document observer
  mDocObserver   = new nsEnderDocumentObserver();
  if (!mDocObserver) { return NS_ERROR_OUT_OF_MEMORY; }
  mDocObserver->SetFrame(this);
  NS_ADDREF(mDocObserver);

  // create the drag listener for the content node
  if (mContent)
  {
    mListenerForContent = new nsEnderListenerForContent();
    if (!mListenerForContent) { return NS_ERROR_OUT_OF_MEMORY; }
    mListenerForContent->SetFrame(this);
    NS_ADDREF(mListenerForContent);

    // get the DOM event receiver
    nsCOMPtr<nsIDOMEventReceiver> contentER;
    result = mContent->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(contentER));
    if NS_FAILED(result) { return result; }
    if (contentER) 
    {
      nsCOMPtr<nsIDOMDragListener> dragListenerForContent;
      mListenerForContent->QueryInterface(NS_GET_IID(nsIDOMDragListener), getter_AddRefs(dragListenerForContent));
      if (dragListenerForContent)
      {
        result = contentER->AddEventListenerByIID(dragListenerForContent, NS_GET_IID(nsIDOMDragListener));
        if NS_FAILED(result) { return result; }
      }
    }
  }

  // create the focus listener for HTML Input display content
  if (mDisplayContent)
  {
    mFocusListenerForDisplayContent = new nsEnderFocusListenerForDisplayContent();
    if (!mFocusListenerForDisplayContent) { return NS_ERROR_OUT_OF_MEMORY; }
    mFocusListenerForDisplayContent->SetFrame(this);
    NS_ADDREF(mFocusListenerForDisplayContent);
    // get the DOM event receiver
    nsCOMPtr<nsIDOMEventReceiver> er;
    result = mDisplayContent->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(er));
    if (NS_SUCCEEDED(result) && er) 
      result = er->AddEventListenerByIID(mFocusListenerForDisplayContent, NS_GET_IID(nsIDOMFocusListener));
    // should check to see if mDisplayContent or mContent has focus and call CreateSubDoc instead if it does
    // do something with result
  }

  nsCOMPtr<nsIHTMLEditor> theEditor;
  result = nsComponentManager::CreateInstance(kHTMLEditorCID,
                                              nsnull,
                                              NS_GET_IID(nsIHTMLEditor), getter_AddRefs(theEditor));
  if (NS_FAILED(result)) { return result; }
  if (!theEditor) { return NS_ERROR_OUT_OF_MEMORY; }
  mEditor = do_QueryInterface(theEditor);
  if (!mEditor) { return NS_ERROR_NO_INTERFACE; }
  return NS_OK;
}


NS_METHOD nsGfxTextControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                             nsGUIEvent* aEvent,
                                             nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aEvent);
  NS_ENSURE_ARG_POINTER(aEventStatus);

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  *aEventStatus = nsEventStatus_eConsumeDoDefault;   // this is the default

  /* the handling of mouse messages here has two purposes:
   * 1) to create a subdoc if one doesn't exist on click, and to pass
   *    mouse messages along to the content of the subdoc in the case
   *    where we created it in response to a click.
   * 2) to call MouseClicked
   */
  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      // don't pass through a click if we're already handling a click
      if (eGotDown != mPassThroughMouseEvents)
      {
        mPassThroughMouseEvents = eGotDown;
        if (!mDocShell) {
          NS_ENSURE_SUCCESS(CreateSubDoc(nsnull), NS_ERROR_FAILURE);
        }
        NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
        return RedispatchMouseEventToSubDoc(aPresContext, aEvent, aEventStatus, PR_TRUE);
      }
      break;
    
    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_UP:
      // only pass through the mouseUp if we're the one who handled the mouseDown
      if (eGotDown==mPassThroughMouseEvents)
      {
        mPassThroughMouseEvents = eGotUp;
        NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
        return RedispatchMouseEventToSubDoc(aPresContext, aEvent, aEventStatus, PR_FALSE);
      }
      break;

    case NS_MOUSE_LEFT_CLICK:
    case NS_MOUSE_MIDDLE_CLICK:
    case NS_MOUSE_RIGHT_CLICK:
      MouseClicked(aPresContext);
      // only pass through the mouseClick if we're the one who handled the mouseUp
      if (eGotUp==mPassThroughMouseEvents)
      {
        mPassThroughMouseEvents = eGotClick;
        NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
        return RedispatchMouseEventToSubDoc(aPresContext, aEvent, aEventStatus, PR_FALSE);
      }
      break;

    case NS_MOUSE_LEFT_DOUBLECLICK:
    case NS_MOUSE_MIDDLE_DOUBLECLICK:
    case NS_MOUSE_RIGHT_DOUBLECLICK:
      // XXX shouldn't this have the same check that we handled the mouse up
      // XXX as did the single click events?
      // XX Do we ever get here, i.e. are these messages obsolete anyway?
      mPassThroughMouseEvents = eGotClick;
      NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
      return RedispatchMouseEventToSubDoc(aPresContext, aEvent, aEventStatus, PR_FALSE);
      break;

    case NS_MOUSE_MOVE:
      // only pass through the mouseMove if we're the one who handled the mouseDown
      if (eGotDown==mPassThroughMouseEvents)
      {
        if (mDocShell) {
          return RedispatchMouseEventToSubDoc(aPresContext, aEvent, aEventStatus, PR_FALSE);
        }
      }
      break;

    case NS_DRAGDROP_ENTER:
    case NS_DRAGDROP_OVER_SYNTH:
    case NS_DRAGDROP_DROP:
    case NS_DRAGDROP_EXIT_SYNTH:
      // currently unused
      break;
      
    case NS_DRAGDROP_GESTURE:
      // this currently seems to have no effect
      NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
      return RedispatchMouseEventToSubDoc(aPresContext, aEvent, aEventStatus, PR_FALSE);
      break;

    case NS_KEY_PRESS:
    if (NS_KEY_EVENT == aEvent->eventStructType) {
	    nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
      // XXX What is this all about?  SHouldn't this all be handled
      // XXX by XBL key bindings, rather than hardwired in here?
      // XXX Even if it should be hardwired, why VK_RETURN and not VK_ENTER?
	    if (NS_VK_RETURN == keyEvent->keyCode) 
      {
	      EnterPressed(aPresContext);
        *aEventStatus = nsEventStatus_eConsumeNoDefault;
	    }
	    else if (NS_VK_SPACE == keyEvent->keyCode) 
      {
	      MouseClicked(aPresContext);
	    }
    }
    break;
    
    case NS_FORM_SELECTED:
      return UpdateTextControlCommands(NS_ConvertASCIItoUCS2("select"));
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::RedispatchMouseEventToSubDoc(nsIPresContext* aPresContext, 
                                                    nsGUIEvent* aEvent,
                                                    nsEventStatus* aEventStatus,
                                                    PRBool aAdjustForView)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aEvent);
  NS_ENSURE_ARG_POINTER(aEventStatus);

  nsCOMPtr<nsIContentViewer> viewer;
  NS_ENSURE_SUCCESS(mDocShell->GetContentViewer(getter_AddRefs(viewer)), NS_ERROR_FAILURE);
  if (viewer) 
  {
    nsCOMPtr<nsIDocumentViewer> docv;
    viewer->QueryInterface(kIDocumentViewerIID, getter_AddRefs(docv));
    if (docv) 
    {
      nsCOMPtr<nsIPresContext> cx;
      NS_ENSURE_SUCCESS(docv->GetPresContext(*(getter_AddRefs(cx))), NS_ERROR_FAILURE);
      if (cx) 
      {
        nsCOMPtr<nsIPresShell> shell;
        NS_ENSURE_SUCCESS(cx->GetShell(getter_AddRefs(shell)), NS_ERROR_FAILURE);
        if (shell) 
        {
          nsIFrame * rootFrame;
          NS_ENSURE_SUCCESS(shell->GetRootFrame(&rootFrame), NS_ERROR_FAILURE);

          nsCOMPtr<nsIViewManager> vm;
          NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
          if (vm) 
          {
            // create a new mouse event
            nsMouseEvent event;
            event = *((nsMouseEvent*)aEvent);
            // if we're told to, translate the event point based on the view of this text control
            if (PR_TRUE==aAdjustForView)
            {
              nsIView * view = nsnull;
              GetView(aPresContext, &view);
              NS_ASSERTION(view, "text control frame has no view");
              if (!view) { return NS_ERROR_NULL_POINTER;}
              nsRect viewBounds;
              view->GetBounds(viewBounds);
              event.point.x -= viewBounds.x;
              event.point.y -= viewBounds.y;
            }
            // translate the event point based on this frame's border and padding
            nsSize  size;
            GetSize(size);
            nsRect subBounds;
            nsMargin border;
            nsMargin padding;
            border.SizeTo(0, 0, 0, 0);
            padding.SizeTo(0, 0, 0, 0);
            const nsStyleSpacing* spacing;
            GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
            spacing->CalcBorderFor(this, border);
            spacing->CalcPaddingFor(this, padding);
            CalcSizeOfSubDocInTwips(border, padding, size, subBounds);
            event.point.x -= (border.left + padding.left);
            if (0>event.point.x) {
              event.point.x = 0;
            }
            else if (event.point.x > subBounds.width) {
              event.point.x = subBounds.width-1;
            }
            event.point.y -= (border.top + padding.top);
            if (0>event.point.y) {
              event.point.y = 0;
            }
            else if (event.point.y > subBounds.height) {
              event.point.x = subBounds.height-1;
            }

            // translate the event point to pixels for consumption by the embedded view manager
            nsCOMPtr<nsIDeviceContext> dc;
            aPresContext->GetDeviceContext(getter_AddRefs(dc));
            NS_ASSERTION(dc, "text control frame has no dc");
            if (!dc) { return NS_ERROR_NULL_POINTER;}
            float t2p;
            dc->GetAppUnitsToDevUnits(t2p);

            event.point.x = NSTwipsToIntPixels(event.point.x, t2p);
            event.point.y = NSTwipsToIntPixels(event.point.y, t2p);
            NS_ENSURE_SUCCESS(vm->DispatchEvent(&event, aEventStatus), NS_ERROR_FAILURE);
          }
        }
      }
    }
  }
  return NS_OK;
}

void
nsGfxTextControlFrame::EnterPressed(nsIPresContext* aPresContext) 
{
  // Send out onChange event here to get http://maps working (bug 18501)
  nsCOMPtr<nsIDOMFocusListener> fListener = do_QueryInterface(mEventListener);
  if (fListener) {
    fListener->Blur(nsnull);
  }

  // Submit the form
  if (mFormFrame && mFormFrame->CanSubmit(this)) {
    nsIContent *formContent = nsnull;

    nsEventStatus status = nsEventStatus_eIgnore;

    mFormFrame->GetContent(&formContent);
    if (nsnull != formContent) {
      nsEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_FORM_SUBMIT;
      formContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
      NS_RELEASE(formContent);
    }

    if (nsEventStatus_eConsumeNoDefault != status) {
      mFormFrame->OnSubmit(aPresContext, this);
    }
  }
}

nsWidgetInitData*
nsGfxTextControlFrame::GetWidgetInitData(nsIPresContext* aPresContext)
{
  return nsnull;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetText(nsString* aText, PRBool aInitialValue)
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) 
  {
    if (PR_TRUE==aInitialValue)
    {
      result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
    }
    else
    {
      if (PR_TRUE==IsInitialized())
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
          result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
        }
      }
      else {
        if (mCachedState) {
          *aText = *mCachedState;
	        result = NS_OK;
	      } else {
          result = nsFormControlHelper::GetInputElementValue(mContent, aText, aInitialValue);
	      }
      }
    }
    RemoveNewlines(*aText);
  } 
  else 
  {
    nsIDOMHTMLTextAreaElement* textArea = nsnull;
    result = mContent->QueryInterface(kIDOMHTMLTextAreaElementIID, (void**)&textArea);
    if ((NS_OK == result) && textArea) {
      if (PR_TRUE == aInitialValue) {
        result = textArea->GetDefaultValue(*aText);
      }
      else {
        if(mEditor) {
          nsCOMPtr<nsIEditorIMESupport> imeSupport = do_QueryInterface(mEditor);
          if(imeSupport) 
            imeSupport->ForceCompositionEnd();
        }
        result = textArea->GetValue(*aText);
      }
      NS_RELEASE(textArea);
    }
  }
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent*     aChild,
                                        PRInt32         aNameSpaceID,
                                        nsIAtom*        aAttribute,
                                        PRInt32         aHint)
{
  nsresult result = NS_OK;

  if (nsHTMLAtoms::value == aAttribute) 
  {
    if (mEditor)
      mEditor->EnableUndo(PR_FALSE);      // wipe out undo info
    nsString value;
    GetText(&value, PR_TRUE);             // get the initial value from the content attribute
    SetTextControlFrameState(value);      // set new text value
    if (mEditor)
      mEditor->EnableUndo(PR_TRUE);       // fire up a new txn stack
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
    nsCOMPtr<nsISelectionController> selCon;
    mEditor->GetSelectionController(getter_AddRefs(selCon));
    
    nsresult rv = DoesAttributeExist(nsHTMLAtoms::readonly);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= nsIHTMLEditor::eEditorReadonlyMask;
      if (selCon)
        selCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIHTMLEditor::eEditorReadonlyMask);
      if (selCon)
        selCon->SetCaretEnabled(PR_TRUE);
    }    
    mEditor->SetFlags(flags);
  }
  else if (mEditor && nsHTMLAtoms::disabled == aAttribute) 
  {
    nsCOMPtr<nsISelectionController> selCon;
    mEditor->GetSelectionController(getter_AddRefs(selCon));
    nsCOMPtr<nsIPresShell> presShell;
    presShell = do_QueryInterface(selCon);

    nsresult rv = DoesAttributeExist(nsHTMLAtoms::disabled);
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    if (NS_CONTENT_ATTR_NOT_THERE != rv) 
    { // set readonly
      flags |= nsIHTMLEditor::eEditorDisabledMask;
      selCon->SetCaretEnabled(PR_FALSE);
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
    }
    else 
    { // unset readonly
      flags &= ~(nsIHTMLEditor::eEditorDisabledMask);
      selCon->SetCaretEnabled(PR_TRUE);
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
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
    result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
  }

  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::DoesAttributeExist(nsIAtom *aAtt)
{
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) 
  {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(aAtt, value);
    NS_RELEASE(content);
  }
  return result;
}

// aSizeOfSubdocContainer is in pixels, not twips
nsresult 
nsGfxTextControlFrame::CreateSubDoc(nsRect *aSizeOfSubdocContainer)
{
  if (!mFramePresContext) { return NS_ERROR_NULL_POINTER; }

  nsresult rv = NS_OK;

  if (!IsInitialized() && !mCreatingViewer)
  {
    // if the editor hasn't been created yet, create it
    if (!mEditor)
    {
      rv = CreateEditor();
      if (NS_FAILED(rv)) { return rv; }
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

      NS_ASSERTION(mEditor, "null EDITOR after attempt to create.");
    }

    // initialize the subdoc, if it hasn't already been constructed
    // if the size is not 0 and there is a src, create the web shell
  
    if (!mDocShell) 
    {
      nsSize  size;
      GetSize(size);
      nsRect subBounds;
      if (aSizeOfSubdocContainer)
      {
        subBounds = *aSizeOfSubdocContainer;
      }
      else
      {
        nsMargin border;
        nsMargin padding;
        border.SizeTo(0, 0, 0, 0);
        padding.SizeTo(0, 0, 0, 0);
        // Get the CSS border
        const nsStyleSpacing* spacing;
        GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
        spacing->CalcBorderFor(this, border);
        spacing->CalcPaddingFor(this, padding);
        CalcSizeOfSubDocInTwips(border, padding, size, subBounds);
        float t2p;
        mFramePresContext->GetTwipsToPixels(&t2p);
        subBounds.x      = NSToCoordRound(subBounds.x * t2p);
        subBounds.y      = NSToCoordRound(subBounds.y * t2p);
        subBounds.width  = NSToCoordRound(subBounds.width * t2p);
        subBounds.height = NSToCoordRound(subBounds.height * t2p);
      }

      rv = CreateDocShell(mFramePresContext, size);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
#ifdef NOISY 
      printf("%p docshell in CreateSubDoc set to bounds: x=%d, y=%d, w=%d, h=%d\n", mDocShell.get(), subBounds.x, subBounds.y, subBounds.width, subBounds.height);
#endif
      nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
      NS_ENSURE_TRUE(docShellWin, NS_ERROR_FAILURE);
      docShellWin->SetPositionAndSize(subBounds.x, subBounds.y, 
         subBounds.width, subBounds.height, PR_FALSE);
    }
    mCreatingViewer=PR_TRUE;
    
    // create document
    nsCOMPtr<nsIDocument> doc;
    rv = NS_NewHTMLDocument(getter_AddRefs(doc));
    if (NS_FAILED(rv)) { return rv; }
    if (!doc) { return NS_ERROR_NULL_POINTER; }

    nsCOMPtr<nsINodeInfoManager> nimgr;
    rv = doc->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsINodeInfo> nodeInfo;
    nimgr->GetNodeInfo(nsHTMLAtoms::html, nsnull, kNameSpaceID_None,
                       *getter_AddRefs(nodeInfo));

    // create document content
    nsCOMPtr<nsIHTMLContent> htmlElement;
    nsCOMPtr<nsIHTMLContent> headElement;
    nsCOMPtr<nsIHTMLContent> bodyElement;
      // create the root
    rv = NS_NewHTMLHtmlElement(getter_AddRefs(htmlElement), nodeInfo);
    if (NS_FAILED(rv)) { return rv; }
    if (!htmlElement) { return NS_ERROR_NULL_POINTER; }
      // create the head

    nimgr->GetNodeInfo(NS_ConvertASCIItoUCS2("head"), nsnull,
                       kNameSpaceID_None, *getter_AddRefs(nodeInfo));

    rv = NS_NewHTMLHeadElement(getter_AddRefs(headElement), nodeInfo);
    if (NS_FAILED(rv)) { return rv; }
    if (!headElement) { return NS_ERROR_NULL_POINTER; }
    headElement->SetDocument(doc, PR_FALSE, PR_TRUE);
      // create the body

    nimgr->GetNodeInfo(nsHTMLAtoms::body, nsnull, kNameSpaceID_None,
                      *getter_AddRefs(nodeInfo));

    rv = NS_NewHTMLBodyElement(getter_AddRefs(bodyElement), nodeInfo);
    if (NS_FAILED(rv)) { return rv; }
    if (!bodyElement) { return NS_ERROR_NULL_POINTER; }
    bodyElement->SetDocument(doc, PR_FALSE, PR_TRUE);
      // put the head and body into the root
    rv = htmlElement->AppendChildTo(headElement, PR_FALSE);
    if (NS_FAILED(rv)) { return rv; }
    rv = htmlElement->AppendChildTo(bodyElement, PR_FALSE);
    if (NS_FAILED(rv)) { return rv; }
    
    // load the document into the docshell
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
    if (!domDoc) { return NS_ERROR_NULL_POINTER; }
    nsCOMPtr<nsIDOMElement> htmlDOMElement = do_QueryInterface(htmlElement);
    if (!htmlDOMElement) { return NS_ERROR_NULL_POINTER; }
    rv = mDocShell->SetDocument(domDoc, htmlDOMElement);

    // force an incremental reflow of the text control
    /* XXX: this is to get the view/docshell positioned correctly.
            I don't know why it's required, it looks like this code positions the view and docshell
            exactly the same way reflow does, but reflow works and the code above does not
            when the text control is in a view which is scrolled.
    */
    nsCOMPtr<nsIReflowCommand> cmd;
    rv = NS_NewHTMLReflowCommand(getter_AddRefs(cmd), this, nsIReflowCommand::StyleChanged);
    if (NS_FAILED(rv)) { return rv; }
    if (!cmd) { return NS_ERROR_NULL_POINTER; }
    nsCOMPtr<nsIPresShell> shell;
    rv = mFramePresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv)) { return rv; }
    if (!shell) { return NS_ERROR_NULL_POINTER; }    
    rv = shell->AppendReflowCommand(cmd);    
  }
  return rv;
}

void nsGfxTextControlFrame::CalcSizeOfSubDocInTwips(const nsMargin &aBorder, 
                                                    const nsMargin &aPadding, 
                                                    const nsSize &aFrameSize, 
                                                    nsRect &aSubBounds)
{

    // XXX: the point here is to make a single-line edit field as wide as it wants to be, 
    //      so it will scroll horizontally if the characters take up more space than the field
    aSubBounds.x      = aBorder.left + aPadding.left;
    aSubBounds.y      = aBorder.top + aPadding.top;
    aSubBounds.width  = (aFrameSize.width - (aBorder.left + aPadding.left + aBorder.right + aPadding.right));
    if (aSubBounds.width<0) {
      aSubBounds.width = 0;
    }
    aSubBounds.height = (aFrameSize.height - (aBorder.top + aPadding.top + aBorder.bottom + aPadding.bottom));
    if (aSubBounds.height<0) {
      aSubBounds.height = 0;
    }
}

PRInt32 
nsGfxTextControlFrame::GetMaxNumValues()
{
  return 1;
}

PRBool
nsGfxTextControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                        nsString* aValues, nsString* aNames)
{
  if (!aValues || !aNames) { return PR_FALSE; }

  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_NOT_THERE == result)) {
    return PR_FALSE;
  }

  aNames[0] = name;  
  aNumValues = 1;

  GetText(&(aValues[0]), PR_FALSE);
  // XXX: error checking
  return PR_TRUE;
}


void 
nsGfxTextControlFrame::Reset(nsIPresContext* aPresContext) 
{
  nsAutoString value;
  nsresult valStatus = GetText(&value, PR_TRUE);
  NS_ASSERTION((NS_SUCCEEDED(valStatus)), "GetText failed");
  if (NS_SUCCEEDED(valStatus))
  {
    SetTextControlFrameState(value);
  }
}  

NS_METHOD 
nsGfxTextControlFrame::Paint(nsIPresContext* aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer)
{
#ifdef NOISY
  printf("%p paint layer %d at (%d, %d, %d, %d)\n", this, aWhichLayer, 
    aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
#endif
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
  {
    nsAutoString text; text.AssignWithConversion(" ");
    nsRect rect(0, 0, mRect.width, mRect.height);
    PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, 
                     text, mStyleContext, rect);
  }
  return NS_OK;
}

void 
nsGfxTextControlFrame::PaintTextControlBackground(nsIPresContext* aPresContext,
                                                  nsIRenderingContext& aRenderingContext,
                                                  const nsRect& aDirtyRect,
                                                  nsFramePaintLayer aWhichLayer)
{
  // we paint our own border, but everything else is painted by the mDocshell
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
  {
    nsAutoString text; text.AssignWithConversion(" ");
    nsRect rect(0, 0, mRect.width, mRect.height);
    PaintTextControl(aPresContext, aRenderingContext, aDirtyRect, 
                     text, mStyleContext, rect);
  }
}

// stolen directly from nsContainerFrame
void
nsGfxTextControlFrame::PaintChild(nsIPresContext*      aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect&        aDirtyRect,
                                  nsIFrame*            aFrame,
                                  nsFramePaintLayer    aWhichLayer)
{
  nsIView *pView;
  aFrame->GetView(aPresContext, &pView);
  if (nsnull == pView) {
    nsRect kidRect;
    aFrame->GetRect(kidRect);
    nsFrameState state;
    aFrame->GetFrameState(&state);

    // Compute the constrained damage area; set the overlap flag to
    // PR_TRUE if any portion of the child frame intersects the
    // dirty rect.
    nsRect damageArea;
    PRBool overlap;
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
      // If the child frame has children that leak out of our box
      // then we don't constrain the damageArea to just the childs
      // bounding rect.
      damageArea = aDirtyRect;
      overlap = PR_TRUE;
    }
    else {
      // Compute the intersection of the dirty rect and the childs
      // rect (both are in our coordinate space). This limits the
      // damageArea to just the portion that intersects the childs
      // rect.
      overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
#ifdef NS_DEBUG
      if (!overlap && (0 == kidRect.width) && (0 == kidRect.height)) {
        overlap = PR_TRUE;
      }
#endif
    }

    if (overlap) {
      // Translate damage area into the kids coordinate
      // system. Translate rendering context into the kids
      // coordinate system.
      damageArea.x -= kidRect.x;
      damageArea.y -= kidRect.y;
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);

      // Paint the kid
      aFrame->Paint(aPresContext, aRenderingContext, damageArea, aWhichLayer);
      PRBool clipState;
      aRenderingContext.PopState(clipState);

#ifdef NS_DEBUG
      // Draw a border around the child
      if (nsIFrameDebug::GetShowFrameBorders() && !kidRect.IsEmpty()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(kidRect);
      }
#endif
    }
  }
}

void 
nsGfxTextControlFrame::PaintTextControl(nsIPresContext* aPresContext,
                                        nsIRenderingContext& aRenderingContext,
                                        const nsRect& aDirtyRect, 
                                        nsString& aText,
                                        nsIStyleContext* aStyleContext,
                                        nsRect& aRect)
{
  // XXX: aText is currently unused!
#ifdef DEBUG
  mDebugTotalPaints++;
  mDebugPaintsSinceLastReflow++;
#endif
  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (disp->IsVisible()) 
  {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)aStyleContext->GetStyleData(eStyleStruct_Spacing);
    PRIntn skipSides = 0;
    nsRect rect(0, 0, mRect.width, mRect.height);
    const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
	  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect,  *color, *mySpacing, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *mySpacing, aStyleContext, skipSides);
    if (!mDocShell)
    {
      if (mDisplayFrame) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, mDisplayFrame, NS_FRAME_PAINT_LAYER_FOREGROUND);
        //mDisplayFrame->Paint(aPresContext, aRenderingContext, aDirtyRect, NS_FRAME_PAINT_LAYER_FOREGROUND);
      }
    }
  }
#ifdef DEBUG
#ifdef NOISY
  printf("gfxTC: %p totalPaints=%d, paintsSinceLastReflow=%d\n",
          mDebugTotalPaints, mDebugPaintsSinceLastReflow);
#endif
#endif
}

//XXX: this needs to be fixed for HTML output
void nsGfxTextControlFrame::GetTextControlFrameState(nsString& aValue)
{
  aValue.SetLength(0);  // initialize out param
  
  if (mEditor && PR_TRUE==IsInitialized()) 
  {
    nsString format; format.AssignWithConversion("text/plain");
    PRUint32 flags = 0;

    if (PR_TRUE==IsPlainTextControl()) {
      flags |= nsIDocumentEncoder::OutputBodyOnly;
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
    if (mCachedState) {
      aValue = *mCachedState;
    } else {
      GetText(&aValue, PR_TRUE);
    }
  }
}     

void nsGfxTextControlFrame::SetTextControlFrameState(const nsString& aValue)
{
  if (mEditor && PR_TRUE==IsInitialized()) 
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
  else {
    if (mCachedState == nsnull) {
      mCachedState = new nsString(aValue);
    } else {
      *mCachedState = aValue;
    }
    if (mDisplayContent)
    {
      const PRUnichar *text = mCachedState->GetUnicode();
      PRInt32 len = mCachedState->Length();
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
    }
  }
}

NS_IMETHODIMP
nsGfxTextControlFrame::ContentChanged(nsIPresContext* aPresContext,
                                      nsIContent*     aChild,
                                      nsISupports*    aSubContent)
{
  // Generate a reflow command with this frame as the target frame
  nsIReflowCommand* cmd;
  nsresult          rv;
                                                
  rv = NS_NewHTMLReflowCommand(&cmd, this, nsIReflowCommand::ContentChanged);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_SUCCEEDED(rv) && shell) {
      rv = shell->AppendReflowCommand(cmd);
      // we need this reflow to be synchronous because the text may have changed
      // length.  That would make the content and frame data out of synch, and
      // can lead to a crash if we're asked to paint the text before this reflow
      // fires.  See bug 37264.
      if (NS_SUCCEEDED(rv)) { 
        rv = shell->FlushPendingNotifications();
      }      
    }
    NS_RELEASE(cmd);
  }

  return rv;
}


NS_IMETHODIMP nsGfxTextControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue)
{
  if (!mIsProcessing)
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
    else if (nsHTMLAtoms::select == aName)
    {
      // select all the text
      SelectAllContents();
    }
    else
    {
      return Inherited::SetProperty(aPresContext, aName, aValue);
    }
    mIsProcessing = PR_FALSE;
  }
  return NS_OK;
}      

NS_IMETHODIMP nsGfxTextControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If widget is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::value == aName) {
    GetTextControlFrameState(aValue);
  }
  else {
    return Inherited::GetProperty(aName, aValue);
  }

  return NS_OK;
}  

void nsGfxTextControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  // !! IF REMOVING THIS ANY CODE HERE 
  // !! READ HIDE THIS AND BELOW
  //
  // There are 3 cases where focus can get set:
  // 1) from script
  // 2) from tabbing into it
  // 3) from clicking on it
  //
  // If removing any of this code PLEASE read comment below

  nsresult rv = EnsureEditor();
  if (NS_FAILED(rv)) return;
  
  if (aOn)
  {
    nsCOMPtr<nsIContentViewer> viewer;
    mDocShell->GetContentViewer(getter_AddRefs(viewer));
    if (viewer) {
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(viewer));
      if (docv) {
        nsCOMPtr<nsIPresContext> cx;
        docv->GetPresContext(*getter_AddRefs(cx));
        if (cx) {
          nsCOMPtr<nsIPresShell>  shell;
          cx->GetShell(getter_AddRefs(shell));
          if (shell) {
            nsIFrame * rootFrame;
            shell->GetRootFrame(&rootFrame);

            nsCOMPtr<nsIViewManager>  vm;
            shell->GetViewManager(getter_AddRefs(vm));
            if (nsnull != vm) {
              nsIView *rootview = nsnull;
              vm->GetRootView(rootview);
              if (rootview) {
                // instead of using the rootview's widget we must find the deepest
                // the deepest view and use its widget
                nsCOMPtr<nsIWidget> 
                           widget(getter_AddRefs(GetDeepestWidget(rootview)));
                if (widget) {
                  // XXX Here we need to remember whether we set focus on the widget
                  // Later this is used to decided to continue to dispatch a Focus 
                  // notification into the DOM. So if we reach here, it means 
                  // a Focus message will be dispatched to the nsEnderEventListner 
                  // and setting this to true won't propigate into the DOM, because we
                  // got here via the DOM
                  // See: nsEnderEventListener::Focus
                  rv = widget->SetFocus();
                  mDidSetFocus = PR_TRUE;
                }
              }
            }
          }
        }
      }
    }
  }
  else 
  {
    /* experimental code, since mDocshell->removeFocus is a noop */
    /* this code doesn't seem to have any effect either.  bug 19392
    nsIView*  view;
    GetView(mFramePresContext, &view);
    if (view)
    {
      nsCOMPtr<nsIWidget> widget;
      view->GetWidget(*(getter_AddRefs(widget)));
      if (widget) {
        widget->SetFocus();
      }
    }
    */
    // since the embedded docshell is not in the docshell hierarchy, RemoveFocus has no effect
    // that's why we find the widget attached to this and set focus on it explicitly
    //mDocShell->RemoveFocus();
  }
}
/* --------------------- Ender methods ---------------------- */


nsresult
nsGfxTextControlFrame::CreateDocShell(nsIPresContext* aPresContext,
                                      const nsSize& aSize)
{
  nsresult rv;

  mDocShell = do_CreateInstance(kWebShellCID);
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // pass along marginwidth and marginheight so sub document can use it
  mDocShell->SetMarginWidth(0);
  mDocShell->SetMarginHeight(0);
  
  /* our parent must be a docshell.  we need to get our prefs from our parent */
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(container, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDocShell> outerShell = do_QueryInterface(container);
  NS_ENSURE_TRUE(outerShell, NS_ERROR_UNEXPECTED);

  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  nsCOMPtr<nsIPresShell> presShell;
  rv = aPresContext->GetShell(getter_AddRefs(presShell));     
  if (NS_FAILED(rv)) { return rv; }

  // create, init, set the parent of the view
  nsIView* view;
  rv = nsComponentManager::CreateInstance(kCViewCID, nsnull, kIViewIID,
                                         (void **)&view);
  if (NS_FAILED(rv)) { return rv; }

  nsIView* parView;
  nsPoint origin;
  GetOffsetFromView(aPresContext, origin, &parView);  

  nsRect viewBounds(origin.x, origin.y, aSize.width, aSize.height);
#ifdef NOISY
  printf("%p view bounds: x=%d, y=%d, w=%d, h=%d\n", view, origin.x, origin.y, aSize.width, aSize.height);
#endif

  nsCOMPtr<nsIViewManager> viewMan;
  presShell->GetViewManager(getter_AddRefs(viewMan));  
  rv = view->Init(viewMan, viewBounds, parView);
  if (NS_FAILED(rv)) { return rv; }
  viewMan->InsertChild(parView, view, 0);
  rv = view->CreateWidget(kCChildCID);
  if (NS_FAILED(rv)) { return rv; }
  SetView(aPresContext, view);

  // if the visibility is hidden, reflect that in the view
  const nsStyleDisplay* display;
  GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
  if (!display->IsVisible()) {
    view->SetVisibility(nsViewVisibility_kHide);
  }

  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
  nsMargin border;
  spacing->CalcBorderFor(this, border);

  nsCOMPtr<nsIWidget> widget;
  view->GetWidget(*getter_AddRefs(widget));

  mDocShell->SetAllowPlugins(PR_FALSE);
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));

  PRInt32 x = NSToCoordRound(border.left * t2p);
  PRInt32 y = NSToCoordRound(border.top * t2p);
  PRInt32 cx = NSToCoordRound((aSize.width  - border.right) * t2p);
  PRInt32 cy = NSToCoordRound((aSize.height - border.bottom) * t2p);
  NS_ENSURE_SUCCESS(docShellAsWin->InitWindow(nsnull, widget, (x >= 0) ? x : 0,
      (y >= 0) ? y : 0, cx, cy), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(docShellAsWin->Create(), NS_ERROR_FAILURE);

  // move the view to the proper location
  viewMan->MoveViewTo(view, origin.x, origin.y);

  mDocShell->SetDocLoaderObserver(mTempObserver);
 
  PRInt32 type;
  GetType(&type);
  if ((PR_FALSE==IsSingleLineTextControl()) || (NS_FORM_INPUT_PASSWORD == type)) {
    docShellAsWin->SetVisibility(PR_TRUE);
  }
  return NS_OK;
}


PRInt32
nsGfxTextControlFrame::CalculateSizeNavQuirks (nsIPresContext*       aPresContext, 
                                              nsIRenderingContext*  aRendContext,
                                              nsIFormControlFrame*  aFrame,
                                              const nsSize&         aCSSSize, 
                                              nsInputDimensionSpec& aSpec, 
                                              nsSize&               aDesiredSize, 
                                              nsSize&               aMinSize, 
                                              PRBool&               aWidthExplicit, 
                                              PRBool&               aHeightExplicit, 
                                              nscoord&              aRowHeight,
                                              nsMargin&             aBorder,
                                              nsMargin&             aPadding)
{
  nscoord charWidth   = 0; 
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  // Quirks does not use rowAttr
  nsHTMLValue colAttr;
  nsresult    colStatus;
  nsHTMLValue rowAttr;
  nsresult    rowStatus;
  if (NS_ERROR_FAILURE == GetColRowSizeAttr(aFrame, 
                                            aSpec.mColSizeAttr, colAttr, colStatus,
                                            aSpec.mRowSizeAttr, rowAttr, rowStatus)) {
    return 0;
  }

  // Get the Font Metrics for the Control
  // without it we can't calculate  the size
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult res = nsFormControlHelper::GetFrameFontFM(aPresContext, aFrame, getter_AddRefs(fontMet));
  if (NS_SUCCEEDED(res) && fontMet) {
    aRendContext->SetFont(fontMet);

    // Figure out the number of columns
    // and set that as the default col size
    if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
      PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
      col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
      aSpec.mColDefaultSize = col;
    }
    charWidth = nsFormControlHelper::CalcNavQuirkSizing(aPresContext, 
                                                        aRendContext, fontMet, 
                                                        aFrame, aSpec, aDesiredSize);
    aMinSize.width = aDesiredSize.width;

    // XXX I am commenting this out below to let CSS 
    // override the column setting - rods

    // If COLS was not set then check to see if CSS has the width set
    //if (NS_CONTENT_ATTR_HAS_VALUE != colStatus) {  // col attr will provide width
      if (CSS_NOTSET != aCSSSize.width) {  // css provides width
        //NS_ASSERTION(aCSSSize.width >= 0, "form control's computed width is < 0"); 
        if (NS_INTRINSICSIZE != aCSSSize.width) {
          aDesiredSize.width = aCSSSize.width;
          aWidthExplicit = PR_TRUE;
        }
      }
    //}

    aDesiredSize.height = aDesiredSize.height * aSpec.mRowDefaultSize;
    if (CSS_NOTSET != aCSSSize.height) {  // css provides height
      NS_ASSERTION(aCSSSize.height > 0, "form control's computed height is <= 0"); 
      if (NS_INTRINSICSIZE != aCSSSize.height) {
        aDesiredSize.height = aCSSSize.height;
        aHeightExplicit = PR_TRUE;
      }
    }

  } else {
    NS_ASSERTION(fontMet, "Couldn't get Font Metrics"); 
    aDesiredSize.width = 300;  // arbitrary values
    aDesiredSize.width = 1500;
  }

  aRowHeight      = aDesiredSize.height;
  aMinSize.height = aDesiredSize.height;

  PRInt32 numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;

  return numRows;
}

//------------------------------------------------------------------
NS_IMETHODIMP
nsGfxTextControlFrame::ReflowNavQuirks(nsIPresContext*           aPresContext,
                                        nsHTMLReflowMetrics&     aDesiredSize,
                                        const nsHTMLReflowState& aReflowState,
                                        nsReflowStatus&          aStatus,
                                        nsMargin&                aBorder,
                                        nsMargin&                aPadding)
{
  nsMargin borderPadding;
  borderPadding.SizeTo(0, 0, 0, 0);
  // Get the CSS border
  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);

  // This calculates the reflow size
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(aPresContext, aReflowState, styleSize);

  nsSize desiredSize;
  nsSize minSize;
  
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    PRInt32 width = 0;
    if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
      width = GetDefaultColumnWidth();
    }
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, 
                                  PR_FALSE, nsnull, 1);
    CalculateSizeNavQuirks(aPresContext, aReflowState.rendContext, this, styleSize, 
                           textSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorder, aPadding);
  } else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 
                                  nsnull, GetDefaultColumnWidth(), 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSizeNavQuirks(aPresContext, aReflowState.rendContext, this, styleSize, 
                           areaSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorder, aPadding);
  }
  if (widthExplicit) {
    desiredSize.width  += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
  }
  if (heightExplicit) {
    desiredSize.height += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
  }

  aDesiredSize.width   = desiredSize.width;
  aDesiredSize.height  = desiredSize.height;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = widthExplicit?desiredSize.width:minSize.width;
    aDesiredSize.maxElementSize->height = heightExplicit?desiredSize.height:minSize.height;
  }

  // In Nav Quirks mode we only add in extra size for padding
  nsMargin padding;
  padding.SizeTo(0, 0, 0, 0);
  spacing->CalcPaddingFor(this, padding);

  // Check to see if style was responsible 
  // for setting the height or the width
  PRBool addBorder = PR_FALSE;
  PRInt32 width;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetSizeFromContent(&width)) {
    // if a size attr gets incorrectly 
    // put on a textarea it comes back as -1
    if (width > -1) { 
      addBorder = (width < GetDefaultColumnWidth()) && !widthExplicit;
    }
  }

  if (addBorder) {
    if (CSS_NOTSET != styleSize.width || 
        CSS_NOTSET != styleSize.height) {  // css provides width
      nsMargin border;
      border.SizeTo(0, 0, 0, 0);
      spacing->CalcBorderFor(this, border);
      if (CSS_NOTSET != styleSize.width) {  // css provides width
        aDesiredSize.width  += border.left + border.right;
      }
      if (CSS_NOTSET != styleSize.height) {  // css provides heigth
        aDesiredSize.height += border.top + border.bottom;
      }
    }
  }
  return NS_OK;
}

nsresult 
nsGfxTextControlFrame::GetColRowSizeAttr(nsIFormControlFrame*  aFrame,
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
nsGfxTextControlFrame::CalculateSizeStandard (nsIPresContext*       aPresContext, 
                                              nsIRenderingContext*  aRendContext,
                                              nsIFormControlFrame*  aFrame,
                                              const nsSize&         aCSSSize, 
                                              nsInputDimensionSpec& aSpec, 
                                              nsSize&               aDesiredSize, 
                                              nsSize&               aMinSize, 
                                              PRBool&               aWidthExplicit, 
                                              PRBool&               aHeightExplicit, 
                                              nscoord&              aRowHeight,
                                              nsMargin&             aBorder,
                                              nsMargin&             aPadding) 
{
  nscoord charWidth   = 0; 
  aWidthExplicit      = PR_FALSE;
  aHeightExplicit     = PR_FALSE;

  aDesiredSize.width  = CSS_NOTSET;
  aDesiredSize.height = CSS_NOTSET;

  nsHTMLValue colAttr;
  nsresult    colStatus;
  nsHTMLValue rowAttr;
  nsresult    rowStatus;
  if (NS_ERROR_FAILURE == GetColRowSizeAttr(aFrame, 
                                            aSpec.mColSizeAttr, colAttr, colStatus,
                                            aSpec.mRowSizeAttr, rowAttr, rowStatus)) {
    return 0;
  }

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  // determine the width, char height, row height
  if (NS_CONTENT_ATTR_HAS_VALUE == colStatus) {  // col attr will provide width
    PRInt32 col = ((colAttr.GetUnit() == eHTMLUnit_Pixel) ? colAttr.GetPixelValue() : colAttr.GetIntValue());
    col = (col <= 0) ? 1 : col; // XXX why a default of 1 char, why hide it
    charWidth = nsFormControlHelper::GetTextSize(aPresContext, aFrame, col, aDesiredSize, aRendContext);
    aMinSize.width = aDesiredSize.width;
  } else {
    charWidth = nsFormControlHelper::GetTextSize(aPresContext, aFrame, aSpec.mColDefaultSize, aDesiredSize, aRendContext); 
    aMinSize.width = aDesiredSize.width;
    if (CSS_NOTSET != aCSSSize.width) {  // css provides width
      //NS_ASSERTION(aCSSSize.width >= 0, "form control's computed width is < 0"); 
      if (NS_INTRINSICSIZE != aCSSSize.width) {
        aDesiredSize.width = aCSSSize.width;
        aWidthExplicit = PR_TRUE;
      }
    }
  }

  nscoord fontHeight  = 0;
  //nscoord fontLeading = 0;
  // get leading
  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult res = nsFormControlHelper::GetFrameFontFM(aPresContext, aFrame, getter_AddRefs(fontMet));
  if (NS_SUCCEEDED(res) && fontMet) {
    aRendContext->SetFont(fontMet);
    fontMet->GetHeight(fontHeight);
    // leading is NOT suppose to be added in
    //fontMet->GetLeading(fontLeading);
    //aDesiredSize.height += fontLeading;
  }
  aRowHeight      = aDesiredSize.height;
  aMinSize.height = aDesiredSize.height;
  PRInt32 numRows = 0;

  if (NS_CONTENT_ATTR_HAS_VALUE == rowStatus) { // row attr will provide height
    PRInt32 rowAttrInt = ((rowAttr.GetUnit() == eHTMLUnit_Pixel) 
                            ? rowAttr.GetPixelValue() : rowAttr.GetIntValue());
    numRows = (rowAttrInt > 0) ? rowAttrInt : 1;
    aDesiredSize.height = aDesiredSize.height * numRows;
  } else {
    aDesiredSize.height = aDesiredSize.height * aSpec.mRowDefaultSize;
    if (CSS_NOTSET != aCSSSize.height) {  // css provides height
      NS_ASSERTION(aCSSSize.height > 0, "form control's computed height is <= 0"); 
      if (NS_INTRINSICSIZE != aCSSSize.height) {
        aDesiredSize.height = aCSSSize.height;
        aHeightExplicit = PR_TRUE;
      }
    }
  }

  numRows = (aRowHeight > 0) ? (aDesiredSize.height / aRowHeight) : 0;
  if (numRows == 1) {
    PRInt32 type;
    GetType(&type);
    if (NS_FORM_TEXTAREA == type) {
      aDesiredSize.height += fontHeight;
    }
  }

  return numRows;
}

NS_IMETHODIMP
nsGfxTextControlFrame::ReflowStandard(nsIPresContext*          aPresContext,
                                      nsHTMLReflowMetrics&     aDesiredSize,
                                      const nsHTMLReflowState& aReflowState,
                                      nsReflowStatus&          aStatus,
                                      nsMargin&                aBorder,
                                      nsMargin&                aPadding)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(aPresContext, aReflowState, styleSize);

  nsSize desiredSize;
  nsSize minSize;
  
  PRBool widthExplicit, heightExplicit;
  PRInt32 ignore;
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT == type) || (NS_FORM_INPUT_PASSWORD == type)) {
    PRInt32 width = 0;
    if (NS_CONTENT_ATTR_HAS_VALUE != GetSizeFromContent(&width)) {
      width = GetDefaultColumnWidth();
    }
    nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull,
                                  nsnull, width, 
                                  PR_FALSE, nsnull, 1);
    CalculateSizeStandard(aPresContext, aReflowState.rendContext, this, styleSize, 
                           textSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorder, aPadding);
  } else {
    nsInputDimensionSpec areaSpec(nsHTMLAtoms::cols, PR_FALSE, nsnull, 
                                  nsnull, GetDefaultColumnWidth(), 
                                  PR_FALSE, nsHTMLAtoms::rows, 1);
    CalculateSizeStandard(aPresContext, aReflowState.rendContext, this, styleSize, 
                           areaSpec, desiredSize, minSize, widthExplicit, 
                           heightExplicit, ignore, aBorder, aPadding);
  }

  // CalculateSize makes calls in the nsFormControlHelper that figures
  // out the entire size of the control when in NavQuirks mode. For the
  // textarea, this means the scrollbar sizes hav already been added to
  // its overall size and do not need to be added here.
  if (NS_FORM_TEXTAREA == type) {
    float   p2t;
    aPresContext->GetPixelsToTwips(&p2t);

    nscoord scrollbarWidth  = 0;
    nscoord scrollbarHeight = 0;
    float   scale;
    nsCOMPtr<nsIDeviceContext> dx;
    aPresContext->GetDeviceContext(getter_AddRefs(dx));
    if (dx) { 
      float sbWidth;
      float sbHeight;
      dx->GetCanonicalPixelScale(scale);
      dx->GetScrollBarDimensions(sbWidth, sbHeight);
      scrollbarWidth  = PRInt32(sbWidth * scale);
      scrollbarHeight = PRInt32(sbHeight * scale);
    } else {
      scrollbarWidth  = GetScrollbarWidth(p2t);
      scrollbarHeight = scrollbarWidth;
    }

    if (!heightExplicit) {
      desiredSize.height += scrollbarHeight;
      minSize.height     += scrollbarHeight;
    } 
    if (!widthExplicit) {
      desiredSize.width += scrollbarWidth;
      minSize.width     += scrollbarWidth;
    }
  }
  desiredSize.width  += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
  desiredSize.height += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;

  aDesiredSize.width   = desiredSize.width;
  aDesiredSize.height  = desiredSize.height;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = widthExplicit?desiredSize.width:minSize.width;
    aDesiredSize.maxElementSize->height = heightExplicit?desiredSize.height:minSize.height;
  }

  return NS_OK;

}

NS_IMETHODIMP
nsGfxTextControlFrame::Reflow(nsIPresContext* aPresContext,
                              nsHTMLReflowMetrics& aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsGfxTextControlFrame", aReflowState.reason);
#ifdef DEBUG
  mDebugTotalReflows++;
  if (1==mDebugTotalReflows) {  // first reflow, better be initial reflow!
    NS_ASSERTION((eReflowReason_Initial == aReflowState.reason), "Frame got first reflow, but reason is not 'initial'");
  }
  if (eReflowReason_Resize == aReflowState.reason) {
    mDebugResizeReflows++;
  }
  if (aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE) {
    mDebugResizeUnconstrained++;
  }
  mDebugPaintsSinceLastReflow=0;
#endif

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsGfxTextControlFrame::Reflow: aMaxSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");
  // add ourself as an nsIFormControlFrame
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    mPresContext = aPresContext;
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
    nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
  }
#ifdef NOISY
  printf("gfxTCF: reflow reason=%d\n", aReflowState.reason);
#endif

  // Turning off optimization for now
#if 0
  nsresult skiprv = SkipResizeReflow(mCacheSize, mCachedMaxElementSize, aPresContext, 
                                     aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(skiprv)) {
    return skiprv;
  }
#endif

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
#if DEBUG
  nsRect myRect;
  GetRect(myRect);
  if ((aDesiredSize.width != myRect.width) ||
    (aDesiredSize.height != myRect.height)) {
    mDebugResizeReflowsThatChangedMySize++;
  }
#endif

  // Turning off optimization for now
#if 0
  SetupCachedSizes(mCacheSize, mCachedMaxElementSize, aDesiredSize);
#endif

#ifdef NOISY
  printf ("exit nsGfxTextControlFrame::Reflow: size=%d,%d\n",
           aDesiredSize.width, aDesiredSize.height);
#endif

  float t2p, p2t;
  mFramePresContext->GetTwipsToPixels(&t2p);
  mFramePresContext->GetPixelsToTwips(&p2t);
  // resize the sub document within the defined rect 
  // the sub-document will fit inside the border
  if (NS_SUCCEEDED(rv))
  {
    nsRect subBounds;
    nsRect subBoundsInPixels;
    nsSize desiredSize(aDesiredSize.width, aDesiredSize.height);
    CalcSizeOfSubDocInTwips(border, padding, desiredSize, subBounds);
    subBoundsInPixels.x = NSToCoordRound(subBounds.x * t2p);
    subBoundsInPixels.y = NSToCoordRound(subBounds.y * t2p);
    subBoundsInPixels.width = NSToCoordRound(subBounds.width * t2p);
    subBoundsInPixels.height = NSToCoordRound(subBounds.height * t2p);
    if (eReflowReason_Initial == aReflowState.reason)
    {
      if (!mDocShell)
      { // if we haven't already created a docshell, then create something to hold the initial text
        PRInt32 type;
        GetType(&type);
        if ((PR_FALSE==IsSingleLineTextControl()) || (NS_FORM_INPUT_PASSWORD == type))
        { // password controls and multi-line text areas get their subdoc right away
          rv = CreateSubDoc(&subBoundsInPixels);
        }
        else if (mDisplayFrame)
        {
#ifdef NOISY
          printf("Error in nsGfxTextControlFrame: already created display content on intial reflow!\n");
#endif
        }
        else
        { // single line text controls get a display frame rather than a subdoc.
          // the subdoc will be created when the frame first gets focus
          // create anonymous text content
          nsCOMPtr<nsIContent> content;
          rv = NS_NewTextNode(getter_AddRefs(content));
          if (NS_FAILED(rv)) { return rv; }
          if (!content) { return NS_ERROR_NULL_POINTER; }
          nsIDocument* doc;
          mContent->GetDocument(doc);
          content->SetDocument(doc, PR_FALSE, PR_TRUE);
          NS_RELEASE(doc);
          mContent->AppendChildTo(content, PR_FALSE);

          // set the value of the text node
          content->QueryInterface(NS_GET_IID(nsITextContent), getter_AddRefs(mDisplayContent));
          if (!mDisplayContent) {return NS_ERROR_NO_INTERFACE; }

          // get the text value, either from input element attribute or cached state
          nsAutoString value;
          if (mCachedState) {
            value.Assign(*mCachedState);
	        } 
          else 
          {
            nsIHTMLContent *htmlContent = nsnull;
            if (mContent)
            {
              mContent->QueryInterface(kIHTMLContentIID, (void**) &htmlContent);
              if (htmlContent) 
              {
                nsHTMLValue htmlValue;
                if (NS_CONTENT_ATTR_HAS_VALUE ==
                    htmlContent->GetHTMLAttribute(nsHTMLAtoms::value, htmlValue)) 
                {
                  if (eHTMLUnit_String == htmlValue.GetUnit()) 
                  {
                    htmlValue.GetStringValue(value);
                  }
                }
              }
              NS_RELEASE(htmlContent);
            }
	        }

          PRInt32 len = value.Length();
          if (0<len)
          {
            // for password fields, set the display text to '*', one per character
            // XXX: the replacement character should be controllable via CSS
            // for normal text fields, set the display text normally
            if (NS_FORM_INPUT_PASSWORD == type) 
            {
              PRUnichar *initialPasswordText;
              initialPasswordText = new PRUnichar[len+1];
              if (!initialPasswordText) { return NS_ERROR_NULL_POINTER; }
              PRInt32 i=0;
              for (; i<len; i++) {
                initialPasswordText[i] = PASSWORD_REPLACEMENT_CHAR;
              }
              mDisplayContent->SetText(initialPasswordText, len, PR_TRUE);
              delete [] initialPasswordText;
            }
            else
            {
              const PRUnichar *initialText;
              initialText = value.GetUnicode();
              mDisplayContent->SetText(initialText, len, PR_TRUE);
            }
          }        

          // create the pseudo frame for the anonymous content
          if (mDisplayFrame) {
            mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, mDisplayFrame, nsnull);
            rv = mDisplayFrame->Destroy(aPresContext);
            if (NS_FAILED(rv)) return rv;
          }

          nsCOMPtr<nsIPresShell> shell;
          aPresContext->GetShell(getter_AddRefs(shell));
          rv = NS_NewBlockFrame(shell, (nsIFrame**)&mDisplayFrame, NS_BLOCK_SPACE_MGR);
          if (NS_FAILED(rv)) { return rv; }
          if (!mDisplayFrame) { return NS_ERROR_NULL_POINTER; }
        
          // create the style context for the anonymous frame
          nsCOMPtr<nsIStyleContext> styleContext;
          rv = aPresContext->ResolvePseudoStyleContextFor(mContent, 
                                                          nsHTMLAtoms::mozSingleLineTextControlFrame,
                                                          mStyleContext,
                                                          PR_FALSE,
                                                          getter_AddRefs(styleContext));
          if (NS_FAILED(rv)) { return rv; }
          if (!styleContext) { return NS_ERROR_NULL_POINTER; }

          // create a text frame and put it inside the block frame
          nsIFrame *textFrame;
          rv = NS_NewTextFrame(shell, &textFrame);
          if (NS_FAILED(rv)) { return rv; }
          if (!textFrame) { return NS_ERROR_NULL_POINTER; }
          nsCOMPtr<nsIStyleContext> textStyleContext;
          rv = aPresContext->ResolvePseudoStyleContextFor(mContent, 
                                                          nsHTMLAtoms::mozSingleLineTextControlFrame,
                                                          styleContext,
                                                          PR_FALSE,
                                                          getter_AddRefs(textStyleContext));
          if (NS_FAILED(rv)) { return rv; }
          if (!textStyleContext) { return NS_ERROR_NULL_POINTER; }
          textFrame->Init(aPresContext, content, mDisplayFrame, textStyleContext, nsnull);
          textFrame->SetInitialChildList(aPresContext, nsnull, nsnull);
          nsCOMPtr<nsIPresShell> presShell;
          rv = aPresContext->GetShell(getter_AddRefs(presShell));
          if (NS_FAILED(rv)) { return rv; }
          if (!presShell) { return NS_ERROR_NULL_POINTER; }
          nsCOMPtr<nsIFrameManager> frameManager;
          rv = presShell->GetFrameManager(getter_AddRefs(frameManager));
          if (NS_FAILED(rv)) { return rv; }
          if (!frameManager) { return NS_ERROR_NULL_POINTER; }
          frameManager->SetPrimaryFrameFor(content, textFrame);
        
          rv = mDisplayFrame->Init(aPresContext, content, this, styleContext, nsnull);
          if (NS_FAILED(rv)) { return rv; }

          mDisplayFrame->SetInitialChildList(aPresContext, nsnull, textFrame);
        }
      }
    }
    nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
    if (docShellWin) 
    {
      if (mDisplayFrame) 
      {
        docShellWin->SetVisibility(PR_TRUE);
        mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, mDisplayFrame, nsnull);
        mDisplayFrame->Destroy(mFramePresContext);
        mDisplayFrame = nsnull;
      }
#ifdef NOISY
      printf("%p docshell in reflow set to bounds: x=%d, y=%d, w=%d, h=%d\n", mDocShell.get(), subBoundsInPixels.x, subBoundsInPixels.y, subBoundsInPixels.width, subBoundsInPixels.height);
#endif
#ifdef DEBUG
      mDebugReflowsThatMovedSubdoc++;
#endif
      docShellWin->SetPositionAndSize(subBoundsInPixels.x, subBoundsInPixels.y,
         subBoundsInPixels.width, subBoundsInPixels.height, PR_FALSE);
      nsCOMPtr<nsIPresShell> presShell;
      rv = aPresContext->GetShell(getter_AddRefs(presShell));     
      if (NS_FAILED(rv)) { return rv; }
      nsIView *view;
      GetView(aPresContext, &view);
      if (view)
      {
        nsCOMPtr<nsIViewManager> viewMan;
        presShell->GetViewManager(getter_AddRefs(viewMan));  
        nsIView* parView;
        nsPoint origin;
        GetOffsetFromView(aPresContext, origin, &parView);  
        viewMan->MoveViewTo(view, origin.x, origin.y);
      }
    }
    else
    {
      // pass the reflow to mDisplayFrame, forcing him to fit
//      mDisplayFrame->SetSuggestedSize(subBounds.width, subBounds.height);
      if (mDisplayFrame)
      {
        // fix any possible pixel roundoff error (the docshell is sized in whole pixel units, 
        // and we need to make sure the text is painted in exactly the same place using either frame or shell.)
        subBounds.x = NSToCoordRound(subBoundsInPixels.x * p2t);
        subBounds.y = NSToCoordRound(subBoundsInPixels.y * p2t);
        subBounds.width = NSToCoordRound(subBoundsInPixels.width * p2t);
        subBounds.height = NSToCoordRound(subBoundsInPixels.height * p2t);

        // build the data structures for reflowing mDisplayFrame
        nsSize availSize(subBounds.width, subBounds.height);
        nsHTMLReflowMetrics kidSize(&availSize);
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState, mDisplayFrame,
                                         availSize);

        // Send the WillReflow notification, and reflow the child frame
        mDisplayFrame->WillReflow(aPresContext);
        mDisplayFrame->MoveTo(aPresContext, subBounds.x, subBounds.y);
        nsIView*  view;
        mDisplayFrame->GetView(aPresContext, &view);
        if (view) {
          nsContainerFrame::PositionFrameView(aPresContext, mDisplayFrame, view);
        }
        nsReflowStatus status;
        rv = mDisplayFrame->Reflow(aPresContext, kidSize, kidReflowState, status);
        // notice how status is ignored here
#ifdef NOISY
        printf("%p mDisplayFrame resized to: x=%d, y=%d, w=%d, h=%d\n", mDisplayFrame, subBounds.x, subBounds.y, subBounds.width, subBounds.height); 
#endif
        mDisplayFrame->SetRect(aPresContext, subBounds);
        // finish the reflow
        mDisplayFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
      }
    }
  }

#ifdef NOISY
        printf("at the time of the reflow, the frame looks like...\n");
        List(aPresContext, stdout, 0);
#endif

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsGfxTextControlFrame::Reflow: size=%d,%d",
                  aDesiredSize.width, aDesiredSize.height));

// This code below will soon be changed over for NSPR logging
// It is used to figure out what font and font size the textarea or text field
// are and compares it to the know NavQuirks size
#ifdef DEBUG_rodsXXX
//#ifdef NS_DEBUG
  {
    const nsFont * font = nsnull;
    nsresult res = GetFont(aPresContext, font);
    if (NS_SUCCEEDED(res) & font != nsnull) {
      nsCOMPtr<nsIDeviceContext> deviceContext;
      aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

      nsIFontMetrics* fontMet;
      deviceContext->GetMetricsFor(font, fontMet);
    }

    const nsFont& normal = aPresContext->GetDefaultFixedFontDeprecated();
    PRInt32 scaler;
    aPresContext->GetFontScaler(&scaler);
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
    PRInt32 fontSize = nsStyleUtil::FindNextSmallerFontSize(font.size, (PRInt32)normal.size, 
                                                            scaleFactor, aPresContext)+1;
    PRBool doMeasure = PR_FALSE;
    nsILookAndFeel::nsMetricNavFontID fontId;

    nsFont defFont(aPresContext->GetDefaultFixedFontDeprecated());
    if (font.name == defFont.name) {
      fontId    = nsILookAndFeel::eMetricSize_Courier;
      doMeasure = PR_TRUE;
    } else {
      nsAutoString sansSerif("sans-serif");
      if (font.name == sansSerif) {
        fontId    = nsILookAndFeel::eMetricSize_SansSerif;
        doMeasure = PR_TRUE;
      }
    }
    NS_RELEASE(fontMet);

    if (doMeasure) {
      nsCOMPtr<nsILookAndFeel> lf;
      aPresContext->GetLookAndFeel(getter_AddRefs(lf));

      PRInt32 type;
      GetType(&type);
      if (NS_FORM_TEXTAREA == type) {
        if (fontSize > -1) {
          nsSize size;
          lf->GetNavSize(nsILookAndFeel::eMetricSize_TextArea, fontId, fontSize, size);
          COMPARE_QUIRK_SIZE("nsGfxText(textarea)", size.width, size.height)  // text area
        }
      } else {
        if (fontSize > -1) {
          nsSize size;
          lf->GetNavSize(nsILookAndFeel::eMetricSize_TextField, fontId, fontSize, size);
          COMPARE_QUIRK_SIZE("nsGfxText(field)", size.width, size.height)  // text field
        }
      }
    }
  }
#endif
#ifdef DEBUG
#if NOISY
  printf("gfxTC: %p reflow stats at end of reflow:\n\ttotal\tresize\tunconst\tchanged\tmoved\n", this);
  printf("\t%d\t%d\t%d\t%d\t%d\n", 
          mDebugTotalReflows, mDebugResizeReflows, 
          mDebugResizeUnconstrained,
          mDebugResizeReflowsThatChangedMySize, mDebugReflowsThatMovedSubdoc);
#endif
#endif
  return NS_OK;
}

nsresult 
nsGfxTextControlFrame::RequiresWidget(PRBool &aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetPresShellFor(nsIDocShell* aDocShell, nsIPresShell** aPresShell)
{
  NS_ENSURE_ARG(aDocShell);
  NS_ENSURE_ARG_POINTER(aPresShell);
  *aPresShell = nsnull;

  nsCOMPtr<nsIContentViewer> cv;
  aDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) 
  {
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if (docv) 
    {
      nsCOMPtr<nsIPresContext> cx;
      docv->GetPresContext(*getter_AddRefs(cx));
	    if (cx) 
      {
	      NS_ENSURE_SUCCESS(cx->GetShell(aPresShell), NS_ERROR_FAILURE);
	    }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetFirstNodeOfType(const nsString& aTag, nsIDOMDocument *aDOMDoc, nsIDOMNode **aNode)
{
  if (!aDOMDoc || !aNode) { return NS_ERROR_NULL_POINTER; }
  *aNode=nsnull;
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsresult result = aDOMDoc->GetElementsByTagName(aTag, getter_AddRefs(nodeList));
  if ((NS_SUCCEEDED(result)) && nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    result = nodeList->Item(0, aNode);
    if (!aNode) { result = NS_ERROR_NULL_POINTER; }
  }
  return result;
}

nsresult
nsGfxTextControlFrame::GetFirstFrameForType(const nsString& aTag, nsIPresShell *aPresShell, 
                                              nsIDOMDocument *aDOMDoc, nsIFrame **aResult)
{
  if (!aPresShell || !aDOMDoc || !aResult) { return NS_ERROR_NULL_POINTER; }
  nsresult result;
  *aResult = nsnull;
  nsCOMPtr<nsIDOMNode>node;
  result = GetFirstNodeOfType(aTag, aDOMDoc, getter_AddRefs(node));
  if ((NS_SUCCEEDED(result)) && node)
  {
    nsCOMPtr<nsIContent>content = do_QueryInterface(node);
    if (content)
    {
      result = aPresShell->GetPrimaryFrameFor(content, aResult);
    }
  }
  return result;
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame::IsSingleLineTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_TEXT==type) || (NS_FORM_INPUT_PASSWORD==type)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

// XXX: wouldn't it be nice to get this from the style context!
PRBool nsGfxTextControlFrame::IsPlainTextControl() const
{
  // need to check HTML attribute of mContent and/or CSS.
  return PR_TRUE;
}

PRBool nsGfxTextControlFrame::IsPasswordTextControl() const
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_PASSWORD==type) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

// so we don't have to keep an extra flag around, just see if
// we've allocated the event listener or not.
PRBool nsGfxTextControlFrame::IsInitialized() const
{
  return (PRBool)(mEditor && mEventListener);
}

PRInt32 
nsGfxTextControlFrame::GetWidthInCharacters() const
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
  return GetDefaultColumnWidth();
}

NS_IMETHODIMP
nsGfxTextControlFrame::InstallEditor()
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mEditor)
  {
    nsCOMPtr<nsIPresShell> presShell;
    result = GetPresShellFor(mDocShell, getter_AddRefs(presShell));
    if (NS_FAILED(result)) { return result; }
    if (!presShell) { return NS_ERROR_NULL_POINTER; }

    // set the scrolling behavior
    nsCOMPtr<nsIViewManager> vm;
    presShell->GetViewManager(getter_AddRefs(vm));
    if (vm) 
    {
      nsIScrollableView *sv=nsnull;
      vm->GetRootScrollableView(&sv);
      if (sv) {
        if (PR_TRUE==IsSingleLineTextControl()) {
          sv->SetScrollPreference(nsScrollPreference_kNeverScroll);
        } else {
          PRInt32 type;
          GetType(&type);
          if (NS_FORM_TEXTAREA == type) {
            // use a local result value, since we don't care about errors here
            nsScrollPreference scrollPref = nsScrollPreference_kAlwaysScroll;
            nsFormControlHelper::nsHTMLTextWrap wrapProp;
            nsresult wrapResult = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
            if (NS_CONTENT_ATTR_NOT_THERE != wrapResult) {
              if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Soft ||
                  wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard) {
                scrollPref = nsScrollPreference_kAuto;
              }
            }
            sv->SetScrollPreference(scrollPref);
          }
        }
        // views are not refcounted
      }
    }

    nsCOMPtr<nsIDocument> doc; 
    presShell->GetDocument(getter_AddRefs(doc));
    NS_ASSERTION(doc, "null document");
    if (!doc) { return NS_ERROR_NULL_POINTER; }
    mDoc = do_QueryInterface(doc);
    if (!mDoc) { return NS_ERROR_NULL_POINTER; }
    if (mDocObserver) {
      doc->AddObserver(mDocObserver);
    }

    PRUint32 editorFlags = 0;
    if (IsPlainTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPlaintextMask;
    if (IsSingleLineTextControl())
      editorFlags |= nsIHTMLEditor::eEditorSingleLineMask;
    if (IsPasswordTextControl())
      editorFlags |= nsIHTMLEditor::eEditorPasswordMask;
      
    // initialize the editor
    nsCOMPtr<nsISelectionController> selCon;
    selCon = do_QueryInterface(presShell);
    result = mEditor->Init(mDoc, presShell,nsnull, selCon, editorFlags);
    if (NS_FAILED(result)) { return result; }

    nsCOMPtr<nsIPresShell> framePresShell;
    mFramePresContext->GetShell(getter_AddRefs(framePresShell));
    nsCOMPtr<nsIDocument> frameDocument;
    framePresShell->GetDocument(getter_AddRefs(frameDocument));

    nsCOMPtr<nsIDocument>  eDocument;
    presShell->GetDocument(getter_AddRefs(eDocument));

    // This allows the mousewheel code to scroll the main document
    // when the pointer is over the editor control

    eDocument->SetParentDocument(frameDocument);

    // set data from the text control into the editor
    result = InitializeTextControl(presShell, mDoc);
    if (NS_FAILED(result)) { return result; }

    // install our own event handlers before the editor's event handlers
    result = InstallEventListeners();
    if (NS_FAILED(result)) { return result; }

    // finish editor initialization, including event handler installation
    result = mEditor->PostCreate();
		if (NS_FAILED(result)) { return result; }

    // check to see if mContent has focus, and if so tell the docshell.
    nsCOMPtr<nsIEventStateManager> manager;
    result = mFramePresContext->GetEventStateManager(getter_AddRefs(manager));
    if (NS_FAILED(result)) { return result; }
    if (!manager) { return NS_ERROR_NULL_POINTER; }

    nsCOMPtr<nsIContent> focusContent;
    result = manager->GetFocusedContent(getter_AddRefs(focusContent));
    if (NS_FAILED(result)) { return result; }
    if (focusContent)
    {
      if (mContent == focusContent.get())
      {
        // XXX DocShell redesign work
        SetFocus();
      }
    }
  }
  mCreatingViewer = PR_FALSE;
  return result;
}

NS_IMETHODIMP
nsGfxTextControlFrame::InstallEventListeners()
{
  nsresult result;

  // get the DOM event receiver
  nsCOMPtr<nsIDOMEventReceiver> er;
  result = mDoc->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(er));
  if (!er) { result = NS_ERROR_NULL_POINTER; }

  // get the view from the docshell
  nsCOMPtr<nsIPresShell> presShell;
  result = GetPresShellFor(mDocShell, getter_AddRefs(presShell));
  if (NS_FAILED(result)) { return result; }
  if (!presShell) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  if (!vm) { return NS_ERROR_NULL_POINTER; }
  nsIScrollableView *sv=nsnull;
  vm->GetRootScrollableView(&sv);
  if (!sv) { return NS_ERROR_NULL_POINTER; }
  nsIView *view;
  sv->QueryInterface(NS_GET_IID(nsIView), (void **)&view);
  if (!view) { return NS_ERROR_NULL_POINTER; }

  // we need to hook up our listeners before the editor is initialized
  result = NS_NewEnderEventListener(getter_AddRefs(mEventListener));
  if (NS_FAILED(result)) { return result ; }
  if (!mEventListener) { return NS_ERROR_NULL_POINTER; }
  mEventListener->SetFrame(this);
  mEventListener->SetInnerPresShell(presShell);
  mEventListener->SetPresContext(mFramePresContext);
  mEventListener->SetView(view);

  nsCOMPtr<nsIDOMKeyListener> keyListener = do_QueryInterface(mEventListener);
  if (!keyListener) { return NS_ERROR_NO_INTERFACE; }
  result = er->AddEventListenerByIID(keyListener, NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(result)) { return result; }

  nsCOMPtr<nsIDOMMouseListener> mouseListener = do_QueryInterface(mEventListener);
  if (!mouseListener) { return NS_ERROR_NO_INTERFACE; }
  result = er->AddEventListenerByIID(mouseListener, NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(result)) { return result; }
  
  nsCOMPtr<nsIDOMFocusListener> focusListener = do_QueryInterface(mEventListener);
  if (!focusListener) { return NS_ERROR_NO_INTERFACE; }
  result = er->AddEventListenerByIID(focusListener, NS_GET_IID(nsIDOMFocusListener));
  if (NS_FAILED(result)) { return result; }

  // add the selection listener
  if (mEditor && mEventListener)
  {
    nsCOMPtr<nsISelection>selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) { return result; }
    if (!selection) { return NS_ERROR_NULL_POINTER; }
    nsCOMPtr<nsISelectionListener> selectionListener = do_QueryInterface(mEventListener);
    if (!selectionListener) { return NS_ERROR_NO_INTERFACE; }
    result = selection->AddSelectionListener(selectionListener);
    if (NS_FAILED(result)) { return result; }
  }
  
  // add the transation listener
  if (mEditor && mEventListener)
  {
    nsCOMPtr<nsITransactionManager> txMgr;
    result = mEditor->GetTransactionManager(getter_AddRefs(txMgr));
    if (NS_FAILED(result)) return result;
    if (!txMgr) { return NS_ERROR_NULL_POINTER; }

    nsCOMPtr<nsITransactionListener> txnListener = do_QueryInterface(mEventListener);
    if (!txnListener) { return NS_ERROR_NO_INTERFACE; }
    result = txMgr->AddListener(txnListener);
    if (NS_FAILED(result)) { return result; }
  }  
  
  return result;
}

nsresult
nsGfxTextControlFrame::GetFirstFrameWithIID(nsIPresContext *aPresContext, const nsIID& aIID, nsIFrame *aRootFrame, void **
aResultFrame)
{
  if (!aPresContext || !aRootFrame || !aResultFrame)
    return NS_ERROR_NULL_POINTER;

  *aResultFrame = nsnull;

  // Check if the root frame implements the specified interface.

  nsresult result = aRootFrame->QueryInterface(aIID, aResultFrame);

  // Check for any error that may have been thrown.
  // Ignore NS_NOINTERFACE since we want to continue traversal.

  if (NS_FAILED(result) && result != NS_NOINTERFACE)
    return result;

  // Check to see if we are done.

  if (*aResultFrame)
    return NS_OK;

  // We aren't done, so traverse down the root frame's children,
  // calling this method recursively.

  nsIFrame *childFrame = nsnull;

  result = aRootFrame->FirstChild(aPresContext, nsnull, &childFrame);

  if (NS_FAILED(result))
    return result;

  while (childFrame)
  {
    result = GetFirstFrameWithIID(aPresContext, aIID, childFrame, aResultFrame);

    if (NS_FAILED(result))
      return result;

    if (*aResultFrame)
      return NS_OK;

    result = childFrame->GetNextSibling(&childFrame);

    if (NS_FAILED(result))
      return result;
  }

  // No frame was found that implemented the specified
  // interface. Return NULL and NS_OK since no error
  // occured during our traversal.

  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::InitializeTextControl(nsIPresShell *aPresShell, nsIDOMDocument *aDoc)
{
  nsresult result;
  if (!aPresShell || !aDoc) { return NS_ERROR_NULL_POINTER; }

  /* needing a frame here is a hack.  We can't remove this hack until we can
   * set presContext info before style resolution on the docshell
   */
  nsIFrame *frame;
  result = aPresShell->GetRootFrame(&frame);
  if (NS_FAILED(result)) { return result; }
  if (!frame) { return NS_ERROR_NULL_POINTER; }

  PRInt32 type;
  GetType(&type);

  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
  if (!htmlEditor)  { return NS_ERROR_NO_INTERFACE; }
  
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(aPresShell);

  nsCOMPtr<nsIPresContext>presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));
  NS_ASSERTION(presContext, "null presentation context");
  if (!presContext) { return NS_ERROR_NULL_POINTER; }

  /* set all style that propogates from the text control to its content
   * into the presContext for the docshell.
   * what I would prefer to do is hand the docshell my own 
   * pres context at creation, rather than having it create its own.
   */

  const nsFont * font = nsnull;
  nsresult res = GetFont(presContext, font);
  if (NS_SUCCEEDED(res) && font != nsnull) {
    //nsFont defFont = *font;
    presContext->SetDefaultFont(*font);
    presContext->SetDefaultFixedFont(*font);
  }

  const nsStyleFont* controlFont;
  GetStyleData(eStyleStruct_Font,  (const nsStyleStruct *&)controlFont);


  const nsStyleColor* controlColor;
  GetStyleData(eStyleStruct_Color,  (const nsStyleStruct *&)controlColor);
  presContext->SetDefaultColor(controlColor->mColor);
  presContext->SetDefaultBackgroundColor(controlColor->mBackgroundColor);
  presContext->SetDefaultBackgroundImageRepeat(controlColor->mBackgroundRepeat);
  presContext->SetDefaultBackgroundImageAttachment(controlColor->mBackgroundAttachment);
  presContext->SetDefaultBackgroundImageOffset(controlColor->mBackgroundXPosition, controlColor->mBackgroundYPosition);
  presContext->SetDefaultBackgroundImage(controlColor->mBackgroundImage);

  /* HACK:
   * since I don't yet have a hook for setting info on the pres context before style is
   * resolved, I need to call remap style on the root frame's style context.
   * The above code for setting presContext data should happen on a presContext that
   * I create and pass into the docshell, rather than having the docshell create its own
   */
  nsCOMPtr<nsIStyleContext> sc;
  result = frame->GetStyleContext(getter_AddRefs(sc));
  if (NS_FAILED(result)) { return result; }
  if (nsnull==sc) { return NS_ERROR_NULL_POINTER; }
  sc->RemapStyle(presContext);

  if (IsSingleLineTextControl())
  {
    // We need to tweak the overflow property on the scrollframe to tell it
    // that we want to hide the scrollbars! Note that this has to be done
    // after the RemapStyle() above, otherwise our tweak will be blown away.

    nsIScrollableFrame *scrollFrame = nsnull;

    result = GetFirstFrameWithIID(presContext, NS_GET_IID(nsIScrollableFrame), frame, (void **)&scrollFrame);

    if (NS_FAILED(result))
      return result;

    if (scrollFrame)
    {
      nsIFrame *sFrame = nsnull;

      result = scrollFrame->QueryInterface(kIFrameIID, (void **)&sFrame);

      if (NS_FAILED(result) && result != NS_NOINTERFACE)
        return result;

      if (sFrame)
      {
        nsCOMPtr<nsIStyleContext> scrollFrameStyleContext;

        result = sFrame->GetStyleContext(getter_AddRefs(scrollFrameStyleContext));

        if (NS_FAILED(result))
          return result;

        if (scrollFrameStyleContext)
        {
          nsStyleDisplay* display = (nsStyleDisplay*)scrollFrameStyleContext->GetMutableStyleData(eStyleStruct_Display);

          if (display)
            display->mOverflow = NS_STYLE_OVERFLOW_SCROLLBARS_NONE;
        }
      }
    }
  }

  // end HACK

  // now that the style context is initialized, initialize the content
  nsAutoString value;
  if (mCachedState) {
    value.Assign(*mCachedState);
    delete mCachedState;
    mCachedState = nsnull;
  } else {
    GetText(&value, PR_TRUE);
  }
  mEditor->EnableUndo(PR_FALSE);

  PRInt32 maxLength;
  result = GetMaxLength(&maxLength);
  if (NS_CONTENT_ATTR_NOT_THERE != result) {
    htmlEditor->SetMaxTextLength(maxLength);
  }

  nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(mEditor);
  if (mailEditor)
  {
    PRBool wrapToContainerWidth = PR_TRUE;
    if (PR_TRUE==IsSingleLineTextControl())
    { // no wrapping for single line text controls
      result = mailEditor->SetBodyWrapWidth(-1);
      wrapToContainerWidth = PR_FALSE;
    }
    else
    { // if WRAP="OFF", turn wrapping off in the editor
      nsFormControlHelper::nsHTMLTextWrap wrapProp;
      result = nsFormControlHelper::GetWrapPropertyEnum(mContent, wrapProp);
      if (NS_CONTENT_ATTR_NOT_THERE != result) 
      {
        if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Off)
        {
          result = mailEditor->SetBodyWrapWidth(-1);
          wrapToContainerWidth = PR_FALSE;
        }
        if (wrapProp == nsFormControlHelper::eHTMLTextWrap_Hard)
        {
          PRInt32 widthInCharacters = GetWidthInCharacters();
          result = mailEditor->SetBodyWrapWidth(widthInCharacters);
          wrapToContainerWidth = PR_FALSE;
        }
      } else {
        result = mailEditor->SetBodyWrapWidth(-1);
        wrapToContainerWidth = PR_FALSE;
      }
    }
    if (PR_TRUE==wrapToContainerWidth)
    { // if we didn't set wrapping explicitly, turn on default wrapping here
      result = mailEditor->SetBodyWrapWidth(0);
    }
    NS_ASSERTION((NS_SUCCEEDED(result)), "error setting body wrap width");
    if (NS_FAILED(result)) { return result; }
  }

  nsCOMPtr<nsIEditor>editor = do_QueryInterface(mEditor);
  NS_ASSERTION(editor, "bad QI to nsIEditor from mEditor");
  if (editor)
  {
    nsCOMPtr<nsISelection>selection;
    result = editor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) { return result; }
    if (!selection) { return NS_ERROR_NULL_POINTER; }
    nsCOMPtr<nsIDOMNode>bodyNode;
    result = GetFirstNodeOfType(NS_ConvertASCIItoUCS2("body"), aDoc,
                                getter_AddRefs(bodyNode));
    if (NS_SUCCEEDED(result) && bodyNode)
    {
      result = mEditor->SelectAll();
      if (NS_SUCCEEDED(result))
      {
        if (0!=value.Length())
        {
          result = htmlEditor->InsertText(value);
          if (NS_FAILED(result)) { return result; }
          // collapse selection to beginning of text
          nsCOMPtr<nsIDOMNode>firstChild;
          result = bodyNode->GetFirstChild(getter_AddRefs(firstChild));
          if (NS_FAILED(result)) { return result; }
          selection->Collapse(firstChild, 0);
        }
      }
    }
  }

  // finish initializing editor
  // turn in undo, *except* for password fields.
  if (!IsPasswordTextControl())
    mEditor->EnableUndo(PR_TRUE);

  // set readonly and disabled states
  if (mContent)
  {
    PRUint32 flags=0;
    if (IsPlainTextControl()) {
      flags |= nsIHTMLEditor::eEditorPlaintextMask;
    }
    if (IsSingleLineTextControl()) {
      flags |= nsIHTMLEditor::eEditorSingleLineMask;
    }
    if (IsPasswordTextControl()) {
      flags |= nsIHTMLEditor::eEditorPasswordMask;
    }
    nsCOMPtr<nsIContent> content;
    result = mContent->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(content));
    if (NS_SUCCEEDED(result) && content)
    {
      PRInt32 nameSpaceID;
      content->GetNameSpaceID(nameSpaceID);
      nsAutoString resultValue;
      result = content->GetAttribute(nameSpaceID, nsHTMLAtoms::readonly, resultValue);
      if (NS_CONTENT_ATTR_NOT_THERE != result) {
        flags |= nsIHTMLEditor::eEditorReadonlyMask;
        if (selCon)
          selCon->SetCaretEnabled(PR_FALSE);
      }
      result = content->GetAttribute(nameSpaceID, nsHTMLAtoms::disabled, resultValue);
      if (NS_CONTENT_ATTR_NOT_THERE != result) 
      {
        flags |= nsIHTMLEditor::eEditorDisabledMask;
        if (selCon)
        {
          selCon->SetCaretEnabled(PR_FALSE);
          selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
        }
      }
    }
    mEditor->SetFlags(flags);

    // turn on oninput notifications now that I'm done manipulating the editable content
    mNotifyOnInput = PR_TRUE;
  }
  return result;
}

// this is where we propogate a content changed event
NS_IMETHODIMP
nsGfxTextControlFrame::InternalContentChanged()
{
  NS_PRECONDITION(mContent, "illegal to call unless we map to a content node");

  if (!mContent) { return NS_ERROR_NULL_POINTER; }

  if (PR_FALSE==mNotifyOnInput) { 
    return NS_OK; // if notification is turned off, just return ok
  } 

  // Dispatch the change event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent theEvent;
  theEvent.eventStructType = NS_GUI_EVENT;
  theEvent.widget = nsnull;
  theEvent.message = NS_FORM_INPUT;
  theEvent.flags = NS_EVENT_FLAG_INIT;

  // Have the content handle the event, propogating it according to normal DOM rules.
  return mContent->HandleDOMEvent(mFramePresContext, &theEvent, nsnull, NS_EVENT_FLAG_INIT, &status); 
}

nsresult nsGfxTextControlFrame::UpdateTextControlCommands(const nsString& aCommand)
{
  nsresult rv = NS_OK;
  
  if (mEditor)
  {  
    if (aCommand == NS_ConvertASCIItoUCS2("select"))   // optimize select updates
    {
      nsCOMPtr<nsISelection> domSelection;
      rv = mEditor->GetSelection(getter_AddRefs(domSelection));
      if (NS_FAILED(rv)) return rv;
      if (!domSelection) return NS_ERROR_UNEXPECTED;
    
      PRBool selectionCollapsed;
      domSelection->GetIsCollapsed(&selectionCollapsed);
      if (mGotSelectionState && mSelectionWasCollapsed == selectionCollapsed)
      {
        return NS_OK;   // no update necessary
      }
      else
      {
        mGotSelectionState = PR_TRUE;
        mSelectionWasCollapsed = selectionCollapsed;
      }
    }
  }

  nsCOMPtr<nsIContent> content;
  rv = GetContent(getter_AddRefs(content));
  if (NS_FAILED(rv)) return rv;
  if (!content) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDocument> doc;
  rv = content->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv)) return rv;
  if (!doc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
  rv = doc->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));
  if (NS_FAILED(rv)) return rv;
  if (!scriptGlobalObject) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(scriptGlobalObject);
  if (!domWindow) return NS_ERROR_FAILURE;

  return domWindow->UpdateCommands(aCommand);
}

void nsGfxTextControlFrame::RemoveNewlines(nsString &aString)
{
  // strip CR/LF and null
  static const char badChars[] = {10, 13, 0};
  aString.StripChars(badChars);
}

NS_IMETHODIMP
nsGfxTextControlFrame::GetAdditionalChildListName(PRInt32 aIndex,
                                                  nsIAtom **aListName) const
{
  if (aIndex == 0) {
    *aListName = nsLayoutAtoms::editorDisplayList;
    NS_IF_ADDREF(*aListName);
    return NS_OK;
  }

  return nsLeafFrame::GetAdditionalChildListName(aIndex, aListName);
}

NS_IMETHODIMP
nsGfxTextControlFrame::FirstChild(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame**      aFirstChild) const
{
  if (nsLayoutAtoms::editorDisplayList) {
    *aFirstChild = mDisplayFrame;
    return NS_OK;
  }
  
  return nsLeafFrame::FirstChild(aPresContext, aListName, aFirstChild);
}

NS_IMETHODIMP
nsGfxTextControlFrame::Destroy(nsIPresContext* aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);

  if (mDisplayFrame) {
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, mDisplayFrame, nsnull);
    mDisplayFrame->Destroy(aPresContext);
    mDisplayFrame=nsnull;
  }
  return nsLeafFrame::Destroy(aPresContext);
}
//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsGfxTextControlFrame::GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType)
{
  *aStateType = nsIStatefulFrame::eTextType;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxTextControlFrame::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  // Construct a pres state.
  NS_NewPresState(aState); // The addref happens here.
  
  nsString theString;
  nsresult res = GetProperty(nsHTMLAtoms::value, theString);
  if (NS_FAILED(res))
    return res;
    
  res = nsLinebreakConverter::ConvertStringLineBreaks(theString,
           nsLinebreakConverter::eLinebreakPlatform, nsLinebreakConverter::eLinebreakContent);
  NS_ASSERTION(NS_SUCCEEDED(res), "Converting linebreaks failed!");  
  
  (*aState)->SetStateProperty(NS_ConvertASCIItoUCS2("value"), theString);
  return res;
}

NS_IMETHODIMP
nsGfxTextControlFrame::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  nsAutoString stateString;
  aState->GetStateProperty(NS_ConvertASCIItoUCS2("value"), stateString);
  nsresult res = SetProperty(aPresContext, nsHTMLAtoms::value, stateString);
  return res;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsGfxTextControlFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
  nsIView* view;
  GetView(aPresContext, &view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fputs("<\n", out);

  // Dump out frames contained in interior web-shell
  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> viewer;
    mDocShell->GetContentViewer(getter_AddRefs(viewer));
    if (viewer) {
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(viewer));
      if (docv) {
        nsCOMPtr<nsIPresContext> cx;
        docv->GetPresContext(*getter_AddRefs(cx));
        if (cx) {
          nsCOMPtr<nsIPresShell> shell;
          cx->GetShell(getter_AddRefs(shell));
          if (shell) {
            nsIFrame* rootFrame;
            shell->GetRootFrame(&rootFrame);
            if (rootFrame) {
              nsIFrameDebug*  frameDebug;

              if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
                frameDebug->List(aPresContext, out, aIndent + 1);
              }
              nsCOMPtr<nsIDocument> doc;
              docv->GetDocument(*getter_AddRefs(doc));
              if (doc) {
                nsCOMPtr<nsIContent> rootContent(getter_AddRefs(doc->GetRootContent()));
                if (rootContent) {
                  rootContent->List(out, aIndent + 1);
                }
              }
            }
          }
        }
      }
    }
  }

  IndentBy(out, aIndent);
  fputs(">\n", out);

  if (mDisplayFrame) {
    nsIFrameDebug*  frameDebug;
    
    IndentBy(out, aIndent);
    nsAutoString tmp;
    fputs("<\n", out);
    if (NS_SUCCEEDED(mDisplayFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->List(aPresContext, out, aIndent + 1);
    }
    IndentBy(out, aIndent);
    fputs(">\n", out);
  }

  return NS_OK;
}
#endif

/*******************************************************************************
 * EnderFrameLoadingInfo
 ******************************************************************************/
class EnderFrameLoadingInfo : public nsISupports
{
public:
  EnderFrameLoadingInfo(const nsSize& aSize);

  // nsISupports interface...
  NS_DECL_ISUPPORTS

protected:
  virtual ~EnderFrameLoadingInfo() {}

public:
  nsSize mFrameSize;
};

EnderFrameLoadingInfo::EnderFrameLoadingInfo(const nsSize& aSize)
{
  NS_INIT_REFCNT();

  mFrameSize = aSize;
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS(EnderFrameLoadingInfo,kISupportsIID);


#ifdef XP_MAC
#pragma mark -
#endif

/*******************************************************************************
 * nsEnderDocumentObserver
 ******************************************************************************/
NS_IMPL_ADDREF(nsEnderDocumentObserver);
NS_IMPL_RELEASE(nsEnderDocumentObserver);

nsEnderDocumentObserver::nsEnderDocumentObserver()
{ 
  NS_INIT_REFCNT(); 
}

nsEnderDocumentObserver::~nsEnderDocumentObserver() 
{
}

nsresult
nsEnderDocumentObserver::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    *aInstancePtr = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIDocumentObserver*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



NS_IMETHODIMP 
nsEnderDocumentObserver::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;
  return NS_OK;
}

NS_IMETHODIMP nsEnderDocumentObserver::BeginUpdate(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::EndUpdate(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::BeginLoad(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::EndLoad(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK; 
}

NS_IMETHODIMP nsEnderDocumentObserver::EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{ return NS_OK; }

NS_IMETHODIMP nsEnderDocumentObserver::ContentChanged(nsIDocument *aDocument,
                                                      nsIContent* aContent,
                                                      nsISupports* aSubContent)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentStatesChanged(nsIDocument* aDocument,
                                                            nsIContent* aContent1,
                                                            nsIContent* aContent2)
{ return NS_OK; }

NS_IMETHODIMP nsEnderDocumentObserver::AttributeChanged(nsIDocument *aDocument,
                                                        nsIContent*  aContent,
                                                        PRInt32      aNameSpaceID,
                                                        nsIAtom*     aAttribute,
                                                        PRInt32      aHint)
{ return NS_OK; }

NS_IMETHODIMP nsEnderDocumentObserver::ContentAppended(nsIDocument *aDocument,
                                                       nsIContent* aContainer,
                                                       PRInt32     aNewIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentInserted(nsIDocument *aDocument,
                                                       nsIContent* aContainer,
                                                       nsIContent* aChild,
                                                       PRInt32 aIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentReplaced(nsIDocument *aDocument,
                                                       nsIContent* aContainer,
                                                       nsIContent* aOldChild,
                                                       nsIContent* aNewChild,
                                                       PRInt32 aIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::ContentRemoved(nsIDocument *aDocument,
                                                      nsIContent* aContainer,
                                                      nsIContent* aChild,
                                                      PRInt32 aIndexInContainer)
{
  nsresult result = NS_OK;
  if (mFrame) {
    result = mFrame->InternalContentChanged();
  }
  return result;
}

NS_IMETHODIMP nsEnderDocumentObserver::StyleSheetAdded(nsIDocument *aDocument,
                                                       nsIStyleSheet* aStyleSheet)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleSheetRemoved(nsIDocument *aDocument,
                                                         nsIStyleSheet* aStyleSheet)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                                                      nsIStyleSheet* aStyleSheet,
                                                                      PRBool aDisabled)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleRuleChanged(nsIDocument *aDocument,
                                                        nsIStyleSheet* aStyleSheet,
                                                        nsIStyleRule* aStyleRule,
                                                        PRInt32 aHint)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleRuleAdded(nsIDocument *aDocument,
                                                      nsIStyleSheet* aStyleSheet,
                                                      nsIStyleRule* aStyleRule)
{ return NS_OK; }
NS_IMETHODIMP nsEnderDocumentObserver::StyleRuleRemoved(nsIDocument *aDocument,
                                                        nsIStyleSheet* aStyleSheet,
                                                        nsIStyleRule* aStyleRule)
{ return NS_OK; }

NS_IMETHODIMP nsEnderDocumentObserver::DocumentWillBeDestroyed(nsIDocument *aDocument)
{ 
  return NS_OK; 
}

#ifdef XP_MAC
#pragma mark -
#endif


/*******************************************************************************
 * nsEnderEventListener
 ******************************************************************************/

nsresult 
NS_NewEnderEventListener(nsIEnderEventListener ** aInstancePtr)
{
  nsEnderEventListener* it = new nsEnderEventListener();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIEnderEventListener), (void **) aInstancePtr);   
}

NS_IMPL_ADDREF(nsEnderEventListener)

NS_IMPL_RELEASE(nsEnderEventListener)


nsEnderEventListener::nsEnderEventListener()
: mView(nsnull)
, mSkipFocusDispatch(PR_FALSE)    // needed for when mouse down set focus on native widgets
, mFirstDoOfFirstUndo(PR_TRUE)
{
  NS_INIT_REFCNT();
}

nsEnderEventListener::~nsEnderEventListener()
{
  // all refcounted objects are held as nsCOMPtrs, clear themselves
}

NS_IMETHODIMP
nsEnderEventListener::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame.SetReference(aFrame->WeakReferent());
  if (aFrame)
  {
    aFrame->GetContent(getter_AddRefs(mContent));
  }
  return NS_OK;
}

nsresult
nsEnderEventListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMKeyListener *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);  
  if (aIID.Equals(kIDOMEventListenerIID)) {
    nsIDOMKeyListener *kl = (nsIDOMKeyListener*)this;
    nsIDOMEventListener *temp = kl;
    *aInstancePtr = (void*)temp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  if (aIID.Equals(NS_GET_IID(nsIDOMKeyListener))) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener))) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMFocusListener))) {
    *aInstancePtr = (void*)(nsIDOMFocusListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISelectionListener))) {
    *aInstancePtr = (void*)(nsISelectionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsITransactionListener))) {
    *aInstancePtr = (void*)(nsITransactionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIEnderEventListener))) {
    *aInstancePtr = (void*)(nsIEnderEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsresult
nsEnderEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

/*================== nsIKeyListener =========================*/

nsresult
nsEnderEventListener::DispatchKeyEvent(nsIDOMKeyEvent* aOrigEvent,
                                       PRInt32 aEventType)
{
  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (!gfxFrame || !mContent || !mContext || !mView)
    return NS_ERROR_NOT_INITIALIZED;

  nsKeyEvent newEvent;
  newEvent.message = aEventType;
  newEvent.eventStructType = NS_KEY_EVENT;
  newEvent.widget = nsnull;
  newEvent.flags = NS_EVENT_FLAG_INIT;
  aOrigEvent->GetKeyCode(&(newEvent.keyCode));
  if (newEvent.message == NS_KEY_PRESS)
    aOrigEvent->GetCharCode(&(newEvent.charCode));
  else
    newEvent.charCode = 0;
  aOrigEvent->GetShiftKey(&(newEvent.isShift));
  aOrigEvent->GetCtrlKey(&(newEvent.isControl));
  aOrigEvent->GetAltKey(&(newEvent.isAlt));
  aOrigEvent->GetMetaKey(&(newEvent.isMeta));

  nsCOMPtr<nsIEventStateManager> manager;
  nsresult result = mContext->GetEventStateManager(getter_AddRefs(manager));
  if (NS_FAILED(result))
    return result;
  if (!manager)
    return NS_ERROR_UNEXPECTED;

  nsEventStatus status = nsEventStatus_eIgnore;

#ifndef SIMPLIFIED_EVENTS_FOR_KEY_UP_AND_DOWN
  // 1. Give event to event manager for pre event state changes
  //    and generation of synthetic events.
  result = manager->PreHandleEvent(mContext, &newEvent, gfxFrame, &status, mView);

  // 2. Give event to the DOM for third party and JS use.
  if (NS_SUCCEEDED(result)) {
    result = mContent->HandleDOMEvent(mContext, &newEvent, nsnull,
                                      NS_EVENT_FLAG_INIT, &status); 
  }

  // 3. Give event to the frame for browser default processing
  gfxFrame = mFrame.Reference(); // check for deletion
  if (gfxFrame && NS_SUCCEEDED(result)) {
    result = gfxFrame->HandleEvent(mContext, &newEvent, &status);
  }

  // 4. Give event to event manager for post event state changes and
  //    generation of synthetic events.
  gfxFrame = mFrame.Reference(); // check for deletion
  if (gfxFrame && NS_SUCCEEDED(result)) {
    result = manager->PostHandleEvent(mContext, &newEvent, gfxFrame,
                                      &status, mView);
                                       
  }

  // Call consume focus events on the inner event state manager to prevent its
  // PostHandleEvent from immediately blurring us.
  nsCOMPtr<nsIPresContext> pc;
  mInnerShell->GetPresContext(getter_AddRefs(pc));

  nsCOMPtr<nsIEventStateManager> esm;
  pc->GetEventStateManager(getter_AddRefs(esm));
  esm->ConsumeFocusEvents(PR_TRUE);

  if (newEvent.flags & NS_EVENT_FLAG_STOP_DISPATCH && aOrigEvent) {
    aOrigEvent->PreventCapture();
    aOrigEvent->PreventBubble();
  }

  // Used to do this simplified version for KeyUp and KeyDown events.
  // This seems bad ... we shouldn't try to second-guess what types
  // of events the user might want to track.
#else /* SIMPLIFIED_EVENTS_FOR_KEY_UP_AND_DOWN */
  // 1. Give event to event manager for pre event state changes
  //    and generation of synthetic events.
  result = aManager->PreHandleEvent(mContext, &newEvent, gfxFrame, &status, mView);

  // 2. Give event to the DOM for third party and JS use.
  if (NS_SUCCEEDED(result)) {
    result = mContent->HandleDOMEvent(mContext, &newEvent, nsnull,
                                      NS_EVENT_FLAG_INIT, &status); 
  }

  // 3. In this case, the frame does no processing of the event

  // 4. Give event to event manager for post event state changes and
  //    generation of synthetic events.
  gfxFrame = mFrame.Reference(); // check for deletion
  if (gfxFrame && NS_SUCCEEDED(result)) {
    result = aManager->PostHandleEvent(mContext, &newEvent, gfxFrame, &status, mView);
  }
#endif /* SIMPLIFIED_EVENTS_FOR_KEY_UP_AND_DOWN */

  return result;
}

nsresult
nsEnderEventListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent>keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) { //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  return DispatchKeyEvent(keyEvent, NS_KEY_DOWN);
}

nsresult
nsEnderEventListener::KeyUp(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent>keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) { //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  return DispatchKeyEvent(keyEvent, NS_KEY_UP);
}

nsresult
nsEnderEventListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent>keyEvent;
  keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) { //non-key event passed to keydown.  bad things.
    return NS_OK;
  }

  return DispatchKeyEvent(keyEvent, NS_KEY_PRESS);
}

/*=============== nsIMouseListener ======================*/

nsresult
nsEnderEventListener::DispatchMouseEvent(nsIDOMMouseEvent *aEvent, PRInt32 aEventType)
{
  nsresult result = NS_OK;

  // The ESM has already dispatched the "click" event to DOM
  // so we need to ignore it here or we will get double notifications
  if (aEventType == NS_MOUSE_LEFT_CLICK ||
      aEventType == NS_MOUSE_MIDDLE_CLICK ||
      aEventType == NS_MOUSE_RIGHT_CLICK) {
    return result;
  }

  if (aEvent)
  {
    nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
    if (gfxFrame)
    {
      // Dispatch the event
      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event;
      event.eventStructType = NS_MOUSE_EVENT;
      event.widget = nsnull;
      event.flags = NS_EVENT_FLAG_INIT;
      aEvent->GetShiftKey(&(event.isShift));
      aEvent->GetCtrlKey(&(event.isControl));
      aEvent->GetAltKey(&(event.isAlt));
      aEvent->GetMetaKey(&(event.isMeta));
      aEvent->GetClientX(&(event.refPoint.x));
      aEvent->GetClientY(&(event.refPoint.y));
      aEvent->GetScreenX(&(event.point.x));
      aEvent->GetScreenY(&(event.point.y));

      PRInt32 clickCount;
      aEvent->GetDetail(&clickCount);
      event.clickCount = clickCount;
      event.message = aEventType;
      GetWidgetForView(mView, event.widget);
      NS_ASSERTION(event.widget, "null widget in mouse event redispatch.  right mouse click will crash!");

      nsIEventStateManager *manager=nsnull;
      result = mContext->GetEventStateManager(&manager);
      if (NS_SUCCEEDED(result) && manager) 
      {
        //1. Give event to event manager for pre event state changes and generation of synthetic events.
        result = manager->PreHandleEvent(mContext, &event, gfxFrame, &status, mView);

        //2. Give event to the DOM for third party and JS use.
        if (NS_SUCCEEDED(result)) {
          result = mContent->HandleDOMEvent(mContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
        }
    
        //3. Give event to the frame for browser default processing
        if (NS_SUCCEEDED(result)) {
          result = gfxFrame->HandleEvent(mContext, &event, &status);
        }

        //4. Give event to event manager for post event state changes and generation of synthetic events.
        gfxFrame = mFrame.Reference(); // check for deletion
        if (gfxFrame && NS_SUCCEEDED(result)) {
          result = manager->PostHandleEvent(mContext, &event, gfxFrame, &status, mView);
        }
        NS_IF_RELEASE(manager);

        if ((aEventType == NS_MOUSE_LEFT_BUTTON_DOWN) ||
            (aEventType == NS_MOUSE_MIDDLE_BUTTON_DOWN) ||
            (aEventType == NS_MOUSE_RIGHT_BUTTON_DOWN))
        {
          // Call consume focus events on the inner event state manager to prevent its
          // PostHandleEvent from immediately blurring us.
          nsCOMPtr<nsIPresContext> pc;
          mInnerShell->GetPresContext(getter_AddRefs(pc));

          nsCOMPtr<nsIEventStateManager> esm;
          pc->GetEventStateManager(getter_AddRefs(esm));
          esm->ConsumeFocusEvents(PR_TRUE);
          gfxFrame = mFrame.Reference(); // check for deletion
          if (gfxFrame) {
            gfxFrame->SetFocus();
          }
        }
      }
      NS_IF_RELEASE(event.widget);
    }
  }
  return result;
}

// This makes sure the the mSkipFocusDispatch gets set to false
class SemiphoreFocusDispatch {
  PRBool * mBool;
public:
  SemiphoreFocusDispatch(PRBool * aBool) :mBool(NULL) { if (aBool) {mBool = aBool; *mBool = PR_TRUE;} }
  ~SemiphoreFocusDispatch()              { if (mBool) *mBool = PR_FALSE; }
};
  
nsresult
nsEnderEventListener::MouseDown(nsIDOMEvent* aEvent)
{
  SemiphoreFocusDispatch skipFocusDispatch(&mSkipFocusDispatch);
  mSkipFocusDispatch = PR_TRUE;

  nsCOMPtr<nsIDOMMouseEvent>mouseEvent;
  mouseEvent = do_QueryInterface(aEvent);
  if (!mouseEvent) { //non-key event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    PRUint16 button;
    mouseEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1: //XXX: I can't believe there isn't a symbol for this!
        eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
        break;
      case 2: //XXX: I can't believe there isn't a symbol for this!
        eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
        // XXX See bug 23336: the ender event listener is superfluous,
        // causing double mouse click events to get through.
        // This is a temporary way of disabling it in the middle-click case.
        // Arguably, the whole event listener should be disabled,
        // or at least rewritten; we'll look into that post-M14.
        return NS_OK;
      case 3: //XXX: I can't believe there isn't a symbol for this!
        // XXX See bug 27827 -- we need to disable right clicks
        // just like we disabled middle clicks.
        eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
        return NS_OK;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(mouseEvent, eventType);
  }  
  mSkipFocusDispatch = PR_FALSE;
  return result;
}

nsresult
nsEnderEventListener::MouseUp(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMMouseEvent>mouseEvent;
  mouseEvent = do_QueryInterface(aEvent);
  if (!mouseEvent) { //non-mouse event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    PRUint16 button;
    mouseEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1:
        eventType = NS_MOUSE_LEFT_BUTTON_UP;
        break;
      case 2:
        eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
        break;
      case 3:
        eventType = NS_MOUSE_RIGHT_BUTTON_UP;
        break;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(mouseEvent, eventType);
  }
  return result;
}

nsresult
nsEnderEventListener::MouseClick(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMMouseEvent>mouseEvent;
  mouseEvent = do_QueryInterface(aEvent);
  if (!mouseEvent) { //non-mouse event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    // Dispatch the event
    PRUint16 button;
    mouseEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1:
        eventType = NS_MOUSE_LEFT_CLICK;
        break;
      case 2:
        eventType = NS_MOUSE_MIDDLE_CLICK;
        break;
      case 3:
        eventType = NS_MOUSE_RIGHT_CLICK;
        break;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(mouseEvent, eventType);
  }
  return result;
}

nsresult
nsEnderEventListener::MouseDblClick(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMMouseEvent>mouseEvent;
  mouseEvent = do_QueryInterface(aEvent);
  if (!mouseEvent) { //non-mouse event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    // Dispatch the event
    PRUint16 button;
    mouseEvent->GetButton(&button);
    PRInt32 eventType;
    switch(button)
    {
      case 1:
        eventType = NS_MOUSE_LEFT_DOUBLECLICK;
        break;
      case 2:
        eventType = NS_MOUSE_MIDDLE_DOUBLECLICK;
        break;
      case 3:
        eventType = NS_MOUSE_RIGHT_DOUBLECLICK;
        break;
      default:
        NS_ASSERTION(0, "bad button type");
        return NS_OK;
    }
    result = DispatchMouseEvent(mouseEvent, eventType);
  }
  
  return result;
}

nsresult
nsEnderEventListener::MouseOver(nsIDOMEvent* aEvent)
{
  /*
  nsCOMPtr<nsIDOMMouseEvent>mouseEvent;
  mouseEvent = do_QueryInterface(aEvent);
  if (!mouseEvent) { //non-mouse event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    // XXX: Need to synthesize MouseEnter here
    result = DispatchMouseEvent(mouseEvent, NS_MOUSE_MOVE);
  }
  
  return result;
  */
  return NS_OK;
}

nsresult
nsEnderEventListener::MouseOut(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMMouseEvent>mouseEvent;
  mouseEvent = do_QueryInterface(aEvent);
  if (!mouseEvent) { //non-mouse event passed in.  bad things.
    return NS_OK;
  }

  nsresult result = NS_OK;

  if (mContent && mContext && mView)
  {
    result = DispatchMouseEvent(mouseEvent, NS_MOUSE_EXIT);
  }
  
  return result;
}

/*=============== nsIFocusListener ======================*/

nsresult
nsEnderEventListener::Focus(nsIDOMEvent* aEvent)
{
  //Need to set text value for onchange
  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();

  if (gfxFrame && mContent && mView) {
    mTextValue.SetLength(0);
    gfxFrame->GetText(&mTextValue, PR_FALSE);
  }

  /*
  // In this case, the focus has all ready been set because of the mouse down
  // and setting it on the native widget causes an event to be dispatched
  // and this listener then gets call. So we want to skip it here
  // this is probably NOt need when native widgets are removed
  if (mSkipFocusDispatch == PR_TRUE) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aEvent);
  if (!uiEvent) { 
    return NS_OK;
  }

  nsresult result = NS_OK;

  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();

  // Here are notified that we received focus, this event
  // may come as a result of JS or of the native Widget getting
  // the focus set on it
  // Check here to see if the GfxText was able to set focus, if it did
  // the we want to skip this notification
  // If this came from the DOM (and not the widget) then we DO 
  // want to do the notification
  if (gfxFrame && mContent && mView && !gfxFrame->DidSetFocus())
  {
    mTextValue = "";
    gfxFrame->GetText(&mTextValue, PR_FALSE);
    // Dispatch the event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.widget = nsnull;
    event.message = NS_FOCUS_CONTENT;
    event.flags = NS_EVENT_FLAG_INIT;
    event.isShift = false;
    event.isControl = false;
    event.isAlt = false;
    event.isMeta = false;

    nsIEventStateManager *manager=nsnull;
    result = mContext->GetEventStateManager(&manager);
    if (NS_SUCCEEDED(result) && manager) 
    {
      //1. Give event to event manager for pre event state changes and generation of synthetic events.
      result = manager->PreHandleEvent(mContext, &event, gfxFrame, &status, mView);

      //2. Give event to the DOM for third party and JS use.
      if (NS_SUCCEEDED(result)) {
        result = mContent->HandleDOMEvent(mContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
      }
    
      //3. In this case, the frame does no processing of the event

      //4. Give event to event manager for post event state changes and generation of synthetic events.
      gfxFrame = mFrame.Reference(); // check for deletion
      if (gfxFrame && NS_SUCCEEDED(result)) {
        result = manager->PostHandleEvent(mContext, &event, gfxFrame, &status, mView);
      }
      NS_RELEASE(manager);
    }
  }
*/
  return NS_OK;
}

nsresult
nsEnderEventListener::Blur(nsIDOMEvent* aEvent)
{
  // XXX We don't use aEvent, shouldn't matter what it is.
  // In addition, EnterPressed calls Blur and passes null (Bug 18501)
  // So, if we get a null, skip the QI for the time being.
  if (aEvent) {
    nsCOMPtr<nsIDOMUIEvent>uiEvent;
    uiEvent = do_QueryInterface(aEvent);
    if (!uiEvent) {
      return NS_OK;
    }
  }

  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();

  if (gfxFrame && mContent && mView) {
    nsString currentValue;
    gfxFrame->GetText(&currentValue, PR_FALSE);
    if (PR_FALSE==currentValue.Equals(mTextValue)) {
      // Dispatch th1e change event
      nsEventStatus status = nsEventStatus_eIgnore;
      nsInputEvent event;
      event.eventStructType = NS_INPUT_EVENT;
      event.widget = nsnull;
      event.message = NS_FORM_CHANGE;
      event.flags = NS_EVENT_FLAG_INIT;
      event.isShift = PR_FALSE;
      event.isControl = PR_FALSE;
      event.isAlt = PR_FALSE;
      event.isMeta = PR_FALSE;

      // Have the content handle the event.
      mContent->HandleDOMEvent(mContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
    }
  }
/*
    // XXX No longer dispatching event
    // this notification has already taken place
    // this causes two blur notifcations to be sent
    // I removed this 
    // rods - 11/12/99 
    // Dispatch the change event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.widget = nsnull;
    event.message = NS_BLUR_CONTENT;
    event.flags = NS_EVENT_FLAG_INIT;
    event.isShift = false;
    event.isControl = false;
    event.isAlt = false;
    event.isMeta = false;
    
    gfxFrame->SetShouldSetFocus();

    // Have the content handle the event.
    mContent->HandleDOMEvent(mContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 

  }
*/
  return NS_OK;
}

NS_IMETHODIMP
nsEnderEventListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, short)
{
  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame && mContent)
  {
    // Dispatch the event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    event.widget = nsnull;
    event.message = NS_FORM_SELECTED;
    event.flags = NS_EVENT_FLAG_INIT;

    // Have the content handle the event.
    mContent->HandleDOMEvent(mContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status); 
    
    // Now have the frame handle the event
    gfxFrame->HandleEvent(mContext, &event, &status);
  }
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::WillDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::DidDo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aDoResult)
{
  // we only need to update if the undo count is now 1
  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  
  if (gfxFrame)
  {
    PRInt32 undoCount;
    aManager->GetNumberOfUndoItems(&undoCount);
    if (undoCount == 1)
    {
      if (mFirstDoOfFirstUndo)
        gfxFrame->UpdateTextControlCommands(NS_ConvertASCIItoUCS2("undo"));
 
      mFirstDoOfFirstUndo = PR_FALSE;
    }
  }   
    
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::WillUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::DidUndo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, nsresult aUndoResult)
{
  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame)
  {
    PRInt32 undoCount;
    aManager->GetNumberOfUndoItems(&undoCount);
    if (undoCount == 0)
      mFirstDoOfFirstUndo = PR_TRUE;    // reset the state for the next do

    gfxFrame->UpdateTextControlCommands(NS_ConvertASCIItoUCS2("undo"));
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::WillRedo(nsITransactionManager *aManager,
  nsITransaction *aTransaction, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::DidRedo(nsITransactionManager *aManager,  
  nsITransaction *aTransaction, nsresult aRedoResult)
{
  nsGfxTextControlFrame *gfxFrame = mFrame.Reference();
  if (gfxFrame)
  {
    gfxFrame->UpdateTextControlCommands(NS_ConvertASCIItoUCS2("undo"));
  }

  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::WillBeginBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::DidBeginBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::WillEndBatch(nsITransactionManager *aManager, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::DidEndBatch(nsITransactionManager *aManager, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::WillMerge(nsITransactionManager *aManager,
        nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge, PRBool *aInterrupt)
{
  *aInterrupt = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsEnderEventListener::DidMerge(nsITransactionManager *aManager,
  nsITransaction *aTopTransaction, nsITransaction *aTransactionToMerge,
                    PRBool aDidMerge, nsresult aMergeResult)
{
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

/*******************************************************************************
 * nsEnderFocusListenerForDisplayContent
 ******************************************************************************/

NS_IMPL_ADDREF(nsEnderFocusListenerForDisplayContent);
NS_IMPL_RELEASE(nsEnderFocusListenerForDisplayContent);

nsresult
nsEnderFocusListenerForDisplayContent::QueryInterface(const nsIID& aIID,
                                               void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMFocusListener))) {
    *aInstancePtr = (void*) ((nsIDOMFocusListener*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIDOMFocusListener*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsEnderFocusListenerForDisplayContent::nsEnderFocusListenerForDisplayContent() :
  mFrame(nsnull)
{
  NS_INIT_REFCNT();
}

nsEnderFocusListenerForDisplayContent::~nsEnderFocusListenerForDisplayContent()
{
}

nsresult 
nsEnderFocusListenerForDisplayContent::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


nsresult
nsEnderFocusListenerForDisplayContent::Focus(nsIDOMEvent* aEvent)
{
  if (mFrame)
  {
    mFrame->CreateSubDoc(nsnull);
  }
  return NS_OK;
}

nsresult
nsEnderFocusListenerForDisplayContent::Blur (nsIDOMEvent* aEvent)
{
  return NS_OK;
}


NS_IMETHODIMP
nsEnderFocusListenerForDisplayContent::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;  // frames are not ref counted
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif

/*******************************************************************************
 * EnderTempObserver
 ******************************************************************************/
// XXX temp implementation

NS_IMPL_ADDREF(EnderTempObserver);
NS_IMPL_RELEASE(EnderTempObserver);

EnderTempObserver::EnderTempObserver()
{ 
  NS_INIT_REFCNT(); 
  mFirstCall = PR_TRUE;
}

EnderTempObserver::~EnderTempObserver()
{
}

nsresult
EnderTempObserver::QueryInterface(const nsIID& aIID,
                                  void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentLoaderObserverIID)) {
    *aInstancePtr = (void*) ((nsIDocumentLoaderObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIDocumentLoaderObserver*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


// document loader observer implementation
NS_IMETHODIMP
EnderTempObserver::OnStartDocumentLoad(nsIDocumentLoader* loader, 
                                       nsIURI* aURL, 
                                       const char* aCommand)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnEndDocumentLoad(nsIDocumentLoader* loader,
                                     nsIChannel* channel,
                                     nsresult aStatus)
{
  if (PR_TRUE==mFirstCall)
  {
    mFirstCall = PR_FALSE;
    if (mFrame) {
      mFrame->InstallEditor();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnStartURLLoad(nsIDocumentLoader* loader,
                                  nsIChannel* channel)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnProgressURLLoad(nsIDocumentLoader* loader,
                                     nsIChannel* channel,
                                     PRUint32 aProgress, 
                                     PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnStatusURLLoad(nsIDocumentLoader* loader,
                                   nsIChannel* channel,
                                   nsString& aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
EnderTempObserver::OnEndURLLoad(nsIDocumentLoader* loader,
                                nsIChannel* channel,
                                nsresult aStatus)
{
  return NS_OK;
}


NS_IMETHODIMP
EnderTempObserver::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif

/*******************************************************************************
 * nsEnderListenerForContent
 ******************************************************************************/

NS_IMPL_ADDREF(nsEnderListenerForContent);
NS_IMPL_RELEASE(nsEnderListenerForContent);

nsresult
nsEnderListenerForContent::QueryInterface(const nsIID& aIID,
                                              void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener))) {
    *aInstancePtr = (void*) ((nsIDOMMouseListener*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDragListener))) {
    *aInstancePtr = (void*) ((nsIDOMDragListener*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)((nsIDOMFocusListener*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsEnderListenerForContent::nsEnderListenerForContent() :
  mFrame(nsnull)
{
  NS_INIT_REFCNT();
}

nsEnderListenerForContent::~nsEnderListenerForContent()
{
}

nsresult 
nsEnderListenerForContent::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

/* XXX:
   drag events should be forwarded to the editor's own drag event listener?
*/

nsresult
nsEnderListenerForContent::DragGesture(nsIDOMEvent* aDragEvent)
{
  //printf("frame forContent DragGesture\n");
  // ...figure out if a drag should be started...
  
  // ...until we have this implemented, just eat the drag event so it
  // ...doesn't leak out into the rest of the app/handlers.
  aDragEvent->PreventBubble();
  
  return NS_OK;
}


nsresult
nsEnderListenerForContent::DragEnter(nsIDOMEvent* aDragEvent)
{
  //printf("frame forContent DragEnter\n");
  // see nsTextEditorDragListener
  return NS_OK;
}


nsresult
nsEnderListenerForContent::DragOver(nsIDOMEvent* aDragEvent)
{
  //printf("frame forContent DragOver\n");
  // see nsTextEditorDragListener
  return NS_OK;
}


nsresult
nsEnderListenerForContent::DragExit(nsIDOMEvent* aDragEvent)
{
  //printf("frame forContent DragExit\n");
  // see nsTextEditorDragListener
  return NS_OK;
}



nsresult
nsEnderListenerForContent::DragDrop(nsIDOMEvent* aMouseEvent)
{
  //printf("frame forContent DragDrop\n");
  // see nsTextEditorDragListener
  return NS_OK;
}

NS_IMETHODIMP
nsEnderListenerForContent::SetFrame(nsGfxTextControlFrame *aFrame)
{
  mFrame = aFrame;  // frames are not ref counted
  return NS_OK;
}

#endif
