/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#ifndef __editor_h__
#define __editor_h__

#include "prmon.h"
#include "nsIEditor.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMRange.h"
#include "nsIDOMTextRangeList.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsITransactionManager.h"
#include "TransactionFactory.h"
#include "nsIComponentManager.h"
#include "nsISupportsArray.h"
#include "nsIEditProperty.h"
#include "nsIFileSpec.h"
#include "nsIDOMCharacterData.h"
#include "nsICSSStyleSheet.h"
#include "nsIDTD.h"

class nsIEditActionListener;
class nsIDocumentStateListener;
class nsIDOMCharacterData;
class nsIDOMRange;
class nsIPresShell;
class nsIViewManager;
class ChangeAttributeTxn;
class CreateElementTxn;
class InsertElementTxn;
class DeleteElementTxn;
class InsertTextTxn;
class DeleteTextTxn;
class SplitElementTxn;
class JoinElementTxn;
class EditAggregateTxn;
class nsVoidArray;
class nsISupportsArray;
class nsIPref;
class nsIStringBundleService;
class nsIStringBundle;
class nsILocale;
class IMETextTxn;
class AddStyleSheetTxn;
class RemoveStyleSheetTxn;

#ifdef ENABLE_JS_EDITOR_LOG
class nsJSEditorLog;
class nsJSTxnLog;
#endif // ENABLE_JS_EDITOR_LOG

//This is the monitor for the editor.
PRMonitor *GetEditorMonitor();


/** implementation of an editor object.  it will be the controler/focal point 
 *  for the main editor services. i.e. the GUIManager, publishing, transaction 
 *  manager, event interfaces. the idea for the event interfaces is to have them 
 *  delegate the actual commands to the editor independent of the XPFE implementation.
 */
class nsEditor : public nsIEditor
{
private:
  nsIPresShell   *mPresShell;
  nsIViewManager *mViewManager;
  PRUint32        mUpdateCount;
  nsCOMPtr<nsITransactionManager> mTxnMgr;
  nsCOMPtr<nsIEditProperty>  mEditProperty;
  nsCOMPtr<nsICSSStyleSheet> mLastStyleSheet;			// is owning this dangerous?

  //
  // data necessary to build IME transactions
  //
  nsCOMPtr<nsIDOMCharacterData> mIMETextNode;
  PRUint32						mIMETextOffset;
  PRUint32						mIMEBufferLength;

  friend PRBool NSCanUnload(nsISupports* serviceMgr);
  static PRInt32 gInstanceCount;

  nsVoidArray*                  mActionListeners;
  nsCOMPtr<nsISupportsArray>    mDocStateListeners;
  nsCOMPtr<nsIStringBundle>     mStringBundle;

  PRInt8                        mDocDirtyState;		// -1 = not initialized

protected:
  nsIDOMDocument * mDoc;
  nsCOMPtr<nsIDTD> mDTD;
  // Services are not nsCOMPtr friendly
  nsIPref* mPrefs;

#ifdef ENABLE_JS_EDITOR_LOG
  nsJSEditorLog *mJSEditorLog;
  nsJSTxnLog *mJSTxnLog;
#endif // ENABLE_JS_EDITOR_LOG

public:

  enum IterDirection
  {
    kIterForward,
    kIterBackward
  };

  static const char* kMOZEditorBogusNodeAttr;
  static const char* kMOZEditorBogusNodeValue;

  /** The default constructor. This should suffice. the setting of the interfaces is done
   *  after the construction of the editor class.
   */
  nsEditor();
  /** The default destructor. This should suffice. Should this be pure virtual 
   *  for someone to derive from the nsEditor later? I dont believe so.
   */
  virtual ~nsEditor();

/*BEGIN nsIEditor for more details*/
  
//Interfaces for addref and release and queryinterface
//NOTE: Use   NS_DECL_ISUPPORTS_INHERITED in any class inherited from nsEditor
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell);

  NS_IMETHOD GetDocument(nsIDOMDocument **aDoc);

  NS_IMETHOD GetBodyElement(nsIDOMElement **aElement);

  NS_IMETHOD GetPresShell(nsIPresShell **aPS);

  NS_IMETHOD GetSelection(nsIDOMSelection **aSelection);

  NS_IMETHOD SetProperties(nsVoidArray *aPropList);

  NS_IMETHOD GetProperties(nsVoidArray *aPropList);

  NS_IMETHOD SetAttribute(nsIDOMElement * aElement, 
                          const nsString& aAttribute, 
                          const nsString& aValue);

  NS_IMETHOD GetAttributeValue(nsIDOMElement * aElement, 
                               const nsString& aAttribute, 
                               nsString&       aResultValue, 
                               PRBool&         aResultIsSet);

  NS_IMETHOD RemoveAttribute(nsIDOMElement *aElement, const nsString& aAttribute);

  //NOTE: Most callers are dealing with Nodes,
  //  but these objects must supports nsIDOMElement
  NS_IMETHOD CopyAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode);

  NS_IMETHOD CreateNode(const nsString& aTag,
                        nsIDOMNode *    aParent,
                        PRInt32         aPosition,
                        nsIDOMNode **   aNewNode);

  NS_IMETHOD InsertNode(nsIDOMNode * aNode,
                        nsIDOMNode * aParent,
                        PRInt32      aPosition);

  NS_IMETHOD InsertText(const nsString& aStringToInsert);

  NS_IMETHOD BeginComposition(void);

  NS_IMETHOD SetCompositionString(const nsString& aCompositionString, nsIDOMTextRangeList* aTextRangeList);

  NS_IMETHOD EndComposition(void);
  
  NS_IMETHOD OutputToString(nsString& aOutputString,
                            const nsString& aFormatType,
                            PRUint32 aFlags);
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsString& aFormatType,
                            const nsString* aCharsetOverride,
                            PRUint32 aFlags);
  NS_IMETHOD DumpContentTree();

  NS_IMETHOD DeleteNode(nsIDOMNode * aChild);

  NS_IMETHOD DeleteSelection(nsIEditor::ECollapsedSelectionAction aAction);

  NS_IMETHOD DeleteSelectionAndCreateNode(const nsString& aTag, nsIDOMNode ** aNewNode);


  NS_IMETHOD SplitNode(nsIDOMNode * aExistingRightNode,
                       PRInt32      aOffset,
                       nsIDOMNode ** aNewLeftNode);

  NS_IMETHOD JoinNodes(nsIDOMNode * aLeftNode,
                       nsIDOMNode * aRightNode,
                       nsIDOMNode * aParent);

  NS_IMETHOD InsertBreak();

  NS_IMETHOD EnableUndo(PRBool aEnable);

  NS_IMETHOD Do(nsITransaction *aTxn);

  NS_IMETHOD Undo(PRUint32 aCount);

  NS_IMETHOD CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo);

  NS_IMETHOD Redo(PRUint32 aCount);

  NS_IMETHOD CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo);

  NS_IMETHOD BeginTransaction();

  NS_IMETHOD EndTransaction();

  NS_IMETHOD GetLayoutObject(nsIDOMNode *aNode, nsISupports **aLayoutObject);

  NS_IMETHOD ScrollIntoView(PRBool aScrollToBegin);

  NS_IMETHOD SelectAll();

  NS_IMETHOD BeginningOfDocument();

  NS_IMETHOD EndOfDocument();

  NS_IMETHOD Cut();
  
  NS_IMETHOD Copy();
  
  NS_IMETHOD Paste();

  NS_IMETHOD PasteAsQuotation();

  NS_IMETHOD InsertAsQuotation(const nsString& aQuotedText);

  NS_IMETHOD ApplyStyleSheet(const nsString& aURL);
  NS_IMETHOD AddStyleSheet(nsICSSStyleSheet* aSheet);
  NS_IMETHOD RemoveStyleSheet(nsICSSStyleSheet* aSheet);

  NS_IMETHOD AddEditActionListener(nsIEditActionListener *aListener);

  NS_IMETHOD RemoveEditActionListener(nsIEditActionListener *aListener);

  NS_IMETHOD AddDocumentStateListener(nsIDocumentStateListener *aListener);
  NS_IMETHOD RemoveDocumentStateListener(nsIDocumentStateListener *aListener);
  NS_IMETHOD GetDocumentModified(PRBool *outDocModified);

  NS_IMETHOD DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed);

  NS_IMETHOD StartLogging(nsIFileSpec *aLogFile);
  NS_IMETHOD StopLogging();

/*END nsIEditor interfaces*/

  /* StyleSheet load callback */
  static void ApplyStyleSheetToPresShellDocument(nsICSSStyleSheet* aSheet, void *aData);
  

/*BEGIN private methods used by the implementations of the above functions*/
protected:
  /** create a transaction for setting aAttribute to aValue on aElement
    */
  NS_IMETHOD CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                      const nsString& aAttribute, 
                                      const nsString& aValue,
                                      ChangeAttributeTxn ** aTxn);

  /** create a transaction for removing aAttribute on aElement
    */
  NS_IMETHOD CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                         const nsString& aAttribute,
                                         ChangeAttributeTxn ** aTxn);

  /** create a transaction for creating a new child node of aParent of type aTag.
    */
  NS_IMETHOD CreateTxnForCreateElement(const nsString& aTag,
                                       nsIDOMNode     *aParent,
                                       PRInt32         aPosition,
                                       CreateElementTxn ** aTxn);

  /** create a transaction for inserting aNode as a child of aParent.
    */
  NS_IMETHOD CreateTxnForInsertElement(nsIDOMNode * aNode,
                                       nsIDOMNode * aParent,
                                       PRInt32      aOffset,
                                       InsertElementTxn ** aTxn);

  /** create a transaction for removing aElement from its parent.
    */
  NS_IMETHOD CreateTxnForDeleteElement(nsIDOMNode * aElement,
                                       DeleteElementTxn ** aTxn);

  /** create a transaction for inserting aStringToInsert into aTextNode
    * if aTextNode is null, the string is inserted at the current selection.
    */
  NS_IMETHOD CreateTxnForInsertText(const nsString & aStringToInsert,
                                    nsIDOMCharacterData *aTextNode,
                                    InsertTextTxn ** aTxn);

  NS_IMETHOD CreateTxnForIMEText(const nsString & aStringToInsert,
								 nsIDOMTextRangeList* aTextRangeList,
                                 IMETextTxn ** aTxn);

  /** create a transaction for adding a style sheet
    */
  NS_IMETHOD CreateTxnForAddStyleSheet(nsICSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn);

  /** create a transaction for removing a style sheet
    */
  NS_IMETHOD CreateTxnForRemoveStyleSheet(nsICSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn);

  /* remove the old style sheet, and apply the supplied one */
  NS_IMETHOD ReplaceStyleSheet(nsICSSStyleSheet *aNewSheet);
  
  /** insert aStringToInsert as the first text in the document
    */
  NS_IMETHOD DoInitialInsert(const nsString & aStringToInsert);

  NS_IMETHOD DoInitialInputMethodInsert(const nsString& aStringToInsert,nsIDOMTextRangeList* aTextRangeList);


  NS_IMETHOD DeleteText(nsIDOMCharacterData *aElement,
                        PRUint32             aOffset,
                        PRUint32             aLength);

  NS_IMETHOD CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                    PRUint32             aOffset,
                                    PRUint32             aLength,
                                    DeleteTextTxn      **aTxn);

  NS_IMETHOD CreateTxnForDeleteSelection(nsIEditor::ECollapsedSelectionAction aAction,
                                         EditAggregateTxn  ** aTxn);

  NS_IMETHOD CreateTxnForDeleteInsertionPoint(nsIDOMRange         *aRange, 
                                              nsIEditor::ECollapsedSelectionAction aAction, 
                                              EditAggregateTxn    *aTxn);

  NS_IMETHOD CreateTxnForSplitNode(nsIDOMNode *aNode,
                                   PRUint32    aOffset,
                                   SplitElementTxn **aTxn);

  NS_IMETHOD CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                  nsIDOMNode  *aRightNode,
                                  JoinElementTxn **aTxn);

  /** Create an aggregate transaction for deleting current selection
   *  Used by all methods that need to delete current selection,
   *    then insert something new to replace it
   *  @param nsString& aTxnName is the name of the aggregated transaction
   *  @param EditTxn **aAggTxn is the return location of the aggregate TXN,
   *    with the DeleteSelectionTxn as the first child ONLY
   *    if there was a selection to delete.
   */
  NS_IMETHOD CreateAggregateTxnForDeleteSelection(nsIAtom *aTxnName, EditAggregateTxn **aAggTxn);

  NS_IMETHOD DebugDumpContent() const;

  NS_IMETHOD SetInputMethodText(const nsString& aStringToInsert, nsIDOMTextRangeList* aTextRangeList);

  // called each time we modify the document. Increments the mod
  // count of the doc.
  NS_IMETHOD IncDocModCount(PRInt32 inNumMods);
  
  // return the mod count of the doc we are editing. Zero means unchanged.
  NS_IMETHOD GetDocModCount(PRInt32 &outModCount);
  
  // called ONLY when we need to override the doc's modification
  // state. This should already be handled by nsIDiskDocument.
  NS_IMETHOD ResetDocModCount();
  
  // called after a transaction is done successfully
  NS_IMETHOD DoAfterDoTransaction(nsITransaction *aTxn);
  // called after a transaction is undone successfully
  NS_IMETHOD DoAfterUndoTransaction();
  // called after a transaction is redone successfully
  NS_IMETHOD DoAfterRedoTransaction();
  
  // called after the document has been saved
  NS_IMETHOD DoAfterDocumentSave();
  
  // tell the doc state listeners that the doc state has changed
  NS_IMETHOD NotifyDocumentStateListeners();
  
  /** make the given selection span the entire document */
  NS_IMETHOD SelectEntireDocument(nsIDOMSelection *aSelection);
  
protected:
// XXXX: Horrible hack! We are doing this because
// of an error in Gecko which is not rendering the
// document after a change via the DOM - gpk 2/13/99
  void HACKForceRedraw(void);

  NS_IMETHOD DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, PRInt32& offsetOfNewNode);

public:
  /** 
   * SplitNode() creates a new node identical to an existing node, and split the contents between the two nodes
   * @param aExistingRightNode   the node to split.  It will become the new node's next sibling.
   * @param aOffset              the offset of aExistingRightNode's content|children to do the split at
   * @param aNewLeftNode         [OUT] the new node resulting from the split, becomes aExistingRightNode's previous sibling.
   * @param aParent              the parent of aExistingRightNode
   */
  static nsresult SplitNodeImpl(nsIDOMNode *aExistingRightNode,
                                PRInt32      aOffset,
                                nsIDOMNode *aNewLeftNode,
                                nsIDOMNode *aParent);

  /** 
   * JoinNodes() takes 2 nodes and merge their content|children.
   * @param aNodeToKeep   The node that will remain after the join.
   * @param aNodeToJoin   The node that will be joined with aNodeToKeep.
   *                      There is no requirement that the two nodes be of the same type.
   * @param aParent       The parent of aExistingRightNode
   * @param aNodeToKeepIsFirst  if PR_TRUE, the contents|children of aNodeToKeep come before the
   *                            contents|children of aNodeToJoin, otherwise their positions are switched.
   */
  static nsresult JoinNodesImpl(nsIDOMNode *aNodeToKeep,
                                nsIDOMNode *aNodeToJoin,
                                nsIDOMNode *aParent,
                                PRBool      aNodeToKeepIsFirst);

  /**
   *  Set aOffset to the offset of aChild in aParent.  
   *  Returns an error if aChild is not an immediate child of aParent.
   */
  static nsresult GetChildOffset(nsIDOMNode *aChild, 
                                 nsIDOMNode *aParent, 
                                 PRInt32    &aOffset);

  /**
   *  Set aParent to the parent of aChild.
   *  Set aOffset to the offset of aChild in aParent.  
   */
  static nsresult GetNodeLocation(nsIDOMNode *aChild, 
                                 nsCOMPtr<nsIDOMNode> *aParent, 
                                 PRInt32    *aOffset);

  /** set aIsInline to PR_TRUE if aNode is inline as defined by HTML DTD */
  static nsresult IsNodeInline(nsIDOMNode *aNode, PRBool &aIsInline);

  /** set aIsBlock to PR_TRUE if aNode is inline as defined by HTML DTD */
  static nsresult IsNodeBlock(nsIDOMNode *aNode, PRBool &aIsBlock);

  /** returns the closest block parent of aNode, not including aNode itself.
    * can return null, for example if aNode is in a document fragment.
    * @param aNode        The node whose parent we seek.
    * @param aBlockParent [OUT] The block parent, if any.
    * @return a success value unless an unexpected error occurs.
    */
  static nsresult GetBlockParent(nsIDOMNode     *aNode, 
                                 nsIDOMElement **aBlockParent);

  /** Determines the bounding nodes for the block section containing aNode.
    * The calculation is based on some nodes intrinsically being block elements
    * acording to HTML.  Style sheets are not considered in this calculation.
    * <BR> tags separate block content sections.  So the HTML markup:
    * <PRE>
    *      <P>text1<BR>text2<B>text3</B></P>
    * </PRE>
    * contains two block content sections.  The first has the text node "text1"
    * for both endpoints.  The second has "text2" as the left endpoint and
    * "text3" as the right endpoint.
    * Notice that offsets aren't required, only leaf nodes.  Offsets are implicit.
    *
    * @param aNode      the block content returned includes aNode
    * @param aLeftNode  [OUT] the left endpoint of the block content containing aNode
    * @param aRightNode [OUT] the right endpoint of the block content containing aNode
    *
    */
  static nsresult GetBlockSection(nsIDOMNode  *aNode,
                                  nsIDOMNode **aLeftNode, 
                                  nsIDOMNode **aRightNode);

  /** Compute the set of block sections in a given range.
    * A block section is the set of (leftNode, rightNode) pairs given
    * by GetBlockSection.  The set is computed by computing the 
    * block section for every leaf node in the range and throwing 
    * out duplicates.
    *
    * @param aRange     The range to compute block sections for.
    * @param aSections  Allocated storage for the resulting set, stored as nsIDOMRanges.
    */
  static nsresult GetBlockSectionsForRange(nsIDOMRange      *aRange, 
                                           nsISupportsArray *aSections);

  /** returns PR_TRUE in out-param aResult if all nodes between (aStartNode, aStartOffset)
    * and (aEndNode, aEndOffset) are inline as defined by HTML DTD. 
    */
  static nsresult IntermediateNodesAreInline(nsIDOMRange  *aRange,
                                             nsIDOMNode   *aStartNode, 
                                             PRInt32       aStartOffset, 
                                             nsIDOMNode   *aEndNode,
                                             PRInt32       aEndOffset,
                                             PRBool       &aResult);

  /** returns the number of things inside aNode in the out-param aCount.  
    * @param  aNode is the node to get the length of.  
    *         If aNode is text, returns number of characters. 
    *         If not, returns number of children nodes.
    * @param  aCount [OUT] the result of the above calculation.
    */
  static nsresult GetLengthOfDOMNode(nsIDOMNode *aNode, PRUint32 &aCount);

  /** get the node immediately prior to aCurrentNode
    * @param aCurrentNode   the node from which we start the search
    * @param aEditableNode  if PR_TRUE, only return an editable node
    * @param aResultNode    [OUT] the node that occurs before aCurrentNode in the tree,
    *                       skipping non-editable nodes if aEditableNode is PR_TRUE.
    *                       If there is no prior node, aResultNode will be nsnull.
    */
  nsresult GetPriorNode(nsIDOMNode  *aCurrentNode, 
                        PRBool       aEditableNode,
                        nsIDOMNode **aResultNode);

  /** get the node immediately after to aCurrentNode
    * @param aCurrentNode   the node from which we start the search
    * @param aEditableNode  if PR_TRUE, only return an editable node
    * @param aResultNode    [OUT] the node that occurs after aCurrentNode in the tree,
    *                       skipping non-editable nodes if aEditableNode is PR_TRUE.
    *                       If there is no prior node, aResultNode will be nsnull.
    */
  nsresult GetNextNode(nsIDOMNode  *aCurrentNode, 
                       PRBool       aEditableNode,
                       nsIDOMNode **aResultNode);

  /** Get the rightmost child of aCurrentNode, and return it in aResultNode
    * aResultNode is set to nsnull if aCurrentNode has no children.
    */
  static nsresult GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  /** Get the leftmost child of aCurrentNode, and return it in aResultNode
    * aResultNode is set to nsnull if aCurrentNode has no children.
    */
  static nsresult GetLeftmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  /** GetFirstTextNode ADDREFFS and will get the next available text node from the passed
   *  in node parameter it can also return NS_ERROR_FAILURE if no text nodes are available
   *  now it simply returns the first node in the dom
   *  @param nsIDOMNode *aNode is the node to start looking from
   *  @param nsIDOMNode **aRetNode is the return location of the text dom node
   *
   * NOTE: this method will probably be removed.
   */
  static nsresult GetFirstTextNode(nsIDOMNode *aNode, nsIDOMNode **aRetNode);

  /** GetFirstNodeOfType ADDREFFS and will get the next available node from the passed
   *  in aStartNode parameter of type aTag.
   *  It can also return NS_ERROR_FAILURE if no such nodes are available
   *  @param   aStartNode is the node to start looking from
   *  @param   aTag is the type of node we are searching for
   *  @param   aResult is the node we found, or nsnull if there is none
   */
  static nsresult GetFirstNodeOfType(nsIDOMNode     *aStartNode, 
                                     const nsString &aTag, 
                                     nsIDOMNode    **aResult);

  /** returns PR_TRUE if aNode is of the type implied by aTag */
  static PRBool NodeIsType(nsIDOMNode *aNode, nsIAtom *aTag);

  /** returns PR_TRUE if aParent can contain a child of type aTag */
  PRBool     CanContainTag(nsIDOMNode* aParent, const nsString &aTag);
  
  /** returns PR_TRUE if aNode is an editable node */
  PRBool IsEditable(nsIDOMNode *aNode);

  /** counts number of editable child nodes */
  nsresult CountEditableChildren(nsIDOMNode *aNode, PRUint32 &outCount);
  
  /** Find the deep first and last children */
  nsCOMPtr<nsIDOMNode> GetDeepFirstChild(nsCOMPtr<nsIDOMNode> aRoot);
  nsCOMPtr<nsIDOMNode> GetDeepLastChild(nsCOMPtr<nsIDOMNode> aRoot);


  /** from html rules code - migration in progress */
  static nsresult GetTagString(nsIDOMNode *aNode, nsString& outString);
  static nsCOMPtr<nsIAtom> GetTag(nsIDOMNode *aNode);
  static PRBool NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2);
  static PRBool IsBlockNode(nsIDOMNode *aNode);
  static PRBool IsInlineNode(nsIDOMNode *aNode);
  static nsCOMPtr<nsIDOMNode> GetBlockNodeParent(nsIDOMNode *aNode);
  static PRBool HasSameBlockNodeParent(nsIDOMNode *aNode1, nsIDOMNode *aNode2);
  
  static PRBool IsTextOrElementNode(nsIDOMNode *aNode);
  static PRBool IsTextNode(nsIDOMNode *aNode);
  
  static PRInt32 GetIndexOf(nsIDOMNode *aParent, nsIDOMNode *aChild);
  static nsCOMPtr<nsIDOMNode> GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset);
  
  static nsCOMPtr<nsIDOMNode> NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir);
  static nsresult GetStartNodeAndOffset(nsIDOMSelection *aSelection, nsCOMPtr<nsIDOMNode> *outStartNode, PRInt32 *outStartOffset);
  static nsresult GetEndNodeAndOffset(nsIDOMSelection *aSelection, nsCOMPtr<nsIDOMNode> *outEndNode, PRInt32 *outEndOffset);

  nsresult IsPreformatted(nsIDOMNode *aNode, PRBool *aResult);
  nsresult IsNextCharWhitespace(nsIDOMNode *aParentNode, PRInt32 aOffset, PRBool *aResult);
  nsresult IsPrevCharWhitespace(nsIDOMNode *aParentNode, PRInt32 aOffset, PRBool *aResult);

  nsresult SplitNodeDeep(nsIDOMNode *aNode, nsIDOMNode *aSplitPointParent, PRInt32 aSplitPointOffset);
  nsresult JoinNodeDeep(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMSelection *aSelection); 

  nsresult GetString(const nsString& name, nsString& value);

  nsresult BeginUpdateViewBatch(void);
  nsresult EndUpdateViewBatch(void);
};


class nsAutoEditBatch
{
  private:
    nsCOMPtr<nsIEditor> mEd;
  public:
    nsAutoEditBatch( nsIEditor *aEd) : mEd(do_QueryInterface(aEd)) 
                   { if (mEd) mEd->BeginTransaction(); }
    ~nsAutoEditBatch() { if (mEd) mEd->EndTransaction(); }
};

class nsAutoSelectionReset
{
  private:
    /** ref-counted reference to the selection that we are supposed to restore */
    nsCOMPtr<nsIDOMSelection> mSel;

    /** PR_TRUE if this instance initialized itself correctly */
    PRBool mInitialized;

    nsCOMPtr<nsIDOMNode> mStartNode;
    nsCOMPtr<nsIDOMNode> mEndNode;
    PRInt32 mStartOffset;
    PRInt32 mEndOffset;

  public:
    /** constructor responsible for remembering all state needed to restore aSel */
    nsAutoSelectionReset(nsIDOMSelection *aSel);
    
    /** destructor restores mSel to its former state */
    ~nsAutoSelectionReset();
};


#endif
