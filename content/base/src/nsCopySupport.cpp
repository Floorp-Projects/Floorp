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

#include "nsIDocShell.h"
#include "nsIContentViewerEdit.h"
#include "nsIClipboardDragDropHooks.h"
#include "nsIClipboardDragDropHookList.h"

#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIHTMLDocument.h"
#include "nsHTMLAtoms.h"

// image copy stuff
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIImage.h"
#include "nsContentUtils.h"
#include "nsContentCID.h"

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

nsresult nsCopySupport::HTMLCopy(nsISelection *aSel, nsIDocument *aDoc, PRInt16 aClipboardID)
{
  nsresult rv = NS_OK;
  
  PRBool bIsPlainTextContext = PR_FALSE;

  rv = IsPlainTextContext(aSel, aDoc, &bIsPlainTextContext);
  if (NS_FAILED(rv)) 
    return rv;

  PRBool bIsHTMLCopy = !bIsPlainTextContext;
  nsAutoString mimeType;

  nsCOMPtr<nsIDocumentEncoder> docEncoder;

  docEncoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  // We always require a plaintext version
  mimeType.AssignLiteral(kUnicodeMime);
  PRUint32 flags = nsIDocumentEncoder::OutputPreformatted;

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

    flags = 0;

    rv = docEncoder->Init(domDoc, mimeType, flags);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = docEncoder->SetSelection(aSel);
    NS_ENSURE_SUCCESS(rv, rv);

    // encode the selection as html with contextual info
    rv = docEncoder->EncodeToStringWithContext(parents, info, buffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  // Get the Clipboard
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
  if (NS_FAILED(rv)) 
    return rv;

  if ( clipboard ) 
  {
    // Create a transferable for putting data on the Clipboard
    nsCOMPtr<nsITransferable> trans = do_CreateInstance(kCTransferableCID);
    if ( trans ) 
    {
      if (bIsHTMLCopy)
      {
        // set up the data converter
        trans->SetConverter(htmlConverter);

        if (!buffer.IsEmpty())
        {
          // Add the html DataFlavor to the transferable
          rv = AppendString(trans, buffer, kHTMLMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        {
          // Add the htmlcontext DataFlavor to the transferable
          // Even if parents is empty string, this flavor should
          // be attached to the transferable
          rv = AppendString(trans, parents, kHTMLContext);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        if (!info.IsEmpty())
        {
          // Add the htmlinfo DataFlavor to the transferable
          rv = AppendString(trans, info, kHTMLInfo);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        if (!plaintextBuffer.IsEmpty())
        {
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
      }
      else
      {
        if (!textBuffer.IsEmpty())
        {
         // Add the unicode DataFlavor to the transferable
          rv = AppendString(trans, textBuffer, kUnicodeMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      PRBool doPutOnClipboard = PR_TRUE;
      DoHooks(aDoc, trans, &doPutOnClipboard);

      // put the transferable on the clipboard
      if (doPutOnClipboard)
        clipboard->SetData(trans, nsnull, aClipboardID);
    }
  }
  return rv;
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

    if (!selContent->IsNodeOfType(nsINode::eHTML)) {
      continue;
    }

    nsIAtom *atom = selContent->Tag();

    if (atom == nsHTMLAtoms::input ||
        atom == nsHTMLAtoms::textarea)
    {
      *aIsPlainTextContext = PR_TRUE;
      break;
    }

    if (atom == nsHTMLAtoms::body)
    {
      // check for moz prewrap style on body.  If it's there we are 
      // in a plaintext editor.  This is pretty cheezy but I haven't 
      // found a good way to tell if we are in a plaintext editor.
      nsCOMPtr<nsIDOMElement> bodyElem = do_QueryInterface(selContent);
      nsAutoString wsVal;
      rv = bodyElem->GetAttribute(NS_LITERAL_STRING("style"), wsVal);
      if (NS_SUCCEEDED(rv) && (kNotFound != wsVal.Find(NS_LITERAL_STRING("-moz-pre-wrap"))))
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
  if (!htmlDoc || aDoc->IsCaseSensitive())
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

  PRUint32 flags = aFlags;
  
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
    nsCOMPtr<nsIImage> image =
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

  NS_ENSURE_TRUE(!(document->IsCaseSensitive()), NS_OK);

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
