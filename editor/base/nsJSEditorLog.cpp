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

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsJSEditorLog::nsJSEditorLog(nsIEditor *aEditor, nsIFileSpec *aLogFile)
{
  mRefCnt = 0;
  mEditor = aEditor;
  mLocked = -1;

  mFileSpec = do_QueryInterface(aLogFile);

  nsresult result;

  if (mFileSpec)
    result = mFileSpec->openStreamForWriting();
  else
    result = NS_ERROR_NULL_POINTER;

  if (NS_FAILED(result))
  {
    mFileSpec = nsCOMPtr<nsIFileSpec>();
  }
}

nsJSEditorLog::~nsJSEditorLog()
{
  if (mFileSpec)
    /* nsresult result = */ mFileSpec->closeStream();
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
  Write("window.editorShell.SetTextProperty(\"");
  PrintUnicode(propStr);
  Write("\", \"");
  if (aAttribute)
    PrintUnicode(*aAttribute);
  Write("\", \"");
  if (aValue)
    PrintUnicode(*aValue);
  Write("\");\n");

  Flush();

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
  Write("window.editorShell.paragraphFormat = \"");
  PrintUnicode(aParagraphFormat);
  Write("\";\n");

  Flush();

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
  Write("window.editorShell.RemoveTextProperty(\"");
  PrintUnicode(propStr);
  Write("\", \"");
  if (aAttribute)
    PrintUnicode(*aAttribute);
  Write("\");\n");

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::DeleteSelection(nsIEditor::ECollapsedSelectionAction aAction)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.DeleteSelection(");
  WriteInt("%d", aAction);
  Write(");\n");

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertText(const nsString& aStringToInsert)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  Write("window.editorShell.InsertText(\"");
  PrintUnicode(aStringToInsert);
  Write("\");\n");

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertBreak()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.InsertBreak();\n");
  Flush();

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

  Write("window.editorShell.Undo();\n");
  Flush();

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

  Write("window.editorShell.Redo();\n");
  Flush();

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

  Write("window.editorShell.BeginBatchChanges();\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::EndTransaction()
{
  if (mLocked)
    return NS_OK;

  Write("window.editorShell.EndBatchChanges();\n");
  Flush();

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

  Write("window.editorShell.SelectAll();\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::BeginningOfDocument()
{
  if (mLocked)
    return NS_OK;

  Write("window.editorShell.BeginningOfDocument();\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::EndOfDocument()
{
  if (mLocked)
    return NS_OK;

  Write("window.editorShell.EndOfDocument();\n");
  Flush();

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
  Write("window.editorShell.Cut();\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Copy()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.Copy();\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Paste()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.Paste();\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::PasteAsQuotation()
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.PasteAsQuotation();\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::PasteAsCitedQuotation(const nsString& aCitation)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.PasteAsCitedQuotation(\"");
  PrintUnicode(aCitation);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertAsQuotation(const nsString& aQuotedText)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.InsertAsQuotation(\"");
  PrintUnicode(aQuotedText);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertAsCitedQuotation(const nsString& aQuotedText, const nsString& aCitation)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();
  Write("window.editorShell.InsertAsCitedQuotation(\"");
  PrintUnicode(aQuotedText);
  Write("\", \"");
  PrintUnicode(aCitation);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertHTML(const nsString &aInputString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::OutputToString(nsString& aOutputString,
                              const nsString& aFormatType,
                              PRUint32 aFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::OutputToStream(nsIOutputStream* aOutputStream,
                              const nsString& aFormatType,
                              const nsString* aCharsetOverride,
                              PRUint32 aFlags)
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

  Write("window.editorShell.SetBackgroundColor(\"");
  PrintUnicode(aColor);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::SetBodyAttribute(const nsString& aAttr, const nsString& aValue)
{
  if (mLocked)
    return NS_OK;

  Write("window.editorShell.SetBodyAttribute(\"");
  PrintUnicode(aAttr);
  Write("\", \"");
  PrintUnicode(aValue);
  Write("\");\n");
  Flush();

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

  Write("window.editorShell.InsertLink(\"");
  PrintUnicode(aURL);
  Write("\");\n");
  Flush();

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

  Write("window.editorShell.InsertImage(\"");
  PrintUnicode(aURL);
  Write("\", \"");
  PrintUnicode(aWidth);
  Write("\", \"");
  PrintUnicode(aHeight);
  Write("\", \"");
  PrintUnicode(aHspace);
  Write("\", \"");
  PrintUnicode(aVspace);
  Write("\", \"");
  PrintUnicode(aBorder);
  Write("\", \"");
  PrintUnicode(aAlt);
  Write("\", \"");
  PrintUnicode(aAlignment);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::InsertList(const nsString& aListType)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  Write("window.editorShell.InsertList(\"");
  PrintUnicode(aListType);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Indent(const nsString& aIndent)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  Write("window.editorShell.Indent(\"");
  PrintUnicode(aIndent);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::Align(const nsString& aAlign)
{
  if (mLocked)
    return NS_OK;

  PrintSelection();

  Write("window.editorShell.Align(\"");
  PrintUnicode(aAlign);
  Write("\");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::GetElementOrParentByTagName(const nsString &aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  Write("window.editorShell.InsertElement(n0, ");
  Write(aDeleteSelection ? "true" : "false");
  Write(");\n");
  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::SaveHLineSettings(nsIDOMElement* aElement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  Write("window.editorShell.InsertLinkAroundSelection(n0);\n");
  Flush();

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
nsJSEditorLog::GetEmbeddedObjects(nsISupportsArray** aNodeList)
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
nsJSEditorLog::GetCellIndexes(nsIDOMElement *aCell, PRInt32 &aColIndex, PRInt32 &aRowIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJSEditorLog::GetRowIndex(nsIDOMElement *aCell, PRInt32 &aRowIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
nsJSEditorLog::GetColumnIndex(nsIDOMElement *aCell, PRInt32 &aColIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJSEditorLog::GetColumnCellCount(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJSEditorLog::GetRowCellCount(nsIDOMElement* aTable, PRInt32 aColIndex, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJSEditorLog::GetMaxColumnCellCount(nsIDOMElement* aTable, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJSEditorLog::GetMaxRowCellCount(nsIDOMElement* aTable, PRInt32& aCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//TODO: This should be implemented by layout for efficiency
NS_IMETHODIMP 
nsJSEditorLog::GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsJSEditorLog::GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell, 
                            PRInt32& aStartRowIndex, PRInt32& aStartColIndex, PRInt32& aRowSpan, PRInt32& aColSpan)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::BeginComposition(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::SetCompositionString(const nsString& aCompositionString,nsIDOMTextRangeList* aTextRangeList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::EndComposition(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::ApplyStyleSheet(const nsString& aURL)
{
  Write("window.editorShell.ApplyStyleSheet(\"");
  PrintUnicode(aURL);
  Write("\");\n");

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::StartLogging(nsIFileSpec *aLogFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJSEditorLog::StopLogging()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsJSEditorLog::Write(const char *aBuffer)
{
  nsresult result;

  if (!aBuffer)
    return NS_ERROR_NULL_POINTER;

  PRInt32 len = strlen(aBuffer);

  if (mFileSpec)
  {
    PRInt32 retval;

    result = mFileSpec->write(aBuffer, len, &retval);

    if (NS_FAILED(result))
      return result;

#ifdef VERY_SLOW

    result = mFileSpec->flush();

    if (NS_FAILED(result))
      return result;

#endif // VERY_SLOW
  }
  else
    fwrite(aBuffer, 1, len, stdout);

  return NS_OK;
}

nsresult
nsJSEditorLog::WriteInt(const char *aFormat, PRInt32 aInt)
{
  if (!aFormat)
    return NS_ERROR_NULL_POINTER;

  char buf[256];

  sprintf(buf, aFormat, aInt);

  return Write(buf);
}

nsresult
nsJSEditorLog::Flush()
{
  nsresult result = NS_OK;

  if (mFileSpec)
    result = mFileSpec->flush();
  else
    fflush(stdout);

  return result;
}

nsresult
nsJSEditorLog::PrintUnicode(const nsString &aString)
{
  const PRUnichar *uc = aString.GetUnicode();
  PRInt32 i, len = aString.Length();
  char buf[10];

  for (i = 0; i < len; i++)
  {
    if (nsString::IsAlpha(uc[i]) || nsString::IsDigit(uc[i]) || uc[i] == ' ')
      sprintf(buf, "%c", uc[i]);
    else
      sprintf(buf, "\\u%.4x", uc[i]);

    nsresult result = Write(buf);

    if (NS_FAILED(result))
      return result;
  }

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

  result = presShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));

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

    Write("n");
    WriteInt("%d", aDepth);
    Write(".appendChild(n");
    WriteInt("%d", aDepth+1);
    Write(");\n");
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

  Write("n");
  WriteInt("%d", aDepth);
  Write(" = window.editorShell.editorDocument.createTextNode(\"");
  PrintUnicode(str);
  Write("\");\n");

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
