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

#ifndef nsITextEditor_h__
#define nsITextEditor_h__

#define NS_ITEXTEDITOR_IID \
{/* afe65c90-bb82-11d2-86d8-000064657374*/     \
0xafe65c90, 0xbb82, 0x11d2, \
{0x8f, 0xd8, 0x0, 0x00, 0x64, 0x65, 0x73, 0x74} }

#include "nsIEditor.h"
#include "nscore.h"

class nsIEditorCallback;
class nsISupportsArray;
class nsIAtom;
class nsIInputStream;
//class nsIOutputStream;
class nsString;

/**
 * The general text editor interface. 
 * <P>
 * Use to edit text represented as a DOM tree. 
 * This interface is used for both plain text and rich text (attributed text).
 * Different types of text fields are instantiated as a result of installing the
 * proper GUI Manager and Edit Rules.  For example, 
 * a single line plain text editor is instantiated by using the SingleLinePlainTextGUIManager 
 * to limit UI and the SingleLinePlainTextEditRules to filter input and output.
 */
class nsITextEditor  : public nsISupports{
public:
  typedef enum {ePlainText=0, eRichText=1} TextType;
  typedef enum {eSingleLine=0, eMultipleLines=1, ePassword=2} EditorType;

  static const nsIID& GetIID() { static nsIID iid = NS_ITEXTEDITOR_IID; return iid; }

  /** Initialize the text editor 
    *
    */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell)=0;

  /**
   * SetTextProperties() sets the aggregate properties on the current selection
   *
   * @param aPropList  a list of properties to set across the selection 
   * NOTE: this method is experimental, expect it to change.
   */
  NS_IMETHOD SetTextProperty(nsIAtom *aProperty)=0;

  /**
   * GetTextProperties() gets the aggregate properties of the current selection.
   * All object in the current selection are scanned and their attributes are
   * represented in a list of Property object.
   *
   * @param aPropList A list of the various properties that are applied to the text content 
   *                  in the current selection.  Each item in aPropList is an nsEditProperty.
   * NOTE: this method is experimental, expect it to change.
   */
  NS_IMETHOD GetTextProperty(nsIAtom *aProperty, PRBool &aFirst, PRBool &aAny, PRBool &aAll)=0;

  /**
   * RemoveTextProperties() deletes the properties from all text in the current selection.
   * If aProperty is not set on the selection, nothing is done.
   *
   * @param aElement      the content element to operate on
   * @param aAttribute    the string representation of the attribute to get
   */
  NS_IMETHOD RemoveTextProperty(nsIAtom *aProperty)=0;

  /** 
   * DeleteSelection removes all nodes in the current selection.
   * @param aDir  if eLTR, delete to the right (for example, the DEL key)
   *              if eRTL, delete to the left (for example, the BACKSPACE key)
   */
  NS_IMETHOD DeleteSelection(nsIEditor::Direction aDir)=0;

  /**
   * InsertText() Inserts a string at the current location, given by the selection.
   * If the selection is not collapsed, the selection is deleted and the insertion
   * takes place at the resulting collapsed selection.
   * It is expected that this would be used for Paste.
   *
   * NOTE: what happens if the string contains a CR?
   *
   * @param aString   the string to be inserted
   */
  NS_IMETHOD InsertText(const nsString& aStringToInsert)=0;

  /**
   * The handler for the ENTER key.
   * @see nsIEditor::InsertBreak
   */
  NS_IMETHOD InsertBreak()=0;

  /** turn the undo system on or off
    * @param aEnable  if PR_TRUE, the undo system is turned on if it is available
    *                 if PR_FALSE the undo system is turned off if it was previously on
    * @return         if aEnable is PR_TRUE, returns NS_OK if the undo system could be initialized properly
    *                 if aEnable is PR_FALSE, returns NS_OK.
    */
  NS_IMETHOD EnableUndo(PRBool aEnable)=0;

  /** Undo reverses the effects of the last Do operation, if Undo is enabled in the editor.
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to undo and the result of
    * that undo is returned.  
    * Otherwise, the Undo request is ignored and an error NS_ERROR_NOT_AVAILABLE is returned.
    *
    */
  NS_IMETHOD Undo(PRUint32 aCount)=0;

  /** returns state information about the undo system.
    * @param aIsEnabled [OUT] PR_TRUE if undo is enabled
    * @param aCanUndo   [OUT] PR_TRUE if at least one transaction is currently ready to be undone.
    */
  NS_IMETHOD CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)=0;

  /** Redo reverses the effects of the last Undo operation
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to redo and the result of the previously undone
    * transaction is reapplied to the document.
    * If no transaction is available for Redo, or if the document has no transaction manager,
    * the Redo request is ignored and an error NS_ERROR_NOT_AVAILABLE is returned.
    *
    */
  NS_IMETHOD Redo(PRUint32 aCount)=0;

  /** returns state information about the redo system.
    * @param aIsEnabled [OUT] PR_TRUE if redo is enabled
    * @param aCanRedo   [OUT] PR_TRUE if at least one transaction is currently ready to be redone.
    */
  NS_IMETHOD CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)=0;

  /** BeginTransaction is a signal to the editor that the caller will execute multiple updates
    * to the content tree that should be treated as a single operation, 
    * in the most efficient way possible.<br>
    * All transactions executed between a call to BeginTransaction and EndTransaction 
    * will be undoable as an atomic action.<br>
    * EndTransaction must be called after BeginTransaction.<br>
    * Calls to BeginTransaction can be nested, as long as EndTransaction is called once per BeginTransaction.
    */
  NS_IMETHOD BeginTransaction()=0;

  /** EndTransaction is a signal to the editor that the caller is finished updating the content model.
    * BeginTransaction must be called before EndTransaction is called.
    * Calls to BeginTransaction can be nested, as long as EndTransaction is called once per BeginTransaction.
    */
  NS_IMETHOD EndTransaction()=0;

/* the following methods are the convenience methods to make text editing easy for the embedding App
 * all simple getter and setter methods use Get/Set/RemoveProperties
 * we're looking for a balance between convenience and API bloat, since many of these actions can be
 * achieved in other ways.
 */

// Selection and navigation -- exposed here for convenience

  /** move the selection up (towards the beginning of the document.)
    * @param aIncrement  the amount to move the Selection 
    *                    legal values are nsIEditor::Line, nsIEditor::Page
    */
  NS_IMETHOD MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)=0;

  /** move the selection down (towards the end of the document.)
    * @param aIncrement  the amount to move the Selection 
    *                    legal values are nsIEditor::Line, nsIEditor::Page
    */
  NS_IMETHOD MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)=0;

  /** move the selection by some increment.
    * The dir attribute for the document is used to decide if "next" is LTR or RTL
    * @param aIncrement  the amount to move the Selection 
    *                    legal values are nsIEditor::Word, nsIEditor::Sentence, nsIEditor::Paragraph
    * @param aExtendSelection
    */
  NS_IMETHOD MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)=0;

  NS_IMETHOD MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)=0;

  /** select the next portion of the document
    * The dir attribute for the document is used to decide if "next" is LTR or RTL
    * @param aType  the kind of selection to create.
    *               legal values are nsIEditor::Word, nsIEditor::Sentence, nsIEditor::Paragraph
    *                                nsIEditor::Document
    * @param aExtendSelection
    */
  NS_IMETHOD SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection)=0; 

  /** select the previous portion of the document
    * The dir attribute for the document is used to decide if "next" is LTR or RTL
    * @param aType  the kind of selection to create.
    *               legal values are nsIEditor::Word, nsIEditor::Sentence, nsIEditor::Paragraph
    *                                nsIEditor::Document
    * @param aExtendSelection
    */
  NS_IMETHOD SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)=0;

  /** select the entire contents of the document */
  NS_IMETHOD SelectAll()=0;

  /** cut the currently selected text, putting it into the OS clipboard
    * What if no text is selected?
    * What about mixed selections?
    * What are the clipboard formats?
   */
  NS_IMETHOD Cut()=0;

  /** copy the currently selected text, putting it into the OS clipboard
    * What if no text is selected?
    * What about mixed selections?
    * What are the clipboard formats?
   */
  NS_IMETHOD Copy()=0;
  
  /** paste the text in the OS clipboard at the cursor position, replacing
    * the selected text (if any)
   */
  NS_IMETHOD Paste()=0;
  
  /** scroll the viewport up (towards the beginning of the document.)
    * @param aIncrement  the amount to scroll
    *                    legal values are nsIEditor::Line, nsIEditor::Page
    */
  NS_IMETHOD ScrollUp(nsIAtom *aIncrement)=0;
  
  /** scroll the viewport down (towards the end of the document.)
    * @param aIncrement  the amount to scroll
    *                    legal values are nsIEditor::Line, nsIEditor::Page
    */
  NS_IMETHOD ScrollDown(nsIAtom *aIncrement)=0;

  /** scroll the viewport so the selection is in view.
    * @param aScrollToBegin  PR_TRUE if the beginning of the selection is to be scrolled into view.
    *                        PR_FALSE if the end of the selection is to be scrolled into view
    */
  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin)=0;

// Input/Output
  // nsString will be a stream, as soon as I can figure out what kind of stream we should be using
  NS_IMETHOD Insert(nsString& aInputString)=0;
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

};

#endif //nsIEditor_h__

