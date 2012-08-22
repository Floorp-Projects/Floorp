/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  nsCOMPtr<nsIOutputStream>     mFileStream;
  nsEditorTxnLog       *mEditorTxnLog;
  int32_t               mLocked;
  int32_t               mDepth;

public:

  // Interfaces for AddRef, Release, and QueryInterface.
  NS_DECL_ISUPPORTS_INHERITED

           nsHTMLEditorLog();
  virtual ~nsHTMLEditorLog();

  /* nsIHTMLEditor method implementations. */
  NS_IMETHOD SetInlineProperty(nsIAtom *aProperty, 
                            const nsAString & aAttribute, 
                            const nsAString & aValue);
  NS_IMETHOD SetParagraphFormat(const nsAString& aParagraphFormat);
  NS_IMETHOD RemoveInlineProperty(nsIAtom *aProperty, const nsAString& aAttribute);
  NS_IMETHOD DeleteSelection(nsIEditor::EDirection aAction,
                             nsIEditor::EStripWrappers aStripWrappers);
  NS_IMETHOD InsertText(const nsAString& aStringToInsert);
  NS_IMETHOD InsertLineBreak();
  NS_IMETHOD Undo(uint32_t aCount);
  NS_IMETHOD Redo(uint32_t aCount);
  NS_IMETHOD BeginTransaction();
  NS_IMETHOD EndTransaction();
  NS_IMETHOD SelectAll();
  NS_IMETHOD BeginningOfDocument();
  NS_IMETHOD EndOfDocument();

  NS_IMETHOD Cut();
  NS_IMETHOD Copy();
  NS_IMETHOD Paste(int32_t aSelectionType);
  NS_IMETHOD PasteAsQuotation(int32_t aSelectionType);
  NS_IMETHOD PasteAsPlaintextQuotation(int32_t aSelectionType);
  NS_IMETHOD PasteAsCitedQuotation(const nsAString& aCitation,
                                   int32_t aSelectionType);
  NS_IMETHOD InsertAsQuotation(const nsAString& aQuotedText, nsIDOMNode** aNodeInserted);
  NS_IMETHOD InsertAsPlaintextQuotation(const nsAString& aQuotedText,
                                        bool aAddCites,
                                        nsIDOMNode** aNodeInserted);
  NS_IMETHOD InsertAsCitedQuotation(const nsAString& aQuotedText, const nsAString& aCitation, 
                                    bool aInsertHTML,
                                    nsIDOMNode** aNodeInserted);

  NS_IMETHOD SetDocumentTitle(const nsAString& aTitle);

  NS_IMETHOD SetBackgroundColor(const nsAString& aColor);
  NS_IMETHOD SetBodyAttribute(const nsAString& aAttr, const nsAString& aValue);
  NS_IMETHOD MakeOrChangeList(const nsAString& aListType, bool entireList, const nsAString& aBulletType);
  NS_IMETHOD Indent(const nsAString& aIndent);
  NS_IMETHOD Align(const nsAString& aAlign);
  NS_IMETHOD InsertElementAtSelection(nsIDOMElement* aElement, bool aDeleteSelection);
  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);
  
  /* Table Editing */
  NS_IMETHOD InsertTableCell(int32_t aNumber, bool aAfter);
  NS_IMETHOD InsertTableColumn(int32_t aNumber, bool aAfter);
  NS_IMETHOD InsertTableRow(int32_t aNumber, bool aAfter);
  NS_IMETHOD DeleteTable();
  NS_IMETHOD DeleteTableCell(int32_t aNumber);
  NS_IMETHOD DeleteTableCellContents();
  NS_IMETHOD DeleteTableColumn(int32_t aNumber);
  NS_IMETHOD DeleteTableRow(int32_t aNumber);
  NS_IMETHOD JoinTableCells(bool aMergeNonContiguousContents);
  NS_IMETHOD SplitTableCell();
  NS_IMETHOD NormalizeTable(nsIDOMElement *aTable);
  NS_IMETHOD SwitchTableCellHeaderType(nsIDOMElement *aSourceCell, nsIDOMElement **aNewCell);

  NS_IMETHOD StartLogging(nsIFile *aLogFile);
  NS_IMETHOD StopLogging();

  /* nsHTMLEditorLog public methods. */
  nsresult Write(const char *aBuffer);
  nsresult WriteInt(int32_t aInt);
  nsresult Flush();
  nsresult PrintUnicode(const nsAString &aString);
  nsresult PrintSelection();
  nsresult PrintNode(nsIDOMNode *aNode, int32_t aDepth=0);
  nsresult PrintElementNode(nsIDOMNode *aNode, int32_t aDepth);
  nsresult PrintTextNode(nsIDOMNode *aNode, int32_t aDepth);
  nsresult PrintAttributeNode(nsIDOMNode *aNode, int32_t aDepth=0);
  nsresult PrintNodeChildren(nsIDOMNode *aNode, int32_t aDepth=0);
  nsresult GetNodeTreeOffsets(nsIDOMNode *aNode, int32_t **aResult, int32_t *aLength);
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
