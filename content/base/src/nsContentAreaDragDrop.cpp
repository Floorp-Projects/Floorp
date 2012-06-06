/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReadableUtils.h"

// Local Includes
#include "nsContentAreaDragDrop.h"

// Helper Classes
#include "nsString.h"

// Interfaces needed to be included
#include "nsCopySupport.h"
#include "nsIDOMUIEvent.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMDragEvent.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsITransferable.h"
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
#include "nsITextControlElement.h"
#include "nsUnicharUtils.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebBrowserPersist.h"
#include "nsEscape.h"
#include "nsContentUtils.h"
#include "nsIMIMEService.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsDOMDataTransfer.h"
#include "mozilla/dom/Element.h"

class NS_STACK_CLASS DragDataProducer
{
public:
  DragDataProducer(nsIDOMWindow* aWindow,
                   nsIContent* aTarget,
                   nsIContent* aSelectionTargetNode,
                   bool aIsAltKeyPressed);
  nsresult Produce(nsDOMDataTransfer* aDataTransfer,
                   bool* aCanDrag,
                   nsISelection** aSelection,
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
                                            bool* outDragSelectedText);
  static already_AddRefed<nsIContent> FindParentLinkNode(nsIContent* inNode);
  static void GetAnchorURL(nsIContent* inNode, nsAString& outURL);
  static void GetNodeString(nsIContent* inNode, nsAString & outNodeString);
  static void CreateLinkText(const nsAString& inURL, const nsAString & inText,
                              nsAString& outLinkText);
  static void GetSelectedLink(nsISelection* inSelection,
                              nsIContent **outLinkNode);

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCOMPtr<nsIContent> mTarget;
  nsCOMPtr<nsIContent> mSelectionTargetNode;
  bool mIsAltKeyPressed;

  nsString mUrlString;
  nsString mImageSourceString;
  nsString mImageDestFileName;
  nsString mTitleString;
  // will be filled automatically if you fill urlstring
  nsString mHtmlString;
  nsString mContextString;
  nsString mInfoString;

  bool mIsAnchor;
  nsCOMPtr<imgIContainer> mImage;
};


nsresult
nsContentAreaDragDrop::GetDragData(nsIDOMWindow* aWindow,
                                   nsIContent* aTarget,
                                   nsIContent* aSelectionTargetNode,
                                   bool aIsAltKeyPressed,
                                   nsDOMDataTransfer* aDataTransfer,
                                   bool* aCanDrag,
                                   nsISelection** aSelection,
                                   nsIContent** aDragNode)
{
  NS_ENSURE_TRUE(aSelectionTargetNode, NS_ERROR_INVALID_ARG);

  *aCanDrag = true;

  DragDataProducer
    provider(aWindow, aTarget, aSelectionTargetNode, aIsAltKeyPressed);
  return provider.Produce(aDataTransfer, aCanDrag, aSelection, aDragNode);
}


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

  persist->SetPersistFlags(nsIWebBrowserPersist::PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION);

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
//    transferable with an nsIFile for the directory we are to
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
    nsCOMPtr<nsIFile> destDirectory = do_QueryInterface(dirPrimitive);
    if (!destDirectory)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIFile> file;
    rv = destDirectory->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    file->Append(targetFilename);

    rv = SaveURIToFile(sourceURLString, file);
    // send back an nsIFile
    if (NS_SUCCEEDED(rv)) {
      CallQueryInterface(file, aData);
      *aDataLen = sizeof(nsIFile*);
    }
  }

  return rv;
}

DragDataProducer::DragDataProducer(nsIDOMWindow* aWindow,
                                   nsIContent* aTarget,
                                   nsIContent* aSelectionTargetNode,
                                   bool aIsAltKeyPressed)
  : mWindow(aWindow),
    mTarget(aTarget),
    mSelectionTargetNode(aSelectionTargetNode),
    mIsAltKeyPressed(aIsAltKeyPressed),
    mIsAnchor(false)
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
DragDataProducer::FindParentLinkNode(nsIContent* inNode)
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
DragDataProducer::GetAnchorURL(nsIContent* inNode, nsAString& outURL)
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
DragDataProducer::CreateLinkText(const nsAString& inURL,
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
DragDataProducer::GetNodeString(nsIContent* inNode,
                                nsAString & outNodeString)
{
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(inNode);

  outNodeString.Truncate();

  // use a range to get the text-equivalent of the node
  nsCOMPtr<nsIDOMDocument> doc;
  node->GetOwnerDocument(getter_AddRefs(doc));
  if (doc) {
    nsCOMPtr<nsIDOMRange> range;
    doc->CreateRange(getter_AddRefs(range));
    if (range) {
      range->SelectNode(node);
      range->ToString(outNodeString);
    }
  }
}

nsresult
DragDataProducer::Produce(nsDOMDataTransfer* aDataTransfer,
                          bool* aCanDrag,
                          nsISelection** aSelection,
                          nsIContent** aDragNode)
{
  NS_PRECONDITION(aCanDrag && aSelection && aDataTransfer && aDragNode,
                  "null pointer passed to Produce");
  NS_ASSERTION(mWindow, "window not set");
  NS_ASSERTION(mSelectionTargetNode, "selection target node should have been set");

  *aDragNode = nsnull;

  nsresult rv;
  nsIContent* dragNode = nsnull;
  *aSelection = nsnull;

  // Find the selection to see what we could be dragging and if what we're
  // dragging is in what is selected. If this is an editable textbox, use
  // the textbox's selection, otherwise use the window's selection.
  nsCOMPtr<nsISelection> selection;
  nsIContent* editingElement = mSelectionTargetNode->IsEditable() ?
                               mSelectionTargetNode->GetEditingHost() : nsnull;
  nsCOMPtr<nsITextControlElement> textControl =
    nsITextControlElement::GetTextControlElementFromEditingHost(editingElement);
  if (textControl) {
    nsISelectionController* selcon = textControl->GetSelectionController();
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    }

    if (!selection)
      return NS_OK;
  }
  else {
    mWindow->GetSelection(getter_AddRefs(selection));
    if (!selection)
      return NS_OK;

    // Check if the node is inside a form control. Don't set aCanDrag to false
    //however, as we still want to allow the drag.
    nsCOMPtr<nsIContent> findFormNode = mSelectionTargetNode;
    nsIContent* findFormParent = findFormNode->GetParent();
    while (findFormParent) {
      nsCOMPtr<nsIFormControl> form(do_QueryInterface(findFormParent));
      if (form && !form->AllowDraggableChildren()) {
        return NS_OK;
      }
      findFormParent = findFormParent->GetParent();
    }
  }
    
  // if set, serialize the content under this node
  nsCOMPtr<nsIContent> nodeToSerialize;

  bool isChromeShell = false;
  nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(mWindow);
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(webnav);
  if (dsti) {
    PRInt32 type = -1;
    if (NS_SUCCEEDED(dsti->GetItemType(&type)) &&
        type == nsIDocShellTreeItem::typeChrome) {
      isChromeShell = true;
    }
  }

  // In chrome shells, only allow dragging inside editable areas.
  if (isChromeShell && !editingElement)
    return NS_OK;

  if (isChromeShell && textControl) {
    // Only use the selection if the target node is in the selection.
    bool selectionContainsTarget = false;
    nsCOMPtr<nsIDOMNode> targetNode = do_QueryInterface(mSelectionTargetNode);
    selection->ContainsNode(targetNode, false, &selectionContainsTarget);
    if (!selectionContainsTarget)
      return NS_OK;

    selection.swap(*aSelection);
  }
  else {
    // In content shells, a number of checks are made below to determine
    // whether an image or a link is being dragged. If so, add additional
    // data to the data transfer. This is also done for chrome shells, but
    // only when in a non-textbox editor.

    bool haveSelectedContent = false;

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
        *aCanDrag = false;
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
        *aCanDrag = false;
        return NS_OK;
      }

      selection.swap(*aSelection);
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
        *aCanDrag = false;
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
        mIsAnchor = true;

        // gives an absolute link
        GetAnchorURL(draggedNode, mUrlString);

        mHtmlString.AssignLiteral("<a href=\"");
        mHtmlString.Append(mUrlString);
        mHtmlString.AppendLiteral("\">");
        mHtmlString.Append(mTitleString);
        mHtmlString.AppendLiteral("</a>");

        dragNode = draggedNode;
      } else if (image) {
        mIsAnchor = true;
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
        nsCOMPtr<imgIContainer> img =
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

              bool validExtension;
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
        mIsAnchor = true;
        GetAnchorURL(linkNode, mUrlString);
        dragNode = linkNode;
      }
    }
  }

  if (nodeToSerialize || *aSelection) {
    mHtmlString.Truncate();
    mContextString.Truncate();
    mInfoString.Truncate();
    mTitleString.Truncate();

    nsCOMPtr<nsIDOMDocument> domDoc;
    mWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    // if we have selected text, use it in preference to the node
    nsCOMPtr<nsITransferable> transferable;
    if (*aSelection) {
      rv = nsCopySupport::GetTransferableForSelection(*aSelection, doc,
                                                      getter_AddRefs(transferable));
    }
    else {
      rv = nsCopySupport::GetTransferableForNode(nodeToSerialize, doc,
                                                 getter_AddRefs(transferable));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsString> data;
    PRUint32 dataSize;
    rv = transferable->GetTransferData(kHTMLMime, getter_AddRefs(data), &dataSize);
    if (NS_SUCCEEDED(rv)) {
      data->GetData(mHtmlString);
    }
    rv = transferable->GetTransferData(kHTMLContext, getter_AddRefs(data), &dataSize);
    if (NS_SUCCEEDED(rv)) {
      data->GetData(mContextString);
    }
    rv = transferable->GetTransferData(kHTMLInfo, getter_AddRefs(data), &dataSize);
    if (NS_SUCCEEDED(rv)) {
      data->GetData(mInfoString);
    }
    rv = transferable->GetTransferData(kUnicodeMime, getter_AddRefs(data), &dataSize);
    NS_ENSURE_SUCCESS(rv, rv); // require plain text at a minimum
    data->GetData(mTitleString);
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
  rv = AddStringsToDataTransfer(
         dragNode ? dragNode : mSelectionTargetNode.get(), aDataTransfer);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aDragNode = dragNode);
  return NS_OK;
}

void
DragDataProducer::AddString(nsDOMDataTransfer* aDataTransfer,
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
DragDataProducer::AddStringsToDataTransfer(nsIContent* aDragNode,
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

  // add a special flavor for the html context data
  if (!mContextString.IsEmpty())
    AddString(aDataTransfer, NS_LITERAL_STRING(kHTMLContext), mContextString, principal);

  // add a special flavor if we have html info data
  if (!mInfoString.IsEmpty())
    AddString(aDataTransfer, NS_LITERAL_STRING(kHTMLInfo), mInfoString, principal);

  // add the full html
  if (!mHtmlString.IsEmpty())
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
DragDataProducer::GetDraggableSelectionData(nsISelection* inSelection,
                                            nsIContent* inRealTargetNode,
                                            nsIContent **outImageOrLinkNode,
                                            bool* outDragSelectedText)
{
  NS_ENSURE_ARG(inSelection);
  NS_ENSURE_ARG(inRealTargetNode);
  NS_ENSURE_ARG_POINTER(outImageOrLinkNode);

  *outImageOrLinkNode = nsnull;
  *outDragSelectedText = false;

  bool selectionContainsTarget = false;

  bool isCollapsed = false;
  inSelection->GetIsCollapsed(&isCollapsed);
  if (!isCollapsed) {
    nsCOMPtr<nsIDOMNode> realTargetNode = do_QueryInterface(inRealTargetNode);
    inSelection->ContainsNode(realTargetNode, false,
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
        bool hasChildren;
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
      *outDragSelectedText = true;
    }
  }

  return NS_OK;
}

// static
void
DragDataProducer::GetSelectedLink(nsISelection* inSelection,
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
