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

#ifndef mozilla_dom_indexeddb_idbobjectstore_h__
#define mozilla_dom_indexeddb_idbobjectstore_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IDBTransaction.h"

#include "nsIIDBObjectStore.h"
#include "nsIIDBTransaction.h"

#include "nsDOMEventTargetHelper.h"

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;

struct ObjectStoreInfo;
struct IndexInfo;
struct IndexUpdateInfo;

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

  bool operator==(const Key& aOther) const
  {
    if (mType == aOther.mType) {
      switch (mType) {
        case UNSETKEY:
        case NULLKEY:
          return true;

        case STRINGKEY:
          return mString == aOther.mString;

        case INTKEY:
          return mInt == aOther.mInt;

        default:
          NS_NOTREACHED("Unknown type!");
      }
    }
    return false;
  }

  bool operator!=(const Key& aOther) const
  {
    return !(*this == aOther);
  }

  bool operator<(const Key& aOther) const
  {
    switch (mType) {
      case UNSETKEY:
        if (aOther.mType == UNSETKEY) {
          return false;
        }
        return true;

      case NULLKEY:
        if (aOther.mType == UNSETKEY ||
            aOther.mType == NULLKEY) {
          return false;
        }
        return true;

      case STRINGKEY:
        if (aOther.mType == UNSETKEY ||
            aOther.mType == NULLKEY ||
            aOther.mType == INTKEY) {
          return false;
        }
        NS_ASSERTION(aOther.mType == STRINGKEY, "Unknown type!");
        return mString < aOther.mString;

      case INTKEY:
        if (aOther.mType == UNSETKEY ||
            aOther.mType == NULLKEY) {
          return false;
        }
        if (aOther.mType == STRINGKEY) {
          return true;
        }
        NS_ASSERTION(aOther.mType == INTKEY, "Unknown type!");
        return mInt < aOther.mInt;

      default:
        NS_NOTREACHED("Unknown type!");
    }
    return false;
  }

  bool operator>(const Key& aOther) const
  {
    return !(*this == aOther || *this < aOther);
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

class IDBObjectStore : public nsDOMEventTargetHelper,
                       public nsIIDBObjectStore
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBOBJECTSTORE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBObjectStore,
                                           nsDOMEventTargetHelper)

  static already_AddRefed<IDBObjectStore>
  Create(IDBTransaction* aTransaction,
         const ObjectStoreInfo* aInfo);

  static nsresult
  GetKeyFromVariant(nsIVariant* aKeyVariant,
                    Key& aKey);

  static nsresult
  GetKeyFromJSVal(jsval aKeyVal,
                  Key& aKey);

  static nsresult
  GetJSValFromKey(const Key& aKey,
                  JSContext* aCx,
                  jsval* aKeyVal);

  static nsresult
  GetJSONFromArg0(/* jsval arg0, */
                  nsAString& aJSON);

  static nsresult
  GetKeyPathValueFromJSON(const nsAString& aJSON,
                          const nsAString& aKeyPath,
                          JSContext** aCx,
                          Key& aValue);

  static nsresult
  GetIndexUpdateInfo(ObjectStoreInfo* aObjectStoreInfo,
                     JSContext* aCx,
                     jsval aObject,
                     nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  static nsresult
  UpdateIndexes(IDBTransaction* aTransaction,
                PRInt64 aObjectStoreId,
                const Key& aObjectStoreKey,
                bool aAutoIncrement,
                bool aOverwrite,
                PRInt64 aObjectDataId,
                const nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  const nsString& Name() const
  {
    return mName;
  }

  bool TransactionIsOpen() const
  {
    return mTransaction->TransactionIsOpen();
  }

  bool IsAutoIncrement() const
  {
    return mAutoIncrement;
  }

  bool IsWriteAllowed() const
  {
    return mTransaction->IsWriteAllowed();
  }

  PRInt64 Id() const
  {
    NS_ASSERTION(mId != LL_MININT, "Don't ask for this yet!");
    return mId;
  }

  const nsString& KeyPath() const
  {
    return mKeyPath;
  }

  IDBTransaction* Transaction()
  {
    return mTransaction;
  }

  ObjectStoreInfo* GetObjectStoreInfo();

protected:
  IDBObjectStore();
  ~IDBObjectStore();

  nsresult GetAddInfo(JSContext* aCx,
                      jsval aValue,
                      jsval aKeyVal,
                      nsString& aJSON,
                      Key& aKey,
                      nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

private:
  nsRefPtr<IDBTransaction> mTransaction;

  PRInt64 mId;
  nsString mName;
  nsString mKeyPath;
  PRBool mAutoIncrement;
  PRUint32 mDatabaseId;

  // Only touched on the main thread.
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbobjectstore_h__
