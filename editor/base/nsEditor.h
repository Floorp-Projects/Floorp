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
#include "nsIEditorSupport.h"
#include "nsIContextLoader.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsCOMPtr.h"
#include "nsITransactionManager.h"
#include "TransactionFactory.h"
#include "nsIComponentManager.h"

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

//This is the monitor for the editor.
PRMonitor *getEditorMonitor();


/** implementation of an editor object.  it will be the controler/focal point 
 *  for the main editor services. i.e. the GUIManager, publishing, transaction 
 *  manager, event interfaces. the idea for the event interfaces is to have them 
 *  delegate the actual commands to the editor independent of the XPFE implementation.
 */
class nsEditor : public nsIEditor, public nsIEditorSupport
{
private:
  nsIPresShell   *mPresShell;
  nsIViewManager *mViewManager;
  PRUint32        mUpdateCount;
  nsCOMPtr<nsITransactionManager> mTxnMgr;


  friend PRBool NSCanUnload(nsISupports* serviceMgr);
  static PRInt32 gInstanceCount;

protected:
  nsCOMPtr<nsIDOMDocument> mDoc;

public:
  /** The default constructor. This should suffice. the setting of the interfaces is done
   *  after the construction of the editor class.
   */
  nsEditor();
  /** The default destructor. This should suffice. Should this be pure virtual 
   *  for someone to derive from the nsEditor later? I dont believe so.
   */
  virtual ~nsEditor();

/*BEGIN nsIEdieditor for more details*/
  
//Interfaces for addref and release and queryinterface
//NOTE: Use   NS_DECL_ISUPPORTS_INHERITED in any class inherited from nsEditor
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell);

  NS_IMETHOD GetDocument(nsIDOMDocument **aDoc);

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
  
  NS_IMETHOD DeleteNode(nsIDOMNode * aChild);

  NS_IMETHOD DeleteSelection(nsIEditor::Direction aDir);

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

  NS_IMETHOD Cut();
  
  NS_IMETHOD Copy();
  
  NS_IMETHOD Paste();


/*END nsIEditor interfaces*/


/*BEGIN public methods unique to nsEditor.  These will get moved to an interface
  if they survive.
*/

  /** GetFirstTextNode ADDREFFS and will get the next available text node from the passed
   *  in node parameter it can also return NS_ERROR_FAILURE if no text nodes are available
   *  now it simply returns the first node in the dom
   *  @param nsIDOMNode *aNode is the node to start looking from
   *  @param nsIDOMNode **aRetNode is the return location of the text dom node
   *
   * NOTE: this method will probably be removed.
   */
  NS_IMETHOD GetFirstTextNode(nsIDOMNode *aNode, nsIDOMNode **aRetNode);

  /** GetFirstNodeOfType ADDREFFS and will get the next available node from the passed
   *  in aStartNode parameter of type aTag.
   *  It can also return NS_ERROR_FAILURE if no such nodes are available
   *  @param nsIDOMNode *aStartNode is the node to start looking from
   *  @param nsIAtom *aTag is the type of node we are searching for
   *  @param nsIDOMNode **aResult is the node we found, or nsnull if there is none
   */
  NS_IMETHOD GetFirstNodeOfType(nsIDOMNode *aStartNode, const nsString &aTag, nsIDOMNode **aResult);

/*END public methods of nsEditor*/


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

  /** insert aStringToInsert as the first text in the document
    */
  NS_IMETHOD DoInitialInsert(const nsString & aStringToInsert);


  NS_IMETHOD DeleteText(nsIDOMCharacterData *aElement,
                        PRUint32             aOffset,
                        PRUint32             aLength);

  NS_IMETHOD CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                    PRUint32             aOffset,
                                    PRUint32             aLength,
                                    DeleteTextTxn      **aTxn);

  NS_IMETHOD CreateTxnForDeleteSelection(nsIEditor::Direction aDir,
                                         EditAggregateTxn  ** aTxn);

  NS_IMETHOD CreateTxnForDeleteInsertionPoint(nsIDOMRange         *aRange, 
                                              nsIEditor::Direction aDir, 
                                              EditAggregateTxn    *aTxn);

  NS_IMETHOD CreateTxnForSplitNode(nsIDOMNode *aNode,
                                   PRUint32    aOffset,
                                   SplitElementTxn **aTxn);

  NS_IMETHOD SplitNodeImpl(nsIDOMNode * aExistingRightNode,
                           PRInt32      aOffset,
                           nsIDOMNode * aNewLeftNode,
                           nsIDOMNode * aParent);

  NS_IMETHOD CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                  nsIDOMNode  *aRightNode,
                                  JoinElementTxn **aTxn);

  NS_IMETHOD JoinNodesImpl(nsIDOMNode * aNodeToKeep,
                           nsIDOMNode * aNodeToJoin,
                           nsIDOMNode * aParent,
                           PRBool       aNodeToKeepIsFirst);

  NS_IMETHOD GetPriorNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  NS_IMETHOD GetNextNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  NS_IMETHOD GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  NS_IMETHOD GetLeftmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

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

protected:
// XXXX: Horrible hack! We are doing this because
// of an error in Gecko which is not rendering the
// document after a change via the DOM - gpk 2/13/99
  void HACKForceRedraw(void);

  NS_IMETHOD DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, PRInt32& offsetOfNewNode);

};


/*
factory method(s)
*/

nsresult NS_MakeEditorLoader(nsIContextLoader **aResult);


#endif
