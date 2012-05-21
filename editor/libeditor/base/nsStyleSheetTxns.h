/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStylesheetTxns_h__
#define nsStylesheetTxns_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsIEditor.h"

class AddStyleSheetTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aSheet   the stylesheet to add
    */
  NS_IMETHOD Init(nsIEditor         *aEditor,
                  nsCSSStyleSheet   *aSheet);

  AddStyleSheetTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AddStyleSheetTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

protected:

  nsIEditor*  mEditor;                  // the editor that created this transaction
  nsRefPtr<nsCSSStyleSheet>  mSheet;    // the style sheet to add

};


class RemoveStyleSheetTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aSheet   the stylesheet to remove
    */
  NS_IMETHOD Init(nsIEditor         *aEditor,
                  nsCSSStyleSheet   *aSheet);

  RemoveStyleSheetTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RemoveStyleSheetTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

protected:

  nsIEditor*  mEditor;                  // the editor that created this transaction
  nsRefPtr<nsCSSStyleSheet>  mSheet;    // the style sheet to remove

};


#endif /* nsStylesheetTxns_h__ */
