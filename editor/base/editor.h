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

#include "prmon.h"
#include "nsIEditor.h"
#include "nsIContextLoader.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsCOMPtr.h"
#include "editorInterfaces.h"
#include "nsITransactionManager.h"
#include "nsRepository.h"
//#include "nsISelection.h"

class nsIDOMCharacterData;
class nsIPresShell;

//This is the monitor for the editor.
PRMonitor *getEditorMonitor();


/** implementation of an editor object.  it will be the controler/focal point 
 *  for the main editor services. i.e. the GUIManager, publishing, transaction 
 *  manager, event interfaces. the idea for the event interfaces is to have them 
 *  delegate the actual commands to the editor independent of the XPFE implementation.
 */
class nsEditor : public nsIEditor
{
private:
  nsIPresShell *mPresShell;
  nsCOMPtr<nsIDOMDocument>      mDomInterfaceP;
  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;
//  nsCOMPtr<nsISelection>        mSelectionP;
  //nsCOMPtr<nsITransactionManager> mTxnMgrP;
  nsITransactionManager * mTxnMgr;
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

  virtual nsresult Init(nsIDOMDocument *aDomInterface, nsIPresShell* aPresShell);

  virtual nsresult GetDomInterface(nsIDOMDocument **aDomInterface);

  virtual nsresult SetProperties(PROPERTIES aProperty);

  virtual nsresult GetProperties(PROPERTIES **);

  virtual nsresult SetAttribute(nsIDOMElement * aElement, 
                                const nsString& aAttribute, 
                                const nsString& aValue);

  virtual nsresult GetAttributeValue(nsIDOMElement * aElement, 
                                     const nsString& aAttribute, 
                                     nsString&       aResultValue, 
                                     PRBool&         aResultIsSet);

  virtual nsresult RemoveAttribute(nsIDOMElement *aElement, const nsString& aAttribute);

  virtual nsresult InsertString(nsString *aString);
  
  virtual nsresult Commit(PRBool aCtrlKey);



/*END nsIEditor interfaces*/

/*BEGIN nsEditor interfaces*/


/*KeyListener Methods*/
  /** KeyDown is called from the key event lister "probably"
   *  @param int aKeycode the keycode or ascii
   *  value of the key that was hit for now
   *  @return False if ignored
   */
  PRBool KeyDown(int aKeycode);

/*MouseListener Methods*/
  /** MouseClick
   *  @param int x the xposition of the click
   *  @param int y the yposition of the click
   */
  PRBool MouseClick(int aX,int aY); //it should also tell us the dom element that was selected.

/*BEGIN private methods used by the implementations of the above functions*/

  /** AppendText is a private method that accepts a pointer to a string
   *  and will append it to the current node.  this will be depricated
   *  @param nsString *aStr is the pointer to the valid string
   */
  nsresult AppendText(nsString *aStr);

  /** GetCurrentNode ADDREFFS and will get the current node from selection.
   *  now it simply returns the first node in the dom
   *  @param nsIDOMNode **aNode is the return location of the dom node
   */
  nsresult GetCurrentNode(nsIDOMNode ** aNode);

  /** GetFirstTextNode ADDREFFS and will get the next available text node from the passed
   *  in node parameter it can also return NS_ERROR_FAILURE if no text nodes are available
   *  now it simply returns the first node in the dom
   *  @param nsIDOMNode *aNode is the node to start looking from
   *  @param nsIDOMNode **aRetNode is the return location of the text dom node
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

  /** ExecuteTransaction fires a transaction.  It is provided here so 
    * clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is used.  
    * Otherwise, the transaction is just executed directly.
    *
    * @param aTxn the transaction to execute
    */
  nsresult ExecuteTransaction(nsITransaction *aTxn);

  /** Undo reverses the effects of the last ExecuteTransaction operation
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to undo and the result of
    * that undo is returned.  
    * Otherwise, the Undo request is ignored.
    *
    */
  nsresult Undo();

  /** Redo reverses the effects of the last Undo operation
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to redo and the result of
    * that redo is returned.  
    * Otherwise, the Redo request is ignored.
    *
    */
  nsresult Redo();

  nsresult CreateElement(const nsString& aTag,
                         nsIDOMNode *    aParent,
                         PRInt32         aPosition);

  nsresult DeleteElement(nsIDOMNode * aParent,
                         nsIDOMNode * aElement);

  nsresult DeleteSelection();

  nsresult InsertText(nsIDOMCharacterData *aElement,
                      PRUint32             aOffset,
                      const nsString&      aStringToInsert);

  nsresult DeleteText(nsIDOMCharacterData *aElement,
                      PRUint32             aOffset,
                      PRUint32             aLength);

  static  nsresult SplitNode(nsIDOMNode * aNode,
                             PRInt32      aOffset,
                             nsIDOMNode * aNewNode,
                             nsIDOMNode * aParent);

  static nsresult JoinNodes(nsIDOMNode * aNodeToKeep,
                            nsIDOMNode * aNodeToJoin,
                            nsIDOMNode * aParent,
                            PRBool       aNodeToKeepIsFirst);

  nsresult Delete(PRBool aForward, PRUint32 aCount);

/*END private methods of nsEditor*/
};


/*
factory method(s)
*/

nsresult NS_MakeEditorLoader(nsIContextLoader **aResult);
