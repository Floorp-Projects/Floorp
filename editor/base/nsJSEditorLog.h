/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#ifndef nsJSEditorLog_h__
#define nsJSEditorLog_h__

#include "nsIHTMLEditor.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"

/** implementation of a transaction listener object.
 *
 */
class nsJSEditorLog : public nsIHTMLEditor
{
private:

  nsCOMPtr<nsIFileSpec> mFileSpec;
  nsIEditor  *mEditor;
  PRInt32    mLocked;
  PRInt32    mDepth;

public:

  /** The default constructor.
   */
  nsJSEditorLog(nsIEditor *aEditor, nsIFileSpec *aLogFile);

  /** The default destructor.
   */
  virtual ~nsJSEditorLog();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsIHTMLEditor method implementations. */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, 
                  nsIPresShell   *aPresShell);
  NS_IMETHOD SetTextProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue);
  NS_IMETHOD GetTextProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAll, PRBool &aAny);
  NS_IMETHOD GetParagraphFormat(nsString& aParagraphFormat);
  NS_IMETHOD SetParagraphFormat(const nsString& aParagraphFormat);
  NS_IMETHOD RemoveTextProperty(nsIAtom *aProperty, const nsString *aAttribute);
  NS_IMETHOD DeleteSelection(nsIEditor::ECollapsedSelectionAction aAction);
  NS_IMETHOD InsertText(const nsString& aStringToInsert);
  NS_IMETHOD InsertBreak();
  NS_IMETHOD EnableUndo(PRBool aEnable);
  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo);
  NS_IMETHOD Redo(PRUint32 aCount);
  NS_IMETHOD CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo);
  NS_IMETHOD BeginTransaction();
  NS_IMETHOD EndTransaction();
  NS_IMETHOD MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection); 
  NS_IMETHOD SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD SelectAll();
  NS_IMETHOD BeginningOfDocument();
  NS_IMETHOD EndOfDocument();
  NS_IMETHOD ScrollUp(nsIAtom *aIncrement);
  NS_IMETHOD ScrollDown(nsIAtom *aIncrement);
  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin);

  NS_IMETHOD Save();
  NS_IMETHOD SaveAs(PRBool aSavingCopy);

  NS_IMETHOD Cut();
  NS_IMETHOD Copy();
  NS_IMETHOD Paste();
  NS_IMETHOD PasteAsQuotation();
  NS_IMETHOD PasteAsCitedQuotation(const nsString& aCitation);
  NS_IMETHOD InsertAsQuotation(const nsString& aQuotedText);
  NS_IMETHOD InsertAsCitedQuotation(const nsString& aQuotedText, const nsString& aCitation);


  NS_IMETHOD InsertHTML(const nsString &aInputString);

  NS_IMETHOD OutputToString(nsString& aOutputString,
                            const nsString& aFormatType,
                            PRUint32 aFlags);
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsString& aFormatType,
                            const nsString* aCharsetOverride,
                            PRUint32 aFlags);

  NS_IMETHOD ApplyStyleSheet(const nsString& aURL);

  NS_IMETHOD GetLocalFileURL(nsIDOMWindow* aParent, const nsString& aFilterType, nsString& aReturn);
  NS_IMETHOD SetBackgroundColor(const nsString& aColor);
  NS_IMETHOD SetBodyAttribute(const nsString& aAttr, const nsString& aValue);
  NS_IMETHOD GetParagraphStyle(nsStringArray *aTagList);
  NS_IMETHOD AddBlockParent(nsString& aParentTag);
  NS_IMETHOD ReplaceBlockParent(nsString& aParentTag);
  NS_IMETHOD RemoveParagraphStyle();
  NS_IMETHOD RemoveParent(const nsString &aParentTag);
  NS_IMETHOD InsertList(const nsString& aListType);
  NS_IMETHOD Indent(const nsString& aIndent);
  NS_IMETHOD Align(const nsString& aAlign);
  NS_IMETHOD GetElementOrParentByTagName(const nsString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn);
  NS_IMETHOD GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn);
  NS_IMETHOD CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn);
  NS_IMETHOD InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection);
  NS_IMETHOD SaveHLineSettings(nsIDOMElement* aElement);
  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);
  NS_IMETHOD SelectElement(nsIDOMElement* aElement);
  NS_IMETHOD SetCaretAfterElement(nsIDOMElement* aElement);
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray** aNodeList);
  NS_IMETHOD GetCellIndexes(nsIDOMElement *aCell, PRInt32 &aColIndex, PRInt32 &aRowIndex);
  NS_IMETHOD GetTableSize(nsIDOMElement *aTable, PRInt32 &aRowCount, PRInt32 &aColCount);
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell);
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell, 
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, PRInt32& aRowSpan, PRInt32& aColSpan, PRBool& aIsSelected);
  NS_IMETHOD InsertTableCell(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableColumn(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableRow(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD DeleteTable();
  NS_IMETHOD DeleteTableCell(PRInt32 aNumber);
  NS_IMETHOD DeleteTableColumn(PRInt32 aNumber);
  NS_IMETHOD DeleteTableRow(PRInt32 aNumber);
  NS_IMETHOD JoinTableCells();
  NS_IMETHOD NormalizeTable(nsIDOMElement *aTable);
  NS_IMETHOD BeginComposition(void);
  NS_IMETHOD SetCompositionString(const nsString& aCompositionString, nsIDOMTextRangeList* aTextRangeList);
  NS_IMETHOD EndComposition(void);
  NS_IMETHOD StartLogging(nsIFileSpec *aLogFile);
  NS_IMETHOD StopLogging();

  /* nsJSEditorLog public methods. */
  nsresult Write(const char *aBuffer);
  nsresult WriteInt(const char *aFormat, PRInt32 aInt);
  nsresult Flush();
  nsresult PrintUnicode(const nsString &aString);
  nsresult PrintSelection();
  nsresult PrintNode(nsIDOMNode *aNode, PRInt32 aDepth=0);
  nsresult PrintElementNode(nsIDOMNode *aNode, PRInt32 aDepth);
  nsresult PrintTextNode(nsIDOMNode *aNode, PRInt32 aDepth);
  nsresult PrintAttributeNode(nsIDOMNode *aNode, PRInt32 aDepth=0);
  nsresult PrintNodeChildren(nsIDOMNode *aNode, PRInt32 aDepth=0);
  nsresult GetNodeTreeOffsets(nsIDOMNode *aNode, PRInt32 **aResult, PRInt32 *aLength);
  nsresult Lock();
  nsresult Unlock();
};

class nsAutoJSEditorLogLock
{
  nsJSEditorLog *mLog;

public:

  nsAutoJSEditorLogLock(nsJSEditorLog *aLog)
  {
    mLog = aLog;

    if (mLog)
      mLog->Lock();
  }

  ~nsAutoJSEditorLogLock()
  {
    if (mLog)
      mLog->Unlock();
  }
};

#endif // nsJSEditorLog_h__
