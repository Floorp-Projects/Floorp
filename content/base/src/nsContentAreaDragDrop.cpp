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

// Interfaces needed to be included
#include "nsIVariant.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMUIEvent.h"
#include "nsISelection.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMDragEvent.h"
#include "nsIDOMAbstractView.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMRange.h"
#include "nsIDocumentEncoder.h"
#include "nsIFormControl.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsITransferable.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIWebNavigation.h"
#include "nsIDocShell.h"
#include "nsIContent.h"
#include "nsIImageLoadingContent.h"
#include "nsINameSpaceManager.h"
#include "nsUnicharUtils.h"
#include "nsIURL.h"
#include "nsIImage.h"
#include "nsIDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocShellTreeItem.h"
#include "nsIFrame.h"
#include "nsRange.h"
#include "nsIWebBrowserPersist.h"
#include "nsEscape.h"
#include "nsContentUtils.h"
#include "nsIMIMEService.h"
#include "imgIRequest.h"
#include "nsContentCID.h"
#include "nsDOMDataTransfer.h"
#include "nsISelectionController.h"
#include "nsFrameSelection.h"
#include "nsIDOMEventTarget.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_CID(kHTMLConverterCID,        NS_HTMLFORMATCONVERTER_CID);

// private clipboard data flavors for html copy, used by editor when pasting
#define kHTMLContext   "text/_moz_htmlcontext"
#define kHTMLInfo      "text/_moz_htmlinfo"


NS_IMPL_ADDREF(nsContentAreaDragDrop)
NS_IMPL_RELEASE(nsContentAreaDragDrop)

NS_INTERFACE_MAP_BEGIN(nsContentAreaDragDrop)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
    NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
    NS_INTERFACE_MAP_ENTRY(nsIDragDropHandler)
NS_INTERFACE_MAP_END


class NS_STACK_CLASS nsTransferableFactory
{
public:
  nsTransferableFactory(nsIDOMWindow* aWindow,
                        nsIContent* aTarget,
                        nsIContent* aSelectionTargetNode,
                        PRBool aIsAltKeyPressed);
  nsresult Produce(nsDOMDataTransfer* aDataTransfer,
                   PRBool* aCanDrag,
                   PRBool* aDragSelection,
                   nsIContent** aDragNode);

private:
  void AddString(nsDOMDataTransfer* aDataTransfer,
                 const nsAString& aFlavor,
                 const nsAString& aData,
                 nsIPrincipal* aPrincipal);
  nsresult AddStringsToDataTransfer(nsIContent* aDragNode,
                                    nsDOMDataTransfer* aDataTransfer);
  static nsresult GetDraggableSelectionData(nsISelection* inSelection,
                                            nsIContent* inRealTargetNode,
                                            nsIContent **outImageOrLinkNode,
                                            PRBool* outDragSelectedText);
  static already_AddRefed<nsIContent> FindParentLinkNode(nsIContent* inNode);
  static void GetAnchorURL(nsIContent* inNode, nsAString& outURL);
  static void GetNodeString(nsIContent* inNode, nsAString & outNodeString);
  static void CreateLinkText(const nsAString& inURL, const nsAString & inText,
                              nsAString& outLinkText);
  static void GetSelectedLink(nsISelection* inSelection,
                              nsIContent **outLinkNode);

  // if inNode is null, use the selection from the window
  static nsresult SerializeNodeOrSelection(nsIDOMWindow* inWindow,
                                           nsIContent* inNode,
                                           nsAString& outResultString,
                                           nsAString& outHTMLContext,
                                           nsAString& outHTMLInfo);

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCOMPtr<nsIContent> mTarget;
  nsCOMPtr<nsIContent> mSelectionTargetNode;
  PRPackedBool mIsAltKeyPressed;

  nsString mUrlString;
  nsString mImageSourceString;
  nsString mImageDestFileName;
  nsString mTitleString;
  // will be filled automatically if you fill urlstring
  nsString mHtmlString;
  nsString mContextString;
  nsString mInfoString;

  PRBool mIsAnchor;
  nsCOMPtr<nsIImage> mImage;
};


//
// nsContentAreaDragDrop ctor
//
nsContentAreaDragDrop::nsContentAreaDragDrop()
  : mNavigator(nsnull)
{
} // ctor


//
// ChromeTooltipListener dtor
//
nsContentAreaDragDrop::~nsContentAreaDragDrop()
{
  RemoveDragListener();

} // dtor


NS_IMETHODIMP
nsContentAreaDragDrop::HookupTo(nsIDOMEventTarget *inAttachPoint,
                                nsIWebNavigation* inNavigator)
{
  NS_ASSERTION(inAttachPoint, "Can't hookup Drag Listeners to NULL receiver");
  mEventTarget = inAttachPoint;
  mNavigator = inNavigator;

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
nsresult
nsContentAreaDragDrop::AddDragListener()
{
  if (mEventTarget) {
    nsresult rv = mEventTarget->AddEventListener(NS_LITERAL_STRING("dragover"),
                                                 this, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->AddEventListener(NS_LITERAL_STRING("drop"), this,
                                        PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


//
// RemoveDragListener
//
// Unsubscribe from all the various drag events that we were listening to.
//
nsresult
nsContentAreaDragDrop::RemoveDragListener()
{
  if (mEventTarget) {
    nsresult rv =
      mEventTarget->RemoveEventListener(NS_LITERAL_STRING("dragover"), this,
                                        PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEventTarget->RemoveEventListener(NS_LITERAL_STRING("drop"), this,
                                           PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    mEventTarget = nsnull;
  }

  return NS_OK;
}


//
// DragOver
//
// Called when an OS drag is in process and the mouse is over a gecko
// window.  The main purpose of this routine is to set the |canDrop|
// property on the drag session to false if we want to disallow the
// drop so that the OS can provide the appropriate feedback. All this
// does is show feedback, it doesn't actually cancel the drop; that
// comes later.
//
nsresult
nsContentAreaDragDrop::DragOver(nsIDOMDragEvent* inEvent)
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
  // the desired action).

  nsCOMPtr<nsIDragSession> session = nsContentUtils::GetDragSession();
  NS_ENSURE_TRUE(session, NS_OK);

  PRBool dropAllowed = PR_TRUE;

  nsCOMPtr<nsIDOMDocument> sourceDoc;
  session->GetSourceDocument(getter_AddRefs(sourceDoc));
  nsCOMPtr<nsIDOMDocument> eventDoc;
  GetEventDocument(inEvent, getter_AddRefs(eventDoc));

  if (sourceDoc == eventDoc) {  // common case
    dropAllowed = PR_FALSE;
  } else if (sourceDoc && eventDoc) {
    // dig deeper
    // XXXbz we need better ways to get from a document to the docshell!
    nsCOMPtr<nsIDocument> sourceDocument(do_QueryInterface(sourceDoc));
    nsCOMPtr<nsIDocument> eventDocument(do_QueryInterface(eventDoc));
    NS_ASSERTION(sourceDocument, "Confused document object");
    NS_ASSERTION(eventDocument, "Confused document object");

    nsPIDOMWindow* sourceWindow = sourceDocument->GetWindow();
    nsPIDOMWindow* eventWindow = eventDocument->GetWindow();

    if (sourceWindow && eventWindow) {
      nsCOMPtr<nsIDocShellTreeItem> sourceShell =
        do_QueryInterface(sourceWindow->GetDocShell());
      nsCOMPtr<nsIDocShellTreeItem> eventShell =
        do_QueryInterface(eventWindow->GetDocShell());

      if (sourceShell && eventShell) {
        // Whew.  Almost there.  Get the roots that are of the same type
        // (otherwise we'll always end up with the root docshell for the
        // window, and drag/drop from chrom to content won't work).
        nsCOMPtr<nsIDocShellTreeItem> sourceRoot;
        nsCOMPtr<nsIDocShellTreeItem> eventRoot;
        sourceShell->GetSameTypeRootTreeItem(getter_AddRefs(sourceRoot));
        eventShell->GetSameTypeRootTreeItem(getter_AddRefs(eventRoot));

        if (sourceRoot && sourceRoot == eventRoot) {
          dropAllowed = PR_FALSE;
        }
      }
    }
  }

  session->SetCanDrop(dropAllowed);
  return NS_OK;
}


//
// ExtractURLFromData
//
// build up a url from whatever data we get from the OS. How we
// interpret the data depends on the flavor as it tells us the
// nsISupports* primitive type we have.
//
void
nsContentAreaDragDrop::ExtractURLFromData(const nsACString & inFlavor,
                                          nsISupports* inDataWrapper,
                                          PRUint32 inDataLen,
                                          nsAString & outURL)
{
  if (!inDataWrapper) {
    return;
  }

  outURL.Truncate();

  if (inFlavor.Equals(kUnicodeMime)  || inFlavor.Equals(kURLDataMime)) {
    // the data is regular unicode, just go with what we get. It may
    // be a url, it may not be. *shrug*
    nsCOMPtr<nsISupportsString> stringData(do_QueryInterface(inDataWrapper));
    if (stringData) {
      stringData->GetData(outURL);
    }
  }
  else if (inFlavor.Equals(kURLMime)) {
    // the data is an internet shortcut of the form
    // <url>\n<title>. Strip out the url piece and return that.
    nsCOMPtr<nsISupportsString> stringData(do_QueryInterface(inDataWrapper));

    if (stringData) {
      nsAutoString data;
      stringData->GetData(data);
      PRInt32 separator = data.FindChar('\n');

      if (separator >= 0)
        outURL = Substring(data, 0, separator);
      else
        outURL = data;
    }
  }
  else if (inFlavor.Equals(kFileMime)) {
    // the data is a file. Use the necko parsing utils to get a file:// url
    // from the OS data.
    nsCOMPtr<nsIFile> file(do_QueryInterface(inDataWrapper));
    if (file) {
      nsCAutoString url;
      NS_GetURLSpecFromFile(file, url);
      CopyUTF8toUTF16(url, outURL);
    }
  }
}


//
// drop
//
// Called when an OS drag is in process and the mouse is released a
// gecko window.  Extract the data from the OS and do something with
// it.
//
nsresult
nsContentAreaDragDrop::Drop(nsIDOMDragEvent* inDragEvent)
{
  // if we don't have a nsIWebNavigation object to do anything with,
  // just bail. The client will have to have another way to deal with it
  if (!mNavigator) {
    return NS_OK;
  }

  // check that someone hasn't already handled this event
  PRBool preventDefault = PR_TRUE;
  nsCOMPtr<nsIDOMNSUIEvent> nsuiEvent(do_QueryInterface(inDragEvent));
  if (nsuiEvent) {
    nsuiEvent->GetPreventDefault(&preventDefault);
  }

  if (preventDefault) {
    return NS_OK;
  }

  // pull the transferable out of the drag service. at the moment, we
  // only care about the first item of the drag. We don't allow
  // dropping multiple items into a content area.
  nsCOMPtr<nsIDragSession> session = nsContentUtils::GetDragSession();
  NS_ENSURE_TRUE(session, NS_OK);

  nsCOMPtr<nsITransferable> trans =
    do_CreateInstance("@mozilla.org/widget/transferable;1");
  if (!trans) {
    return NS_ERROR_FAILURE;
  }

  // add the relevant flavors. order is important (highest fidelity to lowest)
  trans->AddDataFlavor(kURLDataMime);
  trans->AddDataFlavor(kURLMime);
  trans->AddDataFlavor(kFileMime);
  trans->AddDataFlavor(kUnicodeMime);

  // again, we only care about the first object
  nsresult rv = session->GetData(trans, 0);

  if (NS_SUCCEEDED(rv)) {
    nsXPIDLCString flavor;
    nsCOMPtr<nsISupports> dataWrapper;
    PRUint32 dataLen = 0;
    rv = trans->GetAnyTransferData(getter_Copies(flavor),
                                   getter_AddRefs(dataWrapper), &dataLen);
    if (NS_SUCCEEDED(rv) && dataLen > 0) {
      // get the url from one of several possible formats
      nsAutoString url;
      ExtractURLFromData(flavor, dataWrapper, dataLen, url);
      NS_ASSERTION(!url.IsEmpty(), "Didn't get anything we can use as a url");

      // valid urls don't have spaces. bail if this does.
      if (url.IsEmpty() || url.FindChar(' ') >= 0)
        return NS_OK;

      nsCOMPtr<nsIURI> uri;
      NS_NewURI(getter_AddRefs(uri), url);
      if (!uri) {
        // Not actually a URI
        return NS_OK;
      }

      nsCOMPtr<nsIDOMDocument> sourceDocument;
      session->GetSourceDocument(getter_AddRefs(sourceDocument));

      nsCOMPtr<nsIDocument> sourceDoc(do_QueryInterface(sourceDocument));
      if (sourceDoc) {
        rv = nsContentUtils::GetSecurityManager()->
          CheckLoadURIWithPrincipal(sourceDoc->NodePrincipal(), uri,
                                    nsIScriptSecurityManager::STANDARD);

        if (NS_FAILED(rv)) {
          // Security check failed, stop event propagation right here
          // and return the error.
          inDragEvent->StopPropagation();

          return rv;
        }
      }

      // ok, we have the url, load it.
      mNavigator->LoadURI(url.get(), nsIWebNavigation::LOAD_FLAGS_NONE, nsnull,
                          nsnull, nsnull);
    }
  }

  return NS_OK;
}

//
// NormalizeSelection
//
void
nsContentAreaDragDrop::NormalizeSelection(nsIDOMNode* inBaseNode,
                                          nsISelection* inSelection)
{
  nsCOMPtr<nsIDOMNode> parent;
  inBaseNode->GetParentNode(getter_AddRefs(parent));
  if (!parent || !inSelection)
    return;

  nsCOMPtr<nsIDOMNodeList> childNodes;
  parent->GetChildNodes(getter_AddRefs(childNodes));
  if (!childNodes)
    return;
  PRUint32 listLen = 0;
  childNodes->GetLength(&listLen);

  PRUint32 index = 0;
  for (; index < listLen; ++index) {
    nsCOMPtr<nsIDOMNode> indexedNode;
    childNodes->Item(index, getter_AddRefs(indexedNode));
    if (indexedNode == inBaseNode) {
      break;
    }
  }

  if (index >= listLen) {
    return;
  }

  // now make the selection contain all of |inBaseNode|'s siblings up
  // to and including |inBaseNode|
  inSelection->Collapse(parent, index);
  inSelection->Extend(parent, index+1);
}


//
// GetEventDocument
//
// Get the DOM document associated with a given DOM event
//
void
nsContentAreaDragDrop::GetEventDocument(nsIDOMEvent* inEvent,
                                        nsIDOMDocument** outDocument)
{
  *outDocument = nsnull;

  nsCOMPtr<nsIDOMUIEvent> uiEvent(do_QueryInterface(inEvent));
  if (uiEvent) {
    nsCOMPtr<nsIDOMAbstractView> view;
    uiEvent->GetView(getter_AddRefs(view));
    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(view));

    if (window) {
      window->GetDocument(outDocument);
    }
  }
}


nsresult
nsContentAreaDragDrop::GetDragData(nsIDOMWindow* aWindow,
                                   nsIContent* aTarget,
                                   nsIContent* aSelectionTargetNode,
                                   PRBool aIsAltKeyPressed,
                                   nsDOMDataTransfer* aDataTransfer,
                                   PRBool* aCanDrag,
                                   PRBool* aDragSelection,
                                   nsIContent** aDragNode)
{
  NS_ENSURE_TRUE(aSelectionTargetNode, NS_ERROR_INVALID_ARG);

  *aCanDrag = PR_TRUE;

  nsTransferableFactory
    factory(aWindow, aTarget, aSelectionTargetNode, aIsAltKeyPressed);
  return factory.Produce(aDataTransfer, aCanDrag, aDragSelection, aDragNode);
}


NS_IMETHODIMP
nsContentAreaDragDrop::HandleEvent(nsIDOMEvent *event)
{
  // make sure it's a drag event
  nsCOMPtr<nsIDOMDragEvent> dragEvent = do_QueryInterface(event);
  if (dragEvent) {
    nsAutoString eventType;
    event->GetType(eventType);
    if (eventType.EqualsLiteral("dragover"))
      return DragOver(dragEvent);
    if (eventType.EqualsLiteral("drop"))
      return Drop(dragEvent);
  }

  return NS_OK;
}

#if 0
#pragma mark -
#endif

NS_IMPL_ISUPPORTS1(nsContentAreaDragDropDataProvider, nsIFlavorDataProvider)

// SaveURIToFile
// used on platforms where it's possible to drag items (e.g. images)
// into the file system
nsresult
nsContentAreaDragDropDataProvider::SaveURIToFile(nsAString& inSourceURIString,
                                                 nsIFile* inDestFile)
{
  nsCOMPtr<nsIURI> sourceURI;
  nsresult rv = NS_NewURI(getter_AddRefs(sourceURI), inSourceURIString);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURL> sourceURL = do_QueryInterface(sourceURI);
  if (!sourceURL) {
    return NS_ERROR_NO_INTERFACE;
  }

  rv = inDestFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  // we rely on the fact that the WPB is refcounted by the channel etc,
  // so we don't keep a ref to it. It will die when finished.
  nsCOMPtr<nsIWebBrowserPersist> persist =
    do_CreateInstance("@mozilla.org/embedding/browser/nsWebBrowserPersist;1",
                      &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return persist->SaveURI(sourceURI, nsnull, nsnull, nsnull, nsnull, inDestFile);
}

// This is our nsIFlavorDataProvider callback. There are several
// assumptions here that make this work:
//
// 1. Someone put a kFilePromiseURLMime flavor into the transferable
//    with the source URI of the file to save (as a string). We did
//    that in AddStringsToDataTransfer.
//
// 2. Someone put a kFilePromiseDirectoryMime flavor into the
//    transferable with an nsILocalFile for the directory we are to
//    save in. That has to be done by platform-specific code (in
//    widget), which gets the destination directory from
//    OS-specific drag information.
//
NS_IMETHODIMP
nsContentAreaDragDropDataProvider::GetFlavorData(nsITransferable *aTransferable,
                                                 const char *aFlavor,
                                                 nsISupports **aData,
                                                 PRUint32 *aDataLen)
{
  NS_ENSURE_ARG_POINTER(aData && aDataLen);
  *aData = nsnull;
  *aDataLen = 0;

  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

  if (strcmp(aFlavor, kFilePromiseMime) == 0) {
    // get the URI from the kFilePromiseURLMime flavor
    NS_ENSURE_ARG(aTransferable);
    nsCOMPtr<nsISupports> tmp;
    PRUint32 dataSize = 0;
    aTransferable->GetTransferData(kFilePromiseURLMime,
                                   getter_AddRefs(tmp), &dataSize);
    nsCOMPtr<nsISupportsString> supportsString =
      do_QueryInterface(tmp);
    if (!supportsString)
      return NS_ERROR_FAILURE;

    nsAutoString sourceURLString;
    supportsString->GetData(sourceURLString);
    if (sourceURLString.IsEmpty())
      return NS_ERROR_FAILURE;

    aTransferable->GetTransferData(kFilePromiseDestFilename,
                                   getter_AddRefs(tmp), &dataSize);
    supportsString = do_QueryInterface(tmp);
    if (!supportsString)
      return NS_ERROR_FAILURE;

    nsAutoString targetFilename;
    supportsString->GetData(targetFilename);
    if (targetFilename.IsEmpty())
      return NS_ERROR_FAILURE;

    // get the target directory from the kFilePromiseDirectoryMime
    // flavor
    nsCOMPtr<nsISupports> dirPrimitive;
    dataSize = 0;
    aTransferable->GetTransferData(kFilePromiseDirectoryMime,
                                   getter_AddRefs(dirPrimitive), &dataSize);
    nsCOMPtr<nsILocalFile> destDirectory = do_QueryInterface(dirPrimitive);
    if (!destDirectory)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIFile> file;
    rv = destDirectory->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    file->Append(targetFilename);

    rv = SaveURIToFile(sourceURLString, file);
    // send back an nsILocalFile
    if (NS_SUCCEEDED(rv)) {
      CallQueryInterface(file, aData);
      *aDataLen = sizeof(nsIFile*);
    }
  }

  return rv;
}

nsTransferableFactory::nsTransferableFactory(nsIDOMWindow* aWindow,
                                             nsIContent* aTarget,
                                             nsIContent* aSelectionTargetNode,
                                             PRBool aIsAltKeyPressed)
  : mWindow(aWindow),
    mTarget(aTarget),
    mSelectionTargetNode(aSelectionTargetNode),
    mIsAltKeyPressed(aIsAltKeyPressed)
{
}


//
// FindParentLinkNode
//
// Finds the parent with the given link tag starting at |inNode|. If
// it gets up to the root without finding it, we stop looking and
// return null.
//
already_AddRefed<nsIContent>
nsTransferableFactory::FindParentLinkNode(nsIContent* inNode)
{
  nsIContent* content = inNode;
  if (!content) {
    // That must have been the document node; nothing else to do here;
    return nsnull;
  }

  for (; content; content = content->GetParent()) {
    if (nsContentUtils::IsDraggableLink(content)) {
      NS_ADDREF(content);
      return content;
    }
  }

  return nsnull;
}


//
// GetAnchorURL
//
void
nsTransferableFactory::GetAnchorURL(nsIContent* inNode, nsAString& outURL)
{
  nsCOMPtr<nsIURI> linkURI;
  if (!inNode || !inNode->IsLink(getter_AddRefs(linkURI))) {
    // Not a link
    outURL.Truncate();
    return;
  }

  nsCAutoString spec;
  linkURI->GetSpec(spec);
  CopyUTF8toUTF16(spec, outURL);
}


//
// CreateLinkText
//
// Creates the html for an anchor in the form
//  <a href="inURL">inText</a>
//
void
nsTransferableFactory::CreateLinkText(const nsAString& inURL,
                                      const nsAString & inText,
                                      nsAString& outLinkText)
{
  // use a temp var in case |inText| is the same string as
  // |outLinkText| to avoid overwriting it while building up the
  // string in pieces.
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
nsTransferableFactory::GetNodeString(nsIContent* inNode,
                                     nsAString & outNodeString)
{
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(inNode);

  outNodeString.Truncate();

  // use a range to get the text-equivalent of the node
  nsCOMPtr<nsIDOMDocument> doc;
  node->GetOwnerDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIDOMDocumentRange> docRange(do_QueryInterface(doc));
  if (docRange) {
    nsCOMPtr<nsIDOMRange> range;
    docRange->CreateRange(getter_AddRefs(range));
    if (range) {
      range->SelectNode(node);
      range->ToString(outNodeString);
    }
  }
}


nsresult
nsTransferableFactory::Produce(nsDOMDataTransfer* aDataTransfer,
                               PRBool* aCanDrag,
                               PRBool* aDragSelection,
                               nsIContent** aDragNode)
{
  NS_PRECONDITION(aCanDrag && aDragSelection && aDataTransfer && aDragNode,
                  "null pointer passed to Produce");
  NS_ASSERTION(mWindow, "window not set");
  NS_ASSERTION(mSelectionTargetNode, "selection target node should have been set");

  *aDragNode = nsnull;

  nsIContent* dragNode = nsnull;

  mIsAnchor = PR_FALSE;

  // find the selection to see what we could be dragging and if
  // what we're dragging is in what is selected.
  nsCOMPtr<nsISelection> selection;
  mWindow->GetSelection(getter_AddRefs(selection));
  if (!selection) {
    return NS_OK;
  }

  // check if the node is inside a form control. If so, dragging will be
  // handled in editor code (nsPlaintextDataTransfer::DoDrag). Don't set
  // aCanDrag to false however, as we still want to allow the drag.
  nsCOMPtr<nsIContent> findFormNode = mSelectionTargetNode;
  nsIContent* findFormParent = findFormNode->GetParent();
  while (findFormParent) {
    nsCOMPtr<nsIFormControl> form(do_QueryInterface(findFormParent));
    if (form && form->GetType() != NS_FORM_OBJECT &&
                form->GetType() != NS_FORM_FIELDSET &&
                form->GetType() != NS_FORM_LEGEND &&
                form->GetType() != NS_FORM_LABEL)
      return NS_OK;
    findFormParent = findFormParent->GetParent();
  }
    
  // if set, serialize the content under this node
  nsCOMPtr<nsIContent> nodeToSerialize;
  *aDragSelection = PR_FALSE;

  {
    PRBool haveSelectedContent = PR_FALSE;

    // possible parent link node
    nsCOMPtr<nsIContent> parentLink;
    nsCOMPtr<nsIContent> draggedNode;

    {
      // only drag form elements by using the alt key,
      // otherwise buttons and select widgets are hard to use

      // Note that while <object> elements implement nsIFormControl, we should
      // really allow dragging them if they happen to be images.
      nsCOMPtr<nsIFormControl> form(do_QueryInterface(mTarget));
      if (form && !mIsAltKeyPressed && form->GetType() != NS_FORM_OBJECT) {
        *aCanDrag = PR_FALSE;
        return NS_OK;
      }

      draggedNode = mTarget;
    }

    nsCOMPtr<nsIDOMHTMLAreaElement>   area;   // client-side image map
    nsCOMPtr<nsIImageLoadingContent>  image;
    nsCOMPtr<nsIDOMHTMLAnchorElement> link;

    nsCOMPtr<nsIContent> selectedImageOrLinkNode;
    GetDraggableSelectionData(selection, mSelectionTargetNode,
                              getter_AddRefs(selectedImageOrLinkNode),
                              &haveSelectedContent);

    // either plain text or anchor text is selected
    if (haveSelectedContent) {
      link = do_QueryInterface(selectedImageOrLinkNode);
      if (link && mIsAltKeyPressed) {
        // if alt is pressed, select the link text instead of drag the link
        *aCanDrag = PR_FALSE;
        return NS_OK;
      }

      *aDragSelection = PR_TRUE;
    } else if (selectedImageOrLinkNode) {
      // an image is selected
      image = do_QueryInterface(selectedImageOrLinkNode);
    } else {
      // nothing is selected -
      //
      // look for draggable elements under the mouse
      //
      // if the alt key is down, don't start a drag if we're in an
      // anchor because we want to do selection.
      parentLink = FindParentLinkNode(draggedNode);
      if (parentLink && mIsAltKeyPressed) {
        *aCanDrag = PR_FALSE;
        return NS_OK;
      }

      area  = do_QueryInterface(draggedNode);
      image = do_QueryInterface(draggedNode);
      link  = do_QueryInterface(draggedNode);
    }

    {
      // set for linked images, and links
      nsCOMPtr<nsIContent> linkNode;

      if (area) {
        // use the alt text (or, if missing, the href) as the title
        area->GetAttribute(NS_LITERAL_STRING("alt"), mTitleString);
        if (mTitleString.IsEmpty()) {
          // this can be a relative link
          area->GetAttribute(NS_LITERAL_STRING("href"), mTitleString);
        }

        // we'll generate HTML like <a href="absurl">alt text</a>
        mIsAnchor = PR_TRUE;

        // gives an absolute link
        GetAnchorURL(draggedNode, mUrlString);

        mHtmlString.AssignLiteral("<a href=\"");
        mHtmlString.Append(mUrlString);
        mHtmlString.AppendLiteral("\">");
        mHtmlString.Append(mTitleString);
        mHtmlString.AppendLiteral("</a>");

        dragNode = draggedNode;
      } else if (image) {
        mIsAnchor = PR_TRUE;
        // grab the href as the url, use alt text as the title of the
        // area if it's there.  the drag data is the image tag and src
        // attribute.
        nsCOMPtr<nsIURI> imageURI;
        image->GetCurrentURI(getter_AddRefs(imageURI));
        if (imageURI) {
          nsCAutoString spec;
          imageURI->GetSpec(spec);
          CopyUTF8toUTF16(spec, mUrlString);
        }

        nsCOMPtr<nsIDOMElement> imageElement(do_QueryInterface(image));
        // XXXbz Shouldn't we use the "title" attr for title?  Using
        // "alt" seems very wrong....
        if (imageElement) {
          imageElement->GetAttribute(NS_LITERAL_STRING("alt"), mTitleString);
        }

        if (mTitleString.IsEmpty()) {
          mTitleString = mUrlString;
        }

        nsCOMPtr<imgIRequest> imgRequest;

        // grab the image data, and its request.
        nsCOMPtr<nsIImage> img =
          nsContentUtils::GetImageFromContent(image,
                                              getter_AddRefs(imgRequest));

        nsCOMPtr<nsIMIMEService> mimeService =
          do_GetService("@mozilla.org/mime;1");

        // Fix the file extension in the URL if necessary
        if (imgRequest && mimeService) {
          nsCOMPtr<nsIURI> imgUri;
          imgRequest->GetURI(getter_AddRefs(imgUri));

          nsCOMPtr<nsIURL> imgUrl(do_QueryInterface(imgUri));

          if (imgUrl) {
            nsCAutoString extension;
            imgUrl->GetFileExtension(extension);

            nsXPIDLCString mimeType;
            imgRequest->GetMimeType(getter_Copies(mimeType));

            nsCOMPtr<nsIMIMEInfo> mimeInfo;
            mimeService->GetFromTypeAndExtension(mimeType, EmptyCString(),
                                                 getter_AddRefs(mimeInfo));

            if (mimeInfo) {
              nsCAutoString spec;
              imgUrl->GetSpec(spec);

              // pass out the image source string
              CopyUTF8toUTF16(spec, mImageSourceString);

              PRBool validExtension;
              if (extension.IsEmpty() || 
                  NS_FAILED(mimeInfo->ExtensionExists(extension,
                                                      &validExtension)) ||
                  !validExtension) {
                // Fix the file extension in the URL
                nsresult rv = imgUrl->Clone(getter_AddRefs(imgUri));
                NS_ENSURE_SUCCESS(rv, rv);

                imgUrl = do_QueryInterface(imgUri);

                nsCAutoString primaryExtension;
                mimeInfo->GetPrimaryExtension(primaryExtension);

                imgUrl->SetFileExtension(primaryExtension);
              }

              nsCAutoString fileName;
              imgUrl->GetFileName(fileName);

              NS_UnescapeURL(fileName);

              // make the filename safe for the filesystem
              fileName.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS,
                                   '-');

              CopyUTF8toUTF16(fileName, mImageDestFileName);

              // and the image object
              mImage = img;
            }
          }
        }

        if (parentLink) {
          // If we are dragging around an image in an anchor, then we
          // are dragging the entire anchor
          linkNode = parentLink;
          nodeToSerialize = linkNode;
        } else {
          nodeToSerialize = do_QueryInterface(draggedNode);
        }
        dragNode = nodeToSerialize;
      } else if (link) {
        // set linkNode. The code below will handle this
        linkNode = do_QueryInterface(link);    // XXX test this
        GetNodeString(draggedNode, mTitleString);
      } else if (parentLink) {
        // parentLink will always be null if there's selected content
        linkNode = parentLink;
        nodeToSerialize = linkNode;
      } else if (!haveSelectedContent) {
        // nothing draggable
        return NS_OK;
      }

      if (linkNode) {
        mIsAnchor = PR_TRUE;
        GetAnchorURL(linkNode, mUrlString);
        dragNode = linkNode;
      }
    }
  }

  if (nodeToSerialize || *aDragSelection) {
    // if we have selected text, use it in preference to the node
    if (*aDragSelection) {
      nodeToSerialize = nsnull;
    }

    SerializeNodeOrSelection(mWindow, nodeToSerialize,
                             mHtmlString, mContextString, mInfoString);

    nsCOMPtr<nsIFormatConverter> htmlConverter =
      do_CreateInstance(kHTMLConverterCID);
    NS_ENSURE_TRUE(htmlConverter, NS_ERROR_FAILURE);

    nsCOMPtr<nsISupportsString> html =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    NS_ENSURE_TRUE(html, NS_ERROR_FAILURE);
    html->SetData(mHtmlString);

    nsCOMPtr<nsISupportsString> text;
    PRUint32 textLen;
    htmlConverter->Convert(kHTMLMime, html, mHtmlString.Length() * 2,
                           kUnicodeMime, getter_AddRefs(text), &textLen);
    NS_ENSURE_TRUE(text, NS_ERROR_FAILURE);
    text->GetData(mTitleString);

#ifdef CHANGE_SELECTION_ON_DRAG
    // We used to change the selection to wrap the dragged node (mainly
    // to work around now-fixed issues with dragging unselected elements).
    // There is no reason to do this any more.
    NormalizeSelection(selectionNormalizeNode, selection);
#endif
  }

  // default text value is the URL
  if (mTitleString.IsEmpty()) {
    mTitleString = mUrlString;
  }

  // if we haven't constructed a html version, make one now
  if (mHtmlString.IsEmpty() && !mUrlString.IsEmpty())
    CreateLinkText(mUrlString, mTitleString, mHtmlString);

  // if there is no drag node, which will be the case for a selection, just
  // use the selection target node.
  nsresult rv = AddStringsToDataTransfer(
           dragNode ? dragNode : mSelectionTargetNode.get(), aDataTransfer);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aDragNode = dragNode);
  return NS_OK;
}

void
nsTransferableFactory::AddString(nsDOMDataTransfer* aDataTransfer,
                                 const nsAString& aFlavor,
                                 const nsAString& aData,
                                 nsIPrincipal* aPrincipal)
{
  nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (variant) {
    variant->SetAsAString(aData);
    aDataTransfer->SetDataWithPrincipal(aFlavor, variant, 0, aPrincipal);
  }
}

nsresult
nsTransferableFactory::AddStringsToDataTransfer(nsIContent* aDragNode,
                                                nsDOMDataTransfer* aDataTransfer)
{
  NS_ASSERTION(aDragNode, "adding strings for null node");

  // set all of the data to have the principal of the node where the data came from
  nsIPrincipal* principal = aDragNode->NodePrincipal();

  // add a special flavor if we're an anchor to indicate that we have
  // a URL in the drag data
  if (!mUrlString.IsEmpty() && mIsAnchor) {
    nsAutoString dragData(mUrlString);
    dragData.AppendLiteral("\n");
    dragData += mTitleString;

    AddString(aDataTransfer, NS_LITERAL_STRING(kURLMime), dragData, principal);
    AddString(aDataTransfer, NS_LITERAL_STRING(kURLDataMime), mUrlString, principal);
    AddString(aDataTransfer, NS_LITERAL_STRING(kURLDescriptionMime), mTitleString, principal);
    AddString(aDataTransfer, NS_LITERAL_STRING("text/uri-list"), mUrlString, principal);
  }

  // add a special flavor, even if we don't have html context data
  AddString(aDataTransfer, NS_LITERAL_STRING(kHTMLContext), mContextString, principal);

  // add a special flavor if we have html info data
  if (!mInfoString.IsEmpty())
    AddString(aDataTransfer, NS_LITERAL_STRING(kHTMLInfo), mInfoString, principal);

  // add the full html
  AddString(aDataTransfer, NS_LITERAL_STRING(kHTMLMime), mHtmlString, principal);

  // add the plain text. we use the url for text/plain data if an anchor is
  // being dragged, rather than the title text of the link or the alt text for
  // an anchor image.
  AddString(aDataTransfer, NS_LITERAL_STRING(kTextMime),
            mIsAnchor ? mUrlString : mTitleString, principal);

  // add image data, if present. For now, all we're going to do with
  // this is turn it into a native data flavor, so indicate that with
  // a new flavor so as not to confuse anyone who is really registered
  // for image/gif or image/jpg.
  if (mImage) {
    nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID);
    if (variant) {
      variant->SetAsISupports(mImage);
      aDataTransfer->SetDataWithPrincipal(NS_LITERAL_STRING(kNativeImageMime),
                                          variant, 0, principal);
    }

    // assume the image comes from a file, and add a file promise. We
    // register ourselves as a nsIFlavorDataProvider, and will use the
    // GetFlavorData callback to save the image to disk.

    nsCOMPtr<nsIFlavorDataProvider> dataProvider =
      new nsContentAreaDragDropDataProvider();
    if (dataProvider) {
      nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID);
      if (variant) {
        variant->SetAsISupports(dataProvider);
        aDataTransfer->SetDataWithPrincipal(NS_LITERAL_STRING(kFilePromiseMime),
                                            variant, 0, principal);
      }
    }

    AddString(aDataTransfer, NS_LITERAL_STRING(kFilePromiseURLMime),
              mImageSourceString, principal);
    AddString(aDataTransfer, NS_LITERAL_STRING(kFilePromiseDestFilename),
              mImageDestFileName, principal);

    // if not an anchor, add the image url
    if (!mIsAnchor) {
      AddString(aDataTransfer, NS_LITERAL_STRING(kURLDataMime), mUrlString, principal);
      AddString(aDataTransfer, NS_LITERAL_STRING("text/uri-list"), mUrlString, principal);
    }
  }

  return NS_OK;
}

// note that this can return NS_OK, but a null out param (by design)
// static
nsresult
nsTransferableFactory::GetDraggableSelectionData(nsISelection* inSelection,
                                                 nsIContent* inRealTargetNode,
                                                 nsIContent **outImageOrLinkNode,
                                                 PRBool* outDragSelectedText)
{
  NS_ENSURE_ARG(inSelection);
  NS_ENSURE_ARG(inRealTargetNode);
  NS_ENSURE_ARG_POINTER(outImageOrLinkNode);

  *outImageOrLinkNode = nsnull;
  *outDragSelectedText = PR_FALSE;

  PRBool selectionContainsTarget = PR_FALSE;

  PRBool isCollapsed = PR_FALSE;
  inSelection->GetIsCollapsed(&isCollapsed);
  if (!isCollapsed) {
    nsCOMPtr<nsIDOMNode> realTargetNode = do_QueryInterface(inRealTargetNode);
    inSelection->ContainsNode(realTargetNode, PR_FALSE,
                              &selectionContainsTarget);

    if (selectionContainsTarget) {
      // track down the anchor node, if any, for the url
      nsCOMPtr<nsIDOMNode> selectionStart;
      inSelection->GetAnchorNode(getter_AddRefs(selectionStart));

      nsCOMPtr<nsIDOMNode> selectionEnd;
      inSelection->GetFocusNode(getter_AddRefs(selectionEnd));

      // look for a selection around a single node, like an image.
      // in this case, drag the image, rather than a serialization of the HTML
      // XXX generalize this to other draggable element types?
      if (selectionStart == selectionEnd) {
        PRBool hasChildren;
        selectionStart->HasChildNodes(&hasChildren);
        if (hasChildren) {
          // see if just one node is selected
          PRInt32 anchorOffset, focusOffset;
          inSelection->GetAnchorOffset(&anchorOffset);
          inSelection->GetFocusOffset(&focusOffset);
          if (abs(anchorOffset - focusOffset) == 1) {
            nsCOMPtr<nsIContent> selStartContent =
              do_QueryInterface(selectionStart);

            if (selStartContent) {
              PRInt32 childOffset =
                (anchorOffset < focusOffset) ? anchorOffset : focusOffset;
              nsIContent *childContent =
                selStartContent->GetChildAt(childOffset);
              // if we find an image, we'll fall into the node-dragging code,
              // rather the the selection-dragging code
              if (nsContentUtils::IsDraggableImage(childContent)) {
                NS_ADDREF(*outImageOrLinkNode = childContent);
                return NS_OK;
              }
            }
          }
        }
      }

      // see if the selection is a link;  if so, its node will be returned
      GetSelectedLink(inSelection, outImageOrLinkNode);

      // indicate that a link or text is selected
      *outDragSelectedText = PR_TRUE;
    }
  }

  return NS_OK;
}

// static
void nsTransferableFactory::GetSelectedLink(nsISelection* inSelection,
                                            nsIContent **outLinkNode)
{
  *outLinkNode = nsnull;

  nsCOMPtr<nsIDOMNode> selectionStartNode;
  inSelection->GetAnchorNode(getter_AddRefs(selectionStartNode));
  nsCOMPtr<nsIDOMNode> selectionEndNode;
  inSelection->GetFocusNode(getter_AddRefs(selectionEndNode));

  // simple case:  only one node is selected
  // see if it or its parent is an anchor, then exit

  if (selectionStartNode == selectionEndNode) {
    nsCOMPtr<nsIContent> selectionStart = do_QueryInterface(selectionStartNode);
    nsCOMPtr<nsIContent> link = FindParentLinkNode(selectionStart);
    if (link) {
      link.swap(*outLinkNode);
    }

    return;
  }

  // more complicated case:  multiple nodes are selected

  // Unless you use the Alt key while selecting anchor text, it is
  // nearly impossible to avoid overlapping into adjacent nodes.
  // Deal with this by trimming off the leading and/or trailing
  // nodes of the selection if the strings they produce are empty.

  // first, use a range determine if the selection was marked LTR or RTL;
  // if the latter, swap endpoints so we trim in the right direction

  PRInt32 startOffset, endOffset;
  {
    nsCOMPtr<nsIDOMRange> range;
    inSelection->GetRangeAt(0, getter_AddRefs(range));
    if (!range) {
      return;
    }

    nsCOMPtr<nsIDOMNode> tempNode;
    range->GetStartContainer( getter_AddRefs(tempNode));
    if (tempNode != selectionStartNode) {
      selectionEndNode = selectionStartNode;
      selectionStartNode = tempNode;
      inSelection->GetAnchorOffset(&endOffset);
      inSelection->GetFocusOffset(&startOffset);
    } else {
      inSelection->GetAnchorOffset(&startOffset);
      inSelection->GetFocusOffset(&endOffset);
    }
  }

  // trim leading node if the string is empty or
  // the selection starts at the end of the text

  nsAutoString nodeStr;
  selectionStartNode->GetNodeValue(nodeStr);
  if (nodeStr.IsEmpty() ||
      startOffset+1 >= static_cast<PRInt32>(nodeStr.Length())) {
    nsCOMPtr<nsIDOMNode> curr = selectionStartNode;
    nsIDOMNode* next;

    while (curr) {
      curr->GetNextSibling(&next);

      if (next) {
        selectionStartNode = dont_AddRef(next);
        break;
      }

      curr->GetParentNode(&next);
      curr = dont_AddRef(next);
    }
  }

  // trim trailing node if the selection ends before its text begins

  if (endOffset == 0) {
    nsCOMPtr<nsIDOMNode> curr = selectionEndNode;
    nsIDOMNode* next;

    while (curr) {
      curr->GetPreviousSibling(&next);

      if (next){
        selectionEndNode = dont_AddRef(next);
        break;
      }

      curr->GetParentNode(&next);
      curr = dont_AddRef(next);
    }
  }

  // see if the leading & trailing nodes are part of the
  // same anchor - if so, return the anchor node
  nsCOMPtr<nsIContent> selectionStart = do_QueryInterface(selectionStartNode);
  nsCOMPtr<nsIContent> link = FindParentLinkNode(selectionStart);
  if (link) {
    nsCOMPtr<nsIContent> selectionEnd = do_QueryInterface(selectionEndNode);
    nsCOMPtr<nsIContent> link2 = FindParentLinkNode(selectionEnd);

    if (link == link2) {
      NS_IF_ADDREF(*outLinkNode = link);
    }
  }

  return;
}

// static
nsresult
nsTransferableFactory::SerializeNodeOrSelection(nsIDOMWindow* inWindow,
                                                nsIContent* inNode,
                                                nsAString& outResultString,
                                                nsAString& outContext,
                                                nsAString& outInfo)
{
  NS_ENSURE_ARG_POINTER(inWindow);

  nsresult rv;
  nsCOMPtr<nsIDocumentEncoder> encoder =
    do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(encoder, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  inWindow->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

  PRUint32 flags = nsIDocumentEncoder::OutputAbsoluteLinks |
                   nsIDocumentEncoder::OutputEncodeHTMLEntities;
  nsCOMPtr<nsIDOMRange> range;
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(inNode);
  if (node) {
    // make a range around this node
    rv = NS_NewRange(getter_AddRefs(range));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = range->SelectNode(node);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    inWindow->GetSelection(getter_AddRefs(selection));
    flags |= nsIDocumentEncoder::OutputSelectionOnly;
  }

  rv = encoder->Init(domDoc, NS_LITERAL_STRING(kHTMLMime), flags);
  NS_ENSURE_SUCCESS(rv, rv);

  if (range) {
    encoder->SetRange(range);
  } else if (selection) {
    encoder->SetSelection(selection);
  }

  return encoder->EncodeToStringWithContext(outContext, outInfo,
                                            outResultString);
}
