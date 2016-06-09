/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCopySupport.h"
#include "nsIDocumentEncoder.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIComponentManager.h" 
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsIFormControl.h"
#include "nsISelection.h"
#include "nsWidgetsCID.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMRange.h"
#include "nsRange.h"
#include "imgIContainer.h"
#include "nsIPresShell.h"
#include "nsFocusManager.h"
#include "mozilla/dom/DataTransfer.h"

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
#include "nsIFrame.h"
#include "nsIURI.h"
#include "nsISimpleEnumerator.h"

// image copy stuff
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsContentUtils.h"
#include "nsContentCID.h"

#include "mozilla/ContentEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/IntegerRange.h"

using namespace mozilla;
using namespace mozilla::dom;

nsresult NS_NewDomSelection(nsISelection **aDomSelection);

static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kHTMLConverterCID,        NS_HTMLFORMATCONVERTER_CID);

// copy string data onto the transferable
static nsresult AppendString(nsITransferable *aTransferable,
                             const nsAString& aString,
                             const char* aFlavor);

// copy HTML node data
static nsresult AppendDOMNode(nsITransferable *aTransferable,
                              nsINode* aDOMNode);

// Helper used for HTMLCopy and GetTransferableForSelection since both routines
// share common code.
static nsresult
SelectionCopyHelper(nsISelection *aSel, nsIDocument *aDoc,
                    bool doPutOnClipboard, int16_t aClipboardID,
                    uint32_t aFlags, nsITransferable ** aTransferable)
{
  // Clear the output parameter for the transferable, if provided.
  if (aTransferable) {
    *aTransferable = nullptr;
  }

  nsresult rv;

  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  docEncoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  // note that we assign text/unicode as mime type, but in fact nsHTMLCopyEncoder
  // ignore it and use text/html or text/plain depending where the selection
  // is. if it is a selection into input/textarea element or in a html content
  // with pre-wrap style : text/plain. Otherwise text/html.
  // see nsHTMLCopyEncoder::SetSelection
  nsAutoString mimeType;
  mimeType.AssignLiteral(kUnicodeMime);

  // Do the first and potentially trial encoding as preformatted and raw.
  uint32_t flags = aFlags | nsIDocumentEncoder::OutputPreformatted
                          | nsIDocumentEncoder::OutputRaw
                          | nsIDocumentEncoder::OutputForPlainTextClipboardCopy;

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aDoc);
  NS_ASSERTION(domDoc, "Need a document");

  rv = docEncoder->Init(domDoc, mimeType, flags);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = docEncoder->SetSelection(aSel);
  NS_ENSURE_SUCCESS(rv, rv);

  // SetSelection set the mime type to text/plain if the selection is inside a
  // text widget.
  rv = docEncoder->GetMimeType(mimeType);
  NS_ENSURE_SUCCESS(rv, rv);
  bool selForcedTextPlain = mimeType.EqualsLiteral(kTextMime);

  nsAutoString buf;
  rv = docEncoder->EncodeToString(buf);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = docEncoder->GetMimeType(mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!selForcedTextPlain && mimeType.EqualsLiteral(kTextMime)) {
    // SetSelection and EncodeToString use this case to signal that text/plain
    // was forced because the document is either not an nsIHTMLDocument or it's
    // XHTML.  We want to pretty print XHTML but not non-nsIHTMLDocuments.
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(aDoc);
    if (!htmlDoc) {
      selForcedTextPlain = true;
    }
  }

  // The mime type is ultimately text/html if the encoder successfully encoded
  // the selection as text/html.
  bool encodedTextHTML = mimeType.EqualsLiteral(kHTMLMime);

  // First, prepare the text/plain clipboard flavor.
  nsAutoString textPlainBuf;
  if (selForcedTextPlain) {
    // Nothing to do.  buf contains the final, preformatted, raw text/plain.
    textPlainBuf.Assign(buf);
  } else {
    // Redo the encoding, but this time use pretty printing.
    flags =
      nsIDocumentEncoder::OutputSelectionOnly |
      nsIDocumentEncoder::OutputAbsoluteLinks |
      nsIDocumentEncoder::SkipInvisibleContent |
      nsIDocumentEncoder::OutputDropInvisibleBreak |
      (aFlags & (nsIDocumentEncoder::OutputNoScriptContent |
                 nsIDocumentEncoder::OutputRubyAnnotation));

    mimeType.AssignLiteral(kTextMime);
    rv = docEncoder->Init(domDoc, mimeType, flags);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = docEncoder->SetSelection(aSel);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = docEncoder->EncodeToString(textPlainBuf);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Second, prepare the text/html flavor.
  nsAutoString textHTMLBuf;
  nsAutoString htmlParentsBuf;
  nsAutoString htmlInfoBuf;
  if (encodedTextHTML) {
    // Redo the encoding, but this time use the passed-in flags.
    // Don't allow wrapping of CJK strings.
    mimeType.AssignLiteral(kHTMLMime);
    rv = docEncoder->Init(domDoc, mimeType,
                          aFlags |
                          nsIDocumentEncoder::OutputDisallowLineBreaking);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = docEncoder->SetSelection(aSel);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = docEncoder->EncodeToStringWithContext(htmlParentsBuf, htmlInfoBuf,
                                               textHTMLBuf);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the Clipboard
  nsCOMPtr<nsIClipboard> clipboard;
  if (doPutOnClipboard) {
    clipboard = do_GetService(kCClipboardCID, &rv);
    if (NS_FAILED(rv))
      return rv;
  }

  if ((doPutOnClipboard && clipboard) || aTransferable != nullptr) {
    // Create a transferable for putting data on the Clipboard
    nsCOMPtr<nsITransferable> trans = do_CreateInstance(kCTransferableCID);
    if (trans) {
      trans->Init(aDoc->GetLoadContext());
      if (encodedTextHTML) {
        // Set up a format converter so that clipboard flavor queries work.
        // This converter isn't really used for conversions.
        nsCOMPtr<nsIFormatConverter> htmlConverter =
          do_CreateInstance(kHTMLConverterCID);
        trans->SetConverter(htmlConverter);

        if (!textHTMLBuf.IsEmpty()) {
          // Add the html DataFlavor to the transferable
          rv = AppendString(trans, textHTMLBuf, kHTMLMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // Add the htmlcontext DataFlavor to the transferable
        // Even if parents is empty string, this flavor should
        // be attached to the transferable
        rv = AppendString(trans, htmlParentsBuf, kHTMLContext);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!htmlInfoBuf.IsEmpty()) {
          // Add the htmlinfo DataFlavor to the transferable
          rv = AppendString(trans, htmlInfoBuf, kHTMLInfo);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        if (!textPlainBuf.IsEmpty()) {
          // unicode text
          // Add the unicode DataFlavor to the transferable
          // If we didn't have this, then nsDataObj::GetData matches text/unicode against
          // the kURLMime flavour which is not desirable (eg. when pasting into Notepad)
          rv = AppendString(trans, textPlainBuf, kUnicodeMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // Try and get source URI of the items that are being dragged
        nsIURI *uri = aDoc->GetDocumentURI();
        if (uri) {
          nsAutoCString spec;
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
        if (!textPlainBuf.IsEmpty()) {
          // Add the unicode DataFlavor to the transferable
          rv = AppendString(trans, textPlainBuf, kUnicodeMime);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      if (doPutOnClipboard && clipboard) {
        bool actuallyPutOnClipboard = true;
        nsCopySupport::DoHooks(aDoc, trans, &actuallyPutOnClipboard);

        // put the transferable on the clipboard
        if (actuallyPutOnClipboard)
          clipboard->SetData(trans, nullptr, aClipboardID);
      }

      // Return the transferable to the caller if requested.
      if (aTransferable != nullptr) {
        trans.swap(*aTransferable);
      }
    }
  }
  return rv;
}

nsresult
nsCopySupport::HTMLCopy(nsISelection* aSel, nsIDocument* aDoc,
                        int16_t aClipboardID, bool aWithRubyAnnotation)
{
  uint32_t flags = nsIDocumentEncoder::SkipInvisibleContent;
  if (aWithRubyAnnotation) {
    flags |= nsIDocumentEncoder::OutputRubyAnnotation;
  }
  return SelectionCopyHelper(aSel, aDoc, true, aClipboardID, flags, nullptr);
}

nsresult
nsCopySupport::GetTransferableForSelection(nsISelection* aSel,
                                           nsIDocument* aDoc,
                                           nsITransferable** aTransferable)
{
  return SelectionCopyHelper(aSel, aDoc, false, 0,
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
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);
  RefPtr<nsRange> range = new nsRange(aNode);
  rv = range->SelectNode(node);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = selection->AddRange(range);
  NS_ENSURE_SUCCESS(rv, rv);
  // It's not the primary selection - so don't skip invisible content.
  uint32_t flags = 0;
  return SelectionCopyHelper(selection, aDoc, false, 0, flags,
                             aTransferable);
}

nsresult nsCopySupport::DoHooks(nsIDocument *aDoc, nsITransferable *aTrans,
                                bool *aDoPutOnClipboard)
{
  NS_ENSURE_ARG(aDoc);

  *aDoPutOnClipboard = true;

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
  bool hasMoreHooks = false;
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
      override->OnCopyOrDrag(nullptr, aTrans, aDoPutOnClipboard);
      NS_ASSERTION(NS_SUCCEEDED(hookResult), "OnCopyOrDrag hook failed");
      if (!*aDoPutOnClipboard)
        break;
    }
  }

  return rv;
}

nsresult
nsCopySupport::GetContents(const nsACString& aMimeType, uint32_t aFlags, nsISelection *aSel, nsIDocument *aDoc, nsAString& outdata)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIDocumentEncoder> docEncoder;

  nsAutoCString encoderContractID(NS_DOC_ENCODER_CONTRACTID_BASE);
  encoderContractID.Append(aMimeType);
    
  docEncoder = do_CreateInstance(encoderContractID.get());
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  uint32_t flags = aFlags | nsIDocumentEncoder::SkipInvisibleContent;
  
  if (aMimeType.EqualsLiteral("text/plain"))
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
                         nsILoadContext* aLoadContext,
                         int32_t aCopyFlags)
{
  nsresult rv;

  // create a transferable for putting data on the Clipboard
  nsCOMPtr<nsITransferable> trans(do_CreateInstance(kCTransferableCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  trans->Init(aLoadContext);

  if (aCopyFlags & nsIContentViewerEdit::COPY_IMAGE_TEXT) {
    // get the location from the element
    nsCOMPtr<nsIURI> uri;
    rv = aImageElement->GetCurrentURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

    nsAutoCString location;
    rv = uri->GetSpec(location);
    NS_ENSURE_SUCCESS(rv, rv);

    // append the string to the transferable
    rv = AppendString(trans, NS_ConvertUTF8toUTF16(location), kUnicodeMime);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCopyFlags & nsIContentViewerEdit::COPY_IMAGE_HTML) {
    // append HTML data to the transferable
    nsCOMPtr<nsINode> node(do_QueryInterface(aImageElement, &rv));
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
  bool selectionSupported;
  rv = clipboard->SupportsSelectionClipboard(&selectionSupported);
  NS_ENSURE_SUCCESS(rv, rv);

  // put the transferable on the clipboard
  if (selectionSupported) {
    rv = clipboard->SetData(trans, nullptr, nsIClipboard::kSelectionClipboard);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return clipboard->SetData(trans, nullptr, nsIClipboard::kGlobalClipboard);
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
                                        aString.Length() * sizeof(char16_t));
}

static nsresult AppendDOMNode(nsITransferable *aTransferable,
                              nsINode *aDOMNode)
{
  nsresult rv;

  // selializer
  nsCOMPtr<nsIDocumentEncoder>
    docEncoder(do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // get document for the encoder
  nsCOMPtr<nsIDocument> document = aDOMNode->OwnerDoc();

  // Note that XHTML is not counted as HTML here, because we can't copy it
  // properly (all the copy code for non-plaintext assumes using HTML
  // serializers and parsers is OK, and those mess up XHTML).
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  NS_ENSURE_TRUE(document->IsHTMLDocument(), NS_OK);

  // init encoder with document and node
  rv = docEncoder->NativeInit(document, NS_LITERAL_STRING(kHTMLMime),
                              nsIDocumentEncoder::OutputAbsoluteLinks |
                              nsIDocumentEncoder::OutputEncodeW3CEntities);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = docEncoder->SetNativeNode(aDOMNode);
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
  *aSelection = nullptr;

  nsIPresShell* presShell = aDocument->GetShell();
  if (!presShell)
    return nullptr;

  // check if the focused node in the window has a selection
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(aDocument->GetWindow(), false,
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
  NS_IF_ADDREF(*aSelection =
                 presShell->GetCurrentSelection(SelectionType::eNormal));
  return nullptr;
}

bool
nsCopySupport::CanCopy(nsIDocument* aDocument)
{
  if (!aDocument)
    return false;

  nsCOMPtr<nsISelection> sel;
  GetSelectionForCopy(aDocument, getter_AddRefs(sel));
  NS_ENSURE_TRUE(sel, false);

  bool isCollapsed;
  sel->GetIsCollapsed(&isCollapsed);
  return !isCollapsed;
}

static bool
IsInsideRuby(nsINode* aNode)
{
  for (; aNode; aNode = aNode->GetParent()) {
    if (aNode->IsHTMLElement(nsGkAtoms::ruby)) {
      return true;
    }
  }
  return false;
}

static bool
IsSelectionInsideRuby(nsISelection* aSelection)
{
  int32_t rangeCount;
  nsresult rv = aSelection->GetRangeCount(&rangeCount);
  if (NS_FAILED(rv)) {
    return false;
  }
  for (auto i : MakeRange(rangeCount)) {
    nsCOMPtr<nsIDOMRange> range;
    aSelection->GetRangeAt(i, getter_AddRefs(range));
    nsCOMPtr<nsIDOMNode> node;
    range->GetCommonAncestorContainer(getter_AddRefs(node));
    nsCOMPtr<nsINode> n = do_QueryInterface(node);
    if (!IsInsideRuby(n)) {
      return false;
    }
  }
  return true;
}

bool
nsCopySupport::FireClipboardEvent(EventMessage aEventMessage,
                                  int32_t aClipboardType,
                                  nsIPresShell* aPresShell,
                                  nsISelection* aSelection,
                                  bool* aActionTaken)
{
  if (aActionTaken) {
    *aActionTaken = false;
  }

  NS_ASSERTION(aEventMessage == eCut || aEventMessage == eCopy ||
               aEventMessage == ePaste,
               "Invalid clipboard event type");

  nsCOMPtr<nsIPresShell> presShell = aPresShell;
  if (!presShell)
    return false;

  nsCOMPtr<nsIDocument> doc = presShell->GetDocument();
  if (!doc)
    return false;

  nsCOMPtr<nsPIDOMWindowOuter> piWindow = doc->GetWindow();
  if (!piWindow)
    return false;

  // if a selection was not supplied, try to find it
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsISelection> sel = aSelection;
  if (!sel)
    content = GetSelectionForCopy(doc, getter_AddRefs(sel));

  // retrieve the event target node from the start of the selection
  nsresult rv;
  if (sel) {
    nsCOMPtr<nsIDOMRange> range;
    rv = sel->GetRangeAt(0, getter_AddRefs(range));
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
      return false;
  }

  // It seems to be unsafe to fire an event handler during reflow (bug 393696)
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::WarnScriptWasIgnored(doc);
    return false;
  }

  nsCOMPtr<nsIDocShell> docShell = piWindow->GetDocShell();
  const bool chromeShell =
    docShell && docShell->ItemType() == nsIDocShellTreeItem::typeChrome;

  // next, fire the cut, copy or paste event
  bool doDefault = true;
  RefPtr<DataTransfer> clipboardData;
  if (chromeShell || Preferences::GetBool("dom.event.clipboardevents.enabled", true)) {
    clipboardData =
      new DataTransfer(piWindow, aEventMessage, aEventMessage == ePaste,
                       aClipboardType);

    nsEventStatus status = nsEventStatus_eIgnore;
    InternalClipboardEvent evt(true, aEventMessage);
    evt.mClipboardData = clipboardData;
    EventDispatcher::Dispatch(content, presShell->GetPresContext(), &evt,
                              nullptr, &status);
    // If the event was cancelled, don't do the clipboard operation
    doDefault = (status != nsEventStatus_eConsumeNoDefault);
  }

  // No need to do anything special during a paste. Either an event listener
  // took care of it and cancelled the event, or the caller will handle it.
  // Return true to indicate that the event wasn't cancelled.
  if (aEventMessage == ePaste) {
    // Clear and mark the clipboardData as readonly. This prevents someone
    // from reading the clipboard contents after the paste event has fired.
    if (clipboardData) {
      clipboardData->ClearAll();
      clipboardData->SetReadOnly();
    }

    if (aActionTaken) {
      *aActionTaken = true;
    }
    return doDefault;
  }

  // Update the presentation in case the event handler modified the selection,
  // see bug 602231.
  presShell->FlushPendingNotifications(Flush_Frames);
  if (presShell->IsDestroying())
    return false;

  // if the event was not cancelled, do the default copy. If the event was cancelled,
  // use the data added to the data transfer and copy that instead.
  uint32_t count = 0;
  if (doDefault) {
    // find the focused node
    nsCOMPtr<nsIContent> srcNode = content;
    if (content->IsInNativeAnonymousSubtree()) {
      srcNode = content->FindFirstNonChromeOnlyAccessContent();
    }

    // check if we are looking at a password input
    nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(srcNode);
    if (formControl) {
      if (formControl->GetType() == NS_FORM_INPUT_PASSWORD) {
        return false;
      }
    }

    // when cutting non-editable content, do nothing
    // XXX this is probably the wrong editable flag to check
    if (aEventMessage != eCut || content->IsEditable()) {
      // get the data from the selection if any
      bool isCollapsed;
      sel->GetIsCollapsed(&isCollapsed);
      if (isCollapsed) {
        if (aActionTaken) {
          *aActionTaken = true;
        }
        return false;
      }
      // XXX Code which decides whether we should copy text with ruby
      // annotation is currenct depending on whether each range of the
      // selection is inside a same ruby container. But we really should
      // expose the full functionality in browser. See bug 1130891.
      bool withRubyAnnotation = IsSelectionInsideRuby(sel);
      // call the copy code
      rv = HTMLCopy(sel, doc, aClipboardType, withRubyAnnotation);
      if (NS_FAILED(rv)) {
        return false;
      }
    } else {
      return false;
    }
  } else if (clipboardData) {
    // check to see if any data was put on the data transfer.
    clipboardData->GetMozItemCount(&count);
    if (count) {
      nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1"));
      NS_ENSURE_TRUE(clipboard, false);

      nsCOMPtr<nsITransferable> transferable =
        clipboardData->GetTransferable(0, doc->GetLoadContext());

      NS_ENSURE_TRUE(transferable, false);

      // put the transferable on the clipboard
      rv = clipboard->SetData(transferable, nullptr, aClipboardType);
      if (NS_FAILED(rv)) {
        return false;
      }
    }
  }

  // Now that we have copied, update the clipboard commands. This should have
  // the effect of updating the enabled state of the paste menu item.
  if (doDefault || count) {
    piWindow->UpdateCommands(NS_LITERAL_STRING("clipboard"), nullptr, 0);
  }

  if (aActionTaken) {
    *aActionTaken = true;
  }
  return doDefault;
}
