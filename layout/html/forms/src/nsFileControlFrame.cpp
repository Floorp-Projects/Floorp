/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFileControlFrame.h"


#include "nsIElementFactory.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
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
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFilePicker.h"
#include "nsIDOMMouseEvent.h"
#include "nsINodeInfo.h"
#include "nsIDOMEventReceiver.h"
#include "nsIScriptGlobalObject.h"
#include "nsILocalFile.h"
#include "nsITextControlElement.h"

#include "nsContentCID.h"
static NS_DEFINE_CID(kHTMLElementFactoryCID,   NS_HTML_ELEMENT_FACTORY_CID);

#define SYNC_TEXT 0x1
#define SYNC_BUTTON 0x2
#define SYNC_BOTH 0x3

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
  mCachedState(nsnull),
  mDidPreDestroy(PR_FALSE)
{
    //Shrink the area around it's contents
  SetFlags(NS_BLOCK_SHRINK_WRAP);
}

nsFileControlFrame::~nsFileControlFrame()
{
  // remove ourself as a listener of the button (bug 40533)
  if (mBrowse) {
    nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mBrowse));
    reciever->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMMouseListener));
  }

  if (mCachedState) {
    delete mCachedState;
    mCachedState = nsnull;
  }
}

void
nsFileControlFrame::PreDestroy(nsIPresContext* aPresContext)
{
  // Toss the value into the control from the anonymous content, which is about
  // to get lost.
  if (mTextContent) {
    nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface(mTextContent);
    nsAutoString value;
    input->GetValue(value);

    // Have it take the value, just like when input type=text goes away
    nsCOMPtr<nsITextControlElement> fileInput = do_QueryInterface(mContent);
    fileInput->TakeTextFrameValue(value);
  }
  mDidPreDestroy = PR_TRUE;
}

NS_IMETHODIMP
nsFileControlFrame::Destroy(nsIPresContext* aPresContext)
{
  if (!mDidPreDestroy) {
    PreDestroy(aPresContext);
  }
  mTextFrame = nsnull;
  return nsAreaFrame::Destroy(aPresContext);
}

void
nsFileControlFrame::RemovedAsPrimaryFrame(nsIPresContext* aPresContext)
{
  if (!mDidPreDestroy) {
    PreDestroy(aPresContext);
  }
#ifdef DEBUG
  else {
    NS_ERROR("RemovedAsPrimaryFrame called after PreDestroy");
  }
#endif
}

NS_IMETHODIMP
nsFileControlFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                           nsISupportsArray& aChildList)
{
  // Get the NodeInfoManager and tag necessary to create input elements
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();
  nsINodeInfoManager *nimgr = doc->GetNodeInfoManager();
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nimgr->GetNodeInfo(nsHTMLAtoms::input, nsnull, kNameSpaceID_None,
                     getter_AddRefs(nodeInfo));

  nsresult rv;
  nsCOMPtr<nsIElementFactory> ef(do_GetService(kHTMLElementFactoryCID,&rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the text content
  rv = ef->CreateInstanceByTag(nodeInfo,getter_AddRefs(mTextContent));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHTMLInputElement> fileContent(do_QueryInterface(mContent, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 tabIndex = -1;
  if (mTextContent) {
    mTextContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::type, NS_LITERAL_STRING("text"), PR_FALSE);
    nsCOMPtr<nsIDOMHTMLInputElement> textControl = do_QueryInterface(mTextContent);
    if (textControl) {
      // Initialize value when we create the content in case the value was set
      // before we got here
      nsAutoString value;
      fileContent->GetValue(value);
      textControl->SetValue(value);
      fileContent->GetTabIndex(&tabIndex);
      textControl->SetTabIndex(tabIndex);
    }
    aChildList.AppendElement(mTextContent);
  }

  // Create the browse button
  rv = ef->CreateInstanceByTag(nodeInfo,getter_AddRefs(mBrowse));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mBrowse) {
    mBrowse->SetAttr(kNameSpaceID_None, nsHTMLAtoms::type, NS_LITERAL_STRING("button"), PR_FALSE);
    nsCOMPtr<nsIDOMHTMLInputElement> browseControl = do_QueryInterface(mBrowse);
    if (browseControl) {
      fileContent->GetTabIndex(&tabIndex);
      browseControl->SetTabIndex(tabIndex);
    }

    aChildList.AppendElement(mBrowse);

    // register as an event listener of the button to open file dialog on mouse click
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mBrowse));
    receiver->AddEventListenerByIID(this, NS_GET_IID(nsIDOMMouseListener));
  }

  SyncAttr(kNameSpaceID_None, nsHTMLAtoms::size,     SYNC_TEXT);
  SyncAttr(kNameSpaceID_None, nsHTMLAtoms::disabled, SYNC_BOTH);

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
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(PRInt32)
nsFileControlFrame::GetFormControlType() const
{
  return NS_FORM_INPUT_FILE;
}


void 
nsFileControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  // Fix for Bug 6133 
  if (mTextFrame) {
    nsIContent* content = mTextFrame->GetContent();
    if (content) {
      content->SetFocus(mPresContext);
    }
  }
}

void
nsFileControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
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
  nsIContent* content = GetContent();
  if (!content)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  if (!doc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindow> parentWindow =
    do_QueryInterface(doc->GetScriptGlobalObject());

  // Get Loc title
  nsString title;
  nsFormControlHelper::GetLocalizedString(nsFormControlHelper::GetHTMLPropertiesFileName(),
                                          NS_LITERAL_STRING("FileUpload").get(), title);

  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!filePicker)
    return NS_ERROR_FAILURE;

  result = filePicker->Init(parentWindow, title, nsIFilePicker::modeOpen);
  if (NS_FAILED(result))
    return result;

  // Set filter "All Files"
  filePicker->AppendFilters(nsIFilePicker::filterAll);

  // Set default directry and filename
  nsAutoString defaultName;
  GetProperty(nsHTMLAtoms::value, defaultName);

  nsCOMPtr<nsILocalFile> currentFile = do_CreateInstance("@mozilla.org/file/local;1");
  if (currentFile && !defaultName.IsEmpty()) {
    result = currentFile->InitWithPath(defaultName);
    if (NS_SUCCEEDED(result)) {
      nsAutoString leafName;
      currentFile->GetLeafName(leafName);
      if (!leafName.IsEmpty()) {
        filePicker->SetDefaultString(leafName);
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

  // Tell our textframe to remember the currently focused value
  mTextFrame->InitFocusedValue();

  // Open dialog
  PRInt16 mode;
  result = filePicker->Show(&mode);
  if (NS_FAILED(result))
    return result;
  if (mode == nsIFilePicker::returnCancel)
    return NS_OK;

  if (!mTextFrame) {
    // We got destroyed while the filepicker was up.  Don't do anything here.
    return NS_OK;
  }
  
  // Set property
  nsCOMPtr<nsILocalFile> localFile;
  result = filePicker->GetFile(getter_AddRefs(localFile));
  if (localFile) {
    nsAutoString unicodePath;
    result = localFile->GetPath(unicodePath);
    if (!unicodePath.IsEmpty()) {
      mTextFrame->SetProperty(mPresContext, nsHTMLAtoms::value, unicodePath);
      // May need to fire an onchange here
      mTextFrame->CheckFireOnChange();
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
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  aStatus = NS_FRAME_COMPLETE;

  if (eReflowReason_Initial == aReflowState.reason) {
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
    const nsStyleVisibility* vis = GetStyleVisibility();

    nsIFrame* child = GetFirstChild(nsnull);
    if (child == mTextFrame) {
      child = child->GetNextSibling();
    }
    if (child) {
      nsRect buttonRect = child->GetRect();
      nsRect txtRect = mTextFrame->GetRect();

      // check to see if we must reflow just the area frame again 
      // in order for the text field to be the correct height
      // reflowing just the textfield (for some reason) 
      // messes up the button's rect
      if (txtRect.width + buttonRect.width != aDesiredSize.width ||
          txtRect.height != aDesiredSize.height) {
        nsHTMLReflowMetrics txtKidSize(PR_TRUE);
        nsSize txtAvailSize(aReflowState.availableWidth, aDesiredSize.height);
        nsHTMLReflowState   txtKidReflowState(aPresContext, aReflowState, this, txtAvailSize,
                                              eReflowReason_Resize);
        txtKidReflowState.mComputedHeight = aDesiredSize.height;
        rv = nsAreaFrame::WillReflow(aPresContext);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Should have succeeded");
        rv = nsAreaFrame::Reflow(aPresContext, txtKidSize, txtKidReflowState, aStatus);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Should have succeeded");
        rv = nsAreaFrame::DidReflow(aPresContext, &txtKidReflowState, aStatus);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Should have succeeded");

        // If LTR then re-calc and set the correct rect
        if (NS_STYLE_DIRECTION_RTL != vis->mDirection) {
          // now adjust the frame positions
          txtRect.y      = aReflowState.mComputedBorderPadding.top;
          txtRect.height = aDesiredSize.height;
          mTextFrame->SetRect(txtRect);
        }
      }

      // Do RTL positioning
      // for some reason the areaframe does set the X coord of the rects correctly
      // so we must redo them here
      // and we must make sure the text field is the correct height
      if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
        buttonRect.x      = aReflowState.mComputedBorderPadding.left;
        child->SetRect(buttonRect);
        txtRect.x         = aDesiredSize.width - txtRect.width + aReflowState.mComputedBorderPadding.left;
        txtRect.y         = aReflowState.mComputedBorderPadding.top;
        txtRect.height    = aDesiredSize.height;
        mTextFrame->SetRect(txtRect);
      }

    }
  }
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
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

nsNewFrame*
nsFileControlFrame::GetTextControlFrame(nsIPresContext* aPresContext, nsIFrame* aStart)
{
  nsNewFrame* result = nsnull;
#ifndef DEBUG_NEWFRAME
  // find the text control frame.
  nsIFrame* childFrame = aStart->GetFirstChild(nsnull);

  while (childFrame) {
    // see if the child is a text control
    nsCOMPtr<nsIFormControl> formCtrl =
      do_QueryInterface(childFrame->GetContent());

    if (formCtrl && formCtrl->GetType() == NS_FORM_INPUT_TEXT) {
      result = (nsNewFrame*)childFrame;
    }

    // if not continue looking
    nsNewFrame* frame = GetTextControlFrame(aPresContext, childFrame);
    if (frame)
       result = frame;
     
    childFrame = childFrame->GetNextSibling();
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
  return nsFormControlHelper::GetName(mContent, aResult);
}

void
nsFileControlFrame::SyncAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRInt32 aWhichControls)
{
  nsAutoString value;
  nsresult rv = mContent->GetAttr(aNameSpaceID, aAttribute, value);
  if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
    if (aWhichControls & SYNC_TEXT && mTextContent) {
      mTextContent->SetAttr(aNameSpaceID, aAttribute, value, PR_TRUE);
    }
    if (aWhichControls & SYNC_BUTTON && mBrowse) {
      mBrowse->SetAttr(aNameSpaceID, aAttribute, value, PR_TRUE);
    }
  } else {
    if (aWhichControls & SYNC_TEXT && mTextContent) {
      mTextContent->UnsetAttr(aNameSpaceID, aAttribute, PR_TRUE);
    }
    if (aWhichControls & SYNC_BUTTON && mBrowse) {
      mBrowse->UnsetAttr(aNameSpaceID, aAttribute, PR_TRUE);
    }
  }
}

NS_IMETHODIMP
nsFileControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       PRInt32         aNameSpaceID,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aModType)
{
  // propagate disabled to text / button inputs
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsHTMLAtoms::disabled) {
    SyncAttr(aNameSpaceID, aAttribute, SYNC_BOTH);
  // propagate size to text
  } else if (aNameSpaceID == kNameSpaceID_None &&
             aAttribute == nsHTMLAtoms::size) {
    SyncAttr(aNameSpaceID, aAttribute, SYNC_TEXT);
  }

  return nsAreaFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType);
}

NS_IMETHODIMP
nsFileControlFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint,
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame** aFrame)
{
#ifndef DEBUG_NEWFRAME
  if ( nsFormControlHelper::GetDisabled(mContent) && mRect.Contains(aPoint) ) {
    if (GetStyleVisibility()->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  } else {
    return nsAreaFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  }
#endif
  return NS_ERROR_FAILURE;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsFileControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FileControl"), aResult);
}
#endif

NS_IMETHODIMP
nsFileControlFrame::GetFormContent(nsIContent*& aContent) const
{
  aContent = GetContent();
  NS_IF_ADDREF(aContent);
  return NS_OK;
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

NS_IMETHODIMP nsFileControlFrame::SetProperty(nsIPresContext* aPresContext,
                                              nsIAtom* aName,
                                              const nsAString& aValue)
{
  nsresult rv = NS_OK;
  if (nsHTMLAtoms::value == aName) {
    if (mTextFrame) {
      mTextFrame->SetValue(aValue);
    } else {
      if (mCachedState) delete mCachedState;
      mCachedState = new nsString(aValue);
      if (!mCachedState) rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return rv;
}      

NS_IMETHODIMP nsFileControlFrame::GetProperty(nsIAtom* aName, nsAString& aValue)
{
  aValue.Truncate();  // initialize out param

  if (nsHTMLAtoms::value == aName) {
    if (mTextFrame) {
      mTextFrame->GetValue(aValue, PR_FALSE);
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

NS_IMETHODIMP
nsFileControlFrame::OnContentReset()
{
  return NS_OK;
}
