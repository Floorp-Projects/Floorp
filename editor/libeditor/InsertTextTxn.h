/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InsertTextTxn_h__
#define InsertTextTxn_h__

#include "EditTxn.h"                    // base class
#include "nsAutoPtr.h"                  // nsRefPtr members
#include "nsCycleCollectionParticipant.h" // various macros
#include "nsID.h"                       // NS_DECLARE_STATIC_IID_ACCESSOR
#include "nsISupportsImpl.h"            // NS_DECL_ISUPPORTS_INHERITED
#include "nsString.h"                   // nsString members
#include "nscore.h"                     // NS_IMETHOD, nsAString

class nsEditor;
class nsITransaction;

#define NS_INSERTTEXTTXN_IID \
{ 0x8c9ad77f, 0x22a7, 0x4d01, \
  { 0xb1, 0x59, 0x8a, 0x0f, 0xdb, 0x1d, 0x08, 0xe9 } }

namespace mozilla {
namespace dom {

class Text;

/**
  * A transaction that inserts text into a content node.
  */
class InsertTextTxn : public EditTxn
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INSERTTEXTTXN_IID)

  /** @param aElement the text content node
    * @param aOffset  the location in aElement to do the insertion
    * @param aString  the new text to insert
    * @param aPresShell used to get and set the selection
    */
  InsertTextTxn(Text& aTextNode, uint32_t aOffset, const nsAString& aString,
                nsEditor& aEditor);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertTextTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

  /** Return the string data associated with this transaction */
  void GetData(nsString& aResult);

private:
  virtual ~InsertTextTxn();

  /** Return true if aOtherTxn immediately follows this txn */
  bool IsSequentialInsert(InsertTextTxn& aOtherTxn);

  /** The Text node to operate upon */
  nsRefPtr<Text> mTextNode;

  /** The offset into mTextNode where the insertion is to take place */
  uint32_t mOffset;

  /** The text to insert into mTextNode at mOffset */
  nsString mStringToInsert;

  /** The editor, which we'll need to get the selection */
  nsEditor& mEditor;
};

NS_DEFINE_STATIC_IID_ACCESSOR(InsertTextTxn, NS_INSERTTEXTTXN_IID)

}
}

#endif
