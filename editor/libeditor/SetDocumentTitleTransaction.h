/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SetDocumentTitleTransaction_h
#define SetDocumentTitleTransaction_h

#include "mozilla/EditTransactionBase.h" // for EditTransactionBase, etc.
#include "nsString.h"                   // for nsString
#include "nscore.h"                     // for NS_IMETHOD, nsAString, etc.

class nsIHTMLEditor;

namespace mozilla {

/**
 * A transaction that changes the document's title,
 *  which is a text node under the <title> tag in a page's <head> section
 * provides default concrete behavior for all nsITransaction methods.
 */
class SetDocumentTitleTransaction final : public EditTransactionBase
{
public:
  /**
   * Initialize the transaction.
   * @param aEditor     The object providing core editing operations.
   * @param aValue      The new value for document title.
   */
  NS_IMETHOD Init(nsIHTMLEditor* aEditor,
                  const nsAString* aValue);
  SetDocumentTitleTransaction();

private:
  nsresult SetDomTitle(const nsAString& aTitle);

public:
  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;
  NS_IMETHOD GetIsTransient(bool *aIsTransient) override;

protected:

  // The editor that created this transaction.
  nsIHTMLEditor* mEditor;

  // The new title string.
  nsString mValue;

  // The previous title string to use for undo.
  nsString mUndoValue;

  // Set true if we dont' really change the title during Do().
  bool mIsTransient;
};

} // namespace mozilla

#endif // #ifndef SetDocumentTitleTransaction_h
