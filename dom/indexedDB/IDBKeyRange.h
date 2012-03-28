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

#ifndef mozilla_dom_indexeddb_idbkeyrange_h__
#define mozilla_dom_indexeddb_idbkeyrange_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/Key.h"

#include "nsIIDBKeyRange.h"

#include "nsCycleCollectionParticipant.h"

class mozIStorageStatement;

BEGIN_INDEXEDDB_NAMESPACE

class IDBKeyRange : public nsIIDBKeyRange
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIIDBKEYRANGE
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBKeyRange)

  static JSBool DefineConstructors(JSContext* aCx,
                                   JSObject* aObject);

  static
  nsresult FromJSVal(JSContext* aCx,
                     const jsval& aVal,
                     IDBKeyRange** aKeyRange);

  IDBKeyRange(bool aLowerOpen, bool aUpperOpen, bool aIsOnly)
  : mCachedLowerVal(JSVAL_VOID), mCachedUpperVal(JSVAL_VOID),
    mLowerOpen(aLowerOpen), mUpperOpen(aUpperOpen), mIsOnly(aIsOnly),
    mHaveCachedLowerVal(false), mHaveCachedUpperVal(false), mRooted(false)
  { }

  const Key& Lower() const
  {
    return mLower;
  }

  Key& Lower()
  {
    return mLower;
  }

  const Key& Upper() const
  {
    return mIsOnly ? mLower : mUpper;
  }

  Key& Upper()
  {
    return mIsOnly ? mLower : mUpper;
  }

  bool IsLowerOpen() const
  {
    return mLowerOpen;
  }

  bool IsUpperOpen() const
  {
    return mUpperOpen;
  }

  bool IsOnly() const
  {
    return mIsOnly;
  }

  void GetBindingClause(const nsACString& aKeyColumnName,
                        nsACString& _retval) const
  {
    NS_NAMED_LITERAL_CSTRING(andStr, " AND ");
    NS_NAMED_LITERAL_CSTRING(spacecolon, " :");
    NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

    if (IsOnly()) {
      // Both keys are set and they're equal.
      _retval = andStr + aKeyColumnName + NS_LITERAL_CSTRING(" =") + spacecolon +
                lowerKey;
    }
    else {
      nsCAutoString clause;

      if (!Lower().IsUnset()) {
        // Lower key is set.
        clause.Append(andStr + aKeyColumnName);
        clause.AppendLiteral(" >");
        if (!IsLowerOpen()) {
          clause.AppendLiteral("=");
        }
        clause.Append(spacecolon + lowerKey);
      }

      if (!Upper().IsUnset()) {
        // Upper key is set.
        clause.Append(andStr + aKeyColumnName);
        clause.AppendLiteral(" <");
        if (!IsUpperOpen()) {
          clause.AppendLiteral("=");
        }
        clause.Append(spacecolon + NS_LITERAL_CSTRING("upper_key"));
      }

      _retval = clause;
    }
  }

  nsresult BindToStatement(mozIStorageStatement* aStatement) const
  {
    NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

    if (IsOnly()) {
      return Lower().BindToStatement(aStatement, lowerKey);
    }

    nsresult rv;

    if (!Lower().IsUnset()) {
      rv = Lower().BindToStatement(aStatement, lowerKey);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }

    if (!Upper().IsUnset()) {
      rv = Upper().BindToStatement(aStatement, NS_LITERAL_CSTRING("upper_key"));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }

    return NS_OK;
  }

protected:
  ~IDBKeyRange();

  Key mLower;
  Key mUpper;
  jsval mCachedLowerVal;
  jsval mCachedUpperVal;
  bool mLowerOpen;
  bool mUpperOpen;
  bool mIsOnly;
  bool mHaveCachedLowerVal;
  bool mHaveCachedUpperVal;
  bool mRooted;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbkeyrange_h__
