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

#ifndef SplitElementTxn_h__
#define SplitElementTxn_h__

#include "EditTxn.h"

class nsIDOMNode;
class nsIDOMElement;
class nsIDOMDocument;

/**
 * A transaction that splits an element E into two identical nodes, E1 and E2
 * with the children of E divided between E1 and E2.
 */
class SplitElementTxn : public EditTxn
{
public:

  SplitElementTxn(nsEditor   *aEditor,
                  nsIDOMNode *aNode,
                  PRInt32     aOffset);

  virtual ~SplitElementTxn();

  virtual nsresult Do(void);

  virtual nsresult Undo(void);

  virtual nsresult Redo(void);

  virtual nsresult GetIsTransient(PRBool *aIsTransient);

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  virtual nsresult Write(nsIOutputStream *aOutputStream);

  virtual nsresult GetUndoString(nsString **aString);

  virtual nsresult GetRedoString(nsString **aString);

protected:
  
  /** the element to operate upon */
  nsIDOMNode *mNode;

  /** the offset into mElement where the children of mElement are split.<BR>
    * mOffset is the index of the last child in the left node. 
    * -1 means the new node gets no children.
    */
  PRInt32  mOffset;

  /** the element we create when splitting mElement */
  nsIDOMNode *mNewNode;
  nsIDOMNode *mParent;

};

#endif
