/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 */

/*
  This file provides the implementation for xul popup listener which
  tracks xul popups, context menus, and tooltips
 */

#include "nsCOMPtr.h"
#include "nsXULAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIXULPopupListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsRDFCID.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMMouseEvent.h"
#include "nsITimer.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMEventTarget.h"

#include "nsIBoxObject.h"
#include "nsIPopupSetBoxObject.h"
#include "nsIMenuBoxObject.h"

// for event firing in context menus
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"

#include "nsIFrame.h"
#include "nsIStyleContext.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULPopupListenerCID,      NS_XULPOPUPLISTENER_CID);



////////////////////////////////////////////////////////////////////////
// PopupListenerImpl
//
//   This is the popup listener implementation for popup menus, context menus,
//   and tooltips.
//
class XULPopupListenerImpl : public nsIXULPopupListener,
                             public nsIDOMMouseListener,
                             public nsIDOMMouseMotionListener
{
public:
    XULPopupListenerImpl(void);
    virtual ~XULPopupListenerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIXULPopupListener
    NS_IMETHOD Init(nsIDOMElement* aElement, const XULPopupType& popupType);

    // nsIDOMMouseListener
    virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
    virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
    virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) ;

    // nsIDOMMouseMotionListener
    virtual nsresult MouseMove(nsIDOMEvent* aMouseEvent);
    virtual nsresult DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; };

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:

    virtual nsresult LaunchPopup(nsIDOMEvent* anEvent);
    virtual nsresult LaunchPopup(PRInt32 aClientX, PRInt32 aClientY) ;
    virtual void ClosePopup ( ) ;

    nsresult FindDocumentForNode(nsIDOMNode* inNode, nsIDOMXULDocument** outDoc) ;

private:
    nsresult PreLaunchPopup(nsIDOMEvent* aMouseEvent);
    nsresult FireFocusOnTargetContent(nsIDOMNode* aTargetNode);

    // |mElement| is the node to which this listener is attached.
    nsIDOMElement* mElement;               // Weak ref. The element will go away first.

    // The popup that is getting shown on top of mElement.
    nsIDOMElement* mPopupContent; 

    // The type of the popup
    XULPopupType popupType;
    
    // The following members are not used unless |popupType| is tooltip.
      
      // a timer for determining if a tooltip should be displayed. 
    static void sTooltipCallback ( nsITimer *aTimer, void *aClosure ) ;
    nsCOMPtr<nsITimer> mTooltipTimer;
    PRInt32 mMouseClientX, mMouseClientY;       // mouse coordinates for tooltip event
    
    // The node hovered over that fired the timer. This may turn into the node that
    // triggered the tooltip, but only if the timer ever gets around to firing.
    // This is a strong reference, because the tooltip content can be destroyed while we're
    // waiting for the tooltip to pup up, and we need to detect that.
    // It's set only when the tooltip timer is created and launched. The timer must
    // either fire or be cancelled (or possibly released?), and we release this
    // reference in each of those cases. So we don't leak.
    nsCOMPtr<nsIDOMNode> mPossibleTooltipNode;
        
};

////////////////////////////////////////////////////////////////////////


XULPopupListenerImpl::XULPopupListenerImpl(void)
  : mElement(nsnull), mPopupContent(nsnull),
    mMouseClientX(0), mMouseClientY(0)
{
	NS_INIT_REFCNT();	
}

XULPopupListenerImpl::~XULPopupListenerImpl(void)
{
  ClosePopup();
  
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: XULPopupListenerImpl\n", gInstanceCount);
#endif
}

NS_IMPL_ADDREF(XULPopupListenerImpl)
NS_IMPL_RELEASE(XULPopupListenerImpl)


NS_INTERFACE_MAP_BEGIN(XULPopupListenerImpl)
  NS_INTERFACE_MAP_ENTRY(nsIXULPopupListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULPopupListener)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
XULPopupListenerImpl::Init(nsIDOMElement* aElement, const XULPopupType& popup)
{
  mElement = aElement; // Weak reference. Don't addref it.
  popupType = popup;
  return NS_OK;
}

////////////////////////////////////////////////////////////////
// nsIDOMMouseListener

nsresult
XULPopupListenerImpl::MouseUp(nsIDOMEvent* aMouseEvent)
{
// MacOS and Linux bring up context menus on mousedown, Windows on mouseup
#ifdef XP_PC
  if(popupType == eXULPopupType_context)
    return PreLaunchPopup(aMouseEvent);
  else 
    return NS_OK;
#else
  return NS_OK;
#endif
}

nsresult
XULPopupListenerImpl::MouseDown(nsIDOMEvent* aMouseEvent)
{
// MacOS and Linux bring up context menus on mousedown, Windows on mouseup
#ifndef XP_PC
  return PreLaunchPopup(aMouseEvent);
#else
  if(popupType != eXULPopupType_context)
    return PreLaunchPopup(aMouseEvent);
  else
    return NS_OK;
#endif
}

nsresult
XULPopupListenerImpl::PreLaunchPopup(nsIDOMEvent* aMouseEvent)
{
  PRUint16 button;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aMouseEvent);
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // check if someone has attempted to prevent this action.
  nsCOMPtr<nsIDOMNSUIEvent> nsUIEvent;
  nsUIEvent = do_QueryInterface(mouseEvent);
  if (!nsUIEvent) {
    return NS_OK;
  }

  PRBool preventDefault;
  nsUIEvent->GetPreventDefault(&preventDefault);
  if (preventDefault) {
    // someone called preventDefault. bail.
    return NS_OK;
  }

  // Get the node that was clicked on.
  nsCOMPtr<nsIDOMEventTarget> target;
  mouseEvent->GetTarget( getter_AddRefs( target ) );
  nsCOMPtr<nsIDOMNode> targetNode;
  if (target) targetNode = do_QueryInterface(target);

  // Get the document with the popup.
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
  nsresult rv;
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Turn the document into a XUL document so we can use SetPopupNode.
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (xulDocument == nsnull) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  // Store clicked-on node in xul document.
  xulDocument->SetPopupNode( targetNode );

  switch (popupType) {
    case eXULPopupType_popup:
      // Check for left mouse button down
      mouseEvent->GetButton(&button);
      if (button == 1) {
        // Time to launch a popup menu.
        LaunchPopup(aMouseEvent);
        aMouseEvent->PreventBubble();
        aMouseEvent->PreventDefault();
      }
      break;
    case eXULPopupType_context:
      // Check for right mouse button down
      mouseEvent->GetButton(&button);
      if (button == 3) {
        // Time to launch a context menu
        
        // If the context menu launches on mousedown,
        // we have to fire focus on the content we clicked on
#ifndef XP_PC
        FireFocusOnTargetContent(targetNode);
#endif
        LaunchPopup(aMouseEvent);
        aMouseEvent->PreventBubble();
        aMouseEvent->PreventDefault();
      }
      break;
    
    case eXULPopupType_tooltip:
      // get rid of the tooltip on a mousedown.
      ClosePopup();
      break;
      
    case eXULPopupType_blur:
      // what on earth is this?!
      break;

  }
  return NS_OK;
}

nsresult
XULPopupListenerImpl::FireFocusOnTargetContent(nsIDOMNode* aTargetNode)
{
  nsresult rv;
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = aTargetNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if(NS_SUCCEEDED(rv) && domDoc)
  {
    nsCOMPtr<nsIPresShell> shell;
    nsCOMPtr<nsIPresContext> context;
    nsCOMPtr<nsIDocument> tempdoc = do_QueryInterface(domDoc);
    shell = getter_AddRefs(tempdoc->GetShellAt(0));  // Get nsIDOMElement for targetNode
    shell->GetPresContext(getter_AddRefs(context));
 
    nsCOMPtr<nsIContent> content = do_QueryInterface(aTargetNode);
    nsIFrame* targetFrame;
    shell->GetPrimaryFrameFor(content, &targetFrame);
  
    PRBool suppressBlur = PR_FALSE;
    const nsStyleUserInterface* ui;
    targetFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
    suppressBlur = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE);

    nsCOMPtr<nsIDOMElement> element;
    nsCOMPtr<nsIContent> newFocus = do_QueryInterface(content);

    nsIFrame* currFrame = targetFrame;
    // Look for the nearest enclosing focusable frame.
    while (currFrame) {
        const nsStyleUserInterface* ui;
        currFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
        if ((ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
            (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE)) 
        {
          currFrame->GetContent(getter_AddRefs(newFocus));
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocus));
          if (domElement) {
            element = domElement;
            break;
          }
        }
        currFrame->GetParent(&currFrame);
    } 
    nsCOMPtr<nsIContent> focusableContent = do_QueryInterface(element);
    nsCOMPtr<nsIEventStateManager> esm;
    context->GetEventStateManager(getter_AddRefs(esm));

    if (focusableContent)
      focusableContent->SetFocus(context);
    else if (!suppressBlur)
      esm->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

    esm->SetContentState(focusableContent, NS_EVENT_STATE_ACTIVE);
  }
  return rv;
}
//
// MouseMove
//
// If we're a tooltip, fire off a timer to see if a tooltip should be shown.
//
nsresult
XULPopupListenerImpl::MouseMove(nsIDOMEvent* aMouseEvent)
{
  // make sure we're a tooltip. if not, bail.
  if ( popupType != eXULPopupType_tooltip )
    return NS_OK;
  
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent)
    return NS_OK;

  // stash the coordinates of the event so that we can still get back to it from within the 
  // timer scallback. Also stash the node that started this so we can put it into the
  // document later on (if the timer ever fires).
  mouseEvent->GetClientX(&mMouseClientX);
  mouseEvent->GetClientY(&mMouseClientY);

  //XXX recognize when a popup is already up and immediately show the
  //XXX tooltip for the new item if the dom element is different than
  //XXX the element for which we are currently displaying the tip.
  //XXX
  //XXX for now, just be stupid to get things working.
  if (mPopupContent || mTooltipTimer)
    return NS_OK;
  
  mTooltipTimer = do_CreateInstance("component://netscape/timer");
  if ( mTooltipTimer ) {
    nsCOMPtr<nsIDOMEventTarget> eventTarget;
    aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
    if ( eventTarget )
      mPossibleTooltipNode = do_QueryInterface(eventTarget);
    if ( mPossibleTooltipNode ) {
      nsresult rv = mTooltipTimer->Init(sTooltipCallback, this, 500, NS_PRIORITY_HIGH); // 500 ms delay
      if (NS_FAILED(rv))
        mPossibleTooltipNode = nsnull;
    }
  }
  else
    NS_WARNING ( "Could not create a timer for tooltip tracking" );
    
  return NS_OK;
  
} // MouseMove


//
// MouseOut
//
// If we're a tooltip, hide any tip that might be showing and remove any
// timer that is pending since the mouse is no longer over this area.
//
nsresult
XULPopupListenerImpl::MouseOut(nsIDOMEvent* aMouseEvent)
{
  // make sure we're a tooltip. if not, bail.
  if ( popupType != eXULPopupType_tooltip )
    return NS_OK;
  
  ClosePopup();
  return NS_OK;
    
} // MouseOut


void
XULPopupListenerImpl :: ClosePopup ( )
{
  if ( mTooltipTimer ) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nsnull;
    // release tooltip target
    mPossibleTooltipNode = nsnull;
  }
  
  if ( mPopupContent ) {
    nsCOMPtr<nsIDOMNode> parent;
    mPopupContent->GetParentNode(getter_AddRefs(parent));
    nsCOMPtr<nsIDOMXULElement> popupSetElement(do_QueryInterface(parent));
    nsCOMPtr<nsIBoxObject> boxObject;
    if (popupSetElement)
      popupSetElement->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsIPopupSetBoxObject> popupSetObject(do_QueryInterface(boxObject));
    if (popupSetObject)
      popupSetObject->DestroyPopup();

    mPopupContent = nsnull;  // release the popup
    
    // clear out the tooltip node on the document
    nsCOMPtr<nsIDOMXULDocument> doc;
    FindDocumentForNode ( mElement, getter_AddRefs(doc) );
    if ( doc )
      doc->SetTooltipNode(nsnull);
  }

} // ClosePopup


//
// FindDocumentForNode
//
// Given a DOM content node, finds the XUL document associated with it
//
nsresult
XULPopupListenerImpl :: FindDocumentForNode ( nsIDOMNode* inElement, nsIDOMXULDocument** outDoc )
{
  nsresult rv = NS_OK;
  
  if ( !outDoc || !inElement )
    return NS_ERROR_INVALID_ARG;
  
  // get the document associated with this content element
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(inElement);
  if ( !content || NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document))) ) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if ( !xulDocument ) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  *outDoc = xulDocument;
  NS_ADDREF ( *outDoc );
  
  return rv;

} // FindDocumentForNode


//
// LaunchPopup
//
nsresult
XULPopupListenerImpl::LaunchPopup ( nsIDOMEvent* anEvent )
{
  // Retrieve our x and y position.
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(anEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  PRInt32 xPos, yPos;
  mouseEvent->GetClientX(&xPos); 
  mouseEvent->GetClientY(&yPos); 

  return LaunchPopup(xPos, yPos);
}


static void
GetImmediateChild(nsIContent* aContent, nsIAtom *aTag, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  aContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

  return;
}

//
// LaunchPopup
//
// Given the element on which the event was triggered and the mouse locations in
// Client and widget coordinates, popup a new window showing the appropriate 
// content.
//
// This looks for an attribute on |aElement| of the appropriate popup type 
// (popup, context, tooltip) and uses that attribute's value as an ID for
// the popup content in the document.
//
nsresult
XULPopupListenerImpl::LaunchPopup(PRInt32 aClientX, PRInt32 aClientY)
{
  nsresult rv = NS_OK;

  nsAutoString type; type.AssignWithConversion("popup");
  if ( popupType == eXULPopupType_context ) {
    type.AssignWithConversion("context");
    
    // position the menu two pixels down and to the right from the current
    // mouse position. This makes it easier to dismiss the menu by just
    // clicking.
    aClientX += 2;
    aClientY += 2;
  }
  else if ( popupType == eXULPopupType_tooltip )
    type.AssignWithConversion("tooltip");

  nsAutoString identifier;
  mElement->GetAttribute(type, identifier);

  if (identifier.IsEmpty())
    return rv;

  // Try to find the popup content and the document. We don't use FindDocumentForNode()
  // in this case because we need the nsIDocument interface anyway for the script 
  // context. 
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Turn the document into a XUL document so we can use getElementById
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (xulDocument == nsnull) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  // Handle the _child case for popups and context menus
  nsCOMPtr<nsIDOMElement> popupContent;

  if (identifier.EqualsWithConversion("_child")) {
    nsCOMPtr<nsIContent> popupset;
    nsCOMPtr<nsIContent> popup;
    GetImmediateChild(content, nsXULAtoms::popupset, getter_AddRefs(popupset));
    if (popupset) {
      GetImmediateChild(popupset, nsXULAtoms::popup, getter_AddRefs(popup));
      if (popup)
        popupContent = do_QueryInterface(popup);
    } else {
      nsCOMPtr<nsIDOMDocumentXBL> nsDoc(do_QueryInterface(xulDocument));
      nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(content));
      nsCOMPtr<nsIDOMNodeList> list;
      nsDoc->GetAnonymousNodes(xulElement, getter_AddRefs(list));
      if (list) {
        PRUint32 ctr,listLength;
        nsCOMPtr<nsIDOMNode> node;
        list->GetLength(&listLength);
        for (ctr = 0; ctr < listLength; ctr++) {
          list->Item(ctr, getter_AddRefs(node));
          nsCOMPtr<nsIContent> childContent(do_QueryInterface(node));
          nsCOMPtr<nsIAtom> childTag;
          childContent->GetTag(*getter_AddRefs(childTag));
          if (childTag.get() == nsXULAtoms::popupset) {
            GetImmediateChild(childContent, nsXULAtoms::popup, getter_AddRefs(popup));
            if (popup)
              popupContent = do_QueryInterface(popup);
            break;
          }
        }
      }
    }
  }
  else if (NS_FAILED(rv = xulDocument->GetElementById(identifier, getter_AddRefs(popupContent)))) {
    // Use getElementById to obtain the popup content and gracefully fail if 
    // we didn't find any popup content in the document. 
    NS_ERROR("GetElementById had some kind of spasm.");
    return rv;
  }
  if ( !popupContent )
    return NS_OK;

  // We have some popup content. Obtain our window.
  nsCOMPtr<nsIScriptContext> context;
  nsCOMPtr<nsIScriptGlobalObject> global;
  document->GetScriptGlobalObject(getter_AddRefs(global));
  if (global) {
    if (NS_OK == global->GetContext(getter_AddRefs(context))) {
      // Get the DOM window
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(global);
      if (domWindow != nsnull) {
        // Find out if we're anchored.
        nsAutoString anchorAlignment; anchorAlignment.AssignWithConversion("none");
        mElement->GetAttribute(NS_ConvertASCIItoUCS2("popupanchor"), anchorAlignment);

        nsAutoString popupAlignment; popupAlignment.AssignWithConversion("topleft");
        mElement->GetAttribute(NS_ConvertASCIItoUCS2("popupalign"), popupAlignment);

        PRInt32 xPos = aClientX, yPos = aClientY;
        
        mPopupContent = popupContent.get();

        nsCOMPtr<nsIDOMNode> parent;
        mPopupContent->GetParentNode(getter_AddRefs(parent));
        nsCOMPtr<nsIDOMXULElement> popupSetElement(do_QueryInterface(parent));
        nsCOMPtr<nsIBoxObject> boxObject;
        if (popupSetElement)
          popupSetElement->GetBoxObject(getter_AddRefs(boxObject));
        nsCOMPtr<nsIPopupSetBoxObject> popupSetObject(do_QueryInterface(boxObject));
        nsCOMPtr<nsIMenuBoxObject> menuObject(do_QueryInterface(boxObject));
        if (popupSetObject)
          popupSetObject->CreatePopup(mElement, mPopupContent, xPos, yPos, 
                                     type.GetUnicode(), anchorAlignment.GetUnicode(), 
                                     popupAlignment.GetUnicode());
        else if (menuObject)
          menuObject->OpenMenu(PR_TRUE);
      }
    }
  }

  return NS_OK;
}

//
// sTooltipCallback
//
// A timer callback, fired when the mouse has hovered inside of a frame for the 
// appropriate amount of time. Getting to this point means that we should show the
// toolip.
//
// This relies on certain things being cached into the |aClosure| object passed to
// us by the timer:
//   -- the x/y coordinates of the mouse
//   -- the dom node the user hovered over
//
void
XULPopupListenerImpl :: sTooltipCallback (nsITimer *aTimer, void *aClosure)
{
  XULPopupListenerImpl* self = NS_STATIC_CAST(XULPopupListenerImpl*, aClosure);
  if ( self && self->mPossibleTooltipNode ) {
    // set the node in the document that triggered the tooltip and show it
    nsCOMPtr<nsIDOMXULDocument> doc;
    self->FindDocumentForNode ( self->mElement, getter_AddRefs(doc) );
    if ( doc ) {
      // Make sure the target node is still attached to some document. It might have been deleted.
      nsCOMPtr<nsIDOMDocument> targetDoc;
      self->mPossibleTooltipNode->GetOwnerDocument(getter_AddRefs(targetDoc));
      if ( targetDoc ) {
        nsCOMPtr<nsIDOMElement> element ( do_QueryInterface(self->mPossibleTooltipNode) );
        if ( element ) {
          // check that node is enabled before showing tooltip
          nsAutoString disabledState;
          element->GetAttribute ( NS_ConvertASCIItoUCS2("disabled"), disabledState );
          if ( !disabledState.EqualsWithConversion("true") ) {
            doc->SetTooltipNode ( element );        
            doc->SetPopupNode ( element );        
            self->LaunchPopup (self->mMouseClientX, self->mMouseClientY+21);
          } // if node enabled
        } else {
          // Tooltip on non-element; e.g., text
          doc->SetTooltipNode ( self->mPossibleTooltipNode );        
          doc->SetPopupNode ( self->mPossibleTooltipNode );        
          self->LaunchPopup ( self->mMouseClientX, self->mMouseClientY+21);
        }
      } // if tooltip target's document exists
     } // if document

    // release tooltip target if there is one, NO MATTER WHAT
    self->mPossibleTooltipNode = nsnull;
  } // if "self" data valid
  
} // sTimerCallback



////////////////////////////////////////////////////////////////
nsresult
NS_NewXULPopupListener(nsIXULPopupListener** pop)
{
    XULPopupListenerImpl* popup = new XULPopupListenerImpl();
    if (!popup)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(popup);
    *pop = popup;
    return NS_OK;
}
