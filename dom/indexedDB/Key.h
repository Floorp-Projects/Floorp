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

#ifndef mozilla_dom_indexeddb_key_h__
#define mozilla_dom_indexeddb_key_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozIStorageStatement.h"

#include "xpcprivate.h"
#include "XPCQuickStubs.h"

BEGIN_INDEXEDDB_NAMESPACE

class Key
{
public:
  Key()
  {
    Unset();
  }

  Key& operator=(const nsAString& aString)
  {
    SetFromString(aString);
    return *this;
  }

  Key& operator=(PRInt64 aInt)
  {
    SetFromInteger(aInt);
    return *this;
  }

  bool operator==(const Key& aOther) const
  {
     NS_ASSERTION(mType != KEYTYPE_VOID && aOther.mType != KEYTYPE_VOID,
                 "Don't compare unset keys!");

    if (mType == aOther.mType) {
      switch (mType) {
        case KEYTYPE_STRING:
          return ToString() == aOther.ToString();

        case KEYTYPE_INTEGER:
          return ToInteger() == aOther.ToInteger();

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
    NS_ASSERTION(mType != KEYTYPE_VOID && aOther.mType != KEYTYPE_VOID,
                 "Don't compare unset keys!");

    switch (mType) {
      case KEYTYPE_STRING: {
        if (aOther.mType == KEYTYPE_INTEGER) {
          return false;
        }
        NS_ASSERTION(aOther.mType == KEYTYPE_STRING, "Unknown type!");
        return ToString() < aOther.ToString();
      }

      case KEYTYPE_INTEGER:
        if (aOther.mType == KEYTYPE_STRING) {
          return true;
        }
        NS_ASSERTION(aOther.mType == KEYTYPE_INTEGER, "Unknown type!");
        return ToInteger() < aOther.ToInteger();

      default:
        NS_NOTREACHED("Unknown type!");
    }
    return false;
  }

  bool operator>(const Key& aOther) const
  {
    return !(*this == aOther || *this < aOther);
  }

  bool operator<=(const Key& aOther) const
  {
    return (*this == aOther || *this < aOther);
  }

  bool operator>=(const Key& aOther) const
  {
    return (*this == aOther || !(*this < aOther));
  }

  void
  Unset()
  {
    mType = KEYTYPE_VOID;
    mStringKey.SetIsVoid(true);
    mIntKey = 0;
  }

  bool IsUnset() const { return mType == KEYTYPE_VOID; }
  bool IsString() const { return mType == KEYTYPE_STRING; }
  bool IsInteger() const { return mType == KEYTYPE_INTEGER; }

  nsresult SetFromString(const nsAString& aString)
  {
    mType = KEYTYPE_STRING;
    mStringKey = aString;
    mIntKey = 0;
    return NS_OK;
  }

  nsresult SetFromInteger(PRInt64 aInt)
  {
    mType = KEYTYPE_INTEGER;
    mStringKey.SetIsVoid(true);
    mIntKey = aInt;
    return NS_OK;
  }

  nsresult SetFromJSVal(JSContext* aCx,
                        jsval aVal)
  {
    if (JSVAL_IS_STRING(aVal)) {
      jsval tempRoot = JSVAL_VOID;
      SetFromString(xpc_qsAString(aCx, aVal, &tempRoot));
      return NS_OK;
    }

    if (JSVAL_IS_INT(aVal)) {
      SetFromInteger(JSVAL_TO_INT(aVal));
      return NS_OK;
    }

    if (JSVAL_IS_DOUBLE(aVal)) {
      jsdouble doubleActual = JSVAL_TO_DOUBLE(aVal);
      int64 doubleAsInt = static_cast<int64>(doubleActual);
      if (doubleActual == doubleAsInt) {
        SetFromInteger(doubleAsInt);
        return NS_OK;
      }
    }

    if (JSVAL_IS_NULL(aVal) || JSVAL_IS_VOID(aVal)) {
      Unset();
      return NS_OK;
    }

    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  nsresult ToJSVal(JSContext* aCx,
                   jsval* aVal) const
  {
    if (IsString()) {
      nsString key = ToString();
      if (!xpc_qsStringToJsval(aCx, key, aVal)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }
    else if (IsInteger()) {
      if (!JS_NewNumberValue(aCx, static_cast<jsdouble>(ToInteger()), aVal)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }
    else if (IsUnset()) {
      *aVal = JSVAL_VOID;
    }
    else {
      NS_NOTREACHED("Unknown key type!");
    }
    return NS_OK;
  }

  PRInt64 ToInteger() const
  {
    NS_ASSERTION(IsInteger(), "Don't call me!");
    return mIntKey;
  }

  const nsString& ToString() const
  {
    NS_ASSERTION(IsString(), "Don't call me!");
    return mStringKey;
  }

  nsresult BindToStatement(mozIStorageStatement* aStatement,
                           const nsACString& aParamName) const
  {
    nsresult rv;

    if (IsString()) {
      rv = aStatement->BindStringByName(aParamName, ToString());
    }
    else if (IsInteger()) {
      rv = aStatement->BindInt64ByName(aParamName, ToInteger());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }

    return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult BindToStatementAllowUnset(mozIStorageStatement* aStatement,
                                     const nsACString& aParamName) const
  {
    nsresult rv;

    if (IsUnset()) {
      rv = aStatement->BindStringByName(aParamName, EmptyString());
      return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    return BindToStatement(aStatement, aParamName);
  }

  nsresult SetFromStatement(mozIStorageStatement* aStatement,
                            PRUint32 aIndex)
  {
    PRInt32 columnType;
    nsresult rv = aStatement->GetTypeOfIndex(aIndex, &columnType);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(columnType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 columnType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Unsupported column type!");

    return SetFromStatement(aStatement, aIndex, columnType);
  }

  nsresult SetFromStatement(mozIStorageStatement* aStatement,
                            PRUint32 aIndex,
                            PRInt32 aColumnType)
  {
    if (aColumnType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      return SetFromInteger(aStatement->AsInt64(aIndex));
    }

    if (aColumnType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      nsString keyString;
      nsresult rv = aStatement->GetString(aIndex, keyString);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      return SetFromString(keyString);
    }

    NS_NOTREACHED("Unsupported column type!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  static
  bool CanBeConstructedFromJSVal(jsval aVal)
  {
    return JSVAL_IS_INT(aVal) || JSVAL_IS_DOUBLE(aVal) || JSVAL_IS_STRING(aVal);
  }

private:
  // Wish we could use JSType here but we will end up supporting types like Date
  // which JSType can't really identify. Rolling our own for now.
  enum Type {
    KEYTYPE_VOID,
    KEYTYPE_STRING,
    KEYTYPE_INTEGER
  };

  // Type of value in mJSVal.
  Type mType;

  // The string if mType is KEYTYPE_STRING, otherwise a void string.
  nsString mStringKey;

  // The integer value if mType is KEYTYPE_INTEGER, otherwise 0.
  int64 mIntKey;
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_key_h__ */
