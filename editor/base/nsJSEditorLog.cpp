/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsJSEditorLog.h"
#include "nsCOMPtr.h"

#include "EditAggregateTxn.h"
#include "InsertTextTxn.h"

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsJSEditorLog::nsJSEditorLog(nsIEditor *aEditor)
{
  mRefCnt = 0;
  mEditor = aEditor;
  mDepth  = 0;
  mLocked = -1;
}

nsJSEditorLog::~nsJSEditorLog()
{
}

#define DEBUG_JS_EDITOR_LOG_REFCNT 1

#ifdef DEBUG_JS_EDITOR_LOG_REFCNT

nsrefcnt nsJSEditorLog::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsJSEditorLog::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsJSEditorLog)
NS_IMPL_RELEASE(nsJSEditorLog)

#endif

NS_IMETHODIMP
nsJSEditorLog::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIHTMLEditor::GetIID())) {
    *aInstancePtr = (void*)(nsIHTMLEditor*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsJSEditorLog::Init(nsIDOMDocument *aDoc, nsIPresShell   *aPresShell)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SetTextProperty(nsIAtom *aProperty, const nsString *aAttribute, const nsString *aValue)
{
  if (mLocked)
    return NS_OK;

  nsAutoString propStr;

  aProperty->ToString(propStr);

  PrintSelection();
  printf("appCore.setTextProperty(\"");
  PrintUnicode(propStr);
  printf("\", \"");
  if (aAttribute)
    PrintUnicode(*aAttribute);
  printf("\", \"");
  if (aValue)
    PrintUnicode(*aValue);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::GetTextProperty(nsIAtom *aProperty, const nsString *aAttribute, const nsString *aValue, PRBool &aFirst, PRBool &aAll, PRBool &aAny)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::GetParagraphFormat(nsString& aParagraphFormat)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SetParagraphFormat(const nsString& aParagraphFormat)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.setParagraphFormat(\"");
  PrintUnicode(aParagraphFormat);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::RemoveTextProperty(nsIAtom *aProperty, const nsString *aAttribute)
{
  if (mLocked)
    return NS_OK;

  nsAutoString propStr;

  aProperty->ToString(propStr);

  PrintSelection();
  printf("appCore.removeTextProperty(\"");
  PrintUnicode(propStr);
  printf("\", \"");
  if (aAttribute)
    PrintUnicode(*aAttribute);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::DeleteSelection(nsIEditor::ECollapsedSelectionAction aAction)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.deleteSelection(%d);\n", aAction);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertText(const nsString& aStringToInsert)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  printf("appCore.insertText(\"");
  PrintUnicode(aStringToInsert);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertBreak()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.insertBreak();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::EnableUndo(PRBool aEnable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::Undo(PRUint32 aCount)
{
  if (mLocked)
    return NS_OK;

  printf("appCore.undo();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::Redo(PRUint32 aCount)
{
  if (mLocked)
    return NS_OK;

  printf("appCore.redo();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::BeginTransaction()
{
  if (mLocked)
    return NS_OK;

  printf("appCore.beginBatchChanges();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::EndTransaction()
{
  if (mLocked)
    return NS_OK;

  printf("appCore.endBatchChanges();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SelectAll()
{
  if (mLocked)
    return NS_OK;

  printf("appCore.selectAll();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::BeginningOfDocument()
{
  if (mLocked)
    return NS_OK;

  printf("appCore.BeginningOfDocument();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::EndOfDocument()
{
  if (mLocked)
    return NS_OK;

  printf("appCore.EndOfDocument();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::ScrollUp(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::ScrollDown(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::ScrollIntoView(PRBool aScrollToBegin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsJSEditorLog::Save()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SaveAs(PRBool aSavingCopy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsJSEditorLog::Cut()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.cut();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Copy()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.copy();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Paste()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.paste();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::PasteAsQuotation()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.pasteAsQuotation();\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::PasteAsCitedQuotation(const nsString& aCitation)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.pasteAsCitedQuotation(\"");
  PrintUnicode(aCitation);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertAsQuotation(const nsString& aQuotedText)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.insertAsQuotation(\"");
  PrintUnicode(aQuotedText);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertAsCitedQuotation(const nsString& aQuotedText, const nsString& aCitation)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  printf("appCore.insertAsCitedQuotation(\"");
  PrintUnicode(aQuotedText);
  printf("\", \"");
  PrintUnicode(aCitation);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertHTML(const nsString &aInputString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::OutputTextToString(nsString& aOutputString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::OutputHTMLToString(nsString& aOutputString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::OutputTextToStream(nsIOutputStream* aOutputStream, nsString* aCharsetOverride)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::OutputHTMLToStream(nsIOutputStream* aOutputStream, nsString* aCharsetOverride)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::GetLocalFileURL(nsIDOMWindow* aParent, const nsString& aFilterType, nsString& aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SetBackgroundColor(const nsString& aColor)
{
  if (mLocked)
    return NS_OK;

  printf("appCore.setBackgroundColor(\"");
  PrintUnicode(aColor);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::SetBodyAttribute(const nsString& aAttr, const nsString& aValue)
{
  if (mLocked)
    return NS_OK;

  printf("appCore.setBodyAttribute(\"");
  PrintUnicode(aAttr);
  printf("\", \"");
  PrintUnicode(aValue);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::GetParagraphStyle(nsStringArray *aTagList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::AddBlockParent(nsString& aParentTag)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::ReplaceBlockParent(nsString& aParentTag)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::RemoveParagraphStyle()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::RemoveParent(const nsString &aParentTag)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::InsertLink(nsString& aURL)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  printf("appCore.insertLink(\"");
  PrintUnicode(aURL);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertImage(nsString& aURL, nsString& aWidth, nsString& aHeight,
                           nsString& aHspace, nsString& aVspace, nsString& aBorder,
                           nsString& aAlt, nsString& aAlignment)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  printf("appCore.insertImage(\"");
  PrintUnicode(aURL);
  printf("\", \"");
  PrintUnicode(aWidth);
  printf("\", \"");
  PrintUnicode(aHeight);
  printf("\", \"");
  PrintUnicode(aHspace);
  printf("\", \"");
  PrintUnicode(aVspace);
  printf("\", \"");
  PrintUnicode(aBorder);
  printf("\", \"");
  PrintUnicode(aAlt);
  printf("\", \"");
  PrintUnicode(aAlignment);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertList(const nsString& aListType)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  printf("appCore.insertList(\"");
  PrintUnicode(aListType);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Indent(const nsString& aIndent)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  printf("appCore.indent(\"");
  PrintUnicode(aIndent);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Align(const nsString& aAlign)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  printf("appCore.align(\"");
  PrintUnicode(aAlign);
  printf("\");\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection)
{
  if (!aElement)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);

  if (!node)
    return NS_ERROR_NULL_POINTER;

  PrintSelection();
  PrintNode(node, 0);
  printf("appCore.insertElement(n0, %s);\n", aDeleteSelection ? "true" : "false");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  if (!aAnchorElement)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aAnchorElement);

  if (!node)
    return NS_ERROR_NULL_POINTER;

  PrintSelection();
  PrintNode(node, 0);
  printf("appCore.insertLinkAroundSelection(n0);\n");

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::SelectElement(nsIDOMElement* aElement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SetCaretAfterElement(nsIDOMElement* aElement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::InsertTable()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::InsertTableCell(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::InsertTableColumn(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::InsertTableRow(PRInt32 aNumber, PRBool aAfter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::DeleteTable()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::DeleteTableCell(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::DeleteTableColumn(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::DeleteTableRow(PRInt32 aNumber)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::JoinTableCells(PRBool aCellToRight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::BeginComposition(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SetCompositionString(const nsString& aCompositionString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::EndComposition(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

const char *
nsJSEditorLog::GetString(nsITransaction *aTransaction)
{
  static char buf[256];

  nsString str = "";

  aTransaction->GetRedoString(&str);

  if (str.Length() == 0)
  {
    EditAggregateTxn *at = NS_STATIC_CAST(EditAggregateTxn*, aTransaction);

    if (at)
    {
      nsIAtom *atom = 0;

      at->GetName(&atom);

      if (atom)
        atom->ToString(str);
    }

    if (str.Length() == 0)
      str = "<NULL>";
  }

  buf[0] = '\0';
  str.ToCString(buf, 256);

  return buf;
}

nsresult
nsJSEditorLog::PrintUnicode(const nsString &aString)
{
  const PRUnichar *uc = aString.GetUnicode();
  PRInt32 i, len = aString.Length();

  for (i = 0; i < len; i++)
  {
    if (nsString::IsAlpha(uc[i]) || nsString::IsDigit(uc[i]) || uc[i] == ' ')
      printf("%c", uc[i]);
    else
      printf("\\u%.4x", uc[i]);
  }

  return NS_OK;
}

nsresult
nsJSEditorLog::PrintIndent(PRInt32 aIndentLevel)
{
  PRInt32 i;

  for (i = 0; i < aIndentLevel; i++)
    printf("  ");

  return NS_OK;
}

nsresult
nsJSEditorLog::PrintSelection()
{
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult result;
  PRInt32 rangeCount;

  result = mEditor->GetPresShell(getter_AddRefs(presShell));

  if (NS_FAILED(result))
    return result;

  result = presShell->GetSelection(getter_AddRefs(selection));

  if (NS_FAILED(result))
    return result;

  result = selection->GetRangeCount(&rangeCount);

  if (NS_FAILED(result))
    return result;

  printf("selRanges = [ ");

  PRInt32 i, j;
  nsCOMPtr<nsIDOMRange> range;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;

  for (i = 0; i < rangeCount; i++)
  {
    result = selection->GetRangeAt(i, getter_AddRefs(range));

    if (NS_FAILED(result))
      return result;
    
    result = range->GetStartParent(getter_AddRefs(startNode));

    if (NS_FAILED(result))
      return result;

    if (!startNode)
      return NS_ERROR_NULL_POINTER;

    result = range->GetStartOffset(&startOffset);

    if (NS_FAILED(result))
      return result;

    result = range->GetEndParent(getter_AddRefs(endNode));

    if (NS_FAILED(result))
      return result;

    if (!endNode)
      return NS_ERROR_NULL_POINTER;

    result = range->GetEndOffset(&endOffset);

    if (NS_FAILED(result))
      return result;

    PRInt32 *offsetArray = 0;
    PRInt32 arrayLength = 0;

    result = GetNodeTreeOffsets(startNode, &offsetArray, &arrayLength);

    if (NS_FAILED(result))
      return result;

    if (i != 0)
      printf(",\n              ");

    printf("[ [[");

    for (j = 0; j < arrayLength; j++)
    {
      if (j != 0)
        printf(", ");
      printf("%d", offsetArray[j]);
    }

    printf("], %3d], ", startOffset);

    if (startNode != endNode)
    {
      delete []offsetArray;
      offsetArray = 0;
      arrayLength = 0;

      result = GetNodeTreeOffsets(endNode, &offsetArray, &arrayLength);

      if (NS_FAILED(result))
        return result;
    }

    printf("[[");

    for (j = 0; j < arrayLength; j++)
    {
      if (j != 0)
        printf(", ");
      printf("%d", offsetArray[j]);
    }

    delete []offsetArray;

    printf("], %3d] ]", endOffset);
  }

  printf(" ];\nEditorSetSelectionFromOffsets(selRanges);\n");
  return NS_OK;
}

nsresult
nsJSEditorLog::PrintElementNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;
  nsAutoString tag, name, value;
  nsCOMPtr<nsIDOMElement> ele = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMNamedNodeMap> map;

  if (!ele)
    return NS_ERROR_NULL_POINTER;

  result = ele->GetTagName(tag);

  if (NS_FAILED(result))
    return result;

  printf("n%d = appCore.editorDocument.createElement(\"", aDepth);
  PrintUnicode(tag);
  printf("\");\n");

  result = aNode->GetAttributes(getter_AddRefs(map));

  if (NS_FAILED(result))
    return result;

  if (!map)
    return NS_ERROR_NULL_POINTER;

  PRUint32 i, len;
  nsCOMPtr<nsIDOMNode> attr;

  result = map->GetLength(&len);

  if (NS_FAILED(result))
    return result;

  for (i = 0; i < len; i++)
  {
    result = map->Item(i, getter_AddRefs(attr));

    if (NS_FAILED(result))
      return result;

    if (!attr)
      return NS_ERROR_NULL_POINTER;

    result = PrintAttributeNode(attr, aDepth);

    if (NS_FAILED(result))
      result;
  }

  result = PrintNodeChildren(aNode, aDepth);

  return result;
}

nsresult
nsJSEditorLog::PrintAttributeNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;
  nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(aNode);

  if (!attr)
    return NS_ERROR_NULL_POINTER;

  nsAutoString str;

  result = attr->GetName(str);

  if (NS_FAILED(result))
    return result;

  printf("a%d = appCore.editorDocument.createAttribute(\"", aDepth);
  PrintUnicode(str);
  printf("\");\n");

  result = attr->GetValue(str);

  if (NS_FAILED(result))
    return result;

  printf("a%d.value = \"", aDepth);
  PrintUnicode(str);
  printf("\";\n");
  
  printf("n%d.setAttributeNode(a%d);\n", aDepth, aDepth);

  return NS_OK;
}

nsresult
nsJSEditorLog::PrintNodeChildren(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;

  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNodeList> list;

  result = aNode->GetChildNodes(getter_AddRefs(list));
  
  if (NS_FAILED(result))
    return result;

  if (!list)
  {
    // Must not have any children!
    return NS_OK;
  }

  PRUint32 i, len;
  nsCOMPtr<nsIDOMNode> node;

  result = list->GetLength(&len);

  if (NS_FAILED(result))
    return result;

  for (i = 0; i < len; i++)
  {
    result = list->Item(i, getter_AddRefs(node));

    if (NS_FAILED(result))
      return result;

    result = PrintNode(node, aDepth + 1);

    if (NS_FAILED(result))
      return result;

    printf("n%d.appendChild(n%d);\n", aDepth, aDepth+1);
  }

  return NS_OK;
}

nsresult
nsJSEditorLog::PrintTextNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;

  nsCOMPtr<nsIDOMCharacterData> cd = do_QueryInterface(aNode);

  if (!cd)
    return NS_ERROR_NULL_POINTER;

  nsAutoString str;

  result = cd->GetData(str);

  if (NS_FAILED(result))
    return result;

  printf("n%d = appCore.editorDocument.createTextNode(\"", aDepth);
  PrintUnicode(str);
  printf("\");\n");

  return NS_OK;
}

nsresult
nsJSEditorLog::PrintNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result = NS_OK;

  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  PRUint16 nodeType;
  
  result = aNode->GetNodeType(&nodeType);

  switch (nodeType)
  {
    case nsIDOMNode::ELEMENT_NODE:
      result = PrintElementNode(aNode, aDepth);
      break;
    case nsIDOMNode::TEXT_NODE:
      result = PrintTextNode(aNode, aDepth);
      break;
    case nsIDOMNode::ATTRIBUTE_NODE:
    case nsIDOMNode::CDATA_SECTION_NODE:
    case nsIDOMNode::ENTITY_REFERENCE_NODE:
    case nsIDOMNode::ENTITY_NODE:
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    case nsIDOMNode::COMMENT_NODE:
    case nsIDOMNode::DOCUMENT_NODE:
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    case nsIDOMNode::NOTATION_NODE:
    default:
      break;
  }

  return result;
}

nsresult
nsJSEditorLog::GetNodeTreeOffsets(nsIDOMNode *aNode, PRInt32 **aResult, PRInt32 *aLength)
{
  nsresult result;

  if (!aNode || !aResult || !aLength)
    return NS_ERROR_NULL_POINTER;

  *aResult = 0;
  *aLength = 0;

  nsIDOMNode *parent = aNode;
  PRInt32 i = 0;

  // Count the number of parent nodes above aNode.

  while (parent)
  {
    result = parent->GetParentNode(&parent);

    if (NS_FAILED(result))
      return result;

    if (parent)
      ++i;
  }

  // Allocate an array big enough to hold all the offsets.

  *aResult = new PRInt32[i];

  if (!aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  *aLength = i;

  while (aNode && i > 0)
  {
    PRInt32 offset = 0;

    result = aNode->GetParentNode(&parent);

    if (NS_FAILED(result))
    {
      delete [](*aResult);
      *aResult = 0;
      *aLength = 0;
      return result;
    }

    while (aNode)
    {
      result = aNode->GetPreviousSibling(&aNode);

      if (NS_FAILED(result))
      {
        delete [](*aResult);
        *aResult = 0;
        *aLength = 0;
        return result;
      }

      if (aNode)
        ++offset;
    }

    (*aResult)[--i] = offset;
    aNode = parent;
  }

  return NS_OK;
}

nsresult
nsJSEditorLog::Lock()
{
  mLocked++;
  return NS_OK;
}

nsresult
nsJSEditorLog::Unlock()
{
  --mLocked;
  return NS_OK;
}

#ifdef NEVER_ENABLE_THIS_JAVASCRIPT

function EditorGetNodeAtOffsets(offsets)
{
  var node = null;
  var i;

  node = appCore.editorDocument;

  for (i = 0; i < offsets.length; i++)
  {
    node = node.childNodes[offsets[i]];
  }

  return node;
}

function EditorSetSelectionFromOffsets(selRanges)
{
  var rangeArr, start, end, i, node, offset;
  var selection = appCore.editorSelection;

  selection.clearSelection();

  for (i = 0; i < selRanges.length; i++)
  {
    rangeArr = selRanges[i];
    start    = rangeArr[0];
    end      = rangeArr[1];

    var range = appCore.editorDocument.createRange();

    node   = EditorGetNodeAtOffsets(start[0]);
    offset = start[1];

    range.setStart(node, offset);

    node   = EditorGetNodeAtOffsets(end[0]);
    offset = end[1];

    range.setEnd(node, offset);

    selection.addRange(range);
  }
}

#endif
