/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * Oleg Romashin. Portions created by Oleg Romashin are Copyright (C) Oleg Romashin.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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
#include "EmbedContextMenuInfo.h"
#include "nsIImageLoadingContent.h"
#include "imgILoader.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIWebBrowser.h"
#include "nsIDOM3Document.h"
#include "nsIContent.h"
#include "nsIFormControl.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsIDocument.h"
#include "EmbedPrivate.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <glib.h>

//*****************************************************************************
// class EmbedContextMenuInfo
//*****************************************************************************
EmbedContextMenuInfo::EmbedContextMenuInfo(EmbedPrivate *aOwner) : mCtxFrameNum(-1), mEmbedCtxType(0)
{
  mOwner = aOwner;
  mEventNode = nsnull;
  mCtxDocument = nsnull;
  mNSHHTMLElement = nsnull;
  mNSHHTMLElementSc = nsnull;
  mFormRect = nsRect(0,0,0,0);
}

EmbedContextMenuInfo::~EmbedContextMenuInfo(void)
{
}

NS_IMPL_ADDREF(EmbedContextMenuInfo)
NS_IMPL_RELEASE(EmbedContextMenuInfo)
NS_INTERFACE_MAP_BEGIN(EmbedContextMenuInfo)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsresult
EmbedContextMenuInfo::SetFrameIndex()
{
  nsCOMPtr <nsIDOMWindowCollection> frames;
  mCtxDomWindow->GetFrames (getter_AddRefs (frames));
  nsCOMPtr <nsIDOMWindow> currentWindow;
  PRUint32 frameCount = 0;
  frames->GetLength (&frameCount);
  for (unsigned int i= 0; i < frameCount; i++) {
    frames->Item(i, getter_AddRefs (currentWindow));
    nsCOMPtr<nsIDOMDocument> currentDoc;
    currentWindow->GetDocument (getter_AddRefs(currentDoc));
    if (currentDoc == mCtxDocument) {
      mCtxFrameNum = i;
      mCtxDomWindow = currentWindow;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
EmbedContextMenuInfo::GetFormControlType(nsIDOMEvent* aEvent)
{
  if (!aEvent)
    return NS_OK;
  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));
  nsCOMPtr<nsIDOMEventTarget> target;
  nsevent->GetOriginalTarget(getter_AddRefs(target));
  //    mOrigTarget  = target;
  if (SetFormControlType(target)) {
    nsCOMPtr<nsIDOMNode> eventNode = do_QueryInterface(target);
    if (!eventNode)
      return NS_OK;
    eventNode->GetNodeName(mSEventNode);
    //Frame Stuff
    nsCOMPtr<nsIDOMDocument> domDoc;
    nsresult rv = eventNode->GetOwnerDocument(getter_AddRefs(domDoc));
    if (!NS_SUCCEEDED (rv) || !domDoc) {
      return NS_OK;
    }
    mEventNode = eventNode;
    mCtxDocument = domDoc;          
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mCtxDocument);
    if (!doc)
      return NS_OK;
    nsIPresShell *presShell = doc->GetShellAt(0);
    if (!presShell)
      return NS_OK;
    nsCOMPtr<nsIContent> tgContent = do_QueryInterface(mEventTarget);
#ifdef MOZILLA_1_8_BRANCH
    nsIFrame* frame = nsnull;
    presShell->GetPrimaryFrameFor(tgContent, &frame);
#else
    nsIFrame* frame = presShell->GetPrimaryFrameFor(tgContent);
#endif
    if (frame)
      mFormRect = frame->GetScreenRectExternal();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
EmbedContextMenuInfo::SetFormControlType(nsIDOMEventTarget *originalTarget)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(originalTarget);
  mCtxFormType = 0;
#ifdef MOZILLA_1_8_BRANCH
  if (targetContent && targetContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) {
#else
  if (targetContent && targetContent->IsNodeOfType(nsIContent::eHTML_FORM_CONTROL)) {
#endif
    nsCOMPtr<nsIFormControl> formControl (do_QueryInterface(targetContent));
    if (formControl) {
      mCtxFormType = formControl->GetType();
      rv = NS_OK;
      //#ifdef MOZ_LOGGING
      switch (mCtxFormType) {
      case NS_FORM_BUTTON_BUTTON:
        break;
      case NS_FORM_BUTTON_RESET:
        break;
      case NS_FORM_BUTTON_SUBMIT:
        break;
      case NS_FORM_FIELDSET:
        break;
      case NS_FORM_INPUT_BUTTON:
        break;
      case NS_FORM_INPUT_CHECKBOX:
        break;
      case NS_FORM_INPUT_FILE:
        mEmbedCtxType |= GTK_MOZ_EMBED_CTX_INPUT;
        break;
      case NS_FORM_INPUT_HIDDEN:
        break;
      case NS_FORM_INPUT_RESET:
        break;
      case NS_FORM_INPUT_IMAGE:
        break;
      case NS_FORM_INPUT_PASSWORD:
        mEmbedCtxType |= GTK_MOZ_EMBED_CTX_INPUT;
        mEmbedCtxType |= GTK_MOZ_EMBED_CTX_IPASSWORD;
        break;
      case NS_FORM_INPUT_RADIO:
        break;
      case NS_FORM_INPUT_SUBMIT:
        break;
      case NS_FORM_INPUT_TEXT:
        mEmbedCtxType |= GTK_MOZ_EMBED_CTX_INPUT;
        break;
      case NS_FORM_LABEL:
        break;
      case NS_FORM_OPTION:
        break;
      case NS_FORM_OPTGROUP:
        break;
      case NS_FORM_LEGEND:
        break;
      case NS_FORM_SELECT:
        break;
      case NS_FORM_TEXTAREA:
        mEmbedCtxType |= GTK_MOZ_EMBED_CTX_INPUT;
        break;
      case NS_FORM_OBJECT:
        break;
      default:
        break;
      }
      if (mEmbedCtxType & GTK_MOZ_EMBED_CTX_INPUT) {
        PRBool rdonly = PR_FALSE;
        if (mCtxFormType == NS_FORM_TEXTAREA) {
          nsCOMPtr <nsIDOMHTMLTextAreaElement> input;
          input = do_QueryInterface(mEventNode, &rv);
          if (!NS_FAILED(rv) && input) 
            rv = input->GetReadOnly(&rdonly);
          if (!NS_FAILED(rv) && rdonly) {
            mEmbedCtxType |= GTK_MOZ_EMBED_CTX_ROINPUT;
          }
        } else {
          nsCOMPtr <nsIDOMHTMLInputElement> input;
          input = do_QueryInterface(mEventNode, &rv);
          if (!NS_FAILED(rv) && input) 
            rv = input->GetReadOnly(&rdonly);
          if (!NS_FAILED(rv) && rdonly) {
            mEmbedCtxType |= GTK_MOZ_EMBED_CTX_ROINPUT;
          }  
        }
      }
      //#endif
    }
  }
  return rv;
}

const char*
EmbedContextMenuInfo::GetSelectedText()
{
  nsString cString;
  nsresult rv = NS_ERROR_FAILURE;
  if (mCtxFormType != 0 && mEventNode) {
    PRInt32 TextLength = 0, selStart = 0, selEnd = 0;
    if (mCtxFormType == NS_FORM_INPUT_TEXT || mCtxFormType == NS_FORM_INPUT_FILE) {
      nsCOMPtr<nsIDOMNSHTMLInputElement> nsinput = do_QueryInterface(mEventNode, &rv);
      if (NS_SUCCEEDED(rv) && nsinput)
        nsinput->GetTextLength(&TextLength);
      if (TextLength > 0) {
        nsinput->GetSelectionEnd(&selEnd);
        nsinput->GetSelectionStart(&selStart);
        if (selStart < selEnd || mCtxFormType == NS_FORM_INPUT_FILE) {
          nsCOMPtr<nsIDOMHTMLInputElement> input = do_QueryInterface (mEventNode, &rv);
          rv = input->GetValue(cString);
        }
      }
    } else if (mCtxFormType == NS_FORM_TEXTAREA) {
      nsCOMPtr<nsIDOMNSHTMLTextAreaElement> nsinput = do_QueryInterface(mEventNode, &rv);
      if (NS_SUCCEEDED(rv) && nsinput)
        nsinput->GetTextLength(&TextLength);
      if (TextLength > 0) {
        nsinput->GetSelectionStart(&selStart);
        nsinput->GetSelectionEnd(&selEnd);
        if (selStart < selEnd) {
          nsCOMPtr<nsIDOMHTMLTextAreaElement> input = do_QueryInterface (mEventNode, &rv);
          rv = input->GetValue(cString);
        }
      }
    }
    if (NS_SUCCEEDED(rv) && cString.Length()) {
      if (selStart < selEnd) {
        cString.Cut(0, selStart);
        cString.Cut(selEnd-selStart, TextLength);
      }
      rv = NS_OK;
    }
  } else if (mCtxDocument) {
    nsCOMPtr<nsIDOMNSHTMLDocument> htmlDoc = do_QueryInterface(mCtxDocument, &rv);
    if (NS_SUCCEEDED(rv) && htmlDoc)
	htmlDoc->GetSelection(cString);
    if ( cString.Length() > 0) {
      rv = NS_OK;
    }                      
  }
  if (rv == NS_OK) {
    return NS_ConvertUTF16toUTF8(cString).get();
  }
  return nsnull;
}

nsresult
EmbedContextMenuInfo::CheckDomImageElement(nsIDOMNode *node, nsString& aHref,
                                           PRInt32 *aWidth, PRInt32 *aHeight)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr <nsIDOMHTMLImageElement> image =
    do_QueryInterface(node, &rv);
  if (image) {
    rv = image->GetSrc (aHref);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = image->GetWidth (aWidth);
    rv = image->GetHeight (aHeight);
    rv = NS_OK;
  }
  return rv;
}

nsresult
EmbedContextMenuInfo::CheckDomHtmlNode(nsIDOMNode *node)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsString uTag;
  PRUint16 dnode_type;
  rv = node->GetNodeType(&dnode_type);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIDOMHTMLElement> element  = do_QueryInterface(node);
  if (!((nsIDOMNode::ELEMENT_NODE == dnode_type) && element)) {
    return rv;
  }
  nsCOMPtr <nsIDOMNSHTMLElement> nodeElement = do_QueryInterface(node, &rv);
  if (NS_SUCCEEDED(rv) && nodeElement) {
    mNSHHTMLElement = nodeElement;
  } else {
    mNSHHTMLElement = nsnull;
  }
  rv = element->GetLocalName(uTag);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (uTag.LowerCaseEqualsLiteral("object")) {
  }
  else if (uTag.LowerCaseEqualsLiteral("html")) {
  }
  else if (uTag.LowerCaseEqualsLiteral("a")) {
    nsCOMPtr <nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(node);
    anchor->GetHref (mCtxHref);
    mEmbedCtxType |= GTK_MOZ_EMBED_CTX_LINK;
    if (anchor && !mCtxHref.IsEmpty()) {
      if (mCtxHref.LowerCaseEqualsLiteral("text/smartbookmark")) {
        nsCOMPtr<nsIDOMNode> childNode;
        node->GetFirstChild (getter_AddRefs(childNode));
        if (childNode) {
          PRInt32 width, height;
          rv = CheckDomImageElement(node, mCtxImgHref, &width, &height);
	  if (NS_SUCCEEDED(rv))
	    mEmbedCtxType |= GTK_MOZ_EMBED_CTX_IMAGE;
        }
      } else if (StringBeginsWith(mCtxHref, NS_LITERAL_STRING("mailto:"))) {
          mEmbedCtxType |= GTK_MOZ_EMBED_CTX_EMAIL;
      }
    }
  }
  else if (uTag.LowerCaseEqualsLiteral("area")) {
    nsCOMPtr <nsIDOMHTMLAreaElement> area = do_QueryInterface(node, &rv);
    if (NS_SUCCEEDED(rv) && area) {
      PRBool aNoHref = PR_FALSE;
      rv = area->GetNoHref (&aNoHref);
      if (aNoHref == PR_FALSE)
        rv = area->GetHref (mCtxHref);
      else
        rv = area->GetTarget (mCtxHref);
      mEmbedCtxType |= GTK_MOZ_EMBED_CTX_LINK;
      rv = NS_OK;
    }
  }
  else if (uTag.LowerCaseEqualsLiteral("img")) {
    PRInt32 width, height;
    rv = CheckDomImageElement(node, mCtxImgHref, &width, &height);
    if (NS_SUCCEEDED(rv))
      mEmbedCtxType |= GTK_MOZ_EMBED_CTX_IMAGE;
  } else {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

nsresult
EmbedContextMenuInfo::UpdateContextData(void *aEvent)
{
  NS_ENSURE_ARG_POINTER(aEvent);
  return UpdateContextData((nsIDOMEvent *)aEvent);
}

nsresult
EmbedContextMenuInfo::GetElementForScroll(nsIDOMEvent *aEvent)
{
    if (!aEvent) return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIDOMNSEvent> nsevent (do_QueryInterface(aEvent));
    nsCOMPtr<nsIDOMEventTarget> target;
    nsevent->GetOriginalTarget(getter_AddRefs(target));
    if (!target) return NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIDOMNode> targetDOMNode (do_QueryInterface (target));
    if (!targetDOMNode) return NS_ERROR_UNEXPECTED;
    nsCOMPtr <nsIDOMDocument> targetDOMDocument;
    targetDOMNode->GetOwnerDocument (getter_AddRefs (targetDOMDocument));
    if (!targetDOMDocument) return NS_ERROR_UNEXPECTED;
    return GetElementForScroll(targetDOMDocument);
}

nsresult
EmbedContextMenuInfo::GetElementForScroll(nsIDOMDocument *targetDOMDocument)
{
  nsCOMPtr <nsIDOMElement> targetDOMElement;
  targetDOMDocument->GetDocumentElement (getter_AddRefs (targetDOMElement));
  if (!targetDOMElement) return NS_ERROR_UNEXPECTED;
  nsString bodyName (NS_LITERAL_STRING ("body"));
  nsCOMPtr <nsIDOMNodeList> bodyList;
  targetDOMElement->GetElementsByTagName(bodyName, getter_AddRefs (bodyList));
  PRUint32 i = 0;
  bodyList->GetLength (&i);
  if (i) {
    nsCOMPtr<nsIDOMNode> domBodyNode;
    bodyList->Item(0, getter_AddRefs(domBodyNode));
    if (!domBodyNode) return NS_ERROR_UNEXPECTED;
    mNSHHTMLElementSc = do_QueryInterface(domBodyNode);
    if (!mNSHHTMLElementSc) return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult
EmbedContextMenuInfo::UpdateContextData(nsIDOMEvent *aDOMEvent)
{
  if (mCtxEvent == aDOMEvent)
    return NS_OK;
  mCtxEvent = aDOMEvent;
  NS_ENSURE_ARG_POINTER(mCtxEvent);
  nsresult res = nsnull;
  nsresult rv = nsnull;
  nsCOMPtr<nsIDOMEventTarget> originalTarget = nsnull;
  nsCOMPtr<nsIDOMNode> originalNode = nsnull;
  nsCOMPtr<nsIDOMNSEvent> aEvent = do_QueryInterface(mCtxEvent);
  if (!aEvent) {
    return NS_OK;
  }
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent (do_QueryInterface( mCtxEvent ));
  if(mouseEvent) {
    ((nsIDOMMouseEvent*)mouseEvent)->GetClientX(&mX);
    ((nsIDOMMouseEvent*)mouseEvent)->GetClientY(&mY);
  }
  res = aEvent->GetOriginalTarget(getter_AddRefs(originalTarget));
  originalNode = do_QueryInterface(originalTarget);
  if (NS_FAILED(res) || !originalNode) {
    return NS_ERROR_NULL_POINTER;
  }
  //    nsresult SelText = mOwner->ClipBoardAction(GTK_MOZ_EMBED_CAN_COPY);
  if (originalNode == mOrigNode) {
    return NS_OK;
  }
  mEmbedCtxType = GTK_MOZ_EMBED_CTX_NONE;
  mOrigNode = originalNode;
  originalNode->GetNodeName(mSOrigNode);
  if (mSOrigNode.EqualsLiteral ("#document"))
    return NS_OK;
  if (mSOrigNode.EqualsLiteral ("xul:thumb")
      || mSOrigNode.EqualsLiteral ("xul:slider")
      || mSOrigNode.EqualsLiteral ("xul:scrollbarbutton")
      || mSOrigNode.EqualsLiteral ("xul:vbox")
      || mSOrigNode.EqualsLiteral ("xul:spacer")) {
    mEmbedCtxType |= GTK_MOZ_EMBED_CTX_XUL;
    return NS_OK;
  }
  res = mCtxEvent->GetTarget(getter_AddRefs(mEventTarget));
  if (NS_FAILED(res) || !mEventTarget) {
    return NS_OK;
  }
  nsCOMPtr<nsIDOMNode> eventNode = do_QueryInterface(mEventTarget);
  if (eventNode == mEventNode) {
//    return NS_OK;
  }
  mEventNode = eventNode;
  eventNode->GetNodeName(mSEventNode);
  //Frame Stuff
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = mEventNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!NS_SUCCEEDED (rv) || !domDoc) {
    //  return NS_OK;
  }
  if (NS_SUCCEEDED (rv) && domDoc && mCtxDocument != domDoc) {
    mCtxDocument = domDoc;
    mNSHHTMLElementSc = nsnull;    
//    rv = GetElementForScroll(mCtxDocument);
//    if (NS_ERROR_UNEXPECTED == rv) {
//    }
    nsCOMPtr<nsIDOM3Document> docuri = do_QueryInterface(mCtxDocument);
    docuri->GetDocumentURI (mCtxURI);
    NS_ENSURE_ARG_POINTER(mOwner);
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mOwner->mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    webBrowser->GetContentDOMWindow(getter_AddRefs(mCtxDomWindow));
    nsCOMPtr<nsIDOMDocument> mainDocument;
    mCtxDomWindow->GetDocument (getter_AddRefs(mainDocument));
    if (!mainDocument) {
      return NS_OK;
    }
    mCtxFrameNum = -1;  
    if (mainDocument != domDoc) {
      mEmbedCtxType |= GTK_MOZ_EMBED_CTX_IFRAME;
      SetFrameIndex();
    }
  }  
  nsCOMPtr <nsIDOMElement> targetDOMElement;
  mCtxDocument->GetDocumentElement (getter_AddRefs (targetDOMElement));
  if (!targetDOMElement) return NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIDOMNSHTMLDocument> htmlDoc = do_QueryInterface(mCtxDocument);
  if (htmlDoc) {
    nsString DMode;
    htmlDoc->GetDesignMode(DMode);
    if (DMode.EqualsLiteral ("on")) {
      mEmbedCtxType |= GTK_MOZ_EMBED_CTX_INPUT;
      mEmbedCtxType |= GTK_MOZ_EMBED_CTX_RICHEDIT;
    }
  }
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(mCtxDocument);
  if (!doc)
    return NS_OK;
  nsIPresShell *presShell = doc->GetShellAt(0);
  if (!presShell)
    return NS_OK;
  nsCOMPtr<nsIContent> tgContent = do_QueryInterface(mEventTarget);
  nsIFrame* frame = nsnull;
  if (mEmbedCtxType & GTK_MOZ_EMBED_CTX_RICHEDIT)
    frame = presShell->GetRootFrame();
  else {
#ifdef MOZILLA_1_8_BRANCH
    nsIFrame* frame = nsnull;
    presShell->GetPrimaryFrameFor(tgContent, &frame);
#else
    frame = presShell->GetPrimaryFrameFor(tgContent);
#endif
  }
  if (frame) {
    mFormRect = frame->GetScreenRectExternal();
  }  
  if (NS_SUCCEEDED(SetFormControlType(mEventTarget))) {
    return NS_OK;
  }
  CheckDomHtmlNode(mEventNode);
  nsCOMPtr<nsIDOMNode> node = mEventNode;
  nsCOMPtr<nsIDOMNode> parentNode;
  node->GetParentNode (getter_AddRefs(parentNode));
  node = parentNode;
  while (node) {
    if (NS_FAILED(CheckDomHtmlNode(node))) {
      break;
    }
    node->GetParentNode (getter_AddRefs(parentNode));
    node = parentNode;
  }
  mEmbedCtxType |= GTK_MOZ_EMBED_CTX_DOCUMENT;
  return NS_OK;
}
