/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include <stdio.h>
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsHTMLEditorLog.h"
#include "nsCOMPtr.h"

#include "nsEditorTxnLog.h"

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)

nsHTMLEditorLog::nsHTMLEditorLog()
{
  mLocked   = -1;
  mEditorTxnLog = 0;
  
  NS_INIT_ISUPPORTS();
}

nsHTMLEditorLog::~nsHTMLEditorLog()
{
  StopLogging();
}

NS_IMPL_ISUPPORTS_INHERITED(nsHTMLEditorLog, nsHTMLEditor, nsIEditorLogging);

NS_IMETHODIMP
nsHTMLEditorLog::SetInlineProperty(nsIAtom *aProperty, const nsAReadableString &aAttribute, const nsAReadableString &aValue)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    nsAutoString propStr;

    aProperty->ToString(propStr);

    PrintSelection();
    Write("window.editorShell.SetTextProperty(\"");
    PrintUnicode(propStr);
    Write("\", \"");
    if (aAttribute.Length())
      PrintUnicode(aAttribute);
    Write("\", \"");
    if (aValue.Length())
      PrintUnicode(aValue);
    Write("\");\n");

    Flush();
  }

  return nsHTMLEditor::SetInlineProperty(aProperty, aAttribute, aValue);
}

NS_IMETHODIMP
nsHTMLEditorLog::SetParagraphFormat(const nsAString& aParagraphFormat)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.SetParagraphFormat(\"");
    PrintUnicode(aParagraphFormat);
    Write("\");\n");

    Flush();
  }

  return nsHTMLEditor::SetParagraphFormat(aParagraphFormat);
}

NS_IMETHODIMP
nsHTMLEditorLog::RemoveInlineProperty(nsIAtom *aProperty, const nsAReadableString &aAttribute)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    nsAutoString propStr;

    aProperty->ToString(propStr);

    PrintSelection();
    Write("window.editorShell.RemoveTextProperty(\"");
    PrintUnicode(propStr);
    Write("\", \"");
    if (aAttribute.Length())
      PrintUnicode(aAttribute);
    Write("\");\n");

    Flush();
  }

  return nsHTMLEditor::RemoveInlineProperty(aProperty, aAttribute);
}

NS_IMETHODIMP
nsHTMLEditorLog::DeleteSelection(nsIEditor::EDirection aAction)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.DeleteSelection(");
    WriteInt("%d", aAction);
    Write(");\n");

    Flush();
  }

  return nsHTMLEditor::DeleteSelection(aAction);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertText(const nsAReadableString& aStringToInsert)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("window.editorShell.InsertText(\"");
    nsAutoString str(aStringToInsert);
    PrintUnicode(str);
    Write("\");\n");

    Flush();
  }

  return nsHTMLEditor::InsertText(aStringToInsert);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertLineBreak()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.InsertBreak();\n");
    Flush();
  }

  return nsHTMLEditor::InsertLineBreak();
}

NS_IMETHODIMP
nsHTMLEditorLog::Undo(PRUint32 aCount)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.Undo();\n");
    Flush();
  }

  return nsHTMLEditor::Undo(aCount);
}

NS_IMETHODIMP
nsHTMLEditorLog::Redo(PRUint32 aCount)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.Redo();\n");
    Flush();
  }

  return nsHTMLEditor::Redo(aCount);
}

NS_IMETHODIMP
nsHTMLEditorLog::BeginTransaction()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.BeginBatchChanges();\n");
    Flush();
  }

  return nsHTMLEditor::BeginTransaction();
}

NS_IMETHODIMP
nsHTMLEditorLog::EndTransaction()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.EndBatchChanges();\n");
    Flush();
  }

  return nsHTMLEditor::EndTransaction();
}

NS_IMETHODIMP
nsHTMLEditorLog::SelectAll()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.SelectAll();\n");
    Flush();
  }

  return nsHTMLEditor::SelectAll();
}

NS_IMETHODIMP
nsHTMLEditorLog::BeginningOfDocument()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.BeginningOfDocument();\n");
    Flush();
  }

  return nsHTMLEditor::BeginningOfDocument();
}

NS_IMETHODIMP
nsHTMLEditorLog::EndOfDocument()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.EndOfDocument();\n");
    Flush();
  }

  return nsHTMLEditor::EndOfDocument();
}

NS_IMETHODIMP
nsHTMLEditorLog::Cut()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.Cut();\n");
    Flush();
  }

  return nsHTMLEditor::Cut();
}

NS_IMETHODIMP
nsHTMLEditorLog::Copy()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.Copy();\n");
    Flush();
  }

  return nsHTMLEditor::Copy();
}

NS_IMETHODIMP
nsHTMLEditorLog::Paste(PRInt32 aSelectionType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.Paste();\n");
    Flush();
  }

  return nsHTMLEditor::Paste(aSelectionType);
}

NS_IMETHODIMP
nsHTMLEditorLog::PasteAsQuotation(PRInt32 aSelectionType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.PasteAsQuotation();\n");
    Flush();
  }

  return nsHTMLEditor::PasteAsQuotation(aSelectionType);
}

NS_IMETHODIMP
nsHTMLEditorLog::PasteAsPlaintextQuotation(PRInt32 aSelectionType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.PasteAsQuotation();\n");
    Flush();
  }

  return nsHTMLEditor::PasteAsPlaintextQuotation(aSelectionType);
}

NS_IMETHODIMP
nsHTMLEditorLog::PasteAsCitedQuotation(const nsAReadableString& aCitation,
                                       PRInt32 aSelectionType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.PasteAsCitedQuotation(\"");
    PrintUnicode(aCitation);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::PasteAsCitedQuotation(aCitation, aSelectionType);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertAsQuotation(const nsAReadableString& aQuotedText,
                                   nsIDOMNode **aNodeInserted)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.InsertAsQuotation(\"");
    PrintUnicode(aQuotedText);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertAsQuotation(aQuotedText, aNodeInserted);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertAsPlaintextQuotation(const nsAReadableString& aQuotedText,
                                            nsIDOMNode **aNodeInserted)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.InsertAsQuotation(\"");
    PrintUnicode(aQuotedText);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertAsPlaintextQuotation(aQuotedText, aNodeInserted);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertAsCitedQuotation(const nsAReadableString& aQuotedText,
                                        const nsAReadableString& aCitation,
                                        PRBool aInsertHTML,
                                        const nsAReadableString& aCharset,
                                        nsIDOMNode **aNodeInserted)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("window.editorShell.InsertAsCitedQuotation(\"");
    PrintUnicode(aQuotedText);
    Write("\", \"");
    PrintUnicode(aCitation);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertAsCitedQuotation(aQuotedText, aCitation, aInsertHTML,
                                              aCharset, aNodeInserted);
}

NS_IMETHODIMP
nsHTMLEditorLog::SetBackgroundColor(const nsAReadableString& aColor)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.SetBackgroundColor(\"");
    PrintUnicode(aColor);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::SetBackgroundColor(aColor);
}

NS_IMETHODIMP
nsHTMLEditorLog::SetBodyAttribute(const nsAReadableString& aAttr, const nsAReadableString& aValue)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.SetBodyAttribute(\"");
    PrintUnicode(aAttr);
    Write("\", \"");
    PrintUnicode(aValue);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::SetBodyAttribute(aAttr, aValue);
}

NS_IMETHODIMP
nsHTMLEditorLog:: InsertTableCell(PRInt32 aNumber, PRBool aAfter)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.InsertTableCell(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertTableCell(aNumber, aAfter);
}


NS_IMETHODIMP
nsHTMLEditorLog:: InsertTableColumn(PRInt32 aNumber, PRBool aAfter)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.InsertTableColumn(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertTableColumn(aNumber, aAfter);
}


NS_IMETHODIMP
nsHTMLEditorLog:: InsertTableRow(PRInt32 aNumber, PRBool aAfter)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.InsertTableRow(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertTableRow(aNumber, aAfter);
}

NS_IMETHODIMP
nsHTMLEditorLog:: DeleteTable()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.DeleteTable();\n");
    Flush();
  }

  return nsHTMLEditor::DeleteTable();
}

NS_IMETHODIMP
nsHTMLEditorLog:: DeleteTableCell(PRInt32 aNumber)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.DeleteTableCell(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::DeleteTableCell(aNumber);
}

NS_IMETHODIMP
nsHTMLEditorLog:: DeleteTableCellContents()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.DeleteTableCellContents();\n");
    Flush();
  }

  return nsHTMLEditor::DeleteTableCellContents();
}


NS_IMETHODIMP
nsHTMLEditorLog:: DeleteTableColumn(PRInt32 aNumber)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.DeleteTableColumn(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::DeleteTableColumn(aNumber);
}


NS_IMETHODIMP
nsHTMLEditorLog:: DeleteTableRow(PRInt32 aNumber)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.DeleteTableRow(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::DeleteTableRow(aNumber);
}

NS_IMETHODIMP
nsHTMLEditorLog:: JoinTableCells(PRBool aMergeNonContiguousContents)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.JoinTableCells();\n");
    Flush();
  }

  return nsHTMLEditor::JoinTableCells(aMergeNonContiguousContents);
}

NS_IMETHODIMP
nsHTMLEditorLog:: SplitTableCell()
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.SplitTableCell();\n");
    Flush();
  }

  return nsHTMLEditor::SplitTableCell();
}


NS_IMETHODIMP
nsHTMLEditorLog:: NormalizeTable(nsIDOMElement *aTable)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.NormalizeTable(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::NormalizeTable(aTable);
}

NS_IMETHODIMP
nsHTMLEditorLog::SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("window.editorShell.SwitchTableCellHeaderType(\"");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::SwitchTableCellHeaderType(aSourceCell, aNewCell);
}

NS_IMETHODIMP
nsHTMLEditorLog::MakeOrChangeList(const nsAReadableString& aListType, PRBool entireList)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("window.editorShell.MakeOrChangeList(\"");
    PrintUnicode(aListType);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::MakeOrChangeList(aListType, entireList);
}

NS_IMETHODIMP
nsHTMLEditorLog::Indent(const nsAReadableString& aIndent)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("window.editorShell.Indent(\"");
    PrintUnicode(aIndent);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::Indent(aIndent);
}

NS_IMETHODIMP
nsHTMLEditorLog::Align(const nsAReadableString& aAlign)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("window.editorShell.Align(\"");
    PrintUnicode(aAlign);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::Align(aAlign);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertElementAtSelection(nsIDOMElement* aElement, PRBool aDeleteSelection)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    if (!aElement)
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);

    if (!node)
      return NS_ERROR_NULL_POINTER;

    PrintSelection();
    PrintNode(node, 0);
    Write("window.editorShell.InsertElementAtSelection(n0, ");
    Write(aDeleteSelection ? "true" : "false");
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::InsertElementAtSelection(aElement, aDeleteSelection);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    if (!aAnchorElement)
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aAnchorElement);

    if (!node)
      return NS_ERROR_NULL_POINTER;

    PrintSelection();
    PrintNode(node, 0);
    Write("window.editorShell.InsertLinkAroundSelection(n0);\n");
    Flush();
  }

  return nsHTMLEditor::InsertLinkAroundSelection(aAnchorElement);
}

NS_IMETHODIMP
nsHTMLEditorLog::ApplyStyleSheet(const nsAReadableString& aURL, nsICSSStyleSheet **aStyleSheet)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    // Note that the editorShell (IDL) method does
    //  not return the style sheet created from aURL
    // TODO: Investigate if RemoveStyleSheet works or do we have to 
    //       store the returned style sheet!
    Write("window.editorShell.ApplyStyleSheet(\"");
    PrintUnicode(aURL);
    Write("\");\n");

    Flush();
  }

  return nsHTMLEditor::ApplyStyleSheet(aURL, aStyleSheet);
}

NS_IMETHODIMP
nsHTMLEditorLog::SetDocumentTitle(const nsAReadableString& aTitle)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("window.editorShell.SetDocumentTitle(\"");
    nsAutoString str(aTitle);
    PrintUnicode(str);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::SetDocumentTitle(aTitle);
}

NS_IMETHODIMP
nsHTMLEditorLog::StartLogging(nsIFile *aLogFile)
{
  nsresult result = NS_ERROR_FAILURE;

  if (!aLogFile)
    return NS_ERROR_NULL_POINTER;

  if (mFileStream)
  {
    result = StopLogging();

    if (NS_FAILED(result))
      return result;
  }

  mFileStream = do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &result);
  if (NS_FAILED(result)) return result;

  result = mFileStream->Init(aLogFile, -1, -1);
  if (NS_FAILED(result))
  {
    mFileStream = nsnull;
    return result;
  }

  if (mTxnMgr)
  {
    mEditorTxnLog = new nsEditorTxnLog(this);

    if (mEditorTxnLog)
    {
      NS_ADDREF(mEditorTxnLog);
      mTxnMgr->AddListener(mEditorTxnLog);
    }
    else
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditorLog::StopLogging()
{
  if (mTxnMgr && mEditorTxnLog)
    mTxnMgr->RemoveListener(mEditorTxnLog);

  if (mEditorTxnLog)
  {
    NS_RELEASE(mEditorTxnLog);
    mEditorTxnLog = 0;
  }

  if (mFileStream)
  {
    mFileStream->Close();
    mFileStream = nsnull;
  }

  return NS_OK;
}


nsresult
nsHTMLEditorLog::Write(const char *aBuffer)
{
  nsresult result;

  if (!aBuffer)
    return NS_ERROR_NULL_POINTER;

  PRInt32 len = strlen(aBuffer);

  if (mFileStream)
  {
    PRUint32 retval;

    result = mFileStream->Write(aBuffer, len, &retval);

    if (NS_FAILED(result))
      return result;

#ifdef VERY_SLOW

    result = mFileStream->Flush();

    if (NS_FAILED(result))
      return result;

#endif // VERY_SLOW
  }
  else
    fwrite(aBuffer, 1, len, stdout);

  return NS_OK;
}

nsresult
nsHTMLEditorLog::WriteInt(const char *aFormat, PRInt32 aInt)
{
  if (!aFormat)
    return NS_ERROR_NULL_POINTER;

  char buf[256];

  sprintf(buf, aFormat, aInt);

  return Write(buf);
}

nsresult
nsHTMLEditorLog::Flush()
{
  nsresult result = NS_OK;

  if (mFileStream)
    result = mFileStream->Flush();
  else
    fflush(stdout);

  return result;
}

nsresult
nsHTMLEditorLog::PrintUnicode(const nsAReadableString &aString)
{
  //const PRUnichar *uc = aString.get();
  char buf[10];
  nsReadingIterator <PRUnichar> beginIter,endIter;
  aString.BeginReading(beginIter);
  aString.EndReading(endIter);
  while(beginIter != endIter)
  {
    if (nsCRT::IsAsciiAlpha(*beginIter) || nsCRT::IsAsciiDigit(*beginIter) || *beginIter == ' ')
      sprintf(buf, "%c", *beginIter);
    else
      sprintf(buf, "\\u%.4x", *beginIter);

    nsresult result = Write(buf);

    if (NS_FAILED(result))
      return result;
    ++beginIter;
  }

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintSelection()
{
  nsCOMPtr<nsISelection> selection;
  nsresult result;
  PRInt32 rangeCount;

  result = GetSelection(getter_AddRefs(selection));

  if (NS_FAILED(result))
    return result;

  result = selection->GetRangeCount(&rangeCount);

  if (NS_FAILED(result))
    return result;

  Write("selRanges = [ ");

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
    
    result = range->GetStartContainer(getter_AddRefs(startNode));

    if (NS_FAILED(result))
      return result;

    if (!startNode)
      return NS_ERROR_NULL_POINTER;

    result = range->GetStartOffset(&startOffset);

    if (NS_FAILED(result))
      return result;

    result = range->GetEndContainer(getter_AddRefs(endNode));

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
      Write(",\n              ");

    Write("[ [[");

    for (j = 0; j < arrayLength; j++)
    {
      if (j != 0)
        Write(", ");
      WriteInt("%d", offsetArray[j]);
    }

    Write("], ");
    WriteInt("%3d", startOffset);
    Write("], ");

    if (startNode != endNode)
    {
      delete []offsetArray;
      offsetArray = 0;
      arrayLength = 0;

      result = GetNodeTreeOffsets(endNode, &offsetArray, &arrayLength);

      if (NS_FAILED(result))
        return result;
    }

    Write("[[");

    for (j = 0; j < arrayLength; j++)
    {
      if (j != 0)
        Write(", ");
      WriteInt("%d", offsetArray[j]);
    }

    delete []offsetArray;

    Write("], ");
    WriteInt("%3d", endOffset);
    Write("] ]");
  }

  Write(" ];\nEditorSetSelectionFromOffsets(selRanges);\n");

  Flush();

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintElementNode(nsIDOMNode *aNode, PRInt32 aDepth)
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

  Write("n");
  WriteInt("%d", aDepth);
  Write(" = window.editorShell.editorDocument.createElement(\"");
  PrintUnicode(tag);
  Write("\");\n");

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
      return result;
  }

  result = PrintNodeChildren(aNode, aDepth);

  return result;
}

nsresult
nsHTMLEditorLog::PrintAttributeNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;
  nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(aNode);

  if (!attr)
    return NS_ERROR_NULL_POINTER;

  nsAutoString str;

  result = attr->GetName(str);

  if (NS_FAILED(result))
    return result;

  Write("a");
  WriteInt("%d", aDepth);
  Write(" = window.editorShell.editorDocument.createAttribute(\"");
  PrintUnicode(str);
  Write("\");\n");

  result = attr->GetValue(str);

  if (NS_FAILED(result))
    return result;

  Write("a");
  WriteInt("%d", aDepth);
  Write(".value = \"");
  PrintUnicode(str);
  Write("\";\n");
  
  Write("n");
  WriteInt("%d", aDepth);
  Write(".setAttributeNode(a");
  WriteInt("%d", aDepth);
  Write(");\n");

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintNodeChildren(nsIDOMNode *aNode, PRInt32 aDepth)
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

    Write("n");
    WriteInt("%d", aDepth);
    Write(".appendChild(n");
    WriteInt("%d", aDepth+1);
    Write(");\n");
  }

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintTextNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;

  nsCOMPtr<nsIDOMCharacterData> cd = do_QueryInterface(aNode);

  if (!cd)
    return NS_ERROR_NULL_POINTER;

  nsAutoString str;

  result = cd->GetData(str);

  if (NS_FAILED(result))
    return result;

  Write("n");
  WriteInt("%d", aDepth);
  Write(" = window.editorShell.editorDocument.createTextNode(\"");
  PrintUnicode(str);
  Write("\");\n");

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintNode(nsIDOMNode *aNode, PRInt32 aDepth)
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
      result = PrintAttributeNode(aNode, aDepth);
      break;
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

  Flush();

  return result;
}

nsresult
nsHTMLEditorLog::GetNodeTreeOffsets(nsIDOMNode *aNode, PRInt32 **aResult, PRInt32 *aLength)
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
nsHTMLEditorLog::Lock()
{
  mLocked++;
  return NS_OK;
}

nsresult
nsHTMLEditorLog::Unlock()
{
  --mLocked;
  return NS_OK;
}

#ifdef NEVER_ENABLE_THIS_JAVASCRIPT

function EditorGetNodeAtOffsets(offsets)
{
  var node = null;
  var i;

  node = window.editorShell.editorDocument;

  for (i = 0; i < offsets.length; i++)
  {
    node = node.childNodes[offsets[i]];
  }

  return node;
}

function EditorSetSelectionFromOffsets(selRanges)
{
  var rangeArr, start, end, i, node, offset;
  var selection = window.editorShell.editorSelection;

  selection.clearSelection();

  for (i = 0; i < selRanges.length; i++)
  {
    rangeArr = selRanges[i];
    start    = rangeArr[0];
    end      = rangeArr[1];

    var range = window.editorShell.editorDocument.createRange();

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
