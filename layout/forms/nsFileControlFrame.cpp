/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFileControlFrame.h"
#include "nsFormFrame.h"


#include "nsIElementFactory.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIPresState.h"
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
#include "nsIDocument.h"
#include "nsIDOMMouseListener.h"
#include "nsIPresShell.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIStatefulFrame.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFilePicker.h"
#include "nsIDOMMouseEvent.h"
#include "nsINodeInfo.h"
#include "nsIDOMEventReceiver.h"
#include "nsIScriptGlobalObject.h"

#include "nsContentCID.h"
static NS_DEFINE_CID(kHTMLElementFactoryCID,   NS_HTML_ELEMENT_FACTORY_CID);

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

  // remove ourself as a listener of the button (bug 40533)
  if (mBrowse) {
    nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mBrowse));
    reciever->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMMouseListener));
  }

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

  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsINodeInfoManager> nimgr;
  nsresult rv = doc->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nimgr->GetNodeInfo(nsHTMLAtoms::input, nsnull, kNameSpaceID_None,
                     *getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIElementFactory> ef(do_CreateInstance(kHTMLElementFactoryCID,&rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content;
  rv = ef->CreateInstanceByTag(nodeInfo,getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = content->QueryInterface(NS_GET_IID(nsIHTMLContent),(void**)&mTextContent);

  if (NS_SUCCEEDED(rv)) {
    mTextContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::type, NS_LITERAL_STRING("text"), PR_FALSE);
    if (nsFormFrame::GetDisabled(this)) {
      nsCOMPtr<nsIDOMHTMLInputElement> textControl = do_QueryInterface(mTextContent);
      if (textControl) {
        textControl->SetDisabled(nsFormFrame::GetDisabled(this));
      }
    }
    aChildList.AppendElement(mTextContent);
  }

  // create browse button
  rv = ef->CreateInstanceByTag(nodeInfo,getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  mBrowse = do_QueryInterface(content,&rv);

  if (NS_SUCCEEDED(rv)) {
    mBrowse->SetAttr(kNameSpaceID_None, nsHTMLAtoms::type, NS_LITERAL_STRING("button"), PR_FALSE);
    //browse->SetAttr(kNameSpaceID_None, nsHTMLAtoms::value, nsAutoString("browse..."), PR_FALSE);

    aChildList.AppendElement(mBrowse);

    // register as an event listener of the button to open file dialog on mouse click
    nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mBrowse));
    reciever->AddEventListenerByIID(this, NS_GET_IID(nsIDOMMouseListener));
  }

  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, value)) {
    mTextContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::size, value, PR_FALSE);
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
  } else if (aIID.Equals(NS_GET_IID(nsIAnonymousContentCreator))) {
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  } else  if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener))) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*) this;
    return NS_OK;
  } else  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP 
nsFileControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_INPUT_FILE;
  return NS_OK;
}


void 
nsFileControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  // Fix for Bug 6133 
  if (mTextFrame) {
    nsCOMPtr<nsIContent> content;
    mTextFrame->GetContent(getter_AddRefs(content));
    if (content) {
      content->SetFocus(mPresContext);
    }
  }
}

void
nsFileControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
  }
}

/**
 * This is called when our browse button is clicked
 */
nsresult 
nsFileControlFrame::MouseClick(nsIDOMEvent* aMouseEvent)
{
  // only allow the left button
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (mouseEvent) {
    PRUint16 whichButton;
    if (NS_SUCCEEDED(mouseEvent->GetButton(&whichButton))) {
      if (whichButton != 0) {
        return NS_OK;
      }
    }
  }


  nsresult result;

  // Get parent nsIDOMWindowInternal object.
  nsCOMPtr<nsIContent> content;
  result = GetContent(getter_AddRefs(content));
  if (!content)
    return NS_FAILED(result) ? result : NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  result = content->GetDocument(*getter_AddRefs(doc));
  if (!doc)
    return NS_FAILED(result) ? result : NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
  result = doc->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));
  if (!scriptGlobalObject)
    return NS_FAILED(result) ? result : NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> parentWindow = do_QueryInterface(scriptGlobalObject);
  if (!parentWindow)
    return NS_ERROR_FAILURE;

  // Get Loc title
  nsString title;
  nsFormControlHelper::GetLocalizedString(nsFormControlHelper::GetHTMLPropertiesFileName(),
                                          NS_LITERAL_STRING("FileUpload").get(), title);

  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!filePicker)
    return NS_ERROR_FAILURE;

  result = filePicker->Init(parentWindow, title.get(), nsIFilePicker::modeOpen);
  if (NS_FAILED(result))
    return result;

  // Set filter "All Files"
  filePicker->AppendFilters(nsIFilePicker::filterAll);

  // Set default directry and filename
  nsAutoString defaultName;
  GetProperty(nsHTMLAtoms::value, defaultName);

  nsCOMPtr<nsILocalFile> currentFile = do_CreateInstance("@mozilla.org/file/local;1");
  if (currentFile && !defaultName.IsEmpty()) {
    result = currentFile->InitWithUnicodePath(defaultName.get());
    if (NS_SUCCEEDED(result)) {
      PRUnichar *leafName = nsnull;
      currentFile->GetUnicodeLeafName(&leafName);
      if (leafName) {
        filePicker->SetDefaultString(leafName);
        nsMemory::Free(leafName);
      }

      // set directory
      nsCOMPtr<nsIFile> parentFile;
      currentFile->GetParent(getter_AddRefs(parentFile));
      if (parentFile) {
        nsCOMPtr<nsILocalFile> parentLocalFile = do_QueryInterface(parentFile, &result);
        if (parentLocalFile)
          filePicker->SetDisplayDirectory(parentLocalFile);
      }
    }
  }

  // Open dialog
  PRInt16 mode;
  result = filePicker->Show(&mode);
  if (NS_FAILED(result))
    return result;
  if (mode == nsIFilePicker::returnCancel)
    return NS_OK;

  // Set property
  nsCOMPtr<nsILocalFile> localFile;
  result = filePicker->GetFile(getter_AddRefs(localFile));
  if (localFile) {
    PRUnichar *nativePath = nsnull;
    result = localFile->GetUnicodePath(&nativePath);
    if (nativePath) {
      nsAutoString pathName(nativePath);
      mTextFrame->SetProperty(mPresContext, nsHTMLAtoms::value, pathName);
      nsMemory::Free(nativePath);
      return NS_OK;
    }
  }

  return NS_FAILED(result) ? result : NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsFileControlFrame::Reflow(nsIPresContext*          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFileControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);

  aStatus = NS_FRAME_COMPLETE;

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

  // The Areaframe takes care of all our reflow 
  // except for when style is used to change its size.
  nsresult rv = nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(rv) && mTextFrame != nsnull) {
    nsIFrame * child;
    FirstChild(aPresContext, nsnull, &child);
    while (child == mTextFrame) {
      child->GetNextSibling(&child);
    }
    if (child != nsnull) {
      nsRect buttonRect;
      nsRect txtRect;
      mTextFrame->GetRect(txtRect);
      child->GetRect(buttonRect);

      // check to see if we must reflow just the texField
      // because style width or height was set.
      if (txtRect.width + buttonRect.width != aDesiredSize.width ||
          txtRect.height != aDesiredSize.height) {

        nsSize txtAvailSize(aDesiredSize.width - buttonRect.width, aDesiredSize.height);
        nsHTMLReflowMetrics txtKidSize(&txtAvailSize);
        nsHTMLReflowState   txtKidReflowState(aPresContext, aReflowState, mTextFrame, txtAvailSize);
        txtKidReflowState.reason = eReflowReason_Resize;
        txtKidReflowState.mComputedWidth  = txtAvailSize.width;
        txtKidReflowState.mComputedHeight = txtAvailSize.height;
        mTextFrame->WillReflow(aPresContext);
        nsReflowStatus status;
        rv = mTextFrame->Reflow(aPresContext, txtKidSize, txtKidReflowState, status);
        if (NS_FAILED(rv)) return rv;
        rv = mTextFrame->DidReflow(aPresContext, aStatus);
        if (NS_FAILED(rv)) return rv;

        // now adjust the frame positions
        buttonRect.x = aDesiredSize.width - buttonRect.width + aReflowState.mComputedBorderPadding.left;
        child->SetRect(aPresContext, buttonRect);
        txtRect.y      = aReflowState.mComputedBorderPadding.top;
        txtRect.height = aDesiredSize.height;
        txtRect.width  = aDesiredSize.width - buttonRect.width;
        mTextFrame->SetRect(aPresContext, txtRect);
      }
    }
  }
  return rv;
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

nsNewFrame*
nsFileControlFrame::GetTextControlFrame(nsIPresContext* aPresContext, nsIFrame* aStart)
{
  nsNewFrame* result = nsnull;
#ifndef DEBUG_NEWFRAME
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
          if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, value)) {
            if (value.EqualsIgnoreCase("text")) {
              result = (nsNewFrame*)childFrame;      
            }
          }
        }
      }
    }

    // if not continue looking
    nsNewFrame* frame = GetTextControlFrame(aPresContext, childFrame);
    if (frame)
       result = frame;
     
    res = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(res == NS_OK,"failed to get next child");
  }

  return result;
#else
  return nsnull;
#endif
}

PRIntn
nsFileControlFrame::GetSkipSides() const
{
  return 0;
}


NS_IMETHODIMP
nsFileControlFrame::GetName(nsAString* aResult)
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


NS_IMETHODIMP
nsFileControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       PRInt32         aNameSpaceID,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aModType, 
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
    if (nsnull != mTextContent && NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, value)) {
      mTextContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::size, value, PR_TRUE);
      if (aHint != NS_STYLE_HINT_REFLOW) {
        nsFormFrame::StyleChangeReflow(aPresContext, this);
      }
    }
  }

  return nsAreaFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType, aHint);
}

NS_IMETHODIMP
nsFileControlFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint,
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame** aFrame)
{
#ifndef DEBUG_NEWFRAME
  if ( nsFormFrame::GetDisabled(this) && mRect.Contains(aPoint) ) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      
    if (vis->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  } else {
    return nsAreaFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  }
#endif
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
                                              const nsAReadableString& aValue)
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

NS_IMETHODIMP nsFileControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  aValue.Truncate();  // initialize out param

  if (nsHTMLAtoms::value == aName) {
    if (mTextFrame) {
      mTextFrame->GetTextControlFrameState(aValue);
    }
    else if (mCachedState) {
      aValue.Assign(*mCachedState);
    }
  }
  return NS_OK;
}




NS_METHOD
nsFileControlFrame::Paint(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }
  nsresult rv = nsAreaFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FAILED(rv)) return rv;
  
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsFileControlFrame::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  // Don't save state before we are initialized
  if (!mTextFrame && !mCachedState) {
    return NS_OK;
  }

  // Get the value string
  nsAutoString stateString;
  nsresult res = GetProperty(nsHTMLAtoms::value, stateString);
  NS_ENSURE_SUCCESS(res, res);

  // Compare to default value, and only save if needed (Bug 62713)
  nsAutoString defaultStateString;
  nsCOMPtr<nsIDOMHTMLInputElement> formControl(do_QueryInterface(mContent));
  if (formControl) {
    formControl->GetDefaultValue(defaultStateString);
  }

  if (! stateString.Equals(defaultStateString)) {

    // Construct a pres state and store value in it.
    res = NS_NewPresState(aState);
    NS_ENSURE_SUCCESS(res, res);
    res = (*aState)->SetStateProperty(NS_LITERAL_STRING("value"), stateString);
  }

  return res;
}

NS_IMETHODIMP
nsFileControlFrame::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsAutoString string;
  aState->GetStateProperty(NS_LITERAL_STRING("value"), string);
  SetProperty(aPresContext, nsHTMLAtoms::value, string);

  return NS_OK;
}

NS_IMETHODIMP
nsFileControlFrame::OnContentReset()
{
  return NS_OK;
}
