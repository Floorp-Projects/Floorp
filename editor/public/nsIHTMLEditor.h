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
#include "nsIDOMDocumentFragment.h"

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
  static const nsIID& GetIID() { static nsIID iid = NS_IHTMLEDITOR_IID; return iid; }

  /** Initialize the text editor 
    *
    */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell )=0;

// Methods shared with nsITextEditor (see nsITextEditor.h for details)
  NS_IMETHOD SetTextProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue)=0;
  NS_IMETHOD GetTextProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAll, PRBool &aAny)=0;
  NS_IMETHOD RemoveTextProperty(nsIAtom *aProperty, const nsString *aAttribute)=0;
  NS_IMETHOD DeleteSelection(nsIEditor::Direction aDir)=0;
  NS_IMETHOD InsertText(const nsString& aStringToInsert)=0;
  NS_IMETHOD InsertBreak()=0;
  NS_IMETHOD EnableUndo(PRBool aEnable)=0;
  NS_IMETHOD Undo(PRUint32 aCount)=0;
  NS_IMETHOD CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)=0;
  NS_IMETHOD Redo(PRUint32 aCount)=0;
  NS_IMETHOD CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)=0;
  NS_IMETHOD BeginTransaction()=0;
  NS_IMETHOD EndTransaction()=0;
  NS_IMETHOD MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  NS_IMETHOD MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  NS_IMETHOD MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  NS_IMETHOD MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  NS_IMETHOD SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection)=0; 
  NS_IMETHOD SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)=0;
  NS_IMETHOD SelectAll()=0;
  NS_IMETHOD ScrollUp(nsIAtom *aIncrement)=0;
  NS_IMETHOD ScrollDown(nsIAtom *aIncrement)=0;
  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin)=0;
  NS_IMETHOD Cut()=0;
  NS_IMETHOD Copy()=0;
  NS_IMETHOD Paste()=0;

  NS_IMETHOD Insert(nsString &aInputString)=0;
  NS_IMETHOD OutputText(nsString& aOutputString)=0;
  NS_IMETHOD OutputHTML(nsString& aOutputString)=0;

// Miscellaneous Methods
  /*
  NS_IMETHOD CheckSpelling()=0;
  NS_IMETHOD SpellingLanguage(nsIAtom *aLanguage)=0;
  */
  /* The editor doesn't know anything about specific services like SpellChecking.  
   * Services can be invoked on the content, and these services can use the editor if they choose
   * to get transactioning/undo/redo.
   * For things like auto-spellcheck, the App should implement nsIDocumentObserver and 
   * trigger off of ContentChanged notifications.
   */

  /** Add a block parent node around the selected content.
    * If the selected content already has a block parent, it is changed to  the type given by aParentTag
    * @param aParentTag  The tag from which the new parent is created.
    */
  NS_IMETHOD AddBlockParent(nsString& aParentTag)=0;

  /** Remove the most deeply nested block parent node from around the selected content.
    * @param aParentTag  The tag from which the new parent is created.
    */
  NS_IMETHOD RemoveBlockParent()=0;

  NS_IMETHOD InsertLink(nsString& aURL)=0;
  NS_IMETHOD InsertImage(nsString& aURL,
                         nsString& aWidth, nsString& aHeight,
                         nsString& aHspace, nsString& aVspace,
                         nsString& aBorder,
                         nsString& aAlt, nsString& aAlignment)=0;

  // This should replace InsertLink and InsertImage once it is working
  NS_IMETHOD GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)=0;
  NS_IMETHOD CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)=0;
  NS_IMETHOD InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection, nsIDOMElement** aReturn)=0;

// Table editing Methods
  NS_IMETHOD InsertTable()=0;
  NS_IMETHOD InsertTableCell(PRInt32 aNumber, PRBool aAfter)=0;
  NS_IMETHOD InsertTableColumn(PRInt32 aNumber, PRBool aAfter)=0;
  NS_IMETHOD InsertTableRow(PRInt32 aNumber, PRBool aAfter)=0;
  NS_IMETHOD DeleteTable()=0;
  NS_IMETHOD DeleteTableCell(PRInt32 aNumber)=0;
  NS_IMETHOD DeleteTableColumn(PRInt32 aNumber)=0;
  NS_IMETHOD DeleteTableRow(PRInt32 aNumber)=0;
  NS_IMETHOD JoinTableCells(PRBool aCellToRight)=0;
};

#endif //nsIEditor_h__

