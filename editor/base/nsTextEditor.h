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

class nsISupportsArray;


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
  NS_IMETHOD InitTextEditor(nsIDOMDocument *aDoc, 
                            nsIPresShell   *aPresShell,
                            nsIEditorCallback *aCallback=nsnull);
  virtual ~nsTextEditor();

// Editing Operations
  NS_IMETHOD SetTextProperties(nsISupportsArray *aPropList);
  NS_IMETHOD GetTextProperties(nsISupportsArray *aPropList);
  NS_IMETHOD RemoveTextProperties(nsISupportsArray *aPropList);
  NS_IMETHOD DeleteSelection(nsIEditor::Direction aDir);
  NS_IMETHOD InsertText(const nsString& aStringToInsert);
  NS_IMETHOD InsertBreak(PRBool aCtrlKey);

// Transaction control
  NS_IMETHOD EnableUndo(PRBool aEnable);
  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo);
  NS_IMETHOD Redo(PRUint32 aCount);
  NS_IMETHOD CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo);
  NS_IMETHOD BeginTransaction();
  NS_IMETHOD EndTransaction();

// Selection and navigation -- exposed here for convenience
  NS_IMETHOD MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection); 
  NS_IMETHOD SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD ScrollUp(nsIAtom *aIncrement);
  NS_IMETHOD ScrollDown(nsIAtom *aIncrement);
  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin);

// Input/Output
  NS_IMETHOD Insert(nsIInputStream *aInputStream);
  NS_IMETHOD OutputText(nsIOutputStream *aOutputStream);
  NS_IMETHOD OutputHTML(nsIOutputStream *aOutputStream);

protected:
// Utility Methods
  NS_IMETHOD SetTextPropertiesForNode(nsIDOMNode *aNode, 
                                      nsIDOMNode *aParent,
                                      PRInt32     aStartOffset,
                                      PRInt32     aEndOffset,
                                      nsIAtom    *aPropName);

  NS_IMETHOD SetTextPropertiesForNodesWithSameParent(nsIDOMNode *aStartNode,
                                                     PRInt32     aStartOffset,
                                                     nsIDOMNode *aEndNode,
                                                     PRInt32     aEndOffset,
                                                     nsIDOMNode *aParent,
                                                     nsIAtom    *aPropName);

  NS_IMETHOD SetTextPropertiesForNodeWithDifferentParents(nsIDOMNode *aStartNode,
                                                          PRInt32     aStartOffset,
                                                          nsIDOMNode *aEndNode,
                                                          PRInt32     aEndOffset,
                                                          nsIDOMNode *aParent,
                                                          nsIAtom    *aPropName);

  NS_IMETHOD SetTagFromProperty(nsAutoString &aTag, nsIAtom *aPropName) const;

// Data members
protected:
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;

};

#endif //nsTextEditor_h__

