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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMMouseListener.h"
#include "nsIPresShell.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsPIDOMWindow.h"
#include "nsIFilePicker.h"
#include "nsIDOMMouseEvent.h"
#include "nsINodeInfo.h"
#include "nsIDOMEventTarget.h"
#include "nsILocalFile.h"
#include "nsIFileControlElement.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOM3EventTarget.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

#define SYNC_TEXT 0x1
#define SYNC_BUTTON 0x2
#define SYNC_BOTH 0x3

nsIFrame*
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFileControlFrame(aContext);
}

nsFileControlFrame::nsFileControlFrame(nsStyleContext* aContext):
  nsBlockFrame(aContext),
  mTextFrame(nsnull), 
  mCachedState(nsnull)
{
}

nsFileControlFrame::~nsFileControlFrame()
{
  if (mCachedState) {
    delete mCachedState;
    mCachedState = nsnull;
  }
}

NS_IMETHODIMP
nsFileControlFrame::Init(nsIContent* aContent,
                         nsIFrame*   aParent,
                         nsIFrame*   aPrevInFlow)
{
  nsresult rv = nsBlockFrame::Init(aContent, aParent, aPrevInFlow);
  NS_ENSURE_SUCCESS(rv, rv);

  mMouseListener = new MouseListener(this);
  NS_ENSURE_TRUE(mMouseListener, NS_ERROR_OUT_OF_MEMORY);

  return rv;
}

void
nsFileControlFrame::Destroy()
{
  mTextFrame = nsnull;
  ENSURE_TRUE(mContent);

  // remove mMouseListener as a mouse event listener (bug 40533, bug 355931)
  NS_NAMED_LITERAL_STRING(click, "click");

  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  mContent->GetSystemEventGroup(getter_AddRefs(systemGroup));

  nsCOMPtr<nsIDOM3EventTarget> dom3Browse = do_QueryInterface(mBrowse);
  if (dom3Browse) {
    dom3Browse->RemoveGroupedEventListener(click, mMouseListener, PR_FALSE,
                                           systemGroup);
    nsContentUtils::DestroyAnonymousContent(&mBrowse);
  }
  nsCOMPtr<nsIDOM3EventTarget> dom3TextContent =
    do_QueryInterface(mTextContent);
  if (dom3TextContent) {
    dom3TextContent->RemoveGroupedEventListener(click, mMouseListener, PR_FALSE,
                                                systemGroup);
    nsContentUtils::DestroyAnonymousContent(&mTextContent);
  }

  mMouseListener->ForgetFrame();
  nsBlockFrame::Destroy();
}

nsresult
nsFileControlFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  // Get the NodeInfoManager and tag necessary to create input elements
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::input, nsnull,
                                                 kNameSpaceID_None);

  // Create the text content
  NS_NewHTMLElement(getter_AddRefs(mTextContent), nodeInfo, PR_FALSE);
  if (!mTextContent)
    return NS_ERROR_OUT_OF_MEMORY;

  mTextContent->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                        NS_LITERAL_STRING("text"), PR_FALSE);

  nsCOMPtr<nsIDOMHTMLInputElement> textControl = do_QueryInterface(mTextContent);
  if (textControl) {
    nsCOMPtr<nsIFileControlElement> fileControl = do_QueryInterface(mContent);
    if (fileControl) {
      // Initialize value when we create the content in case the value was set
      // before we got here
      nsAutoString value;
      fileControl->GetFileName(value);
      textControl->SetValue(value);
    }

    textControl->SetTabIndex(-1);
    textControl->SetReadOnly(PR_TRUE);
  }

  if (!aElements.AppendElement(mTextContent))
    return NS_ERROR_OUT_OF_MEMORY;

  NS_NAMED_LITERAL_STRING(click, "click");
  nsCOMPtr<nsIDOMEventGroup> systemGroup;
  mContent->GetSystemEventGroup(getter_AddRefs(systemGroup));
  nsCOMPtr<nsIDOM3EventTarget> dom3TextContent =
    do_QueryInterface(mTextContent);
  NS_ENSURE_STATE(dom3TextContent);
  // Register as an event listener of the textbox
  // to open file dialog on mouse click
  dom3TextContent->AddGroupedEventListener(click, mMouseListener, PR_FALSE,
                                           systemGroup);

  // Create the browse button
  NS_NewHTMLElement(getter_AddRefs(mBrowse), nodeInfo, PR_FALSE);
  if (!mBrowse)
    return NS_ERROR_OUT_OF_MEMORY;

  mBrowse->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("button"), PR_FALSE);
  nsCOMPtr<nsIDOMHTMLInputElement> fileContent = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMHTMLInputElement> browseControl = do_QueryInterface(mBrowse);
  if (fileContent && browseControl) {
    PRInt32 tabIndex;
    nsAutoString accessKey;

    fileContent->GetAccessKey(accessKey);
    browseControl->SetAccessKey(accessKey);
    fileContent->GetTabIndex(&tabIndex);
    browseControl->SetTabIndex(tabIndex);
  }

  if (!aElements.AppendElement(mBrowse))
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOM3EventTarget> dom3Browse = do_QueryInterface(mBrowse);
  NS_ENSURE_STATE(dom3Browse);
  // Register as an event listener of the button
  // to open file dialog on mouse click
  dom3Browse->AddGroupedEventListener(click, mMouseListener, PR_FALSE,
                                      systemGroup);

  SyncAttr(kNameSpaceID_None, nsGkAtoms::size,     SYNC_TEXT);
  SyncAttr(kNameSpaceID_None, nsGkAtoms::disabled, SYNC_BOTH);

  return NS_OK;
}

NS_QUERYFRAME_HEAD(nsFileControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

void 
nsFileControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  // Fix for Bug 6133 
  if (mTextFrame) {
    nsIContent* content = mTextFrame->GetContent();
    if (content) {
      content->SetFocus(PresContext());
    }
  }
}

/**
 * This is called when our browse button is clicked
 */
NS_IMETHODIMP
nsFileControlFrame::MouseListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  NS_ASSERTION(mFrame, "We should have been unregistered");

  // only allow the left button
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  nsCOMPtr<nsIDOMNSUIEvent> uiEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_STATE(uiEvent);
  PRBool defaultPrevented = PR_FALSE;
  uiEvent->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }

  PRUint16 whichButton;
  if (NS_FAILED(mouseEvent->GetButton(&whichButton)) || whichButton != 0) {
    return NS_OK;
  }

  PRInt32 clickCount;
  if (NS_FAILED(mouseEvent->GetDetail(&clickCount)) || clickCount > 1) {
    return NS_OK;
  }

  nsresult result;

  // Get parent nsIDOMWindowInternal object.
  nsIContent* content = mFrame->GetContent();
  if (!content)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  if (!doc)
    return NS_ERROR_FAILURE;

  // Get Loc title
  nsXPIDLString title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     "FileUpload", title);

  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!filePicker)
    return NS_ERROR_FAILURE;

  nsPIDOMWindow* win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  result = filePicker->Init(win, title, nsIFilePicker::modeOpen);
  if (NS_FAILED(result))
    return result;

  // Set filter "All Files"
  filePicker->AppendFilters(nsIFilePicker::filterAll);

  // Set default directry and filename
  nsAutoString defaultName;
  mFrame->GetFormProperty(nsGkAtoms::value, defaultName);

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
  mFrame->mTextFrame->InitFocusedValue();

  // Open dialog
  PRInt16 mode;
  result = filePicker->Show(&mode);
  if (NS_FAILED(result))
    return result;
  if (mode == nsIFilePicker::returnCancel)
    return NS_OK;

  if (!mFrame) {
    // The frame got destroyed while the filepicker was up.  Don't do
    // anything here.
    // (This listener itself can't be destroyed because the event listener
    // manager holds a strong reference to us while it fires the event.)
    return NS_OK;
  }
  
  // Set property
  nsCOMPtr<nsILocalFile> localFile;
  result = filePicker->GetFile(getter_AddRefs(localFile));
  if (localFile) {
    nsAutoString unicodePath;
    result = localFile->GetPath(unicodePath);
    if (!unicodePath.IsEmpty()) {
      // Tell mTextFrame that this update of the value is a user initiated
      // change. Otherwise it'll think that the value is being set by a script
      // and not fire onchange when it should.
      PRBool oldState = mFrame->mTextFrame->GetFireChangeEventState();
      mFrame->mTextFrame->SetFireChangeEventState(PR_TRUE);
      nsCOMPtr<nsIFileControlElement> fileControl = do_QueryInterface(content);
      if (fileControl) {
        fileControl->SetFileName(unicodePath);
      }
      
      mFrame->mTextFrame->SetFireChangeEventState(oldState);
      // May need to fire an onchange here
      mFrame->mTextFrame->CheckFireOnChange();
      return NS_OK;
    }
  }

  return NS_FAILED(result) ? result : NS_ERROR_FAILURE;
}

nscoord
nsFileControlFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  // Our min width is our pref width
  result = GetPrefWidth(aRenderingContext);
  return result;
}

NS_IMETHODIMP nsFileControlFrame::Reflow(nsPresContext*          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFileControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  aStatus = NS_FRAME_COMPLETE;

  if (mState & NS_FRAME_FIRST_REFLOW) {
    mTextFrame = GetTextControlFrame(aPresContext, this);
    NS_ENSURE_TRUE(mTextFrame, NS_ERROR_UNEXPECTED);
    if (mCachedState) {
      mTextFrame->SetFormProperty(nsGkAtoms::value, *mCachedState);
      delete mCachedState;
      mCachedState = nsnull;
    }
  }

  // nsBlockFrame takes care of all our reflow
  return nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState,
                             aStatus);
}

nsNewFrame*
nsFileControlFrame::GetTextControlFrame(nsPresContext* aPresContext, nsIFrame* aStart)
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

void
nsFileControlFrame::SyncAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRInt32 aWhichControls)
{
  nsAutoString value;
  if (mContent->GetAttr(aNameSpaceID, aAttribute, value)) {
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
nsFileControlFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  // propagate disabled to text / button inputs
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::disabled) {
      SyncAttr(aNameSpaceID, aAttribute, SYNC_BOTH);
    // propagate size to text
    } else if (aAttribute == nsGkAtoms::size) {
      SyncAttr(aNameSpaceID, aAttribute, SYNC_TEXT);
    } else if (aAttribute == nsGkAtoms::tabindex) {
      SyncAttr(aNameSpaceID, aAttribute, SYNC_BUTTON);
    }
  }

  return nsBlockFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

PRBool
nsFileControlFrame::IsLeaf() const
{
  return PR_TRUE;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsFileControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FileControl"), aResult);
}
#endif

nsresult
nsFileControlFrame::SetFormProperty(nsIAtom* aName,
                                    const nsAString& aValue)
{
  if (nsGkAtoms::value == aName) {
    if (mTextFrame) {
      mTextFrame->SetValue(aValue);
    } else {
      if (mCachedState) delete mCachedState;
      mCachedState = new nsString(aValue);
      NS_ENSURE_TRUE(mCachedState, NS_ERROR_OUT_OF_MEMORY);
    }
  }
  return NS_OK;
}      

nsresult
nsFileControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  aValue.Truncate();  // initialize out param

  if (nsGkAtoms::value == aName) {
    NS_ASSERTION(!mCachedState || !mTextFrame,
                 "If we have a cached state, we better have no mTextFrame");
    if (mCachedState) {
      aValue.Assign(*mCachedState);
    } else {
      nsCOMPtr<nsIFileControlElement> fileControl =
        do_QueryInterface(mContent);
      if (fileControl) {
        fileControl->GetFileName(aValue);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFileControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  // Our background is inherited to the text input, and we don't really want to
  // paint it or out padding and borders (which we never have anyway, per
  // styles in forms.css) -- doing it just makes us look ugly in some cases and
  // has no effect in others.
  nsDisplayListCollection tempList;
  nsresult rv = nsBlockFrame::BuildDisplayList(aBuilder, aDirtyRect, tempList);
  if (NS_FAILED(rv))
    return rv;

  tempList.BorderBackground()->DeleteAll();

  // Clip height only
  nsRect clipRect(aBuilder->ToReferenceFrame(this), GetSize());
  clipRect.width = GetOverflowRect().XMost();
  rv = OverflowClip(aBuilder, tempList, aLists, clipRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // Disabled file controls don't pass mouse events to their children, so we
  // put an invisible item in the display list above the children
  // just to catch events
  // REVIEW: I'm not sure why we do this, but that's what nsFileControlFrame::
  // GetFrameForPoint was doing
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled) && 
      IsVisibleForPainting(aBuilder)) {
    nsDisplayItem* item = new (aBuilder) nsDisplayEventReceiver(this);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    aLists.Content()->AppendToTop(item);
  }

  return DisplaySelectionOverlay(aBuilder, aLists);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsFileControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  // Accessible object exists just to hold onto its children, for later shutdown
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLGenericAccessible(static_cast<nsIFrame*>(this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

////////////////////////////////////////////////////////////
// Mouse listener implementation

NS_IMPL_ISUPPORTS2(nsFileControlFrame::MouseListener,
                   nsIDOMMouseListener,
                   nsIDOMEventListener)
