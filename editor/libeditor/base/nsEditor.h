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
#include "nsIContextLoader.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsCOMPtr.h"
#include "nsITransactionManager.h"
#include "TransactionFactory.h"
#include "nsRepository.h"
//#include "nsISelection.h"

class nsIDOMCharacterData;
class nsIDOMRange;
class nsIPresShell;
class nsIViewManager;
class ChangeAttributeTxn;
class CreateElementTxn;
class DeleteElementTxn;
class InsertTextTxn;
class DeleteTextTxn;
class SplitElementTxn;
class JoinElementTxn;
class EditAggregateTxn;
class nsVoidArray;

//This is the monitor for the editor.
PRMonitor *getEditorMonitor();

/*
struct Properties
{
  Properties (nsIAtom *aPropName, nsIAtom *aValue, PRBool aAppliesToAll);
  ~Properties();

  nsIAtom *mPropName;
  nsIAtom *mValue;
  PRBool   mAppliesToAll;
};

inline Property::Property(nsIAtom *aPropName, nsIAtom *aValue, PRBool aAppliesToAll)
{
  mPropName = aPropName;
  mPropValue = aPropValue;
  mAppliesToAll = aAppliesToAll;
};

*/

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
  nsCOMPtr<nsIDOMDocument> mDoc;
  nsCOMPtr<nsITransactionManager> mTxnMgr;


  friend PRBool NSCanUnload(void);
  static PRInt32 gInstanceCount;

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
  
  /*interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsIDOMDocument *aDoc, nsIPresShell* aPresShell);

  virtual nsresult GetDocument(nsIDOMDocument **aDoc);

  virtual nsresult SetProperties(nsVoidArray *aPropList);

  virtual nsresult GetProperties(nsVoidArray *aPropList);

  virtual nsresult SetAttribute(nsIDOMElement * aElement, 
                                const nsString& aAttribute, 
                                const nsString& aValue);

  virtual nsresult GetAttributeValue(nsIDOMElement * aElement, 
                                     const nsString& aAttribute, 
                                     nsString&       aResultValue, 
                                     PRBool&         aResultIsSet);

  virtual nsresult RemoveAttribute(nsIDOMElement *aElement, const nsString& aAttribute);

  virtual nsresult CreateNode(const nsString& aTag,
                              nsIDOMNode *    aParent,
                              PRInt32         aPosition);

  virtual nsresult InsertNode(nsIDOMNode * aNode,
                              nsIDOMNode * aParent,
                              PRInt32      aPosition);
  virtual nsresult InsertText(const nsString& aStringToInsert);

  virtual nsresult DeleteNode(nsIDOMNode * aChild);

  virtual nsresult DeleteSelection(nsIEditor::Direction aDir);

  virtual nsresult SplitNode(nsIDOMNode * aExistingRightNode,
                             PRInt32      aOffset,
                             nsIDOMNode * aNewLeftNode,
                             nsIDOMNode * aParent);

  virtual nsresult JoinNodes(nsIDOMNode * aNodeToKeep,
                            nsIDOMNode * aNodeToJoin,
                            nsIDOMNode * aParent,
                            PRBool       aNodeToKeepIsFirst);
  
  virtual nsresult InsertBreak(PRBool aCtrlKey);

  virtual nsresult EnableUndo(PRBool aEnable);

  virtual nsresult Do(nsITransaction *aTxn);

  virtual nsresult Undo(PRUint32 aCount);

  virtual nsresult CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo);

  virtual nsresult Redo(PRUint32 aCount);

  virtual nsresult CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo);

  virtual nsresult BeginTransaction();

  virtual nsresult EndTransaction();

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
  nsresult GetFirstTextNode(nsIDOMNode *aNode, nsIDOMNode **aRetNode);

  /** GetFirstNodeOfType ADDREFFS and will get the next available node from the passed
   *  in aStartNode parameter of type aTag.
   *  It can also return NS_ERROR_FAILURE if no such nodes are available
   *  @param nsIDOMNode *aStartNode is the node to start looking from
   *  @param nsIAtom *aTag is the type of node we are searching for
   *  @param nsIDOMNode **aResult is the node we found, or nsnull if there is none
   */
  nsresult GetFirstNodeOfType(nsIDOMNode *aStartNode, const nsString &aTag, nsIDOMNode **aResult);

/*END public methods of nsEditor*/


/*BEGIN private methods used by the implementations of the above functions*/
protected:
  virtual nsresult CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                            const nsString& aAttribute, 
                                            const nsString& aValue,
                                            ChangeAttributeTxn ** aTxn);

  virtual nsresult CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                               const nsString& aAttribute,
                                               ChangeAttributeTxn ** aTxn);

  virtual nsresult CreateTxnForCreateElement(const nsString& aTag,
                                             nsIDOMNode     *aParent,
                                             PRInt32         aPosition,
                                             CreateElementTxn ** aTxn);

  virtual nsresult CreateTxnForDeleteElement(nsIDOMNode * aElement,
                                             DeleteElementTxn ** aTxn);

  virtual nsresult CreateTxnForInsertText(const nsString & aStringToInsert,
                                          InsertTextTxn ** aTxn);


  virtual nsresult DeleteText(nsIDOMCharacterData *aElement,
                              PRUint32             aOffset,
                              PRUint32             aLength);

  virtual nsresult CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                          PRUint32             aOffset,
                                          PRUint32             aLength,
                                          DeleteTextTxn      **aTxn);

  virtual nsresult CreateTxnForDeleteSelection(nsIEditor::Direction aDir,
                                               EditAggregateTxn  ** aTxn);

  virtual nsresult CreateTxnForDeleteInsertionPoint(nsIDOMRange         *aRange, 
                                            nsIEditor::Direction aDir, 
                                            EditAggregateTxn    *aTxn);

  virtual nsresult CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         PRUint32    aOffset,
                                         SplitElementTxn **aTxn);

  virtual nsresult CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                        nsIDOMNode  *aRightNode,
                                        JoinElementTxn **aTxn);

#if 0
  nsresult CreateTxnToHandleEnterKey(EditAggregateTxn **aTxn);
#endif

  nsresult GetPriorNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  nsresult GetNextNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  nsresult GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);

  nsresult GetLeftmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode);


};


/*
factory method(s)
*/

nsresult NS_MakeEditorLoader(nsIContextLoader **aResult);


#endif