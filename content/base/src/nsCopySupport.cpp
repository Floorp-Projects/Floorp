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
 *   Kathleen Brade <brade@netscape.com>
 *   David Gardiner <david.gardiner@unisa.edu.au>
 *   Mats Palmgren <matpal@gmail.com>
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

#include "nsCopySupport.h"
#include "nsIDocumentEncoder.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIComponentManager.h" 
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsISelection.h"
#include "nsWidgetsCID.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMRange.h"
#include "nsRange.h"
#include "imgIContainer.h"
#include "nsIPresShell.h"
#include "nsFocusManager.h"
#include "nsEventDispatcher.h"

#include "nsIDocShell.h"
#include "nsIContentViewerEdit.h"
#include "nsIClipboardDragDropHooks.h"
#include "nsIClipboardDragDropHookList.h"
#include "nsIClipboardHelper.h"
#include "nsISelectionController.h"

#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIHTMLDocument.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"
#include "nsIFrame.h"

// image copy stuff
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsContentUtils.h"
#include "nsContentCID.h"

nsresult NS_NewDomSelection(nsISelection **aDomSelection);

static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kHTMLConverterCID,        NS_HTMLFORMATCONVERTER_CID);

// private clipboard data flavors for html copy, used by editor when pasting
#define kHTMLContext   "text/_moz_htmlcontext"
#define kHTMLInfo      "text/_moz_htmlinfo"

// copy string data onto the transferable
static nsresult AppendString(nsITransferable *aTransferable,
                             const nsAString& aString,
                             const char* aFlavor);

// copy HTML node data
static nsresult AppendDOMNode(nsITransferable *aTransferable,
                              nsIDOMNode *aDOMNode);

// Helper used for HTMLCopy and GetTransferableForSelection since both routines
// share common code.
static nsresult
SelectionCopyHelper(nsISelection *aSel, nsIDocument *aDoc,
                    PRBool doPutOnClipboard, PRInt16 aClipboardID,
                    PRUint32 aFlags, nsITransferable ** aTransferable)
{
  // Clear the output parameter for the transferable, if provided.
  if (aTransferable) {
    *aTransferable = nsnull;
  }

  nsresult rv = NS_OK;
  
  PRBool bIsPlainTextContext = PR_FALSE;

  rv = nsCopySupport::IsPlainTextContext(aSel, aDoc, &bIsPlainTextContext);
  if (NS_FAILED(rv)) 
    return rv;

  PRBool bIsHTMLCopy = !bIsPlainTextContext;
  nsAutoString mimeType;

  nsCOMPtr<nsIDocumentEncoder> docEncoder;

  docEncoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  // We always require a plaintext version
  
  // note that we assign text/unicode as mime type, but in fact nsHTMLCopyEncoder
  // ignore it and use text/html or text/plain depending where the selection
  // is. if it is a selection into input/textarea element or in a html content
  // with pre-wrap style : text/plain. Otherwise text/html.
  // see nsHTMLCopyEncoder::SetSelection
  mimeType.AssignLiteral(kUnicodeMime);
  
  // we want preformatted for the case where the selection is inside input/textarea
  // and we don't want pretty printing for others cases, to not have additionnal
  // line breaks which are then converted into spaces by the htmlConverter (see bug #524975)
  PRUint32 flags = aFlags | nsIDocumentEncoder::OutputPreformatted
                          | nsIDocumentEncoder::OutputRaw;

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aDoc);
  NS_ASSERTION(domDoc, "Need a document");

  rv = docEncoder->Init(domDoc, mimeType, flags);
  if (NS_FAILED(rv)) 
    return rv;

  rv = docEncoder->SetSelection(aSel);
  if (NS_FAILED(rv)) 
    return rv;

  nsAutoString buffer, parents, info, textBuffer, plaintextBuffer;

  rv = docEncoder->EncodeToString(textBuffer);
  if (NS_FAILED(rv)) 
    return rv;

  nsCOMPtr<nsIFormatConverter> htmlConverter;

  // sometimes we also need the HTML version
  if (bIsHTMLCopy) {

    // this string may still contain HTML formatting, so we need to remove that too.
    htmlConverter = do_CreateInstance(kHTMLConverterCID);
    NS_ENSURE_TRUE(htmlConverter, NS_ERROR_FAILURE);

    nsCOMPtr<nsISupportsString> plainHTML = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    NS_ENSURE_TRUE(plainHTML, NS_ERROR_FAILURE);
    plainHTML->SetData(textBuffer);

    nsCOMPtr<nsISupportsString> ConvertedData;
    PRUint32 ConvertedLen;
    rv = htmlConverter->Convert(kHTMLMime, plainHTML, textBuffer.Length() * 2, kUnicodeMime, getter_AddRefs(ConvertedData), &ConvertedLen);
    NS_ENSURE_SUCCESS(rv, rv);

    ConvertedData->GetData(plaintextBuffer);

    mimeType.AssignLiteral(kHTMLMime);

    flags = aFlags;

    rv = docEncoder->Init(domDoc, mimeType, flags);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = docEncoder->SetSelection(aSel);
    NS_ENSURE_SUCCESS(rv, rv);

    // encode the selection as html with contextual info
    rv = docEncoder->EncodeToStringWithContext(parents, info, buffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  // Get the Clipboard
  nsCOMPtr<nsIClipboard> clipboard;
  if (doPutOnClipboard) {
    clipboard = do_GetService(kCClipboardCID, &rv);
    if (NS_FAILED(rv))
      return rv;
  }

  if ((doPutOnClipboard && clipboard) || aTransferable != nsnull) {
    // Create a transferable for putting data on the Clipboard
    nsCOMPtr<nsITransferable> trans = do_CreateInstance(kCTransferableCID);
    if (trans) {
      if (bIsHTMLCopy) {
        // set up the data converter
        trans->SetConverter(htmlConverter);

        if (!buffer.IsEmpty()) {
          // Add the html DataFlavor to the transferable
          rv = AppendString(trans, buffer, kHTMLMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // Add the htmlcontext DataFlavor to the transferable
        // Even if parents is empty string, this flavor should
        // be attached to the transferable
        rv = AppendString(trans, parents, kHTMLContext);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!info.IsEmpty()) {
          // Add the htmlinfo DataFlavor to the transferable
          rv = AppendString(trans, info, kHTMLInfo);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        if (!plaintextBuffer.IsEmpty()) {
          // unicode text
          // Add the unicode DataFlavor to the transferable
          // If we didn't have this, then nsDataObj::GetData matches text/unicode against
          // the kURLMime flavour which is not desirable (eg. when pasting into Notepad)
          rv = AppendString(trans, plaintextBuffer, kUnicodeMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // Try and get source URI of the items that are being dragged
        nsIURI *uri = aDoc->GetDocumentURI();
        if (uri) {
          nsCAutoString spec;
          uri->GetSpec(spec);
          if (!spec.IsEmpty()) {
            nsAutoString shortcut;
            AppendUTF8toUTF16(spec, shortcut);

            // Add the URL DataFlavor to the transferable. Don't use kURLMime, as it will
            // cause an unnecessary UniformResourceLocator to be added which confuses
            // some apps eg. Outlook 2000 - (See Bug 315370). Don't use
            // kURLDataMime, as it will cause a bogus 'url ' flavor to
            // show up on the Mac clipboard, confusing other apps, like
            // Terminal (see bug 336012).
            rv = AppendString(trans, shortcut, kURLPrivateMime);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        }
      } else {
        if (!textBuffer.IsEmpty()) {
          // Add the unicode DataFlavor to the transferable
          rv = AppendString(trans, textBuffer, kUnicodeMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      if (doPutOnClipboard && clipboard) {
        PRBool actuallyPutOnClipboard = PR_TRUE;
        nsCopySupport::DoHooks(aDoc, trans, &actuallyPutOnClipboard);

        // put the transferable on the clipboard
        if (actuallyPutOnClipboard)
          clipboard->SetData(trans, nsnull, aClipboardID);
      }

      // Return the transferable to the caller if requested.
      if (aTransferable != nsnull) {
        trans.swap(*aTransferable);
      }
    }
  }
  return rv;
}

nsresult
nsCopySupport::HTMLCopy(nsISelection* aSel, nsIDocument* aDoc,
                        PRInt16 aClipboardID)
{
  return SelectionCopyHelper(aSel, aDoc, PR_TRUE, aClipboardID,
                             nsIDocumentEncoder::SkipInvisibleContent,
                             nsnull);
}

nsresult
nsCopySupport::GetTransferableForSelection(nsISelection* aSel,
                                           nsIDocument* aDoc,
                                           nsITransferable** aTransferable)
{
  return SelectionCopyHelper(aSel, aDoc, PR_FALSE, 0,
                             nsIDocumentEncoder::SkipInvisibleContent,
                             aTransferable);
}

nsresult
nsCopySupport::GetTransferableForNode(nsINode* aNode,
                                      nsIDocument* aDoc,
                                      nsITransferable** aTransferable)
{
  nsCOMPtr<nsISelection> selection;
  // Make a temporary selection with aNode in a single range.
  nsresult rv = NS_NewDomSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMRange> range;
  rv = NS_NewRange(getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);
  rv = range->SelectNode(node);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = selection->AddRange(range);
  NS_ENSURE_SUCCESS(rv, rv);
  // It's not the primary selection - so don't skip invisible content.
  PRUint32 flags = 0;
  return SelectionCopyHelper(selection, aDoc, PR_FALSE, 0, flags,
                             aTransferable);
}

nsresult nsCopySupport::DoHooks(nsIDocument *aDoc, nsITransferable *aTrans,
                                PRBool *aDoPutOnClipboard)
{
  NS_ENSURE_ARG(aDoc);

  *aDoPutOnClipboard = PR_TRUE;

  nsCOMPtr<nsISupports> container = aDoc->GetContainer();
  nsCOMPtr<nsIClipboardDragDropHookList> hookObj = do_GetInterface(container);
  if (!hookObj) return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  hookObj->GetHookEnumerator(getter_AddRefs(enumerator));
  if (!enumerator) return NS_ERROR_FAILURE;

  // the logic here should follow the behavior specified in
  // nsIClipboardDragDropHooks.h

  nsCOMPtr<nsIClipboardDragDropHooks> override;
  nsCOMPtr<nsISupports> isupp;
  PRBool hasMoreHooks = PR_FALSE;
  nsresult rv = NS_OK;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreHooks))
         && hasMoreHooks)
  {
    rv = enumerator->GetNext(getter_AddRefs(isupp));
    if (NS_FAILED(rv)) break;
    override = do_QueryInterface(isupp);
    if (override)
    {
#ifdef DEBUG
      nsresult hookResult =
#endif
      override->OnCopyOrDrag(nsnull, aTrans, aDoPutOnClipboard);
      NS_ASSERTION(NS_SUCCEEDED(hookResult), "OnCopyOrDrag hook failed");
      if (!*aDoPutOnClipboard)
        break;
    }
  }

  return rv;
}

nsresult nsCopySupport::IsPlainTextContext(nsISelection *aSel, nsIDocument *aDoc, PRBool *aIsPlainTextContext)
{
  nsresult rv;

  if (!aSel || !aIsPlainTextContext)
    return NS_ERROR_NULL_POINTER;

  *aIsPlainTextContext = PR_FALSE;
  
  nsCOMPtr<nsIDOMRange> range;
  nsCOMPtr<nsIDOMNode> commonParent;
  PRInt32 count = 0;

  rv = aSel->GetRangeCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  // if selection is uninitialized return
  if (!count)
    return NS_ERROR_FAILURE;
  
  // we'll just use the common parent of the first range.  Implicit assumption
  // here that multi-range selections are table cell selections, in which case
  // the common parent is somewhere in the table and we don't really care where.
  rv = aSel->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!range)
    return NS_ERROR_NULL_POINTER;
  range->GetCommonAncestorContainer(getter_AddRefs(commonParent));

  for (nsCOMPtr<nsIContent> selContent(do_QueryInterface(commonParent));
       selContent;
       selContent = selContent->GetParent())
  {
    // checking for selection inside a plaintext form widget

    if (!selContent->IsHTML()) {
      continue;
    }

    nsIAtom *atom = selContent->Tag();

    if (atom == nsGkAtoms::input ||
        atom == nsGkAtoms::textarea)
    {
      *aIsPlainTextContext = PR_TRUE;
      break;
    }

    if (atom == nsGkAtoms::body)
    {
      // check for moz prewrap style on body.  If it's there we are 
      // in a plaintext editor.  This is pretty cheezy but I haven't 
      // found a good way to tell if we are in a plaintext editor.
      nsCOMPtr<nsIDOMElement> bodyElem = do_QueryInterface(selContent);
      nsAutoString wsVal;
      rv = bodyElem->GetAttribute(NS_LITERAL_STRING("style"), wsVal);
      if (NS_SUCCEEDED(rv) && (kNotFound != wsVal.Find(NS_LITERAL_STRING("pre-wrap"))))
      {
        *aIsPlainTextContext = PR_TRUE;
        break;
      }
    }
  }
  
  // also consider ourselves in a text widget if we can't find an html
  // document. Note that XHTML is not counted as HTML here, because we can't
  // copy it properly (all the copy code for non-plaintext assumes using HTML
  // serializers and parsers is OK, and those mess up XHTML).
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(aDoc);
  if (!(htmlDoc && aDoc->IsHTML()))
    *aIsPlainTextContext = PR_TRUE;

  return NS_OK;
}

nsresult
nsCopySupport::GetContents(const nsACString& aMimeType, PRUint32 aFlags, nsISelection *aSel, nsIDocument *aDoc, nsAString& outdata)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIDocumentEncoder> docEncoder;

  nsCAutoString encoderContractID(NS_DOC_ENCODER_CONTRACTID_BASE);
  encoderContractID.Append(aMimeType);
    
  docEncoder = do_CreateInstance(encoderContractID.get());
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  PRUint32 flags = aFlags | nsIDocumentEncoder::SkipInvisibleContent;
  
  if (aMimeType.Equals("text/plain"))
    flags |= nsIDocumentEncoder::OutputPreformatted;

  NS_ConvertASCIItoUTF16 unicodeMimeType(aMimeType);

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aDoc);
  NS_ASSERTION(domDoc, "Need a document");

  rv = docEncoder->Init(domDoc, unicodeMimeType, flags);
  if (NS_FAILED(rv)) return rv;
  
  if (aSel)
  {
    rv = docEncoder->SetSelection(aSel);
    if (NS_FAILED(rv)) return rv;
  } 
  
  // encode the selection
  return docEncoder->EncodeToString(outdata);
}


nsresult
nsCopySupport::ImageCopy(nsIImageLoadingContent* aImageElement,
                         PRInt32 aCopyFlags)
{
  nsresult rv;

  // create a transferable for putting data on the Clipboard
  nsCOMPtr<nsITransferable> trans(do_CreateInstance(kCTransferableCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aCopyFlags & nsIContentViewerEdit::COPY_IMAGE_TEXT) {
    // get the location from the element
    nsCOMPtr<nsIURI> uri;
    rv = aImageElement->GetCurrentURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

    nsCAutoString location;
    rv = uri->GetSpec(location);
    NS_ENSURE_SUCCESS(rv, rv);

    // append the string to the transferable
    rv = AppendString(trans, NS_ConvertUTF8toUTF16(location), kUnicodeMime);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCopyFlags & nsIContentViewerEdit::COPY_IMAGE_HTML) {
    // append HTML data to the transferable
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aImageElement, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AppendDOMNode(trans, node);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCopyFlags & nsIContentViewerEdit::COPY_IMAGE_DATA) {
    // get the image data from the element
    nsCOMPtr<imgIContainer> image =
      nsContentUtils::GetImageFromContent(aImageElement);
    NS_ENSURE_TRUE(image, NS_ERROR_FAILURE);

    nsCOMPtr<nsISupportsInterfacePointer>
      imgPtr(do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = imgPtr->SetData(image);
    NS_ENSURE_SUCCESS(rv, rv);

    // copy the image data onto the transferable
    rv = trans->SetTransferData(kNativeImageMime, imgPtr,
                                sizeof(nsISupports*));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // get clipboard
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // check whether the system supports the selection clipboard or not.
  PRBool selectionSupported;
  rv = clipboard->SupportsSelectionClipboard(&selectionSupported);
  NS_ENSURE_SUCCESS(rv, rv);

  // put the transferable on the clipboard
  if (selectionSupported) {
    rv = clipboard->SetData(trans, nsnull, nsIClipboard::kSelectionClipboard);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return clipboard->SetData(trans, nsnull, nsIClipboard::kGlobalClipboard);
}

static nsresult AppendString(nsITransferable *aTransferable,
                             const nsAString& aString,
                             const char* aFlavor)
{
  nsresult rv;

  nsCOMPtr<nsISupportsString>
    data(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = data->SetData(aString);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aTransferable->AddDataFlavor(aFlavor);
  NS_ENSURE_SUCCESS(rv, rv);

  return aTransferable->SetTransferData(aFlavor, data,
                                        aString.Length() * sizeof(PRUnichar));
}

static nsresult AppendDOMNode(nsITransferable *aTransferable,
                              nsIDOMNode *aDOMNode)
{
  nsresult rv;
  
  // selializer
  nsCOMPtr<nsIDocumentEncoder>
    docEncoder(do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // get document for the encoder
  nsCOMPtr<nsIDOMDocument> domDocument;
  rv = aDOMNode->GetOwnerDocument(getter_AddRefs(domDocument));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocument> document(do_QueryInterface(domDocument, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Note that XHTML is not counted as HTML here, because we can't copy it
  // properly (all the copy code for non-plaintext assumes using HTML
  // serializers and parsers is OK, and those mess up XHTML).
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(domDocument, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  NS_ENSURE_TRUE(document->IsHTML(), NS_OK);

  // init encoder with document and node
  rv = docEncoder->Init(domDocument, NS_LITERAL_STRING(kHTMLMime),
                        nsIDocumentEncoder::OutputAbsoluteLinks |
                        nsIDocumentEncoder::OutputEncodeW3CEntities);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = docEncoder->SetNode(aDOMNode);
  NS_ENSURE_SUCCESS(rv, rv);

  // serialize to string
  nsAutoString html, context, info;
  rv = docEncoder->EncodeToStringWithContext(context, info, html);
  NS_ENSURE_SUCCESS(rv, rv);

  // copy them to the transferable
  if (!html.IsEmpty()) {
    rv = AppendString(aTransferable, html, kHTMLMime);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!info.IsEmpty()) {
    rv = AppendString(aTransferable, info, kHTMLInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // add a special flavor, even if we don't have html context data
  return AppendString(aTransferable, context, kHTMLContext);
}

nsIContent*
nsCopySupport::GetSelectionForCopy(nsIDocument* aDocument, nsISelection** aSelection)
{
  *aSelection = nsnull;

  nsIPresShell* presShell = aDocument->GetShell();
  if (!presShell)
    return nsnull;

  // check if the focused node in the window has a selection
  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(aDocument->GetWindow(), PR_FALSE,
                                         getter_AddRefs(focusedWindow));
  if (content) {
    nsIFrame* frame = content->GetPrimaryFrame();
    if (frame) {
      nsCOMPtr<nsISelectionController> selCon;
      frame->GetSelectionController(presShell->GetPresContext(), getter_AddRefs(selCon));
      if (selCon) {
        selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);
        return content;
      }
    }
  }

  // if no selection was found, use the main selection for the window
  NS_IF_ADDREF(*aSelection = presShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL));
  return nsnull;
}

PRBool
nsCopySupport::CanCopy(nsIDocument* aDocument)
{
  if (!aDocument)
    return PR_FALSE;

  nsCOMPtr<nsISelection> sel;
  GetSelectionForCopy(aDocument, getter_AddRefs(sel));
  NS_ENSURE_TRUE(sel, PR_FALSE);

  PRBool isCollapsed;
  sel->GetIsCollapsed(&isCollapsed);
  return !isCollapsed;
}

PRBool
nsCopySupport::FireClipboardEvent(PRInt32 aType, nsIPresShell* aPresShell, nsISelection* aSelection)
{
  NS_ASSERTION(aType == NS_CUT || aType == NS_COPY || aType == NS_PASTE,
               "Invalid clipboard event type");

  nsCOMPtr<nsIPresShell> presShell = aPresShell;
  if (!presShell)
    return PR_FALSE;

  nsCOMPtr<nsIDocument> doc = presShell->GetDocument();
  if (!doc)
    return PR_FALSE;

  nsCOMPtr<nsPIDOMWindow> piWindow = doc->GetWindow();
  if (!piWindow)
    return PR_FALSE;

  // if a selection was not supplied, try to find it
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsISelection> sel = aSelection;
  if (!sel)
    content = GetSelectionForCopy(doc, getter_AddRefs(sel));

  // retrieve the event target node from the start of the selection
  if (sel) {
    // Only cut or copy when there is an uncollapsed selection
    if (aType == NS_CUT || aType == NS_COPY) {
      PRBool isCollapsed;
      sel->GetIsCollapsed(&isCollapsed);
      if (isCollapsed)
        return PR_FALSE;
    }

    nsCOMPtr<nsIDOMRange> range;
    nsresult rv = sel->GetRangeAt(0, getter_AddRefs(range));
    if (NS_SUCCEEDED(rv) && range) {
      nsCOMPtr<nsIDOMNode> startContainer;
      range->GetStartContainer(getter_AddRefs(startContainer));
      if (startContainer)
        content = do_QueryInterface(startContainer);
    }
  }

  // if no content node was set, just get the root
  if (!content) {
    content = doc->GetRootElement();
    if (!content)
      return PR_FALSE;
  }

  // It seems to be unsafe to fire an event handler during reflow (bug 393696)
  if (!nsContentUtils::IsSafeToRunScript())
    return PR_FALSE;

  // next, fire the cut or copy event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent evt(PR_TRUE, aType);
  nsEventDispatcher::Dispatch(content, presShell->GetPresContext(), &evt, nsnull,
                              &status);
  // if the event was cancelled, don't do the clipboard operation
  if (status == nsEventStatus_eConsumeNoDefault)
    return PR_FALSE;

  if (presShell->IsDestroying())
    return PR_FALSE;

  // No need to do anything special during a paste. Either an event listener
  // took care of it and cancelled the event, or the caller will handle it.
  // Return true to indicate the event wasn't cancelled.
  if (aType == NS_PASTE)
    return PR_TRUE;

  // Update the presentation in case the event handler modified the selection,
  // see bug 602231.
  presShell->FlushPendingNotifications(Flush_Frames);
  if (presShell->IsDestroying())
    return PR_FALSE;

  // call the copy code
  if (NS_FAILED(nsCopySupport::HTMLCopy(sel, doc, nsIClipboard::kGlobalClipboard)))
    return PR_FALSE;

  // Now that we have copied, update the clipboard commands. This should have
  // the effect of updating the paste menu item.
  piWindow->UpdateCommands(NS_LITERAL_STRING("clipboard"));

  return PR_TRUE;
}
