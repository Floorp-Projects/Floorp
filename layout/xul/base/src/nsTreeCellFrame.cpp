/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsTreeCellFrame.h"
#include "nsTreeFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIStyleSet.h"
#include "nsIViewManager.h"
#include "nsCSSRendering.h"
#include "nsXULAtoms.h"
#include "nsCOMPtr.h"

// Includes related to the execution of custom JS event handlers
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIJSScriptObject.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID,      NS_ISCRIPTOBJECTOWNER_IID);

static void ForceDrawFrame(nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view) {
    nsCOMPtr<nsIViewManager> viewMgr;
    view->GetViewManager(*getter_AddRefs(viewMgr));
    if (viewMgr)
      viewMgr->UpdateView(view, rect, 0);
  }

}

//
// NS_NewTreeCellFrame
//
// Creates a new tree cell frame
//
nsresult
NS_NewTreeCellFrame (nsIFrame*& aNewFrame)
{
  nsTreeCellFrame* theFrame = new nsTreeCellFrame();
  if (theFrame == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  aNewFrame = theFrame;
  return NS_OK;
  
} // NS_NewTreeCellFrame


// Constructor
nsTreeCellFrame::nsTreeCellFrame()
:nsTableCellFrame() { mAllowEvents = PR_FALSE; mIsHeader = PR_FALSE; }

// Destructor
nsTreeCellFrame::~nsTreeCellFrame()
{
}

NS_IMETHODIMP
nsTreeCellFrame::Init(nsIPresContext&  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  // Determine if we're a column header or not.
  
  // Get row group frame
  nsIFrame* pRowGroupFrame = nsnull;
  aParent->GetParent(&pRowGroupFrame);
  if (pRowGroupFrame != nsnull)
  {
		// Get the display type of the row group frame and see if it's a header or body
		nsCOMPtr<nsIStyleContext> parentContext;
		pRowGroupFrame->GetStyleContext(getter_AddRefs(parentContext));
		if (parentContext)
		{
			const nsStyleDisplay* display = (const nsStyleDisplay*)
				parentContext->GetStyleData(eStyleStruct_Display);
			if (display->mDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP)
			{
				mIsHeader = PR_TRUE;
			}
			else mIsHeader = PR_FALSE;

			// Get the table frame.
			pRowGroupFrame->GetParent((nsIFrame**) &mTreeFrame);
		}
  }

  return nsTableCellFrame::Init(aPresContext, aContent, aParent, aContext,
                                aPrevInFlow);
}

nsTableFrame* nsTreeCellFrame::GetTreeFrame()
{
	return mTreeFrame;
}

NS_METHOD nsTreeCellFrame::Reflow(nsIPresContext& aPresContext,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus& aStatus)
{
	nsresult rv = nsTableCellFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

	return rv;
}

NS_IMETHODIMP
nsTreeCellFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                  nsIFrame**     aFrame)
{
  if (mAllowEvents)
  {
	  return nsTableCellFrame::GetFrameForPoint(aPoint, aFrame);
  }
  else
  {
    *aFrame = this; // Capture all events so that we can perform selection and expand/collapse.
    return NS_OK;
  }
}

NS_IMETHODIMP 
nsTreeCellFrame::HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus&  aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->mVisible) {
    return NS_OK;
  }

  if(nsEventStatus_eConsumeNoDefault != aEventStatus) {

    aEventStatus = nsEventStatus_eConsumeDoDefault;
	  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
		  HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);
    else if (aEvent->message == NS_MOUSE_LEFT_CLICK)
      HandleClickEvent(aPresContext, aEvent, aEventStatus);
	  else if (aEvent->message == NS_MOUSE_LEFT_DOUBLECLICK)
		  HandleDoubleClickEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleMouseDownEvent(nsIPresContext& aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus&  aEventStatus)
{
  if (mIsHeader)
  {
	  // Nothing to do?
  }
  else
  {
    // Perform a selection
	  if (((nsMouseEvent *)aEvent)->isShift)
	    mTreeFrame->RangedSelection(aPresContext, this); // Applying a ranged selection.
	  else if (((nsMouseEvent *)aEvent)->isControl)
	    mTreeFrame->ToggleSelection(aPresContext, this); // Applying a toggle selection.
	  else mTreeFrame->SetSelection(aPresContext, this); // Doing a single selection only.
  }
  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleClickEvent(nsIPresContext& aPresContext, 
									                nsGUIEvent*     aEvent,
									                nsEventStatus&  aEventStatus)
{
  if (mIsHeader)
  {
	  // Nothing to do?
  }
  else
  {
    // Need to look for an "onItemClick" handler in the tree tag (our great-grandparent). 
    // If one exists, then we should execute the JavaScript code in the context of the
    // treeitem (our parent).
    // Create a DOM event from the nsGUIEvent that occurred.
    nsIDOMEvent* aDOMEvent;
    NS_NewDOMEvent(&aDOMEvent, aPresContext, aEvent);
    ExecuteDefaultJSEventHandler("onitemclick", aDOMEvent);
    // Release the DOM event. We don't care about it any more.
    NS_IF_RELEASE(aDOMEvent);
  }
  return NS_OK;
}


nsresult
nsTreeCellFrame::HandleDoubleClickEvent(nsIPresContext& aPresContext, 
									    nsGUIEvent*     aEvent,
									    nsEventStatus&  aEventStatus)
{
  if (!mIsHeader)
  {
    // Perform an expand/collapse
	  // Iterate up the chain to the row and then to the item.
	  nsCOMPtr<nsIContent> pTreeItemContent;
	  mContent->GetParent(*getter_AddRefs(pTreeItemContent));
	  
	  // Take the tree item content and toggle the value of its open attribute.
	  nsString attrValue;
    nsCOMPtr<nsIAtom> kOpenAtom ( dont_AddRef(NS_NewAtom("open")) );
    nsresult result = pTreeItemContent->GetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded =  (result == NS_CONTENT_ATTR_NO_VALUE ||
						 (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));
    if (isExpanded)
	  {
		  // We're collapsing and need to remove frames from the flow.
		  pTreeItemContent->UnsetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, PR_FALSE);
	  }
	  else
	  {
		  // We're expanding and need to add frames to the flow.
		  pTreeItemContent->SetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, "true", PR_FALSE);
	  }

	  // Ok, try out the hack of doing frame reconstruction
    // XXX: This needs to change
    // XXX: Need to only do this for containers.
	  nsCOMPtr<nsIPresShell> pShell;
    aPresContext.GetShell(getter_AddRefs(pShell));
	  nsCOMPtr<nsIStyleSet> pStyleSet;
    pShell->GetStyleSet(getter_AddRefs(pStyleSet));
	  nsCOMPtr<nsIDocument> pDocument;
    pShell->GetDocument(getter_AddRefs(pDocument));
	  nsCOMPtr<nsIContent> pRoot ( dont_AddRef(pDocument->GetRootContent()) );
	  
	  if (pRoot) {
		  nsIFrame*   docElementFrame;
		  nsIFrame*   parentFrame;
    
		  // Get the frame that corresponds to the document element
		  pShell->GetPrimaryFrameFor(pRoot, &docElementFrame);
		  if (nsnull != docElementFrame) {
		    docElementFrame->GetParent(&parentFrame);
      
		    pShell->EnterReflowLock();
		    pStyleSet->ReconstructFrames(&aPresContext, pRoot,
									     parentFrame, docElementFrame);
		    pShell->ExitReflowLock();
		  }
    }
  }
  return NS_OK;
}


void nsTreeCellFrame::Select(nsIPresContext& aPresContext, PRBool isSelected, PRBool notifyForReflow)
{
	nsCOMPtr<nsIAtom> kSelectedCellAtom(dont_AddRef(NS_NewAtom("selectedcell")));
  nsCOMPtr<nsIAtom> kSelectedAtom(dont_AddRef(NS_NewAtom("selected")));

  nsIContent* pParentContent;
  mContent->GetParent(pParentContent);

  if (isSelected)
	{
		// We're selecting the node.
		mContent->SetAttribute(nsXULAtoms::nameSpaceID, kSelectedCellAtom, "true", notifyForReflow);
    pParentContent->SetAttribute(nsXULAtoms::nameSpaceID, kSelectedAtom, "true", notifyForReflow);
	}
	else
	{
		// We're deselecting the node.
		mContent->UnsetAttribute(nsXULAtoms::nameSpaceID, kSelectedCellAtom, notifyForReflow);
    pParentContent->UnsetAttribute(nsXULAtoms::nameSpaceID, kSelectedAtom, notifyForReflow);
	}

  NS_IF_RELEASE(pParentContent);
}

const char* cEvent[] = {"event"};

void nsTreeCellFrame::ExecuteDefaultJSEventHandler(const nsString& eventName,
                                                   nsIDOMEvent* aDOMEvent)
{
  /* BACKING THIS OUT, SINCE I THINK EVENT BUBBLING ACCOMPLISHED WHAT
      I WANTED.

  // Get our parent, grandparent, and great-grandparent nodes
  nsCOMPtr<nsIContent> treeitem;
  nsCOMPtr<nsIContent> treebody;
  nsCOMPtr<nsIContent> tree;

  mContent->GetParent(*(getter_AddRefs(treeitem)));
  treeitem->GetParent(*(getter_AddRefs(treebody)));
  treebody->GetParent(*(getter_AddRefs(tree)));

  // See if the attribute is even present on the tree node.
  PRInt32 namespaceID;
  tree->GetNameSpaceID(namespaceID);
  nsIAtom* eventNameAtom = NS_NewAtom(eventName);
  nsString codeText;
  nsresult rv = tree->GetAttribute(namespaceID, eventNameAtom, codeText);
  NS_IF_RELEASE(eventNameAtom);

  if (rv == NS_CONTENT_ATTR_HAS_VALUE)
  {
    // Compile/execute the JavaScript code in the context of the treeitem.
    // XXX: Make this code more robust. Check for null pointers.

    // Retrieve our document.
    nsCOMPtr<nsIDocument> document;
    rv = tree->GetDocument(*getter_AddRefs(document));
    if (rv != NS_OK)
      return;

    // Retrieve the script context owner for the document.
    nsIScriptContextOwner* scriptContextOwner = document->GetScriptContextOwner();

    // Retrieve the script context from the owner.
    nsCOMPtr<nsIScriptContext> scriptContext;
    rv = scriptContextOwner->GetScriptContext(getter_AddRefs(scriptContext));
    if (rv != NS_OK)
      return;

    NS_IF_RELEASE(scriptContextOwner);

    // The TREEITEM is considered to be the script object owner.
    nsIScriptObjectOwner* scriptObjectOwner;
    if (treeitem->QueryInterface(kIScriptObjectOwnerIID, (void**) &scriptObjectOwner) != NS_OK)
      return;
    
    // Obtain the script object from its owner.
    JSObject* jsObject;
    rv = scriptObjectOwner->GetScriptObject(scriptContext, (void**)&jsObject);
    NS_IF_RELEASE(scriptObjectOwner);
    if (rv != NS_OK)
      return;

    // Obtain the JS native context
    JSContext* jsContext = (JSContext*)scriptContext->GetNativeContext();

    nsString lowerEventName;
    eventName.ToLowerCase(lowerEventName);

    char* charName = lowerEventName.ToNewCString();

    if (charName)
    {
      // Compile the function.
      JS_CompileUCFunction(jsContext, jsObject, charName,
		           1, cEvent, (jschar*)codeText.GetUnicode(), codeText.Length(),
		           nsnull, 0);
      
      // Execute the function.
      jsval funval, result;
      jsval argv[1];
      
      if (!JS_LookupProperty(jsContext, jsObject, charName, &funval)) {
        delete charName;
        return;
      }

      delete charName;

      if (JS_TypeOfValue(jsContext, funval) != JSTYPE_FUNCTION) {
        return;
      }

      JSObject* aEventObj;
      nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(jsContext);
      if (NS_OK != NS_NewScriptEvent(scriptCX, aDOMEvent, nsnull, (void**)&aEventObj)) {
        return;
      }

      argv[0] = OBJECT_TO_JSVAL(aEventObj);
      if (PR_TRUE == JS_CallFunctionValue(jsContext, jsObject, funval, 1, argv, &result)) {
	      if (JSVAL_IS_BOOLEAN(result) && JSVAL_TO_BOOLEAN(result) == JS_FALSE) {
          return; // XXX: Denoted failure. Do I care?
        }
        return; // XXX: Denoted success.
      }
    }
  } // XXX: Am I missing out on some necessary cleanup?
  */
}