/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEditorTxnLog_h__
#define nsEditorTxnLog_h__

#include "nsITransaction.h"
#include "nsITransactionManager.h"
#include "nsITransactionListener.h"

class nsHTMLEditorLog;

/** implementation of a transaction listener object.
 *
 */
class nsEditorTxnLog : public nsITransactionListener
{
private:

  nsHTMLEditorLog *mEditorLog;
  int32_t mIndentLevel;
  int32_t mBatchCount;

public:

  /** The default constructor.
   */
  nsEditorTxnLog(nsHTMLEditorLog *aEditorLog=0);

  /** The default destructor.
   */
  virtual ~nsEditorTxnLog();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITransactionListener method implementations. */
  NS_DECL_NSITRANSACTIONLISTENER

private:

  /* nsEditorTxnLog private methods. */
  nsresult PrintIndent(int32_t aIndentLevel);
  nsresult Write(const char *aBuffer);
  nsresult WriteInt(int32_t aInt);
  nsresult WriteTransaction(nsITransaction *aTransaction);
  nsresult Flush();
};

#endif // nsEditorTxnLog_h__
