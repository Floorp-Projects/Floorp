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

#ifndef ChangeAttributeTxn_h__
#define ChangeAttributeTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIEditor.h"

#define CHANGE_ATTRIBUTE_TXN_IID \
{/* 97818860-ac48-11d2-86d8-000064657374 */ \
0x97818860, 0xac48, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that changes an attribute of a content node. 
 * This transaction covers add, remove, and change attribute.
 */
class ChangeAttributeTxn : public EditTxn
{
public:

  virtual nsresult Init(nsIEditor      *aEditor,
                        nsIDOMElement  *aElement,
                        const nsString& aAttribute,
                        const nsString& aValue,
                        PRBool aRemoveAttribute);

private:
  ChangeAttributeTxn();

public:

  virtual nsresult Do(void);

  virtual nsresult Undo(void);

  virtual nsresult Redo(void);

  virtual nsresult GetIsTransient(PRBool *aIsTransient);

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  virtual nsresult Write(nsIOutputStream *aOutputStream);

  virtual nsresult GetUndoString(nsString **aString);

  virtual nsresult GetRedoString(nsString **aString);

protected:

  /** the editor that created this transaction */
  nsCOMPtr<nsIEditor> mEditor;
  
  /** the element to operate upon */
  nsCOMPtr<nsIDOMElement> mElement;
  
  /** the attribute to change */
  nsString mAttribute;

  /** the value to set the attribute to (ignored if mRemoveAttribute==PR_TRUE) */
  nsString mValue;

  /** the value to set the attribute to for undo */
  nsString mUndoValue;

  /** PR_TRUE if the mAttribute was set on mElement at the time of execution */
  PRBool   mAttributeWasSet;

  /** PR_TRUE if the operation is to remove mAttribute from mElement */
  PRBool   mRemoveAttribute;

  friend class TransactionFactory;
};

#endif
