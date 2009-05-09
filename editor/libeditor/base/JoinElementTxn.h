/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef JoinElementTxn_h__
#define JoinElementTxn_h__

#include "EditTxn.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsIEditor.h"

class nsEditor;

/**
 * A transaction that joins two elements E1 (left node) and E2 (right node)
 * into a single node E.  
 * The children of E are the children of E1 followed by the children of E2.
 * After DoTransaction() and RedoTransaction(), E1 is removed from the content
 * tree and E2 remains.
 */
class JoinElementTxn : public EditTxn
{
public:
  /** initialize the transaction
    * @param aEditor    the provider of core editing operations
    * @param aLeftNode  the first of two nodes to join
    * @param aRightNode the second of two nodes to join
    */
  NS_IMETHOD Init(nsEditor   *aEditor,
                  nsIDOMNode *aLeftNode,
                  nsIDOMNode *aRightNode);

  JoinElementTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(JoinElementTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

protected:
  
  /** the elements to operate upon.  
    * After the merge, mRightNode remains and mLeftNode is removed from the content tree.
    */
  nsCOMPtr<nsIDOMNode> mLeftNode;
  nsCOMPtr<nsIDOMNode> mRightNode;

  /** the offset into mNode where the children of mElement are split (for undo).<BR>
    * mOffset is the index of the first child in the right node. 
    * -1 means the left node had no children.
    */
  PRUint32  mOffset;

  /** the parent node containing mLeftNode and mRightNode */
  nsCOMPtr<nsIDOMNode> mParent;
  nsEditor*  mEditor;
};

#endif
