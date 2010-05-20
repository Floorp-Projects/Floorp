/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_dom_indexeddb_idbobjectstorerequest_h__
#define mozilla_dom_indexeddb_idbobjectstorerequest_h__

#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/IDBDatabaseRequest.h"

#include "nsIIDBObjectStoreRequest.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBTransactionRequest;

class Key
{
public:
  enum Type { UNSETKEY, NULLKEY, STRINGKEY, INTKEY };

  Key()
  : mType(UNSETKEY), mInt(0)
  { }

  Key(const Key& aOther)
  {
    *this = aOther;
  }

  Key& operator=(const Key& aOther)
  {
    if (this != &aOther) {
      mType = aOther.mType;
      mString = aOther.mString;
      mInt = aOther.mInt;
    }
    return *this;
  }

  Key& operator=(Type aType)
  {
    NS_ASSERTION(aType == UNSETKEY || aType == NULLKEY,
                 "Use one of the other operators to assign your value!");
    mType = aType;
    mString.Truncate();
    mInt = 0;
    return *this;
  }

  Key& operator=(const nsAString& aString)
  {
    mType = STRINGKEY;
    mString = aString;
    mInt = 0;
    return *this;
  }

  Key& operator=(PRInt64 aInt)
  {
    mType = INTKEY;
    mString.Truncate();
    mInt = aInt;
    return *this;
  }

  bool IsUnset() const { return mType == UNSETKEY; }
  bool IsNull() const { return mType == NULLKEY; }
  bool IsString() const { return mType == STRINGKEY; }
  bool IsInt() const { return mType == INTKEY; }

  const nsString& StringValue() const {
    NS_ASSERTION(IsString(), "Wrong type!");
    return mString;
  }

  PRInt64 IntValue() const {
    NS_ASSERTION(IsInt(), "Wrong type!");
    return mInt;
  }

  nsAString& ToString() {
    mType = STRINGKEY;
    mInt = 0;
    return mString;
  }

  PRInt64* ToIntPtr() {
    mType = INTKEY;
    mString.Truncate();
    return &mInt;
  }

private:
  Type mType;
  nsString mString;
  PRInt64 mInt;
};

class IDBObjectStoreRequest : public IDBRequest::Generator,
                              public nsIIDBObjectStoreRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIDBOBJECTSTORE
  NS_DECL_NSIIDBOBJECTSTOREREQUEST

  static already_AddRefed<IDBObjectStoreRequest>
  Create(IDBDatabaseRequest* aDatabase,
         IDBTransactionRequest* aTransaction,
         const ObjectStoreInfo& aStoreInfo,
         PRUint16 aMode);

protected:
  IDBObjectStoreRequest();
  ~IDBObjectStoreRequest();

  nsresult GetJSONAndKeyForAdd(/* jsval aValue, */
                               nsIVariant* aKeyVariant,
                               nsString& aJSON,
                               Key& aKey);

private:
  nsRefPtr<IDBDatabaseRequest> mDatabase;
  nsRefPtr<IDBTransactionRequest> mTransaction;

  nsString mName;
  PRInt64 mId;
  nsString mKeyPath;
  PRBool mAutoIncrement;
  PRUint16 mMode;

  nsTArray<nsString> mIndexes;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbobjectstorerequest_h__
