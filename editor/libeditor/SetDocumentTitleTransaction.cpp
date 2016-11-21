/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SetDocumentTitleTransaction.h"
#include "mozilla/dom/Element.h"        // for Element
#include "nsAString.h"
#include "nsCOMPtr.h"                   // for nsCOMPtr, getter_AddRefs, etc.
#include "nsDebug.h"                    // for NS_ENSURE_SUCCESS, etc.
#include "nsError.h"                    // for NS_OK, NS_ERROR_FAILURE, etc.
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIDOMNode.h"                 // for nsIDOMNode
#include "nsIDOMNodeList.h"             // for nsIDOMNodeList
#include "nsIDOMText.h"                 // for nsIDOMText
#include "nsIDocument.h"                // for nsIDocument
#include "nsIEditor.h"                  // for nsIEditor
#include "nsIHTMLEditor.h"              // for nsIHTMLEditor
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsTextNode.h"                 // for nsTextNode
#include "nsQueryObject.h"              // for do_QueryObject

namespace mozilla {

// Note that aEditor is not refcounted.
SetDocumentTitleTransaction::SetDocumentTitleTransaction()
  : mEditor(nullptr)
  , mIsTransient(false)
{
}

NS_IMETHODIMP
SetDocumentTitleTransaction::Init(nsIHTMLEditor* aEditor,
                                  const nsAString* aValue)

{
  NS_ASSERTION(aEditor && aValue, "null args");
  if (!aEditor || !aValue) {
    return NS_ERROR_NULL_POINTER;
  }

  mEditor = aEditor;
  mValue = *aValue;

  return NS_OK;
}

NS_IMETHODIMP
SetDocumentTitleTransaction::DoTransaction()
{
  return SetDomTitle(mValue);
}

NS_IMETHODIMP
SetDocumentTitleTransaction::UndoTransaction()
{
  // No extra work required; the DOM changes alone are enough
  return NS_OK;
}

NS_IMETHODIMP
SetDocumentTitleTransaction::RedoTransaction()
{
  // No extra work required; the DOM changes alone are enough
  return NS_OK;
}

nsresult
SetDocumentTitleTransaction::SetDomTitle(const nsAString& aTitle)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(mEditor);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  nsresult rv = editor->GetDocument(getter_AddRefs(domDoc));
  if (NS_WARN_IF(!domDoc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNodeList> titleList;
  rv = domDoc->GetElementsByTagName(NS_LITERAL_STRING("title"),
                                    getter_AddRefs(titleList));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // First assume we will NOT really do anything
  // (transaction will not be pushed on stack)
  mIsTransient = true;

  nsCOMPtr<nsIDOMNode> titleNode;
  if(titleList) {
    rv = titleList->Item(0, getter_AddRefs(titleNode));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (titleNode) {
      // Delete existing child textnode of title node
      // (Note: all contents under a TITLE node are always in a single text node)
      nsCOMPtr<nsIDOMNode> child;
      rv = titleNode->GetFirstChild(getter_AddRefs(child));
      if (NS_FAILED(rv)) {
        return rv;
      }

      if(child) {
        // Save current text as the undo value
        nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(child);
        if(textNode) {
          textNode->GetData(mUndoValue);

          // If title text is identical to what already exists,
          // quit now (mIsTransient is now TRUE)
          if (mUndoValue == aTitle) {
            return NS_OK;
          }
        }
        rv = editor->DeleteNode(child);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  // We didn't return above, thus we really will be changing the title
  mIsTransient = false;

  // Get the <HEAD> node, create a <TITLE> and insert it under the HEAD
  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<dom::Element> headElement = document->GetHeadElement();
  if (NS_WARN_IF(!headElement)) {
    return NS_ERROR_UNEXPECTED;
  }

  bool newTitleNode = false;
  uint32_t newTitleIndex = 0;

  if (!titleNode) {
    // Didn't find one above: Create a new one
    nsCOMPtr<nsIDOMElement>titleElement;
    rv = domDoc->CreateElement(NS_LITERAL_STRING("title"),
                               getter_AddRefs(titleElement));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (NS_WARN_IF(!titleElement)) {
      return NS_ERROR_FAILURE;
    }

    titleNode = do_QueryInterface(titleElement);
    newTitleNode = true;

    // Get index so we append new title node after all existing HEAD children.
    newTitleIndex = headElement->GetChildCount();
  }

  // Append a text node under the TITLE only if the title text isn't empty.
  if (titleNode && !aTitle.IsEmpty()) {
    RefPtr<nsTextNode> textNode = document->CreateTextNode(aTitle);

    if (newTitleNode) {
      // Not undoable: We will insert newTitleNode below
      nsCOMPtr<nsINode> title = do_QueryInterface(titleNode);
      MOZ_ASSERT(title);

      ErrorResult result;
      title->AppendChild(*textNode, result);
      if (NS_WARN_IF(result.Failed())) {
        return result.StealNSResult();
      }
    } else {
      // This is an undoable transaction
      nsCOMPtr<nsIDOMNode> newNode = do_QueryObject(textNode);
      if (NS_WARN_IF(!newNode)) {
        return NS_ERROR_FAILURE;
      }

      rv = editor->InsertNode(newNode, titleNode, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    // Calling AppendChild() or InsertNode() could cause removing the head
    // element.  So, let's mark it dirty.
    headElement = nullptr;
  }

  if (newTitleNode) {
    if (!headElement) {
      headElement = document->GetHeadElement();
      if (NS_WARN_IF(!headElement)) {
        // XXX Can we return NS_OK when there is no head element?
        return NS_ERROR_UNEXPECTED;
      }
    }
    // Undoable transaction to insert title+text together
    rv = editor->InsertNode(titleNode, headElement->AsDOMNode(), newTitleIndex);
  }
  return rv;
}

NS_IMETHODIMP
SetDocumentTitleTransaction::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("SetDocumentTitleTransaction: ");
  aString += mValue;
  return NS_OK;
}

NS_IMETHODIMP
SetDocumentTitleTransaction::GetIsTransient(bool* aIsTransient)
{
  if (NS_WARN_IF(!aIsTransient)) {
    return NS_ERROR_NULL_POINTER;
  }
  *aIsTransient = mIsTransient;
  return NS_OK;
}

} // namespace mozilla
