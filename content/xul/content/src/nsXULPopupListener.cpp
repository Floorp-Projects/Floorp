/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *
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

/*
  This file provides the implementation for xul popup listener which
  tracks xul popups, context menus, and tooltips
 */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsXULAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIXULPopupListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMContextMenuListener.h"
#include "nsIDOMKeyListener.h"
#include "nsContentCID.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMMouseEvent.h"
#include "nsITimer.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMEventTarget.h"

#include "nsIBoxObject.h"
#include "nsIPopupBoxObject.h"
#include "nsIMenuBoxObject.h"

// for event firing in context menus
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"

#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsIPref.h"


// on win32 and os/2, context menus come up on mouse up. On other platforms,
// they appear on mouse down. Certain bits of code care about this difference.
#ifndef XP_PC
#define NS_CONTEXT_MENU_IS_MOUSEUP 1
#endif


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
                             public nsIDOMKeyListener,
                             public nsIDOMMouseMotionListener,
                             public nsIDOMContextMenuListener
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
    NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
    NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
    NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent) ;

    // nsIDOMMouseMotionListener
    NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
    NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; };

    // nsIDOMContextMenuListener
    NS_IMETHOD ContextMenu(nsIDOMEvent* aContextMenuEvent);

    // nsIDOMKeyListener
    NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent) ;
    NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent) ;
    NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent) ;

    // nsIDOMEventListener
    NS_IMETHOD HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:

    virtual void KillTooltipTimer ( ) ;
    
    virtual nsresult LaunchPopup(nsIDOMEvent* anEvent);
    virtual nsresult LaunchPopup(PRInt32 aClientX, PRInt32 aClientY) ;
    virtual void ClosePopup ( ) ;

    void CreateAutoHideTimer ( ) ;
    
    nsresult FindDocumentForNode(nsIDOMNode* inNode, nsIDOMXULDocument** outDoc) ;

private:

      // various delays for tooltips
    enum {
      kTooltipAutoHideTime = 5000,       // 5000ms = 5 seconds
      kTooltipShowTime = 500             // 500ms = 0.5 seconds
    };
    
    nsresult PreLaunchPopup(nsIDOMEvent* aMouseEvent);
    nsresult FireFocusOnTargetContent(nsIDOMNode* aTargetNode);

    // |mElement| is the node to which this listener is attached.
    nsIDOMElement* mElement;               // Weak ref. The element will go away first.

    // The popup that is getting shown on top of mElement.
    nsIDOMElement* mPopupContent; 

    // The type of the popup
    XULPopupType popupType;
    
      // a timer for determining if a tooltip should be displayed. 
    nsCOMPtr<nsITimer> mTooltipTimer;
    static void sTooltipCallback ( nsITimer* aTimer, void* aListener ) ;
    PRInt32 mMouseClientX, mMouseClientY;       // mouse coordinates for tooltip event
  
      // a timer for auto-hiding the tooltip after a certain delay
    nsCOMPtr<nsITimer> mAutoHideTimer;
    static void sAutoHideCallback ( nsITimer* aTimer, void* aListener ) ;
    
      // pref callback for when the "show tooltips" pref changes
    static int sTooltipPrefChanged ( const char*, void* );

    // The following members are not used unless |popupType| is tooltip.
    static PRBool sShowTooltips;          // mirrors the "show tooltips" pref

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
      
PRBool XULPopupListenerImpl::sShowTooltips = PR_FALSE;


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
  NS_INTERFACE_MAP_ENTRY(nsIDOMContextMenuListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULPopupListener)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
XULPopupListenerImpl::Init(nsIDOMElement* aElement, const XULPopupType& popup)
{
  mElement = aElement; // Weak reference. Don't addref it.
  popupType = popup;
  static PRBool prefChangeRegistered = PR_FALSE;
  nsresult rv = NS_OK;

  // Only the first time, register the callback and get the initial value of the pref
  if ( !prefChangeRegistered ) {
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));  
    if ( prefs ) {
      // get the initial value of the pref
      rv = prefs->GetBoolPref("browser.chrome.toolbar_tips", &sShowTooltips);
      if ( NS_SUCCEEDED(rv) ) {
        // register the callback so we get notified of updates
        rv = prefs->RegisterCallback("browser.chrome.toolbar_tips", (PrefChangedFunc)sTooltipPrefChanged, nsnull);
        if ( NS_SUCCEEDED(rv) ){
          prefChangeRegistered = PR_TRUE;
        }
      }
    }
  }
  return rv;
}


// 
// sTooltipPrefChanged
//
// Called when the tooltip pref changes. Refetch it.
int 
XULPopupListenerImpl :: sTooltipPrefChanged (const char *, void * inData )
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if ( prefs ) {
    rv = prefs->GetBoolPref("browser.chrome.toolbar_tips", &sShowTooltips);
  }
  return NS_OK;

} // sTooltipPrefChanged


////////////////////////////////////////////////////////////////
// nsIDOMMouseListener

nsresult
XULPopupListenerImpl::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

nsresult
XULPopupListenerImpl::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if(popupType != eXULPopupType_context)
    return PreLaunchPopup(aMouseEvent);
  else
    return NS_OK;
}

nsresult
XULPopupListenerImpl::ContextMenu(nsIDOMEvent* aMouseEvent)
{
  if(popupType == eXULPopupType_context)
    return PreLaunchPopup(aMouseEvent);
  else 
    return NS_OK;
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

  // This is a gross hack to deal with a recursive popup situation happening in AIM code. 
  // See http://bugzilla.mozilla.org/show_bug.cgi?id=96920.
  // If a menu item child was clicked on that leads to a popup needing
  // to show, we know (guaranteed) that we're dealing with a menu or
  // submenu of an already-showing popup.  We don't need to do anything at all.
  if (popupType == eXULPopupType_popup) {
    nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);
    nsCOMPtr<nsIAtom> tag;
    targetContent->GetTag(*getter_AddRefs(tag));
    if (tag && (tag.get() == nsXULAtoms::menu || tag.get() == nsXULAtoms::menuitem))
      return NS_OK;
  }

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
      if (button == 0) {
        // Time to launch a popup menu.
        LaunchPopup(aMouseEvent);
        aMouseEvent->PreventBubble();
        aMouseEvent->PreventDefault();
      }
      break;
    case eXULPopupType_context:

    // Time to launch a context menu
#ifndef NS_CONTEXT_MENU_IS_MOUSEUP
    // If the context menu launches on mousedown,
    // we have to fire focus on the content we clicked on
    FireFocusOnTargetContent(targetNode);
#endif
    LaunchPopup(aMouseEvent);
    aMouseEvent->PreventBubble();
    aMouseEvent->PreventDefault();
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
    tempdoc->GetShellAt(0, getter_AddRefs(shell));  // Get nsIDOMElement for targetNode
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

  // don't kick off the timer or do any hard work if the pref is turned off
  if ( !sShowTooltips )
    return NS_OK;
  
  // stash the coordinates of the event so that we can still get back to it from within the 
  // timer callback. On win32, we'll get a MouseMove event even when a popup goes away --
  // even when the mouse doesn't change position! To get around this, we make sure the
  // mouse has really moved before proceeding.
  PRInt32 newMouseX, newMouseY;
  mouseEvent->GetClientX(&newMouseX);
  mouseEvent->GetClientY(&newMouseY);
  if ( mMouseClientX == newMouseX && mMouseClientY == newMouseY )
    return NS_OK;
  mMouseClientX = newMouseX; mMouseClientY = newMouseY;

  // as the mouse moves, we want to make sure we reset the timer to show it, 
  // so that the delay is from when the mouse stops moving, not when it enters
  // the node.
  KillTooltipTimer();
    
  // If the mouse moves while the popup is up, don't do anything. We make it
  // go away only if it times out or leaves the tooltip node. If nothing is
  // showing, though, we have to do the work. We can tell if we have a popup
  // showing because |mPopupContent| will be non-null.
  if ( !mPopupContent ) {
    mTooltipTimer = do_CreateInstance("@mozilla.org/timer;1");
    if ( mTooltipTimer ) {
      nsCOMPtr<nsIDOMEventTarget> eventTarget;
      aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
      if ( eventTarget )
        mPossibleTooltipNode = do_QueryInterface(eventTarget);
      if ( mPossibleTooltipNode ) {
        nsresult rv = mTooltipTimer->Init(sTooltipCallback, this, kTooltipShowTime, NS_PRIORITY_HIGH);
        if (NS_FAILED(rv))
          mPossibleTooltipNode = nsnull;
      }
    }
    else
      NS_WARNING ( "Could not create a timer for tooltip tracking" );
  }
   
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
  
  // mouse is leaving something which means it is in motion. Kill
  // our timer, but don't close anything just yet. We need to wait
  // and see if we're leaving the right node.
  KillTooltipTimer();
  
  // which node did the mouse leave?
  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
  nsCOMPtr<nsIDOMNode> eventNode ( do_QueryInterface(eventTarget) );
  
  // which node is our tooltip on?
  nsCOMPtr<nsIDOMXULDocument> doc;
  FindDocumentForNode ( mElement, getter_AddRefs(doc) );
  nsCOMPtr<nsIDOMNode> tooltipNode;
  doc->GetTooltipNode ( getter_AddRefs(tooltipNode) );
  
  // if they're the same, the mouse left the node the tooltip appeared on,
  // close the popup.
  if ( tooltipNode == eventNode )
    ClosePopup();
  return NS_OK;
    
} // MouseOut


//
// KeyDown
//
// If we're a tooltip, hide any tip that might be showing and remove any
// timer that is pending since the mouse is no longer over this area.
//
nsresult
XULPopupListenerImpl::KeyDown(nsIDOMEvent* aMouseEvent)
{
  // make sure we're a tooltip. if not, bail.
  if ( popupType != eXULPopupType_tooltip )
    return NS_OK;
  
  ClosePopup();
  return NS_OK;
    
} // KeyDown


//
// KeyUp
// KeyPress
//
// We can ignore these as they are already handled by KeyDown
//
nsresult
XULPopupListenerImpl::KeyUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
    
} // KeyUp

nsresult
XULPopupListenerImpl::KeyPress(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
    
} // KeyPress


//
// KillTooltipTimer
//
// Stop the timer that would show a tooltip dead in its tracks
//
void
XULPopupListenerImpl :: KillTooltipTimer ( )
{
  if ( mTooltipTimer ) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nsnull;
    mPossibleTooltipNode = nsnull;      // release tooltip target
  }
} // KillTooltipTimer


//
// ClosePopup
//
// Do everything needed to shut down the popup, including killing off all
// the timers that may be involved.
//
// NOTE: This routine is safe to call even if the popup is already closed.
//
void
XULPopupListenerImpl :: ClosePopup ( )
{
  KillTooltipTimer();
  if ( mAutoHideTimer ) {
    mAutoHideTimer->Cancel();
    mAutoHideTimer = nsnull;
  }
  
  if ( mPopupContent ) {
    nsCOMPtr<nsIDOMXULElement> popupElement(do_QueryInterface(mPopupContent));
    nsCOMPtr<nsIBoxObject> boxObject;
    if (popupElement)
      popupElement->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsIPopupBoxObject> popupObject(do_QueryInterface(boxObject));
    if (popupObject)
      popupObject->HidePopup();

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

static void ConvertPosition(nsIDOMElement* aPopupElt, nsString& aAnchor, nsString& aAlign, PRInt32& aY)
{
  nsAutoString position;
  aPopupElt->GetAttribute(NS_LITERAL_STRING("position"), position);
  if (position.IsEmpty())
    return;

  if (position.Equals(NS_LITERAL_STRING("before_start"))) {
    aAnchor.AssignWithConversion("topleft");
    aAlign.AssignWithConversion("bottomleft");
  }
  else if (position.Equals(NS_LITERAL_STRING("before_end"))) {
    aAnchor.AssignWithConversion("topright");
    aAlign.AssignWithConversion("bottomright");
  }
  else if (position.Equals(NS_LITERAL_STRING("after_start"))) {
    aAnchor.AssignWithConversion("bottomleft");
    aAlign.AssignWithConversion("topleft");
  }
  else if (position.Equals(NS_LITERAL_STRING("after_end"))) {
    aAnchor.AssignWithConversion("bottomright");
    aAlign.AssignWithConversion("topright");
  }
  else if (position.Equals(NS_LITERAL_STRING("start_before"))) {
    aAnchor.AssignWithConversion("topleft");
    aAlign.AssignWithConversion("topright");
  }
  else if (position.Equals(NS_LITERAL_STRING("start_after"))) {
    aAnchor.AssignWithConversion("bottomleft");
    aAlign.AssignWithConversion("bottomright");
  }
  else if (position.Equals(NS_LITERAL_STRING("end_before"))) {
    aAnchor.AssignWithConversion("topright");
    aAlign.AssignWithConversion("topleft");
  }
  else if (position.Equals(NS_LITERAL_STRING("end_after"))) {
    aAnchor.AssignWithConversion("bottomright");
    aAlign.AssignWithConversion("bottomleft");
  }
  else if (position.Equals(NS_LITERAL_STRING("overlap"))) {
    aAnchor.AssignWithConversion("topleft");
    aAlign.AssignWithConversion("topleft");
  }
  else if (position.Equals(NS_LITERAL_STRING("after_pointer")))
    aY += 21;
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

  if (identifier.IsEmpty()) {
    if (type.EqualsIgnoreCase("popup"))
      mElement->GetAttribute(NS_LITERAL_STRING("menu"), identifier);
    else if (type.EqualsIgnoreCase("context"))
      mElement->GetAttribute(NS_LITERAL_STRING("contextmenu"), identifier);
    if (identifier.IsEmpty())
      return rv;
  }

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

  if (identifier == NS_LITERAL_STRING("_child")) {
    nsCOMPtr<nsIContent> popup;
    
    nsIAtom* tag = (popupType == eXULPopupType_tooltip) ? nsXULAtoms::tooltip : nsXULAtoms::menupopup;

    GetImmediateChild(content, tag, getter_AddRefs(popup));
    if (popup)
      popupContent = do_QueryInterface(popup);
    else {
      nsCOMPtr<nsIDOMDocumentXBL> nsDoc(do_QueryInterface(xulDocument));
      nsCOMPtr<nsIDOMNodeList> list;
      nsDoc->GetAnonymousNodes(mElement, getter_AddRefs(list));
      if (list) {
        PRUint32 ctr,listLength;
        nsCOMPtr<nsIDOMNode> node;
        list->GetLength(&listLength);
        for (ctr = 0; ctr < listLength; ctr++) {
          list->Item(ctr, getter_AddRefs(node));
          nsCOMPtr<nsIContent> childContent(do_QueryInterface(node));
          nsCOMPtr<nsIAtom> childTag;
          childContent->GetTag(*getter_AddRefs(childTag));
          if (childTag.get() == tag) {
            popupContent = do_QueryInterface(childContent);
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
    if ((NS_OK == global->GetContext(getter_AddRefs(context))) && context) {
      // Get the DOM window
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(global);
      if (domWindow != nsnull) {
        // Find out if we're anchored.
        mPopupContent = popupContent.get();

        nsAutoString anchorAlignment;
        mPopupContent->GetAttribute(NS_LITERAL_STRING("popupanchor"), anchorAlignment);

        nsAutoString popupAlignment;
        mPopupContent->GetAttribute(NS_LITERAL_STRING("popupalign"), popupAlignment);

        PRInt32 xPos = aClientX, yPos = aClientY;
        
        ConvertPosition(mPopupContent, anchorAlignment, popupAlignment, yPos);
        if (!anchorAlignment.IsEmpty() && !popupAlignment.IsEmpty())
          xPos = yPos = -1;

        nsCOMPtr<nsIBoxObject> popupBox;
        nsCOMPtr<nsIDOMXULElement> xulPopupElt(do_QueryInterface(mPopupContent));
        xulPopupElt->GetBoxObject(getter_AddRefs(popupBox));
        nsCOMPtr<nsIPopupBoxObject> popupBoxObject(do_QueryInterface(popupBox));
        if (popupBoxObject)
          popupBoxObject->ShowPopup(mElement, mPopupContent, xPos, yPos, 
                                    type.get(), anchorAlignment.get(), 
                                    popupAlignment.get());
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
// This relies on certain things being cached into the |aPopupListener|
// object passed to us by the timer:
//   -- the x/y coordinates of the mouse      (mMouseClientY, mMouseClientX)
//   -- the dom node the user hovered over    (mPossibleTooltipNode)
//
void
XULPopupListenerImpl :: sTooltipCallback (nsITimer *aTimer, void *aPopupListener)
{
  XULPopupListenerImpl* self = NS_STATIC_CAST(XULPopupListenerImpl*, aPopupListener);
  if ( self && self->mPossibleTooltipNode ) {
    // set the node in the document that triggered the tooltip and show it
    nsCOMPtr<nsIDOMXULDocument> doc;
    self->FindDocumentForNode ( self->mElement, getter_AddRefs(doc) );
    if ( doc ) {
      // Make sure the target node is still attached to some document. It might have been deleted.
      nsCOMPtr<nsIDOMDocument> targetDoc;
      self->mPossibleTooltipNode->GetOwnerDocument(getter_AddRefs(targetDoc));
      if ( targetDoc ) {
        doc->SetTooltipNode ( self->mPossibleTooltipNode );        
        doc->SetPopupNode ( self->mPossibleTooltipNode );
        self->LaunchPopup ( self->mMouseClientX, self->mMouseClientY+21);
        
        // at this point, |mPopupContent| holds the content node of
        // the popup. If there is an attribute on the popup telling us
        // not to create the auto-hide timer, don't.
        if ( self->mPopupContent ) {
          nsAutoString noAutoHide;
          self->mPopupContent->GetAttribute ( NS_LITERAL_STRING("noautohide"), noAutoHide );
          if ( noAutoHide != NS_LITERAL_STRING("true") )
            self->CreateAutoHideTimer();          
        }

      } // if tooltip target's document exists
     } // if document

    // release tooltip target if there is one, NO MATTER WHAT
    self->mPossibleTooltipNode = nsnull;
  } // if "self" data valid
  
} // sTooltipCallback


//
// CreateAutoHideTimer
//
// Create a new timer to see if we should auto-hide. It's ok if this fails.
//
void
XULPopupListenerImpl :: CreateAutoHideTimer ( )
{
  // just to be anal (er, safe)
  if ( mAutoHideTimer ) {
    mAutoHideTimer->Cancel();
    mAutoHideTimer = nsnull;
  }
  
  mAutoHideTimer = do_CreateInstance("@mozilla.org/timer;1");
  if ( mAutoHideTimer )
    mAutoHideTimer->Init(sAutoHideCallback, this, kTooltipAutoHideTime, NS_PRIORITY_HIGH);

} // CreateAutoHideTimer


//
// sAutoHideCallback
//
// This fires after a tooltip has been open for a certain length of time. Just tell
// the listener to close the popup. We don't have to worry, because ClosePopup() can
// be called multiple times, even if the popup has already been closed.
//
void
XULPopupListenerImpl :: sAutoHideCallback ( nsITimer *aTimer, void* aListener )
{
  XULPopupListenerImpl* self = NS_STATIC_CAST(XULPopupListenerImpl*, aListener);
  if ( self )
    self->ClosePopup();

  // NOTE: |aTimer| and |self->mAutoHideTimer| are invalid after calling ClosePopup();
  
} // sAutoHideCallback


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
