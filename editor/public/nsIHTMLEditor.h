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
//#include "nsIDOMDocumentFragment.h"

class nsIEditorCallback;
class nsISupportsArray;
class nsStringArray;
class nsIAtom;
class nsIOutputStream;
class nsIDOMWindow;
class nsIFileSpec;

/**
 * The HTML editor interface. 
 * <P>
 * Use to edit text and other HTML objects represented as a DOM tree. 
 */
class nsIHTMLEditor  : public nsISupports{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IHTMLEDITOR_IID; return iid; }

	typedef enum {eSaveFileText = 0, eSaveFileHTML = 1 } ESaveFileType;

  /** Initialize the HTML editor 
    *
    */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, 
                  nsIPresShell   *aPresShell)=0;

// Methods shared with nsITextEditor (see nsITextEditor.h for details)
  NS_IMETHOD SetTextProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue)=0;
  NS_IMETHOD GetTextProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAll, PRBool &aAny)=0;
  NS_IMETHOD GetParagraphFormat(nsString& aParagraphFormat)=0;
  NS_IMETHOD SetParagraphFormat(const nsString& aParagraphFormat)=0;
  NS_IMETHOD RemoveTextProperty(nsIAtom *aProperty, const nsString *aAttribute)=0;
  NS_IMETHOD DeleteSelection(nsIEditor::ECollapsedSelectionAction aAction)=0;
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
  NS_IMETHOD BeginningOfDocument()=0;
  NS_IMETHOD EndOfDocument()=0;
  NS_IMETHOD ScrollUp(nsIAtom *aIncrement)=0;
  NS_IMETHOD ScrollDown(nsIAtom *aIncrement)=0;
  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin)=0;

  NS_IMETHOD Save()=0;
  NS_IMETHOD SaveAs(PRBool aSavingCopy)=0;

  NS_IMETHOD Cut()=0;
  NS_IMETHOD Copy()=0;
  NS_IMETHOD Paste()=0;
  NS_IMETHOD PasteAsQuotation()=0;
  NS_IMETHOD PasteAsCitedQuotation(const nsString& aCitation)=0;
  NS_IMETHOD InsertAsQuotation(const nsString& aQuotedText)=0;
  NS_IMETHOD InsertAsCitedQuotation(const nsString& aQuotedText, const nsString& aCitation)=0;

  NS_IMETHOD InsertHTML(const nsString &aInputString)=0;

  /**
   * Output methods:
   * aFormatType is a mime type, like text/plain.
   */
  NS_IMETHOD OutputToString(nsString& aOutputString,
                            const nsString& aFormatType,
                            PRUint32 aFlags) = 0;
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsString& aFormatType,
                            const nsString* aCharsetOverride,
                            PRUint32 aFlags) = 0;

  NS_IMETHOD ApplyStyleSheet(const nsString& aURL)=0;

// Miscellaneous Methods
  /** Set the background color of the selected table cell, row, columne, or table,
    * or the document background if not in a table
   */
  NS_IMETHOD SetBackgroundColor(const nsString& aColor)=0;
  /** Set any BODY element attribute
   */
  NS_IMETHOD SetBodyAttribute(const nsString& aAttr, const nsString& aValue)=0;

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

  NS_IMETHOD GetParagraphStyle(nsStringArray *aTagList)=0;

  /** Add a block parent node around the selected content.
    * Only legal nestings are allowed.
    * An example of use is for indenting using blockquote nodes.
    *
    * @param aParentTag  The tag from which the new parent is created.
    */
  NS_IMETHOD AddBlockParent(nsString& aParentTag)=0;

  /** Replace the block parent node around the selected content with a new block
    * parent node of type aParentTag.
    * Only legal replacements are allowed.
    * An example of use are is transforming H1 to LI ("paragraph type transformations").
    * For containing block transformations (transforming UL to OL, for example),
    * the caller should RemoveParent("UL"), set the selection appropriately,
    * and call AddBlockParent("OL").
    *
    * @param aParentTag  The tag from which the new parent is created.
    */
  NS_IMETHOD ReplaceBlockParent(nsString& aParentTag)=0;

  /** remove the paragraph style from the selection */
  NS_IMETHOD RemoveParagraphStyle()=0;

  /** remove block parent of type aTagToRemove from the selection.
    * if aTagToRemove is null, the nearest enclosing block that 
    * is <B>not</B> a root is removed.
    */
  NS_IMETHOD RemoveParent(const nsString &aParentTag)=0;

  NS_IMETHOD InsertLink(nsString& aURL)=0;
  NS_IMETHOD InsertImage(nsString& aURL,
                         nsString& aWidth, nsString& aHeight,
                         nsString& aHspace, nsString& aVspace,
                         nsString& aBorder,
                         nsString& aAlt, nsString& aAlignment)=0;

  NS_IMETHOD InsertList(const nsString& aListType)=0;
  NS_IMETHOD Indent(const nsString& aIndent)=0;
  NS_IMETHOD Align(const nsString& aAlign)=0;

  NS_IMETHOD GetElementOrParentByTagName(const nsString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)=0;
  NS_IMETHOD GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)=0;
  NS_IMETHOD CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)=0;
  NS_IMETHOD InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection)=0;
  NS_IMETHOD SaveHLineSettings(nsIDOMElement* aElement)=0;
  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)=0;
  NS_IMETHOD SelectElement(nsIDOMElement* aElement)=0;
  NS_IMETHOD SetCaretAfterElement(nsIDOMElement* aElement)=0;

// MHTML helper methods
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray** aNodeList)=0;

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
  NS_IMETHOD GetRowIndex(nsIDOMElement *aCell, PRInt32 &aRowIndex)=0;
  NS_IMETHOD GetColumnIndex(nsIDOMElement *aCell, PRInt32 &aColIndex)=0;
  NS_IMETHOD GetColumnCellCount(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32& aCount)=0;
  NS_IMETHOD GetRowCellCount(nsIDOMElement* aTable, PRInt32 aColIndex, PRInt32& aCount)=0;
  NS_IMETHOD GetMaxColumnCellCount(nsIDOMElement* aTable, PRInt32& aCount)=0;
  NS_IMETHOD GetMaxRowCellCount(nsIDOMElement* aTable, PRInt32& aCount)=0;
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell)=0;
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell, 
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, PRInt32& aRowSpan, PRInt32& aColSpan)=0;

// IME editing Methods
  NS_IMETHOD BeginComposition(void)=0;
  NS_IMETHOD SetCompositionString(const nsString& aCompositionString, nsIDOMTextRangeList* aTextRangeList)=0;
  NS_IMETHOD EndComposition(void)=0;


  // Logging Methods
  NS_IMETHOD StartLogging(nsIFileSpec *aLogFile)=0;
  NS_IMETHOD StopLogging()=0;
};

#endif //nsIEditor_h__

