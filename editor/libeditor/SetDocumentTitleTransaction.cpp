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
  if (!aEditor || !aValue) { return NS_ERROR_NULL_POINTER; }

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
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsresult res = editor->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> titleList;
  res = domDoc->GetElementsByTagName(NS_LITERAL_STRING("title"), getter_AddRefs(titleList));
  NS_ENSURE_SUCCESS(res, res);

  // First assume we will NOT really do anything
  // (transaction will not be pushed on stack)
  mIsTransient = true;

  nsCOMPtr<nsIDOMNode>titleNode;
  if(titleList)
  {
    res = titleList->Item(0, getter_AddRefs(titleNode));
    NS_ENSURE_SUCCESS(res, res);
    if (titleNode)
    {
      // Delete existing child textnode of title node
      // (Note: all contents under a TITLE node are always in a single text node)
      nsCOMPtr<nsIDOMNode> child;
      res = titleNode->GetFirstChild(getter_AddRefs(child));
      if(NS_FAILED(res)) return res;
      if(child)
      {
        // Save current text as the undo value
        nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(child);
        if(textNode)
        {
          textNode->GetData(mUndoValue);

          // If title text is identical to what already exists,
          // quit now (mIsTransient is now TRUE)
          if (mUndoValue == aTitle)
            return NS_OK;
        }
        res = editor->DeleteNode(child);
        if(NS_FAILED(res)) return res;
      }
    }
  }

  // We didn't return above, thus we really will be changing the title
  mIsTransient = false;

  // Get the <HEAD> node, create a <TITLE> and insert it under the HEAD
  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(document);

  dom::Element* head = document->GetHeadElement();
  NS_ENSURE_STATE(head);

  bool     newTitleNode = false;
  uint32_t newTitleIndex = 0;

  if (!titleNode)
  {
    // Didn't find one above: Create a new one
    nsCOMPtr<nsIDOMElement>titleElement;
    res = domDoc->CreateElement(NS_LITERAL_STRING("title"), getter_AddRefs(titleElement));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(titleElement, NS_ERROR_FAILURE);

    titleNode = do_QueryInterface(titleElement);
    newTitleNode = true;

    // Get index so we append new title node after all existing HEAD children.
    newTitleIndex = head->GetChildCount();
  }

  // Append a text node under the TITLE
  //  only if the title text isn't empty
  if (titleNode && !aTitle.IsEmpty())
  {
    nsCOMPtr<nsIDOMText> textNode;
    res = domDoc->CreateTextNode(aTitle, getter_AddRefs(textNode));
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(textNode);
    NS_ENSURE_TRUE(newNode, NS_ERROR_FAILURE);

    if (newTitleNode)
    {
      // Not undoable: We will insert newTitleNode below
      nsCOMPtr<nsIDOMNode> resultNode;
      res = titleNode->AppendChild(newNode, getter_AddRefs(resultNode));
    }
    else
    {
      // This is an undoable transaction
      res = editor->InsertNode(newNode, titleNode, 0);
    }
    NS_ENSURE_SUCCESS(res, res);
  }

  if (newTitleNode)
  {
    // Undoable transaction to insert title+text together
    res = editor->InsertNode(titleNode, head->AsDOMNode(), newTitleIndex);
  }
  return res;
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
  NS_ENSURE_TRUE(aIsTransient, NS_ERROR_NULL_POINTER);
  *aIsTransient = mIsTransient;
  return NS_OK;
}

} // namespace mozilla
