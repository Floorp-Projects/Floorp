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

#ifndef nsTextEditor_h__
#define nsTextEditor_h__

#include "nsITextEditor.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"


/**
 * The text editor implementation.<br>
 * Use to edit text represented as a DOM tree. 
 * This class is used for editing both plain text and rich text (attributed text).
 */
class nsTextEditor  : public nsITextEditor
{
public:
  // see nsITextEditor for documentation

//Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

//Initialization
  nsTextEditor();
  virtual nsresult InitTextEditor(nsIDOMDocument *aDoc, 
                                  nsIPresShell   *aPresShell,
                                  nsIEditorCallback *aCallback=nsnull);
  virtual ~nsTextEditor();

// Editing Operations
  virtual nsresult SetTextProperties(nsVoidArray *aPropList);
  virtual nsresult GetTextProperties(nsVoidArray *aPropList);
  virtual nsresult RemoveTextProperties(nsVoidArray *aPropList);
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

protected:
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;

};

#endif //nsTextEditor_h__

