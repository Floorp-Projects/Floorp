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

#ifndef nsHTMLEditor_h__
#define nsHTMLEditor_h__

#include "nsITextEditor.h"
#include "nsIHTMLEditor.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "InsertTableTxn.h"

/**
 * The HTML editor implementation.<br>
 * Use to edit HTML document represented as a DOM tree. 
 */
class nsHTMLEditor  : public nsIHTMLEditor
{
public:
  // see nsIHTMLEditor for documentation

//Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

//Initialization
  nsHTMLEditor();
  virtual nsresult InitHTMLEditor(nsIDOMDocument *aDoc, 
                                  nsIPresShell   *aPresShell,
                                  nsIEditorCallback *aCallback=nsnull);
  virtual ~nsHTMLEditor();

//============================================================================
// Methods that are duplicates of nsTextEditor -- exposed here for convenience

// Editing Operations
  virtual nsresult SetTextProperties(nsISupportsArray *aPropList);
  virtual nsresult GetTextProperties(nsISupportsArray *aPropList);
  virtual nsresult RemoveTextProperties(nsISupportsArray *aPropList);
  virtual nsresult DeleteSelection(nsIEditor::Direction aDir);
  virtual nsresult InsertText(const nsString& aStringToInsert);
  virtual nsresult InsertBreak(PRBool aCtrlKey);

// Transaction control
  virtual nsresult EnableUndo(PRBool aEnable);
  virtual nsresult Undo(PRUint32 aCount);
  virtual nsresult CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo);
  virtual nsresult Redo(PRUint32 aCount);
  virtual nsresult CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo);
  virtual nsresult BeginTransaction();
  virtual nsresult EndTransaction();

// Selection and navigation -- exposed here for convenience
  virtual nsresult MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection);
  virtual nsresult MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection);
  virtual nsresult MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection);
  virtual nsresult MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  virtual nsresult SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection); 
  virtual nsresult SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  virtual nsresult ScrollUp(nsIAtom *aIncrement);
  virtual nsresult ScrollDown(nsIAtom *aIncrement);
  virtual nsresult ScrollIntoView(PRBool aScrollToBegin);

// Input/Output
  virtual nsresult Insert(nsIInputStream *aInputStream);
  virtual nsresult OutputText(nsIOutputStream *aOutputStream);
  virtual nsresult OutputHTML(nsIOutputStream *aOutputStream);

//=====================================
// HTML Editing methods

// Table Editing (implemented in EditTable.cpp)
  virtual nsresult CreateTxnForInsertTable(const nsIDOMElement *aTableNode, InsertTableTxn ** aTxn);
  virtual nsresult GetColIndexForCell(nsIDOMNode *aCellNode, PRInt32 &aCellIndex);
  virtual nsresult GetRowIndexForCell(nsIDOMNode *aCellNode, PRInt32 &aCellIndex);
  virtual nsresult GetFirstCellInColumn(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aFirstCellNode);
  virtual nsresult GetNextCellInColumn(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aNextCellNode);
  virtual nsresult GetFirstCellInRow(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aCellNode);
  virtual nsresult GetNextCellInRow(nsIDOMNode *aCurrentCellNode, nsIDOMNode* &aNextCellNode);
  virtual nsresult InsertTable();
  virtual nsresult InsertTableCell(PRInt32 aNumber, PRBool aAfter);
  virtual nsresult InsertTableColumn(PRInt32 aNumber, PRBool aAfter);
  virtual nsresult InsertTableRow(PRInt32 aNumber, PRBool aAfter);
  virtual nsresult DeleteTable();
  virtual nsresult DeleteTableCell(PRInt32 aNumber);
  virtual nsresult DeleteTableColumn(PRInt32 aNumber);
  virtual nsresult DeleteTableRow(PRInt32 aNumber);
  virtual nsresult JoinTableCells(PRBool aCellToRight);

// Data members
protected:
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsITextEditor> mTextEditor;

// EVENT LISTENERS AND COMMAND ROUTING NEEDS WORK
// For now, the listners are tied to the nsTextEditor class
//
//  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
//  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;

};

#endif //nsHTMLEditor_h__

