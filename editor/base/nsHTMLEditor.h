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

#include "nsCOMPtr.h"

#include "nsIHTMLEditor.h"
#include "nsITableEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsIEditorStyleSheets.h"

#include "nsEditor.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsITableLayout.h"

#include "TypeInState.h"
#include "nsEditRules.h"

/**
 * The HTML editor implementation.<br>
 * Use to edit HTML document represented as a DOM tree. 
 */
class nsHTMLEditor : public nsEditor,
                     public nsIHTMLEditor,
                     public nsIEditorMailSupport,
                     public nsITableEditor,
                     public nsIEditorStyleSheets
{

  typedef enum {eNoOp, eReplaceParent=1, eInsertParent=2} BlockTransformationType;

public:
  // see nsIHTMLEditor for documentation

//Interfaces for addref and release and queryinterface
//NOTE macro used is for classes that inherit from 
// another class. Only the base class should use NS_DECL_ISUPPORTS
  NS_DECL_ISUPPORTS_INHERITED

           nsHTMLEditor();
  virtual  ~nsHTMLEditor();

  /* ------------ nsIHTMLEditor methods -------------- */

  NS_IMETHOD GetDocumentLength(PRInt32 *aCount);
  NS_IMETHOD SetMaxTextLength(PRInt32 aMaxTextLength);
  NS_IMETHOD GetMaxTextLength(PRInt32& aMaxTextLength);

  NS_IMETHOD SetInlineProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue);

  NS_IMETHOD GetInlineProperty(nsIAtom *aProperty, 
                             const nsString *aAttribute,
                             const nsString *aValue,
                             PRBool &aFirst, PRBool &aAny, PRBool &aAll);

  NS_IMETHOD RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute);

  NS_IMETHOD InsertBreak();
  NS_IMETHOD InsertText(const nsString& aStringToInsert);
  NS_IMETHOD InsertHTML(const nsString &aInputString);
  NS_IMETHOD InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection);
  
  NS_IMETHOD DeleteSelection(ESelectionCollapseDirection aAction);
  NS_IMETHOD DeleteSelectionAndCreateNode(const nsString& aTag, nsIDOMNode ** aNewNode);
  NS_IMETHOD SelectElement(nsIDOMElement* aElement);
  NS_IMETHOD SetCaretAfterElement(nsIDOMElement* aElement);

  NS_IMETHOD GetParagraphFormat(nsString& aParagraphFormat);
  NS_IMETHOD SetParagraphFormat(const nsString& aParagraphFormat);

  NS_IMETHOD GetParagraphStyle(nsStringArray *aTagList);
  NS_IMETHOD RemoveParagraphStyle();

  NS_IMETHOD AddBlockParent(nsString& aParentTag);
  NS_IMETHOD ReplaceBlockParent(nsString& aParentTag);
  NS_IMETHOD RemoveParent(const nsString &aParentTag);

  NS_IMETHOD InsertList(const nsString& aListType);
  NS_IMETHOD InsertBasicBlock(const nsString& aBlockType);
  NS_IMETHOD Indent(const nsString& aIndent);
  NS_IMETHOD Align(const nsString& aAlign);

  NS_IMETHOD GetElementOrParentByTagName(const nsString& aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn);
  NS_IMETHOD GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn);
  NS_IMETHOD CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn);

  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);


  /* ------------ nsIEditorStyleSheets methods -------------- */

  NS_IMETHOD ApplyStyleSheet(const nsString& aURL);
  NS_IMETHOD AddStyleSheet(nsICSSStyleSheet* aSheet);
  NS_IMETHOD RemoveStyleSheet(nsICSSStyleSheet* aSheet);

  /* ------------ nsIEditorMailSupport methods -------------- */

  NS_IMETHOD GetBodyWrapWidth(PRInt32 *aWrapColumn);
  NS_IMETHOD SetBodyWrapWidth(PRInt32 aWrapColumn);
  NS_IMETHOD PasteAsQuotation();
  NS_IMETHOD InsertAsQuotation(const nsString& aQuotedText);
  NS_IMETHOD PasteAsCitedQuotation(const nsString& aCitation);
  NS_IMETHOD InsertAsCitedQuotation(const nsString& aQuotedText, const nsString& aCitation);
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray** aNodeList);


  /* ------------ nsITableEditor methods -------------- */

  NS_IMETHOD InsertTableCell(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableColumn(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD InsertTableRow(PRInt32 aNumber, PRBool aAfter);
  NS_IMETHOD DeleteTable();
  NS_IMETHOD DeleteTableCell(PRInt32 aNumber);
  NS_IMETHOD DeleteTableColumn(PRInt32 aNumber);
  NS_IMETHOD DeleteTableRow(PRInt32 aNumber);
  NS_IMETHOD JoinTableCells();
  NS_IMETHOD NormalizeTable(nsIDOMElement *aTable);
  NS_IMETHOD GetCellIndexes(nsIDOMElement *aCell, PRInt32& aRowIndex, PRInt32& aColIndex);
  NS_IMETHOD GetTableSize(nsIDOMElement *aTable, PRInt32& aRowCount, PRInt32& aColCount);
  NS_IMETHOD GetCellAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell);
  NS_IMETHOD GetCellDataAt(nsIDOMElement* aTable, PRInt32 aRowIndex, PRInt32 aColIndex, nsIDOMElement* &aCell,
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex,
                           PRInt32& aRowSpan, PRInt32& aColSpan, PRBool& aIsSelected);


// Selection and navigation
  /* obsolete
  NS_IMETHOD MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection); 
  NS_IMETHOD SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection);
  NS_IMETHOD ScrollUp(nsIAtom *aIncrement);
  NS_IMETHOD ScrollDown(nsIAtom *aIncrement);
  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin);
  */

  /* miscellaneous */
  // This sets background on the appropriate container element (table, cell,)
  //   or calls into nsTextEditor to set the page background
  NS_IMETHOD SetBackgroundColor(const nsString& aColor);
  NS_IMETHOD SetBodyAttribute(const nsString& aAttr, const nsString& aValue);



  /* ------------ Overrides of nsEditor interface methods -------------- */

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell, PRUint32 aFlags);
  
  /** we override this here to install event listeners */
  NS_IMETHOD PostCreate();

  NS_IMETHOD GetFlags(PRUint32 *aFlags);
  NS_IMETHOD SetFlags(PRUint32 aFlags);

  NS_IMETHOD Cut();
  NS_IMETHOD Copy();
  NS_IMETHOD Paste();

  NS_IMETHOD OutputToString(nsString& aOutputString,
                            const nsString& aFormatType,
                            PRUint32 aFlags);
                            
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsString& aFormatType,
                            const nsString* aCharsetOverride,
                            PRUint32 aFlags);

  NS_IMETHOD DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed);


  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD GetBodyStyleContext(nsIStyleContext** aStyleContext);

protected:

  virtual void  InitRules();

  /** install the event listeners for the editor 
    * used to be part of Init, but now broken out into a separate method
    * called by PostCreate, giving the caller the chance to interpose
    * their own listeners before we install our own backstops.
    */
  NS_IMETHOD InstallEventListeners();

  /** returns the layout object (nsIFrame in the real world) for aNode
    * @param aNode          the content to get a frame for
    * @param aLayoutObject  the "primary frame" for aNode, if one exists.  May be null
    * @return NS_OK whether a frame is found or not
    *         an error if some serious error occurs
    */
  NS_IMETHOD GetLayoutObject(nsIDOMNode *aInNode, nsISupports **aOutLayoutObject);

  NS_IMETHOD CollapseAdjacentTextNodes(nsIDOMSelection *aInSelection);

  NS_IMETHOD DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, PRInt32& offsetOfNewNode);

  /* StyleSheet load callback */
  static void ApplyStyleSheetToPresShellDocument(nsICSSStyleSheet* aSheet, void *aData);

  /* remove the old style sheet, and apply the supplied one */
  NS_IMETHOD ReplaceStyleSheet(nsICSSStyleSheet *aNewSheet);


  // Return TRUE if aElement is a table-related elemet and caret was set
  PRBool     SetCaretInTableCell(nsIDOMElement* aElement);
  PRBool     IsElementInBody(nsIDOMElement* aElement);

// Table Editing (implemented in EditTable.cpp)

  // Helper used to get nsITableLayout interface for methods implemented in nsTableFrame
  NS_IMETHOD GetTableLayoutObject(nsIDOMElement* aTable, nsITableLayout **tableLayoutObject);

  // Table utilities
  
  // All of the above need to get the same basic context data
  NS_IMETHOD GetCellContext(nsCOMPtr<nsIDOMSelection> &aSelection,
                            nsCOMPtr<nsIDOMElement> &aTable, nsCOMPtr<nsIDOMElement> &aCell, 
                            nsCOMPtr<nsIDOMNode> &aCellParent, PRInt32& aCellOffset, 
                            PRInt32& aRow, PRInt32& aCol);

  // Use the selection iterator to find the first cell in the selection
  NS_IMETHOD GetFirstSelectedCell(nsCOMPtr<nsIDOMElement> &aCell);

  // Setting caret to a logical place can get tricky,
  //  especially after deleting table stuff
  typedef enum { ePreviousColumn, ePreviousRow } SetCaretSearchDirection;
  
  NS_IMETHOD SetCaretAfterTableEdit(nsIDOMElement* aTable, PRInt32 aCol, PRInt32 aRow, SetCaretSearchDirection aDirection);
    
  NS_IMETHOD ReParentContentOfNode(nsIDOMNode *aNode, 
                                   nsString   &aParentTag,
                                   BlockTransformationType aTranformation);

  NS_IMETHOD ReParentBlockContent(nsIDOMNode  *aNode, 
                                  nsString    &aParentTag,
                                  nsIDOMNode  *aBlockParentNode,
                                  nsString    &aBlockParentTag,
                                  BlockTransformationType aTranformation,
                                  nsIDOMNode **aNewParentNode);
  
  NS_IMETHOD ReParentContentOfRange(nsIDOMRange *aRange, 
                                    nsString    &aParentTag,
                                    BlockTransformationType aTranformation);

  NS_IMETHOD RemoveParagraphStyleFromRange(nsIDOMRange *aRange);
  
  NS_IMETHOD RemoveParagraphStyleFromBlockContent(nsIDOMRange *aRange);

  NS_IMETHOD RemoveParentFromRange(const nsString &aParentTag, nsIDOMRange *aRange);
  
  NS_IMETHOD RemoveParentFromBlockContent(const nsString &aParentTag, nsIDOMRange *aRange);

  NS_IMETHOD IsRootTag(nsString &aTag, PRBool &aIsTag);

  NS_IMETHOD IsSubordinateBlock(nsString &aTag, PRBool &aIsTag);


  /** content-based query returns PR_TRUE if <aProperty aAttribute=aValue> effects aNode
    * If <aProperty aAttribute=aValue> contains aNode, 
    * but <aProperty aAttribute=SomeOtherValue> also contains aNode and the second is
    * more deeply nested than the first, then the first does not effect aNode.
    *
    * @param aNode      The target of the query
    * @param aProperty  The property that we are querying for
    * @param aAttribute The attribute of aProperty, example: color in <FONT color="blue">
    *                   May be null.
    * @param aValue     The value of aAttribute, example: blue in <FONT color="blue">
    *                   May be null.  Ignored if aAttribute is null.
    * @param aIsSet     [OUT] PR_TRUE if <aProperty aAttribute=aValue> effects aNode.
    * @param aStyleNode [OUT] set to the node representing <aProperty aAttribute=aValue>, if found.
    *                   null if aIsSet is returned as PR_FALSE;
    */
  virtual void IsTextPropertySetByContent(nsIDOMNode     *aNode,
                                          nsIAtom        *aProperty, 
                                          const nsString *aAttribute,
                                          const nsString *aValue,
                                          PRBool         &aIsSet,
                                          nsIDOMNode    **aStyleNode) const;

  /** style-based query returns PR_TRUE if (aProperty, aAttribute) is set in aSC.
    * WARNING: not well tested yet since we don't do style-based queries anywhere.
    */
  virtual void IsTextStyleSet(nsIStyleContext *aSC, 
                              nsIAtom         *aProperty, 
                              const nsString  *aAttributes, 
                              PRBool          &aIsSet) const;

  /** Moves the content between (aNode, aStartOffset) and (aNode, aEndOffset)
    * into aNewParentNode, splitting aNode as necessary to maintain the relative
    * position of all leaf content.
    * @param aNode          The node whose content we're repositioning.
    *                       aNode can be either a text node or a container node.
    * @param aNewParentNode The node that will be the repositioned contents' parent.
    *                       The caller is responsible for allocating aNewParentNode
    * @param aStartOffset   The start offset of the content of aNode
    * @param aEndOffset     The end offset of the content of aNode.
    */
  NS_IMETHOD MoveContentOfNodeIntoNewParent(nsIDOMNode  *aNode, 
                                            nsIDOMNode  *aNewParentNode, 
                                            PRInt32      aStartOffset, 
                                            PRInt32      aEndOffset);

  /** Moves the content between (aStartNode, aStartOffset) and (aEndNode, aEndOffset)
    * into aNewParentNode, splitting aStartNode and aEndNode as necessary to maintain 
    * the relative position of all leaf content.
    * The content between the two endpoints MUST be "contiguous" in the sense that 
    * it is all in the same block.  Another way of saying this is all content nodes
    * between aStartNode and aEndNode must be inline. 
    * @see IntermediateNodesAreInline
    *
    * @param aStartNode       The left node,  can be either a text node or a container node.
    * @param aStartOffset     The start offset in the content of aStartNode
    * @param aEndNode         The right node,  can be either a text node or a container node.
    * @param aEndOffset       The end offset in the content of aEndNode.
    * @param aGrandParentNode The common ancestor of aStartNode and aEndNode.
    *                         aGrandParentNode will be the parent of aNewParentNode.
    * @param aNewParentNode   The node that will be the repositioned contents' parent.
    *                         The caller is responsible for allocating aNewParentNode
    */
  NS_IMETHOD MoveContiguousContentIntoNewParent(nsIDOMNode *aStartNode, 
                                                PRInt32     aStartOffset, 
                                                nsIDOMNode *aEndNode, 
                                                PRInt32     aEndOffset, 
                                                nsIDOMNode *aGrandParentNode,
                                                nsIDOMNode *aNewParentNode);


  NS_IMETHOD SetTextPropertiesForNode(nsIDOMNode  *aNode, 
                                      nsIDOMNode  *aParent,
                                      PRInt32      aStartOffset,
                                      PRInt32      aEndOffset,
                                      nsIAtom     *aPropName,
                                      const nsString *aAttribute,
                                      const nsString *aValue);

  NS_IMETHOD SetTextPropertiesForNodesWithSameParent(nsIDOMNode  *aStartNode,
                                                     PRInt32      aStartOffset,
                                                     nsIDOMNode  *aEndNode,
                                                     PRInt32      aEndOffset,
                                                     nsIDOMNode  *aParent,
                                                     nsIAtom     *aPropName,
                                                     const nsString *aAttribute,
                                                     const nsString *aValue);

  NS_IMETHOD SetTextPropertiesForNodeWithDifferentParents(nsIDOMRange *aRange,
                                                          nsIDOMNode  *aStartNode,
                                                          PRInt32      aStartOffset,
                                                          nsIDOMNode  *aEndNode,
                                                          PRInt32      aEndOffset,
                                                          nsIDOMNode  *aParent,
                                                          nsIAtom     *aPropName,
                                                          const nsString *aAttribute,
                                                          const nsString *aValue);

  NS_IMETHOD RemoveTextPropertiesForNode(nsIDOMNode  *aNode, 
                                         nsIDOMNode  *aParent,
                                         PRInt32      aStartOffset,
                                         PRInt32      aEndOffset,
                                         nsIAtom     *aPropName,
                                         const nsString *aAttribute);

  NS_IMETHOD RemoveTextPropertiesForNodesWithSameParent(nsIDOMNode  *aStartNode,
                                                        PRInt32      aStartOffset,
                                                        nsIDOMNode  *aEndNode,
                                                        PRInt32      aEndOffset,
                                                        nsIDOMNode  *aParent,
                                                        nsIAtom     *aPropName, 
                                                        const nsString *aAttribute);

  NS_IMETHOD RemoveTextPropertiesForNodeWithDifferentParents(nsIDOMNode  *aStartNode,
                                                             PRInt32      aStartOffset,
                                                             nsIDOMNode  *aEndNode,
                                                             PRInt32      aEndOffset,
                                                             nsIDOMNode  *aParent,
                                                             nsIAtom     *aPropName,
                                                             const nsString *aAttribute);

  NS_IMETHOD SetTypeInStateForProperty(TypeInState &aTypeInState, 
                                       nsIAtom     *aPropName, 
                                       const nsString *aAttribute,
                                       const nsString *aValue);

  NS_IMETHOD GetTextSelectionOffsetsForRange(nsIDOMSelection *aSelection, 
                                             nsIDOMNode **aParent,
                                             PRInt32     &aStartOffset, 
                                             PRInt32     &aEndOffset);

  void ResetTextSelectionForRange(nsIDOMNode *aParent,
                                  PRInt32     aStartOffset,
                                  PRInt32     aEndOffset,
                                  nsIDOMSelection *aSelection);

  /** returns the absolute position of the end points of aSelection
    * in the document as a text stream.
    */
  nsresult GetTextSelectionOffsets(nsIDOMSelection *aSelection,
                                   PRInt32 &aStartOffset, 
                                   PRInt32 &aEndOffset);

  // Methods for handling plaintext quotations
  NS_IMETHOD PasteAsPlaintextQuotation();
  NS_IMETHOD InsertAsPlaintextQuotation(const nsString& aQuotedText);

  TypeInState *GetTypeInState(); 

  /** simple utility to handle any error with event listener allocation or registration */
  void HandleEventListenerError();

// Data members
protected:

  TypeInState*     mTypeInState;
  nsEditRules*     mRules;
  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;
  nsCOMPtr<nsIDOMEventListener> mTextListenerP;
  nsCOMPtr<nsIDOMEventListener> mCompositionListenerP;
  nsCOMPtr<nsIDOMEventListener> mDragListenerP;
  nsCOMPtr<nsIDOMEventListener> mFocusListenerP;
  PRBool 	mIsComposing;
  PRInt32 mMaxTextLength;

// friends
friend class nsHTMLEditRules;
friend class nsTextEditRules;

};

#endif //nsHTMLEditor_h__

