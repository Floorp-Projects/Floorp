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
 */

#ifndef nsHTMLEditorLog_h__
#define nsHTMLEditorLog_h__

#include "nsHTMLEditor.h"
#include "nsIEditorLogging.h"
#include "nsIFileStreams.h"
#include "nsCOMPtr.h"

class nsIFile;
class nsEditorTxnLog;

/** implementation of a transaction listener object.
 *
 */
class nsHTMLEditorLog : public nsHTMLEditor,
                        public nsIEditorLogging
{
private:

  nsCOMPtr<nsIFileOutputStream>     mFileStream;
  nsEditorTxnLog       *mEditorTxnLog;
  PRInt32               mLocked;
  PRInt32               mDepth;

public:

  // Interfaces for AddRef, Release, and QueryInterface.
  NS_DECL_ISUPPORTS_INHERITED

           nsHTMLEditorLog();
  virtual ~nsHTMLEditorLog();

  /* nsIHTMLEditor method implementations. */
  NS_IMETHOD SetInlineProperty(nsIAtom *aProperty, 
                            const nsAReadableString & aAttribute, 
                            const nsAReadableString & aValue);
  NS_IMETHOD SetParagraphFormat(const nsAString& aParagraphFormat);
  NS_IMETHOD RemoveInlineProperty(nsIAtom *aProperty, const nsAReadableString& aAttribute);
  NS_IMETHOD DeleteSelection(nsIEditor::EDirection aAction);
  NS_IMETHOD InsertText(const nsAReadableString& aStringToInsert);
  NS_IMETHOD InsertLineBreak();
  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD Redo(PRUint32 aCount);
  NS_IMETHOD BeginTransaction();
  NS_IMETHOD EndTransaction();
  NS_IMETHOD SelectAll();
  NS_IMETHOD BeginningOfDocument();
  NS_IMETHOD EndOfDocument();

  NS_IMETHOD Cut();
  NS_IMETHOD Copy();
  NS_IMETHOD Paste(PRInt32 aSelectionType);
  NS_IMETHOD PasteAsQuotation(PRInt32 aSelectionType);
  NS_IMETHOD PasteAsPlaintextQuotation(PRInt32 aSelectionType);
  NS_IMETHOD PasteAsCitedQuotation(const nsAReadableString& aCitation,
                                   PRInt32 aSelectionType);
  NS_IMETHOD InsertAsQuotation(const nsAReadableString& aQuotedText, nsIDOMNode** aNodeInserted);
  NS_IMETHOD InsertAsPlaintextQuotation(const nsAReadableString& aQuotedText, nsIDOMNode** aNodeInserted);
  NS_IMETHOD InsertAsCitedQuotation(const nsAReadableString& aQuotedText, const nsAReadableString& aCitation, 
                                    PRBool aInsertHTML, const nsAReadableString& aCharset, 
                                    nsIDOMNode** aNodeInserted);

  NS_IMETHOD ApplyStyleSheet(const nsAReadableString& aURL, nsICSSStyleSheet **aStyleSheet);
  NS_IMETHOD SetDocumentTitle(const nsAReadableString& aTitle);

  NS_IMETHOD SetBackgroundColor(const nsAReadableString& aColor);
  NS_IMETHOD SetBodyAttribute(const nsAReadableString& aAttr, const nsAReadableString& aValue);
  NS_IMETHOD MakeOrChangeList(const nsAReadableString& aListType, PRBool entireList);
  NS_IMETHOD Indent(const nsAReadableString& aIndent);
  NS_IMETHOD Align(const nsAReadableString& aAlign);
  NS_IMETHOD InsertElementAtSelection(nsIDOMElement* aElement, PRBool aDeleteSelection);
  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);
  
  /* Table Editing */
  NS_IMETHOD InsertTableCell(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableColumn(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableRow(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD DeleteTable();
  NS_IMETHOD DeleteTableCell(PRInt32 aNumber);
  NS_IMETHOD DeleteTableCellContents();
  NS_IMETHOD DeleteTableColumn(PRInt32 aNumber);
  NS_IMETHOD DeleteTableRow(PRInt32 aNumber);
  NS_IMETHOD JoinTableCells(PRBool aMergeNonContiguousContents);
  NS_IMETHOD SplitTableCell();
  NS_IMETHOD NormalizeTable(nsIDOMElement *aTable);
  NS_IMETHOD SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell);

  NS_IMETHOD StartLogging(nsIFile *aLogFile);
  NS_IMETHOD StopLogging();

  /* nsHTMLEditorLog public methods. */
  nsresult Write(const char *aBuffer);
  nsresult WriteInt(const char *aFormat, PRInt32 aInt);
  nsresult Flush();
  nsresult PrintUnicode(const nsAReadableString &aString);
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

class nsAutoHTMLEditorLogLock
{
  nsHTMLEditorLog *mLog;

public:

  nsAutoHTMLEditorLogLock(nsHTMLEditorLog *aLog)
  {
    mLog = aLog;

    if (mLog)
      mLog->Lock();
  }

  ~nsAutoHTMLEditorLogLock()
  {
    if (mLog)
      mLog->Unlock();
  }
};

#endif // nsHTMLEditorLog_h__
