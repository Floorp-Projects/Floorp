/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SetDocTitleTxn_h__
#define SetDocTitleTxn_h__

#include "EditTxn.h"                    // for EditTxn, NS_DECL_EDITTXN
#include "nsString.h"                   // for nsString
#include "nscore.h"                     // for NS_IMETHOD, nsAString, etc

class nsIHTMLEditor;

/**
 * A transaction that changes the document's title,
 *  which is a text node under the <title> tag in a page's <head> section
 * provides default concrete behavior for all nsITransaction methods.
 */
class SetDocTitleTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aValue  the new value for document title
    */
  NS_IMETHOD Init(nsIHTMLEditor  *aEditor,
                  const nsAString *aValue);
  SetDocTitleTxn();
private:
  nsresult SetDomTitle(const nsAString& aTitle);

public:
  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();
  NS_IMETHOD GetIsTransient(bool *aIsTransient);

protected:

  /** the editor that created this transaction */
  nsIHTMLEditor*  mEditor;
  
  /** The new title string */
  nsString    mValue;

  /** The previous title string to use for undo */
  nsString    mUndoValue;

  /** Set true if we dont' really change the title during Do() */
  bool mIsTransient;
};

#endif
