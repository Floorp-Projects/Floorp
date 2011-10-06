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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "prprf.h"

#include "nsEditorTxnLog.h"

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)

nsHTMLEditorLog::nsHTMLEditorLog()
{
  mLocked   = -1;
  mEditorTxnLog = 0;
}

nsHTMLEditorLog::~nsHTMLEditorLog()
{
  StopLogging();
}

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLEditorLog, nsHTMLEditor, nsIEditorLogging)

NS_IMETHODIMP
nsHTMLEditorLog::SetInlineProperty(nsIAtom *aProperty, const nsAString &aAttribute, const nsAString &aValue)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    nsAutoString propStr;

    aProperty->ToString(propStr);

    PrintSelection();
    Write("atomService = Components.classes[\"@mozilla.org/atom-service;1\"].getService(Components.interfaces.nsIAtomService);\n");
    Write("propAtom = atomService.getAtom(\"");
    PrintUnicode(propStr);
    Write("\");\n");
    Write("GetCurrentEditor().setInlineProperty(propAtom, \"");
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
    Write("GetCurrentEditor().setParagraphFormat(\"");
    PrintUnicode(aParagraphFormat);
    Write("\");\n");

    Flush();
  }

  return nsHTMLEditor::SetParagraphFormat(aParagraphFormat);
}

NS_IMETHODIMP
nsHTMLEditorLog::RemoveInlineProperty(nsIAtom *aProperty, const nsAString &aAttribute)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    nsAutoString propStr;

    aProperty->ToString(propStr);

    PrintSelection();
    Write("atomService = Components.classes[\"@mozilla.org/atom-service;1\"].getService(Components.interfaces.nsIAtomService);\n");
    Write("propAtom = atomService.getAtom(\"");
    PrintUnicode(propStr);
    Write("\");\n");
    Write("GetCurrentEditor().removeInlineProperty(propAtom, \"");
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
    Write("GetCurrentEditor().deleteSelection(");
    WriteInt(aAction);
    Write(");\n");

    Flush();
  }

  return nsHTMLEditor::DeleteSelection(aAction);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertText(const nsAString& aStringToInsert)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("GetCurrentEditor().insertText(\"");
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
    Write("GetCurrentEditor().insertLineBreak();\n");
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
    Write("GetCurrentEditor().undo(");
    WriteInt(aCount);
    Write(");\n");
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
    Write("GetCurrentEditor().redo(");
    WriteInt(aCount);
    Write(");\n");
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
    Write("GetCurrentEditor().beginTransaction();\n");
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
    Write("GetCurrentEditor().endTransaction();\n");
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
    Write("GetCurrentEditor().selectAll();\n");
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
    Write("GetCurrentEditor().beginningOfDocument();\n");
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
    Write("GetCurrentEditor().endOfDocument();\n");
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
    Write("GetCurrentEditor().cut();\n");
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
    Write("GetCurrentEditor().copy();\n");
    Flush();
  }

  return nsHTMLEditor::Copy();
}

NS_IMETHODIMP
nsHTMLEditorLog::Paste(PRInt32 aClipboardType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("GetCurrentEditor().paste(");
    WriteInt(aClipboardType);
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::Paste(aClipboardType);
}

NS_IMETHODIMP
nsHTMLEditorLog::PasteAsQuotation(PRInt32 aClipboardType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("GetCurrentEditor().pasteAsQuotation(");
    WriteInt(aClipboardType);
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::PasteAsQuotation(aClipboardType);
}

NS_IMETHODIMP
nsHTMLEditorLog::PasteAsPlaintextQuotation(PRInt32 aClipboardType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("GetCurrentEditor().pasteAsQuotation(");
    WriteInt(aClipboardType);
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::PasteAsPlaintextQuotation(aClipboardType);
}

NS_IMETHODIMP
nsHTMLEditorLog::PasteAsCitedQuotation(const nsAString& aCitation,
                                       PRInt32 aClipboardType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("GetCurrentEditor().pasteAsCitedQuotation(\"");
    PrintUnicode(aCitation);
    Write("\", ");
    WriteInt(aClipboardType);
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::PasteAsCitedQuotation(aCitation, aClipboardType);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertAsQuotation(const nsAString& aQuotedText,
                                   nsIDOMNode **aNodeInserted)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("GetCurrentEditor().insertAsQuotation(\"");
    PrintUnicode(aQuotedText);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertAsQuotation(aQuotedText, aNodeInserted);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertAsPlaintextQuotation(const nsAString& aQuotedText,
                                            bool aAddCites,
                                            nsIDOMNode **aNodeInserted)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("GetCurrentEditor().insertAsQuotation(\"");
    PrintUnicode(aQuotedText);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertAsPlaintextQuotation(aQuotedText,
                                                  aAddCites,
                                                  aNodeInserted);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertAsCitedQuotation(const nsAString& aQuotedText,
                                        const nsAString& aCitation,
                                        bool aInsertHTML,
                                        nsIDOMNode **aNodeInserted)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();
    Write("GetCurrentEditor().insertAsCitedQuotation(\"");
    PrintUnicode(aQuotedText);
    Write("\", \"");
    PrintUnicode(aCitation);
    Write("\", ");
    Write(aInsertHTML ? "true" : "false");
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::InsertAsCitedQuotation(aQuotedText, aCitation, aInsertHTML,
                                              aNodeInserted);
}

NS_IMETHODIMP
nsHTMLEditorLog::SetBackgroundColor(const nsAString& aColor)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("GetCurrentEditor().setBackgroundColor(\"");
    PrintUnicode(aColor);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::SetBackgroundColor(aColor);
}

NS_IMETHODIMP
nsHTMLEditorLog::SetBodyAttribute(const nsAString& aAttr, const nsAString& aValue)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("GetCurrentEditor().setBodyAttribute(\"");
    PrintUnicode(aAttr);
    Write("\", \"");
    PrintUnicode(aValue);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::SetBodyAttribute(aAttr, aValue);
}

NS_IMETHODIMP
nsHTMLEditorLog:: InsertTableCell(PRInt32 aNumber, bool aAfter)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("GetCurrentEditor().insertTableCell(");
    WriteInt(aNumber);
    Write(", ");
    Write(aAfter ? "true" : "false");
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::InsertTableCell(aNumber, aAfter);
}


NS_IMETHODIMP
nsHTMLEditorLog:: InsertTableColumn(PRInt32 aNumber, bool aAfter)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("GetCurrentEditor().insertTableColumn(");
    WriteInt(aNumber);
    Write(", ");
    Write(aAfter ? "true" : "false");
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::InsertTableColumn(aNumber, aAfter);
}


NS_IMETHODIMP
nsHTMLEditorLog:: InsertTableRow(PRInt32 aNumber, bool aAfter)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("GetCurrentEditor().insertTableRow(");
    WriteInt(aNumber);
    Write(", ");
    Write(aAfter ? "true" : "false");
    Write(");\n");
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
    Write("GetCurrentEditor().deleteTable();\n");
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
    Write("GetCurrentEditor().deleteTableCell(");
    WriteInt(aNumber);
    Write(");\n");
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
    Write("GetCurrentEditor().deleteTableCellContents();\n");
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
    Write("GetCurrentEditor().deleteTableColumn(");
    WriteInt(aNumber);
    Write(");\n");
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
    Write("GetCurrentEditor().deleteTableRow(");
    WriteInt(aNumber);
    Write(");\n");
    Flush();
  }

  return nsHTMLEditor::DeleteTableRow(aNumber);
}

NS_IMETHODIMP
nsHTMLEditorLog:: JoinTableCells(bool aMergeNonContiguousContents)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    Write("GetCurrentEditor().joinTableCells(");
    Write(aMergeNonContiguousContents ? "true" : "false");
    Write(");\n");
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
    Write("GetCurrentEditor().splitTableCell();\n");
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
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aTable);
    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    PrintNode(node, 0);
    Write("GetCurrentEditor().normalizeTable(n0);\n");
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
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aSourceCell);
    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    PrintNode(node, 0);
    Write("GetCurrentEditor().switchTableCellHeaderType(n0);\n");
    Flush();
  }

  return nsHTMLEditor::SwitchTableCellHeaderType(aSourceCell, aNewCell);
}

NS_IMETHODIMP
nsHTMLEditorLog::MakeOrChangeList(const nsAString& aListType, bool entireList, const nsAString& aBulletType)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("GetCurrentEditor().makeOrChangeList(\"");
    PrintUnicode(aListType);
    Write("\", ");
    Write(entireList ? "true" : "false");
    Write(", \"");
    PrintUnicode(aBulletType);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::MakeOrChangeList(aListType, entireList, aBulletType);
}

NS_IMETHODIMP
nsHTMLEditorLog::Indent(const nsAString& aIndent)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("GetCurrentEditor().indent(\"");
    PrintUnicode(aIndent);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::Indent(aIndent);
}

NS_IMETHODIMP
nsHTMLEditorLog::Align(const nsAString& aAlign)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("GetCurrentEditor().align(\"");
    PrintUnicode(aAlign);
    Write("\");\n");
    Flush();
  }

  return nsHTMLEditor::Align(aAlign);
}

NS_IMETHODIMP
nsHTMLEditorLog::InsertElementAtSelection(nsIDOMElement* aElement, bool aDeleteSelection)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);

    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    PrintSelection();
    PrintNode(node, 0);
    Write("GetCurrentEditor().insertElementAtSelection(n0, ");
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
    NS_ENSURE_TRUE(aAnchorElement, NS_ERROR_NULL_POINTER);

    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aAnchorElement);

    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    PrintSelection();
    PrintNode(node, 0);
    Write("GetCurrentEditor().insertLinkAroundSelection(n0);\n");
    Flush();
  }

  return nsHTMLEditor::InsertLinkAroundSelection(aAnchorElement);
}

NS_IMETHODIMP
nsHTMLEditorLog::SetDocumentTitle(const nsAString& aTitle)
{
  nsAutoHTMLEditorLogLock logLock(this);

  if (!mLocked && mFileStream)
  {
    PrintSelection();

    Write("GetCurrentEditor().setDocumentTitle(\"");
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

  NS_ENSURE_TRUE(aLogFile, NS_ERROR_NULL_POINTER);

  if (mFileStream)
  {
    result = StopLogging();

    NS_ENSURE_SUCCESS(result, result);
  }

  result = NS_NewLocalFileOutputStream(getter_AddRefs(mFileStream), aLogFile);
  NS_ENSURE_SUCCESS(result, result);

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

  NS_ENSURE_TRUE(aBuffer, NS_ERROR_NULL_POINTER);

  PRInt32 len = strlen(aBuffer);

  if (mFileStream)
  {
    PRUint32 retval;

    result = mFileStream->Write(aBuffer, len, &retval);

    NS_ENSURE_SUCCESS(result, result);

#ifdef VERY_SLOW

    result = mFileStream->Flush();

    NS_ENSURE_SUCCESS(result, result);

#endif // VERY_SLOW
  }
  else
    fwrite(aBuffer, 1, len, stdout);

  return NS_OK;
}

nsresult
nsHTMLEditorLog::WriteInt(PRInt32 aInt)
{
  char buf[256];

  PR_snprintf(buf, sizeof(buf), "%d", aInt);

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
nsHTMLEditorLog::PrintUnicode(const nsAString &aString)
{
  //const PRUnichar *uc = aString.get();
  char buf[10];
  nsReadingIterator <PRUnichar> beginIter,endIter;
  aString.BeginReading(beginIter);
  aString.EndReading(endIter);
  while(beginIter != endIter)
  {
    if (nsCRT::IsAsciiAlpha(*beginIter) || nsCRT::IsAsciiDigit(*beginIter) || *beginIter == ' ')
      PR_snprintf(buf, sizeof(buf), "%c", *beginIter);
    else
      PR_snprintf(buf, sizeof(buf), "\\u%.4x", *beginIter);

    nsresult result = Write(buf);

    NS_ENSURE_SUCCESS(result, result);
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

  NS_ENSURE_SUCCESS(result, result);

  result = selection->GetRangeCount(&rangeCount);

  NS_ENSURE_SUCCESS(result, result);

  Write("selRanges = [ ");

  PRInt32 i, j;
  nsCOMPtr<nsIDOMRange> range;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;

  for (i = 0; i < rangeCount; i++)
  {
    result = selection->GetRangeAt(i, getter_AddRefs(range));

    NS_ENSURE_SUCCESS(result, result);
    
    result = range->GetStartContainer(getter_AddRefs(startNode));

    NS_ENSURE_SUCCESS(result, result);

    NS_ENSURE_TRUE(startNode, NS_ERROR_NULL_POINTER);

    result = range->GetStartOffset(&startOffset);

    NS_ENSURE_SUCCESS(result, result);

    result = range->GetEndContainer(getter_AddRefs(endNode));

    NS_ENSURE_SUCCESS(result, result);

    NS_ENSURE_TRUE(endNode, NS_ERROR_NULL_POINTER);

    result = range->GetEndOffset(&endOffset);

    NS_ENSURE_SUCCESS(result, result);

    PRInt32 *offsetArray = 0;
    PRInt32 arrayLength = 0;

    result = GetNodeTreeOffsets(startNode, &offsetArray, &arrayLength);

    NS_ENSURE_SUCCESS(result, result);

    if (i != 0)
      Write(",\n              ");

    Write("[ [[");

    for (j = 0; j < arrayLength; j++)
    {
      if (j != 0)
        Write(", ");
      WriteInt(offsetArray[j]);
    }

    Write("], ");
    WriteInt(startOffset);
    Write("], ");

    if (startNode != endNode)
    {
      delete []offsetArray;
      offsetArray = 0;
      arrayLength = 0;

      result = GetNodeTreeOffsets(endNode, &offsetArray, &arrayLength);

      NS_ENSURE_SUCCESS(result, result);
    }

    Write("[[");

    for (j = 0; j < arrayLength; j++)
    {
      if (j != 0)
        Write(", ");
      WriteInt(offsetArray[j]);
    }

    delete []offsetArray;

    Write("], ");
    WriteInt(endOffset);
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

  NS_ENSURE_TRUE(ele, NS_ERROR_NULL_POINTER);

  result = ele->GetTagName(tag);

  NS_ENSURE_SUCCESS(result, result);

  Write("n");
  WriteInt(aDepth);
  Write(" = GetCurrentEditor().editorDocument.createElement(\"");
  PrintUnicode(tag);
  Write("\");\n");

  result = aNode->GetAttributes(getter_AddRefs(map));

  NS_ENSURE_SUCCESS(result, result);

  NS_ENSURE_TRUE(map, NS_ERROR_NULL_POINTER);

  PRUint32 i, len;
  nsCOMPtr<nsIDOMNode> attr;

  result = map->GetLength(&len);

  NS_ENSURE_SUCCESS(result, result);

  for (i = 0; i < len; i++)
  {
    result = map->Item(i, getter_AddRefs(attr));

    NS_ENSURE_SUCCESS(result, result);

    NS_ENSURE_TRUE(attr, NS_ERROR_NULL_POINTER);

    result = PrintAttributeNode(attr, aDepth);

    NS_ENSURE_SUCCESS(result, result);
  }

  result = PrintNodeChildren(aNode, aDepth);

  return result;
}

nsresult
nsHTMLEditorLog::PrintAttributeNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;
  nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(aNode);

  NS_ENSURE_TRUE(attr, NS_ERROR_NULL_POINTER);

  nsAutoString str;

  result = attr->GetName(str);

  NS_ENSURE_SUCCESS(result, result);

  Write("a");
  WriteInt(aDepth);
  Write(" = GetCurrentEditor().editorDocument.createAttribute(\"");
  PrintUnicode(str);
  Write("\");\n");

  result = attr->GetValue(str);

  NS_ENSURE_SUCCESS(result, result);

  Write("a");
  WriteInt(aDepth);
  Write(".value = \"");
  PrintUnicode(str);
  Write("\";\n");
  
  Write("n");
  WriteInt(aDepth);
  Write(".setAttributeNode(a");
  WriteInt(aDepth);
  Write(");\n");

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintNodeChildren(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;

  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNodeList> list;

  result = aNode->GetChildNodes(getter_AddRefs(list));
  
  NS_ENSURE_SUCCESS(result, result);

  if (!list)
  {
    // Must not have any children!
    return NS_OK;
  }

  PRUint32 i, len;
  nsCOMPtr<nsIDOMNode> node;

  result = list->GetLength(&len);

  NS_ENSURE_SUCCESS(result, result);

  for (i = 0; i < len; i++)
  {
    result = list->Item(i, getter_AddRefs(node));

    NS_ENSURE_SUCCESS(result, result);

    result = PrintNode(node, aDepth + 1);

    NS_ENSURE_SUCCESS(result, result);

    Write("n");
    WriteInt(aDepth);
    Write(".appendChild(n");
    WriteInt(aDepth+1);
    Write(");\n");
  }

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintTextNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result;

  nsCOMPtr<nsIDOMCharacterData> cd = do_QueryInterface(aNode);

  NS_ENSURE_TRUE(cd, NS_ERROR_NULL_POINTER);

  nsAutoString str;

  result = cd->GetData(str);

  NS_ENSURE_SUCCESS(result, result);

  Write("n");
  WriteInt(aDepth);
  Write(" = GetCurrentEditor().editorDocument.createTextNode(\"");
  PrintUnicode(str);
  Write("\");\n");

  return NS_OK;
}

nsresult
nsHTMLEditorLog::PrintNode(nsIDOMNode *aNode, PRInt32 aDepth)
{
  nsresult result = NS_OK;

  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

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

  NS_ENSURE_TRUE(aNode && aResult && aLength, NS_ERROR_NULL_POINTER);

  *aResult = 0;
  *aLength = 0;

  nsIDOMNode *parent = aNode;
  PRInt32 i = 0;

  // Count the number of parent nodes above aNode.

  while (parent)
  {
    result = parent->GetParentNode(&parent);

    NS_ENSURE_SUCCESS(result, result);

    if (parent)
      ++i;
  }

  // Allocate an array big enough to hold all the offsets.

  *aResult = new PRInt32[i];

  NS_ENSURE_TRUE(aResult, NS_ERROR_OUT_OF_MEMORY);

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

  node = GetCurrentEditor().editorDocument;

  for (i = 0; i < offsets.length; i++)
  {
    node = node.childNodes[offsets[i]];
  }

  return node;
}

function EditorSetSelectionFromOffsets(selRanges)
{
  var rangeArr, start, end, i, node, offset;
  var selection = GetCurrentEditor().editorSelection;

  selection.clearSelection();

  for (i = 0; i < selRanges.length; i++)
  {
    rangeArr = selRanges[i];
    start    = rangeArr[0];
    end      = rangeArr[1];

    var range = GetCurrentEditor().editorDocument.createRange();

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
