/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL")=0; you may not use this file except in
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

#ifndef nsIHTMLEditor_h__
#define nsIHTMLEditor_h__

#define NS_IHTMLEDITOR_IID \
{/*BD62F311-CB8A-11d2-983A-00805F8AA8B8*/     \
0xbd62f311, 0xcb8a, 0x11d2, \
{ 0x98, 0x3a, 0x0, 0x80, 0x5f, 0x8a, 0xa8, 0xb8 } }


#include "nsIEditor.h"
#include "nscore.h"

class nsIEditorCallback;
class nsISupportsArray;
class nsIAtom;
class nsIInputStream;
class nsIOutputStream;

/**
 * The HTML editor interface. 
 * <P>
 * Use to edit text and other HTML objects represented as a DOM tree. 
 */
class nsIHTMLEditor  : public nsISupports{
public:
  static const nsIID& IID() { static nsIID iid = NS_IHTMLEDITOR_IID; return iid; }

  /** Initialize the text editor 
    *
    */
  virtual nsresult InitHTMLEditor(nsIDOMDocument *aDoc, 
                                  nsIPresShell   *aPresShell,
                                  nsIEditorCallback *aCallback=nsnull)=0;

// Methods shared with nsITextEditor (see nsITextEditor.h for details)
  virtual nsresult SetTextProperties(nsISupportsArray *aPropList)=0;
  virtual nsresult GetTextProperties(nsISupportsArray *aPropList)=0;
  virtual nsresult RemoveTextProperties(nsISupportsArray *aPropList)=0;
  virtual nsresult DeleteSelection(nsIEditor::Direction aDir)=0;
  virtual nsresult InsertText(const nsString& aStringToInsert)=0;
  virtual nsresult InsertBreak(PRBool aCtrlKey)=0;
  virtual nsresult EnableUndo(PRBool aEnable)=0;
  virtual nsresult Undo(PRUint32 aCount)=0;
  virtual nsresult CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)=0;
  virtual nsresult Redo(PRUint32 aCount)=0;
  virtual nsresult CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)=0;
  virtual nsresult BeginTransaction()=0;
  virtual nsresult EndTransaction()=0;
  virtual nsresult MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  virtual nsresult MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  virtual nsresult MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  virtual nsresult MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  virtual nsresult SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection)=0; 
  virtual nsresult SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  virtual nsresult ScrollUp(nsIAtom *aIncrement)=0;
  virtual nsresult ScrollDown(nsIAtom *aIncrement)=0;
  virtual nsresult ScrollIntoView(PRBool aScrollToBegin)=0;

  virtual nsresult Insert(nsIInputStream *aInputStream)=0;
  virtual nsresult OutputText(nsIOutputStream *aOutputStream)=0;
  virtual nsresult OutputHTML(nsIOutputStream *aOutputStream)=0;

// Miscellaneous Methods
  /*
  virtual nsresult CheckSpelling()=0;
  virtual nsresult SpellingLanguage(nsIAtom *aLanguage)=0;
  */
  /* The editor doesn't know anything about specific services like SpellChecking.  
   * Services can be invoked on the content, and these services can use the editor if they choose
   * to get transactioning/undo/redo.
   * For things like auto-spellcheck, the App should implement nsIDocumentObserver and 
   * trigger off of ContentChanged notifications.
   */


// Table editing Methods
  virtual nsresult InsertTable()=0;
  virtual nsresult InsertTableCell(PRInt32 aNumber, PRBool aAfter)=0;
  virtual nsresult InsertTableColumn(PRInt32 aNumber, PRBool aAfter)=0;
  virtual nsresult InsertTableRow(PRInt32 aNumber, PRBool aAfter)=0;
  virtual nsresult DeleteTable()=0;
  virtual nsresult DeleteTableCell(PRInt32 aNumber)=0;
  virtual nsresult DeleteTableColumn(PRInt32 aNumber)=0;
  virtual nsresult DeleteTableRow(PRInt32 aNumber)=0;
  virtual nsresult JoinTableCells(PRBool aCellToRight)=0;
};

#endif //nsIEditor_h__

