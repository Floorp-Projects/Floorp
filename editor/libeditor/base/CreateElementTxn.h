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

#ifndef CreateElementTxn_h__
#define CreateElementTxn_h__

#include "EditTxn.h"

class nsIDOMDocument;
class nsIDOMNode;
class nsIDOMElement;

/**
 * A transaction that creates a new node in the content tree.
 */
class CreateElementTxn : public EditTxn
{
public:

  enum { eAppend=-1 };

  CreateElementTxn(nsEditor *aEditor,
                   nsIDOMDocument *aDoc,
                   nsString& aTag,
                   nsIDOMNode *aParent,
                   PRUint32 aOffsetInParent);

  virtual ~CreateElementTxn();

  virtual nsresult Do(void);

  virtual nsresult Undo(void);

  virtual nsresult Redo(void);

  virtual nsresult GetIsTransient(PRBool *aIsTransient);

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  virtual nsresult Write(nsIOutputStream *aOutputStream);

  virtual nsresult GetUndoString(nsString **aString);

  virtual nsresult GetRedoString(nsString **aString);

protected:
  
  /** the document into which the new node will be inserted */
  nsIDOMDocument *mDoc;
  
  /** the tag (mapping to object type) for the new element */
  nsString mTag;

  /** the node into which the new node will be inserted */
  nsIDOMNode *mParent;

  /** the index in mParent for the new node */
  PRUint32 mOffsetInParent;

  /** the new node to insert */
  nsIDOMElement *mNewNode;  

  /** the node we will insert mNewNode before.  We compute this ourselves. */
  nsIDOMNode *mRefNode;

};

#endif
