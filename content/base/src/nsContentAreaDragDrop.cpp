/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- *//* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mike Pinkerton <pinkerton@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsReadableUtils.h"

// Local Includes
#include "nsContentAreaDragDrop.h"

// Helper Classes
#include "nsString.h"
#include "nsXPIDLString.h"

// Interfaces needed to be included
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMUIEvent.h"
#include "nsISelection.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMRange.h"
#include "nsIFormControl.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsITransferable.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIServiceManagerUtils.h"
#include "nsPromiseFlatString.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIWebNavigation.h"
#include "nsIDragDropOverride.h"
#include "nsIContent.h"
#include "nsIXMLContent.h"
#include "nsINameSpaceManager.h"
#include "nsUnicharUtils.h"
#include "nsHTMLAtoms.h"
#include "nsIURI.h"
#include "nsIImage.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIImageFrame.h"
#include "nsIFrame.h"
#include "nsLayoutAtoms.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDocumentEncoder.h"



NS_IMPL_ADDREF(nsContentAreaDragDrop)
NS_IMPL_RELEASE(nsContentAreaDragDrop)

NS_INTERFACE_MAP_BEGIN(nsContentAreaDragDrop)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDragListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMDragListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMDragListener)
    NS_INTERFACE_MAP_ENTRY(nsIDragDropHandler)
NS_INTERFACE_MAP_END


//
// nsContentAreaDragDrop ctor
//
nsContentAreaDragDrop::nsContentAreaDragDrop ( ) 
  : mListenerInstalled(PR_FALSE), mNavigator(nsnull), mOverrideDrag(nsnull), mOverrideDrop(nsnull)
{
} // ctor


//
// ChromeTooltipListener dtor
//
nsContentAreaDragDrop::~nsContentAreaDragDrop ( )
{
  RemoveDragListener();

} // dtor


NS_IMETHODIMP
nsContentAreaDragDrop::HookupTo(nsIDOMEventTarget *inAttachPoint, nsIWebNavigation* inNavigator,
                                  nsIOverrideDragSource* inOverrideDrag, nsIOverrideDropSite* inOverrideDrop)
{
  NS_ASSERTION(inAttachPoint, "Can't hookup Drag Listeners to NULL receiver");
  mEventReceiver = do_QueryInterface(inAttachPoint);
  NS_ASSERTION(mEventReceiver, "Target doesn't implement nsIDOMEventReceiver as needed");
  mNavigator = inNavigator;
  
  mOverrideDrag = inOverrideDrag;
  mOverrideDrop = inOverrideDrop;
  
  return AddDragListener();
}


NS_IMETHODIMP
nsContentAreaDragDrop::Detach()
{
  return RemoveDragListener();
}


//
// AddDragListener
//
// Subscribe to the events that will allow us to track drags.
//
NS_IMETHODIMP
nsContentAreaDragDrop::AddDragListener()
{
  nsresult rv = NS_ERROR_FAILURE;
  
  if ( mEventReceiver ) {
    nsIDOMDragListener *pListener = NS_STATIC_CAST(nsIDOMDragListener *, this);
    rv = mEventReceiver->AddEventListenerByIID(pListener, NS_GET_IID(nsIDOMDragListener));
    if (NS_SUCCEEDED(rv))
      mListenerInstalled = PR_TRUE;
  }

  return rv;
}


//
// RemoveDragListener
//
// Unsubscribe from all the various drag events that we were listening to. 
//
NS_IMETHODIMP 
nsContentAreaDragDrop::RemoveDragListener()
{
  nsresult rv = NS_ERROR_FAILURE;
  
  if (mEventReceiver) {
    nsIDOMDragListener *pListener = NS_STATIC_CAST(nsIDOMDragListener *, this);
    rv = mEventReceiver->RemoveEventListenerByIID(pListener, NS_GET_IID(nsIDOMDragListener));
    if (NS_SUCCEEDED(rv))
      mListenerInstalled = PR_FALSE;
    mEventReceiver = nsnull;
  }

  return rv;
}



//
// DragEnter
//
// Called when an OS drag is in process and the mouse enters a gecko window. We
// don't care so much about dragEnters.
//
NS_IMETHODIMP
nsContentAreaDragDrop::DragEnter(nsIDOMEvent* aMouseEvent)
{
  // nothing really to do here.
  return NS_OK;
}


//
// DragOver
//
// Called when an OS drag is in process and the mouse is over a gecko window. 
// The main purpose of this routine is to set the |canDrop| property on the
// drag session to false if we want to disallow the drop so that the OS can
// provide the appropriate feedback. All this does is show feedback, it
// doesn't actually cancel the drop; that comes later.
//
NS_IMETHODIMP
nsContentAreaDragDrop::DragOver(nsIDOMEvent* inEvent)
{
  // first check that someone hasn't already handled this event
  PRBool preventDefault = PR_TRUE;
  nsCOMPtr<nsIDOMNSUIEvent> nsuiEvent(do_QueryInterface(inEvent));
  if ( nsuiEvent )
    nsuiEvent->GetPreventDefault(&preventDefault);
  if ( preventDefault )
    return NS_OK;

  // if the drag originated w/in this content area, bail
  // early. This avoids loading a URL dragged from the content
  // area into the very same content area (which is almost never
  // the desired action). This code is a bit too simplistic and
  // may have problems with nested frames.
  nsCOMPtr<nsIDragService> dragService(do_GetService("@mozilla.org/widget/dragservice;1"));
  if ( !dragService )
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDragSession> session;
  dragService->GetCurrentSession(getter_AddRefs(session));
  if ( session ) {
    // if the client has provided an override callback, check if we
    // the drop is allowed. If it allows it, we should still protect against
    // dropping w/in the same document.
    PRBool dropAllowed = PR_TRUE;
    if ( mOverrideDrop )
      mOverrideDrop->AllowDrop(inEvent, session, &dropAllowed);    
    nsCOMPtr<nsIDOMDocument> sourceDoc;
    session->GetSourceDocument(getter_AddRefs(sourceDoc));
    nsCOMPtr<nsIDOMDocument> eventDoc;
    GetEventDocument(inEvent, getter_AddRefs(eventDoc));
    if ( sourceDoc == eventDoc )
      dropAllowed = PR_FALSE;
    session->SetCanDrop(dropAllowed);
  }
  
  return NS_OK;
}


//
// DragExit
//
// Called when an OS drag is in process and the mouse is over a gecko window. We
// don't care so much about dragExits.
//
NS_IMETHODIMP
nsContentAreaDragDrop::DragExit(nsIDOMEvent* aMouseEvent)
{
  // nothing really to do here.
  return NS_OK;
}


//
// ExtractURLFromData
//
// build up a url from whatever data we get from the OS. How we interpret the
// data depends on the flavor as it tells us the nsISupports* primitive type
// we have.
//
void
nsContentAreaDragDrop::ExtractURLFromData(const nsACString & inFlavor, nsISupports* inDataWrapper, PRUint32 inDataLen,
                                            nsAString & outURL)
{
  if ( !inDataWrapper )
    return;
  outURL.Truncate();
  
  if ( inFlavor.Equals(kUnicodeMime) ) {
    // the data is regular unicode, just go with what we get. It may be a url, it
    // may not be. *shrug*
    nsCOMPtr<nsISupportsString> stringData(do_QueryInterface(inDataWrapper));
    if ( stringData ) {
      stringData->GetData(outURL);
    }
  }
  else if ( inFlavor.Equals(kURLMime) ) {
    // the data is an internet shortcut of the form <url>\n<title>. Strip
    // out the url piece and return that.
    nsCOMPtr<nsISupportsString> stringData(do_QueryInterface(inDataWrapper));
    if ( stringData ) {
      nsAutoString data;
      stringData->GetData(data);
      PRInt32 separator = data.FindChar('\n');
      if ( separator >= 0 )
        outURL = Substring(data, 0, separator);
      else
        outURL = data;
    }  
  }
  else if ( inFlavor.Equals(kFileMime) ) {
    // the data is a file. Use the necko parsing utils to get a file:// url
    // from the OS data.
    nsCOMPtr<nsIFile> file(do_QueryInterface(inDataWrapper));
    if ( file ) {
      nsCAutoString url;
      NS_GetURLSpecFromFile(file, url);
      outURL = NS_ConvertUTF8toUCS2(url);
    } 
  }
}


//
// DragDrop
//
// Called when an OS drag is in process and the mouse is released a gecko window.
// Extract the data from the OS and do something with it.
//
NS_IMETHODIMP
nsContentAreaDragDrop::DragDrop(nsIDOMEvent* inMouseEvent)
{
  // if we don't have a nsIWebNavigation object to do anything with,
  // just bail. The client will have to have another way to deal with it
  if ( !mNavigator )
    return NS_OK;

  // check that someone hasn't already handled this event
  PRBool preventDefault = PR_TRUE;
  nsCOMPtr<nsIDOMNSUIEvent> nsuiEvent(do_QueryInterface(inMouseEvent));
  if ( nsuiEvent )
    nsuiEvent->GetPreventDefault(&preventDefault);
  if ( preventDefault )
    return NS_OK;

  // pull the transferable out of the drag service. at the moment, we
  // only care about the first item of the drag. We don't allow dropping
  // multiple items into a content area.
  nsCOMPtr<nsIDragService> dragService(do_GetService("@mozilla.org/widget/dragservice;1"));
  if ( !dragService )
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDragSession> session;
  dragService->GetCurrentSession(getter_AddRefs(session));
  if ( !session )
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsITransferable> trans(do_CreateInstance("@mozilla.org/widget/transferable;1"));
  if ( !trans )
    return NS_ERROR_FAILURE;
    
  // add the relevant flavors. order is important (higest fidelity to lowest)
  trans->AddDataFlavor(kURLMime);
  trans->AddDataFlavor(kFileMime);
  trans->AddDataFlavor(kUnicodeMime);

  nsresult rv = session->GetData(trans, 0);     // again, we only care about the first object
  if ( NS_SUCCEEDED(rv) ) {
    // if the client has provided an override callback, call it. It may
    // still return that we should continue processing.
    if ( mOverrideDrop ) {
      PRBool actionHandled = PR_FALSE;
      if ( NS_SUCCEEDED(mOverrideDrop->DropAction(inMouseEvent, trans, &actionHandled)) )
        if ( actionHandled )
          return NS_OK;
    }
    nsXPIDLCString flavor;
    nsCOMPtr<nsISupports> dataWrapper;
    PRUint32 dataLen = 0;
    rv = trans->GetAnyTransferData(getter_Copies(flavor), getter_AddRefs(dataWrapper), &dataLen);
    if ( NS_SUCCEEDED(rv) && dataLen > 0 ) {
      // get the url from one of several possible formats
      nsAutoString url;
      ExtractURLFromData(flavor, dataWrapper, dataLen, url);
      NS_ASSERTION(!url.IsEmpty(), "Didn't get anything we can use as a url");
      
      // valid urls don't have spaces. bail if this does.
      if ( url.IsEmpty() || url.FindChar(' ') >= 0 )
        return NS_OK;

      // ok, we have the url, load it.
      mNavigator->LoadURI(url.get(), nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);
    }
  }
  
  return NS_OK;
}


//
// FindFirstAnchor
//
// A recursive routine that finds the first child link initiating anchor
//
void
nsContentAreaDragDrop::FindFirstAnchor(nsIDOMNode* inNode, nsIDOMNode** outAnchor)
{
  if ( !inNode && !outAnchor )
    return;
  *outAnchor = nsnull;
    
  static NS_NAMED_LITERAL_STRING(simple, "simple");

  nsCOMPtr<nsIDOMNode> curr = inNode;
  while ( curr ) {
    // check me (base case of recursion)
    PRUint16 nodeType = 0;
    curr->GetNodeType(&nodeType);
    if ( nodeType == nsIDOMNode::ELEMENT_NODE ) {
      // a?
      nsCOMPtr<nsIDOMHTMLAnchorElement> a(do_QueryInterface(curr));
      if (a) {
        *outAnchor = curr;
        NS_ADDREF(*outAnchor);
        return;
      }

      // area?
      nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(curr));
      if (area) {
        *outAnchor = curr;
        NS_ADDREF(*outAnchor);
        return;
      }

      // Simple XLink?
      nsCOMPtr<nsIContent> content(do_QueryInterface(curr));
      NS_WARN_IF_FALSE(content, "DOM node is not content?");
      if (!content)
        return;
      nsAutoString value;
      content->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::type, value);
      if (value.Equals(simple)) {
        *outAnchor = curr;
        NS_ADDREF(*outAnchor);
        return;
      }
    }
    
    // recursively check my children
    nsCOMPtr<nsIDOMNode> firstChild;
    curr->GetFirstChild(getter_AddRefs(firstChild));
    FindFirstAnchor(firstChild, outAnchor);
    if ( *outAnchor )
      return;
      
    // check my siblings
    nsIDOMNode* temp = nsnull;
    curr->GetNextSibling(&temp);
    curr = dont_AddRef(temp);
  }

}

//
// FindParentLinkNode
//
// Finds the parent with the given link tag starting at |inNode|. If it gets 
// up to <body> or <html> or the top document w/out finding it, we
// stop looking and |outParent| will be null.
//
void
nsContentAreaDragDrop::FindParentLinkNode(nsIDOMNode* inNode, 
                                          nsIDOMNode** outParent)
{
  if ( !inNode || !outParent )
    return;
  *outParent = nsnull;
  nsCOMPtr<nsIDOMNode> node(inNode);      // to make refcounting easier
  
  PRUint16 nodeType = 0;
  inNode->GetNodeType(&nodeType);
  if ( nodeType == nsIDOMNode::TEXT_NODE )
    inNode->GetParentNode(getter_AddRefs(node));
  
  static NS_NAMED_LITERAL_STRING(document, "#document");
  static NS_NAMED_LITERAL_STRING(simple, "simple");

  while ( node ) {
    // (X)HTML body or html?
    node->GetNodeType(&nodeType);
    if ( nodeType == nsIDOMNode::ELEMENT_NODE ) {
      // body?
      nsCOMPtr<nsIDOMHTMLBodyElement> body(do_QueryInterface(node));
      if (body) {
        return;
      }

      // html?
      nsCOMPtr<nsIDOMHTMLHtmlElement> html(do_QueryInterface(node));
      if (html) {
        return;
      }
    }

    // Other document root?
    nsAutoString localName;
    node->GetLocalName(localName);
    if ( localName.IsEmpty() )
      return;
    if ( localName.Equals(document, nsCaseInsensitiveStringComparator()) )
      return; // XXX Check if #document always lower case

    if ( nodeType == nsIDOMNode::ELEMENT_NODE ) {
      PRBool found = PR_FALSE;
      nsCOMPtr<nsIDOMHTMLAnchorElement> a(do_QueryInterface(node));
      if (a) {
        found = PR_TRUE;
      } else {
        nsCOMPtr<nsIContent> content(do_QueryInterface(node));
        NS_WARN_IF_FALSE(content, "DOM node is not content?");
        if (!content)
          return;
        nsAutoString value;
        content->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::type, value);
        if (value.Equals(simple)) {
          found = PR_TRUE;
        }
      }
      if (found) {
        *outParent = node;
        NS_ADDREF(*outParent);
        return;
      }
    }
    
    // keep going, up to parent
    nsIDOMNode* temp;
    node->GetParentNode(&temp);
    node = dont_AddRef(temp);
  }
}


//
// GetAnchorURL
//
// Get the url for this anchor. First try the href, and if that's empty,
// go for the name.
//
void
nsContentAreaDragDrop::GetAnchorURL(nsIDOMNode* inNode, nsAString& outURL)
{
  outURL.Truncate();

  // a?
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(inNode));
  if ( anchor ) {
    anchor->GetHref(outURL);
    if ( outURL.IsEmpty() )
     anchor->GetName(outURL);
  } else {
    // area?
    nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(inNode));
    if ( area ) {
      area->GetHref(outURL);
      if ( outURL.IsEmpty() ) {
        nsCOMPtr<nsIDOMHTMLElement> e(do_QueryInterface(inNode));
        e->GetId(outURL);
      }
    } else {
      // Try XLink next...
      nsCOMPtr<nsIContent> content(do_QueryInterface(inNode));
      nsAutoString value;
      content->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::type, value);
      if (value.Equals(NS_LITERAL_STRING("simple"))) {
        content->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::href, value);
        if (!value.IsEmpty()) {
          nsCOMPtr<nsIXMLContent> xml(do_QueryInterface(inNode));
          if (xml) {
            nsCOMPtr<nsIURI> baseURI;
            if (NS_SUCCEEDED(xml->GetXMLBaseURI(getter_AddRefs(baseURI)))) {
              nsCAutoString absoluteSpec;
              baseURI->Resolve(NS_ConvertUCS2toUTF8(value), absoluteSpec);
              outURL = NS_ConvertUTF8toUCS2(absoluteSpec);
            }
          }
        }
      } else {
        // ... or just get the ID
        nsCOMPtr<nsIXMLContent> xml(do_QueryInterface(inNode));
        nsCOMPtr<nsIAtom> id;
        if (xml && NS_SUCCEEDED(xml->GetID(*getter_AddRefs(id))) && id) {
          id->ToString(outURL);
        }
      }
    }
  }
}


//
// CreateLinkText
//
// Creates the html for an anchor in the form
//  <a href="inURL">inText</a>
// 
void
nsContentAreaDragDrop::CreateLinkText(const nsAString& inURL, const nsAString & inText,
                                        nsAString& outLinkText)
{
  // use a temp var in case |inText| is the same string as |outLinkText| to
  // avoid overwriting it while building up the string in pieces.
  nsAutoString linkText(NS_LITERAL_STRING("<a href=\"") +
                        inURL +
                        NS_LITERAL_STRING("\">") +
                        inText +
                        NS_LITERAL_STRING("</a>") );
  
  outLinkText = linkText;
}


//
// GetNodeString
//
// Gets the text associated with a node
//
void
nsContentAreaDragDrop::GetNodeString(nsIDOMNode* inNode, nsAString & outNodeString)
{
  outNodeString.Truncate();
  
  // use a range to get the text-equivalent of the node
  nsCOMPtr<nsIDOMDocument> doc;
  inNode->GetOwnerDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIDOMDocumentRange> docRange(do_QueryInterface(doc));
  if ( docRange ) {
    nsCOMPtr<nsIDOMRange> range;
    docRange->CreateRange(getter_AddRefs(range));
    if ( range ) {
      range->SelectNode(inNode);
      range->ToString(outNodeString);
    }
  }
}


//
// NormalizeSelection
//
void
nsContentAreaDragDrop::NormalizeSelection(nsIDOMNode* inBaseNode, nsISelection* inSelection)
{
  nsCOMPtr<nsIDOMNode> parent;
  inBaseNode->GetParentNode(getter_AddRefs(parent));
  if ( !parent || !inSelection )
    return;

  nsCOMPtr<nsIDOMNodeList> childNodes;
  parent->GetChildNodes(getter_AddRefs(childNodes));
  if ( !childNodes )
    return;
  PRUint32 listLen = 0;
  childNodes->GetLength(&listLen);
  PRUint32 index = 0;
  for ( ; index < listLen; ++index ) {
    nsCOMPtr<nsIDOMNode> indexedNode;
    childNodes->Item(index, getter_AddRefs(indexedNode));
    if ( indexedNode == inBaseNode )
      break;
  } 
  if ( index >= listLen )
    return;
    
  // now make the selection contain all of |inBaseNode|'s siblings up to and including
  // |inBaseNode|
  inSelection->Collapse(parent, index);
  inSelection->Extend(parent, index+1);
}


//
// GetEventDocument
//
// Get the DOM document associated with a given DOM event
//
void
nsContentAreaDragDrop::GetEventDocument(nsIDOMEvent* inEvent, nsIDOMDocument** outDocument)
{
  if ( !outDocument )
    return;
  *outDocument = nsnull;
  
  nsCOMPtr<nsIDOMUIEvent> uiEvent(do_QueryInterface(inEvent));
  if ( uiEvent ) {
    nsCOMPtr<nsIDOMAbstractView> view;
    uiEvent->GetView(getter_AddRefs(view));
    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(view));
    if ( window )
      window->GetDocument(outDocument);
  }
}


//
// BuildDragData
//
// Given the drag event, build up the url, title, and html data. 
//
PRBool
nsContentAreaDragDrop::BuildDragData(nsIDOMEvent* inMouseEvent, nsAString & outURLString, nsAString & outTitleString,
                                        nsAString & outHTMLString, nsIImage** outImage, PRBool* outIsAnchor)
{
  if ( !outIsAnchor || !outImage )
    return PR_FALSE;
 
  outURLString.Truncate();
  outTitleString.Truncate();
  outHTMLString.Truncate();
  *outImage = nsnull; 
  *outIsAnchor = PR_FALSE;
  
  nsCOMPtr<nsIDOMUIEvent> uiEvent(do_QueryInterface(inMouseEvent));
  if ( !uiEvent )
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMEventTarget> target;
  inMouseEvent->GetTarget(getter_AddRefs(target));

  PRBool isAltKeyDown = PR_FALSE;
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(inMouseEvent));
  if ( mouseEvent )
    mouseEvent->GetAltKey(&isAltKeyDown);
  
  // only drag form elements by using the alt key,
  // otherwise buttons and select widgets are hard to use
  nsCOMPtr<nsIFormControl> form(do_QueryInterface(target));
  if ( form && !isAltKeyDown )
    return PR_FALSE;
  
  // the resulting strings from the beginning of the drag
  nsAutoString urlString;
  nsXPIDLString titleString;
  nsXPIDLString htmlString;        // will be filled automatically if you fill urlstring
  PRBool startDrag = PR_TRUE;
  nsCOMPtr<nsIDOMNode> draggedNode(do_QueryInterface(target));
  
  // find the selection to see what we could be dragging and if
  // what we're dragging is in what is selected.
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMAbstractView> view;
  uiEvent->GetView(getter_AddRefs(view));
  nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(view));
  if ( window )
    window->GetSelection(getter_AddRefs(selection));
  PRBool isCollapsed = PR_FALSE;
  PRBool containsTarget = PR_FALSE;
  if ( selection ) {
    selection->GetIsCollapsed(&isCollapsed);
    selection->ContainsNode(draggedNode, PR_FALSE, &containsTarget);
  }
    
  PRBool getFormattedStrings = PR_FALSE;

  if ( selection && !isCollapsed && containsTarget ) {
    // track down the anchor node, if any, for the url
    nsCOMPtr<nsIDOMNode> selectionStart;
    selection->GetAnchorNode(getter_AddRefs(selectionStart));
    nsCOMPtr<nsIDOMNode> firstAnchor;
    FindFirstAnchor(selectionStart, getter_AddRefs(firstAnchor)); 
    if ( firstAnchor ) {
      PRBool anchorInSelection = PR_FALSE;
      selection->ContainsNode(firstAnchor, PR_FALSE, &anchorInSelection);
      if ( anchorInSelection ) {
        *outIsAnchor = PR_TRUE;
        GetAnchorURL(firstAnchor, urlString);
      }
    }
      
    getFormattedStrings = PR_TRUE;
  } // if drag is within selection
  else {
    // if the alt key is down, don't start a drag if we're in an anchor because
    // we want to do selection.
    nsCOMPtr<nsIDOMNode> parentAnchor;
    FindParentLinkNode(draggedNode, 
                   getter_AddRefs(parentAnchor));
    if ( isAltKeyDown && parentAnchor )
      return NS_OK;
    
    nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(draggedNode));    
    if ( area ) {
      *outIsAnchor = PR_TRUE;
      // grab the href as the url, use alt text as the title of the area if it's there.
      // the drag data is the image tag and src attribute.
      area->GetAttribute(NS_LITERAL_STRING("href"), urlString);
      area->GetAttribute(NS_LITERAL_STRING("alt"), titleString);
      if ( titleString.IsEmpty() )
        titleString = urlString;
      htmlString = NS_LITERAL_STRING("<img src=\"") +
        urlString +
        NS_LITERAL_STRING("\">");
    } // area
    else {
      nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(draggedNode));

      if (img && parentAnchor) {
        // If we are dragging around an image in an anchor, then we
        // are dragging the entire anchor!

        draggedNode = parentAnchor;
        img = nsnull;
      }

      if ( img ) {
        *outIsAnchor = PR_TRUE;
        // grab the href as the url, use alt text as the title of the area if it's there.
        // the drag data is the image tag and src attribute.
        img->GetSrc(urlString);
        img->GetAttribute(NS_LITERAL_STRING("alt"), titleString);
        if ( titleString.IsEmpty() )
          titleString = urlString;
        htmlString = NS_LITERAL_STRING("<img src=\"") +
          urlString +
          NS_LITERAL_STRING("\">");

        // also grab the image data
        GetImageFromDOMNode(draggedNode, outImage);
        // select siblings up to and including the selected link. this
        // shouldn't be fatal, and we should still do the drag if this fails
        NormalizeSelection(draggedNode, selection);
      } // img
      else {
        nsCOMPtr<nsIDOMHTMLLinkElement> link(do_QueryInterface(draggedNode));
        if ( link ) {
          *outIsAnchor = PR_TRUE;
          GetAnchorURL(draggedNode, urlString);
          GetNodeString(draggedNode, titleString);
        } // link
        else {
          nsCOMPtr<nsIDOMNode> linkNode;
          FindParentLinkNode(draggedNode, getter_AddRefs(linkNode));
          if ( linkNode ) {
            *outIsAnchor = PR_TRUE;
            GetAnchorURL(linkNode, urlString);
        
            // select siblings up to and including the selected link. this
            // shouldn't be fatal, and we should still do the drag if this fails
            NormalizeSelection(linkNode, selection);
            getFormattedStrings = PR_TRUE;
          }
          else {
            // indicate that we don't allow drags in this case
            startDrag = PR_FALSE;
          }
        }
      }
    }
  } // else no selection or drag outside it

  if (getFormattedStrings && selection) {
    // find the title for the drag and any associated html
    nsCOMPtr<nsISelectionPrivate> privSelection(do_QueryInterface(selection));
    if ( privSelection ) {        
      // the window has a selection so we should grab that rather
      // than looking for specific elements
      privSelection->ToStringWithFormat("text/html",
                                        nsIDocumentEncoder::OutputAbsoluteLinks |
                                        nsIDocumentEncoder::OutputEncodeW3CEntities,
                                        0, getter_Copies(htmlString));
      privSelection->ToStringWithFormat("text/plain", 0, 0, getter_Copies(titleString));
    }
    else 
      selection->ToString(getter_Copies(titleString));
  }

  if ( startDrag ) {
    // default text value is the URL
    if ( titleString.IsEmpty() )
      titleString = urlString;
    
    // if we haven't constructed a html version, make one now
    if ( htmlString.IsEmpty() && !urlString.IsEmpty() )
      CreateLinkText(urlString, titleString, htmlString);
  }
  
  outURLString = urlString;
  outTitleString = titleString;
  outHTMLString = htmlString;
  
  return startDrag;
}


//
// CreateTransferable
//
// Build the transferable from the data strings generated by BuildDragData().
//
nsresult
nsContentAreaDragDrop::CreateTransferable(const nsAString & inURLString, const nsAString & inTitleString, 
                                          const nsAString & inHTMLString, nsIImage* inImage, PRBool inIsAnchor,
                                          nsITransferable** outTrans)
{
  if ( !outTrans )
    return NS_ERROR_FAILURE;
  *outTrans = nsnull;
  
  // now create the transferable and stuff data into it.
  nsCOMPtr<nsITransferable> trans(do_CreateInstance("@mozilla.org/widget/transferable;1"));
  if ( !trans )
    return NS_ERROR_FAILURE;
  
  // add a special flavor if we're an anchor to indicate that we have a URL
  // in the drag data
  if ( !inURLString.IsEmpty() && inIsAnchor ) {
    nsAutoString dragData ( inURLString );
    dragData += NS_LITERAL_STRING("\n");
    dragData += inTitleString;
    nsCOMPtr<nsISupportsString> urlPrimitive(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
    if ( !urlPrimitive )
      return NS_ERROR_FAILURE;
    urlPrimitive->SetData(dragData);
    trans->SetTransferData(kURLMime, urlPrimitive, dragData.Length() * 2);
  }
  
  // add the full html
  nsCOMPtr<nsISupportsString> htmlPrimitive(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if ( !htmlPrimitive )
    return NS_ERROR_FAILURE;
  htmlPrimitive->SetData(inHTMLString);
  trans->SetTransferData(kHTMLMime, htmlPrimitive, inHTMLString.Length() * 2);

  // add the plain (unicode) text. we use the url for text/unicode data if an anchor
  // is being dragged, rather than the title text of the link or the alt text for
  // an anchor image. 
  nsCOMPtr<nsISupportsString> textPrimitive(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if ( !textPrimitive )
    return NS_ERROR_FAILURE;
  textPrimitive->SetData(inIsAnchor ? inURLString : inTitleString);
  trans->SetTransferData(kUnicodeMime, textPrimitive, (inIsAnchor ? inURLString.Length() : inTitleString.Length()) * 2);  
  
  // add image data, if present. For now, all we're going to do with this is turn it
  // into a native data flavor, so indicate that with a new flavor so as not to confuse
  // anyone who is really registered for image/gif or image/jpg.
  if ( inImage ) {
    nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive(do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID));
    if ( !ptrPrimitive )
      return NS_ERROR_FAILURE;
    ptrPrimitive->SetData(inImage);
    trans->SetTransferData(kNativeImageMime, ptrPrimitive, sizeof(nsIImage*));
  }
  
  *outTrans = trans;
  NS_IF_ADDREF(*outTrans);
  return NS_OK;
}


//
// DragGesture
//
// Determine if the user has started to drag something and kick off
// an OS-level drag if it's applicable
//
NS_IMETHODIMP
nsContentAreaDragDrop::DragGesture(nsIDOMEvent* inMouseEvent)
{
  // first check that someone hasn't already handled this event
  PRBool preventDefault = PR_TRUE;
  nsCOMPtr<nsIDOMNSUIEvent> nsuiEvent(do_QueryInterface(inMouseEvent));
  if ( nsuiEvent )
    nsuiEvent->GetPreventDefault(&preventDefault);
  if ( preventDefault )
    return NS_OK;

  // if the client has provided an override callback, check if we
  // should continue
  if ( mOverrideDrag ) {
    PRBool allow = PR_FALSE;
    if ( NS_SUCCEEDED(mOverrideDrag->AllowStart(inMouseEvent, &allow)) )
      if ( !allow )
        return NS_OK;
  }
  
  nsAutoString urlString, titleString, htmlString;
  PRBool isAnchor = PR_FALSE;
  nsCOMPtr<nsIImage> image;
  
  // crawl the dom for the appropriate drag data depending on what was clicked
  PRBool startDrag = BuildDragData(inMouseEvent, urlString, titleString, htmlString, getter_AddRefs(image), &isAnchor);
  if ( startDrag ) {
    // build up the transferable with all this data.
    nsCOMPtr<nsITransferable> trans;
    nsresult rv = CreateTransferable(urlString, titleString, htmlString, image, isAnchor, getter_AddRefs(trans));
    if ( trans ) {
      // if the client has provided an override callback, let them manipulate
      // the flavors or drag data
      if ( mOverrideDrag )
        mOverrideDrag->Modify(trans);

      nsCOMPtr<nsISupportsArray> transArray(do_CreateInstance("@mozilla.org/supports-array;1"));
      if ( !transArray )
        return NS_ERROR_FAILURE;
      transArray->InsertElementAt(trans, 0);
      
      // kick off the drag
      nsCOMPtr<nsIDOMEventTarget> target;
      inMouseEvent->GetTarget(getter_AddRefs(target));
      nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(target));
      nsCOMPtr<nsIDragService> dragService(do_GetService("@mozilla.org/widget/dragservice;1"));
      if ( !dragService )
        return NS_ERROR_FAILURE;
      dragService->InvokeDragSession(targetNode, transArray, nsnull, nsIDragService::DRAGDROP_ACTION_COPY +
                                      nsIDragService::DRAGDROP_ACTION_MOVE + nsIDragService::DRAGDROP_ACTION_LINK);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsContentAreaDragDrop::HandleEvent(nsIDOMEvent *event)
{
  return NS_OK;

}


//
// GetImageFrame
//
// Finds the image from from a content node
//
nsresult 
nsContentAreaDragDrop::GetImageFrame(nsIContent* aContent, nsIDocument *aDocument, nsIPresContext *aPresContext,
                                        nsIPresShell *aPresShell, nsIImageFrame** aImageFrame)
{
	NS_ENSURE_ARG_POINTER(aImageFrame); 
	*aImageFrame = nsnull;

	nsresult rv = NS_ERROR_FAILURE;

	if (aDocument) {
		// Make sure the presentation is up-to-date
		rv = aDocument->FlushPendingNotifications();
		if (NS_FAILED(rv))
			return rv;
	}

	if (aContent && aDocument && aPresContext && aPresShell) {
		nsIFrame* frame = nsnull;
		rv = aPresShell->GetPrimaryFrameFor(aContent, &frame);
		if (NS_SUCCEEDED(rv) && frame) {
			nsCOMPtr<nsIAtom> type;
			frame->GetFrameType(getter_AddRefs(type));
			if (type == nsLayoutAtoms::imageFrame) {
				nsIImageFrame* imageFrame;
				rv = CallQueryInterface(frame, &imageFrame);
				if (NS_FAILED(rv)) {
					NS_WARNING("Should not happen - frame is not image frame even though type is nsLayoutAtoms::imageFrame");
					return rv;
				}
				*aImageFrame = imageFrame;
			}
			else
				return NS_ERROR_FAILURE;
		}
		else
			rv = NS_OK;
	}
	return rv;
}


//
// GetImage
//
// Given a dom node that's an image, finds the nsIImage associated with it.
//
nsresult
nsContentAreaDragDrop::GetImageFromDOMNode(nsIDOMNode* inNode, nsIImage**outImage)
{
	NS_ENSURE_ARG_POINTER(outImage);
	*outImage = nsnull;

	nsresult rv = NS_ERROR_NOT_AVAILABLE;

	nsCOMPtr<nsIContent> content(do_QueryInterface(inNode));
	if ( content ) {
		nsCOMPtr<nsIDocument> document;
		content->GetDocument(*getter_AddRefs(document));
		if ( document ) {
		  nsCOMPtr<nsIPresShell> presShell;
			document->GetShellAt(0, getter_AddRefs(presShell));
			if ( presShell ) {
			  nsCOMPtr<nsIPresContext> presContext;
				presShell->GetPresContext(getter_AddRefs(presContext));
				if (presContext) {
					nsCOMPtr<imgIContainer> imgContainer;
					
					nsIImageFrame* imageFrame = nsnull;
					if ( NS_SUCCEEDED(GetImageFrame(content, document, presContext, presShell, &imageFrame)) && imageFrame ) {
						nsCOMPtr<imgIRequest> imgRequest;
						imageFrame->GetImageRequest(getter_AddRefs(imgRequest));
						if (imgRequest)
							imgRequest->GetImage(getter_AddRefs(imgContainer));
					}
					else {
						// We could try the background image, but we really don't want to be dragging
						// the bg image in any case.
					}

					if (imgContainer) {
						nsCOMPtr<gfxIImageFrame> imgFrame;
						imgContainer->GetFrameAt(0, getter_AddRefs(imgFrame));
						if (imgFrame)	{
							nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(imgFrame);
							if (ir) {
								rv = CallGetInterface(ir.get(), outImage);
							}
						}
					}
				}
			}
		}
	}

	return rv;
}
