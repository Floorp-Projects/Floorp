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

#ifndef mozilla_dom_indexeddb_idbcursor_h__
#define mozilla_dom_indexeddb_idbcursor_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IDBObjectStore.h"

#include "nsIIDBCursor.h"

#include "nsDOMEventTargetHelper.h"

class nsIRunnable;

BEGIN_INDEXEDDB_NAMESPACE

class IDBIndex;
class IDBRequest;
class IDBTransaction;

struct KeyValuePair
{
  Key key;
  nsString value;
};

struct KeyKeyPair
{
  Key key;
  Key value;
};

class ContinueRunnable;

class IDBCursor : public nsDOMEventTargetHelper,
                  public nsIIDBCursor
{
  friend class ContinueRunnable;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBCURSOR

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBCursor,
                                           nsDOMEventTargetHelper)

  static
  already_AddRefed<IDBCursor>
  Create(IDBRequest* aRequest,
         IDBTransaction* aTransaction,
         IDBObjectStore* aObjectStore,
         PRUint16 aDirection,
         nsTArray<KeyValuePair>& aData);

  static
  already_AddRefed<IDBCursor>
  Create(IDBRequest* aRequest,
         IDBTransaction* aTransaction,
         IDBIndex* aIndex,
         PRUint16 aDirection,
         nsTArray<KeyKeyPair>& aData);

  static
  already_AddRefed<IDBCursor>
  Create(IDBRequest* aRequest,
         IDBTransaction* aTransaction,
         IDBIndex* aIndex,
         PRUint16 aDirection,
         nsTArray<KeyValuePair>& aData);

  enum Type
  {
    OBJECTSTORE = 0,
    INDEX,
    INDEXOBJECT
  };

  IDBTransaction* Transaction()
  {
    return mTransaction;
  }

protected:
  IDBCursor();
  ~IDBCursor();

  static
  already_AddRefed<IDBCursor>
  CreateCommon(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               PRUint16 aDirection);

  nsRefPtr<IDBRequest> mRequest;
  nsRefPtr<IDBTransaction> mTransaction;
  nsRefPtr<IDBObjectStore> mObjectStore;
  nsRefPtr<IDBIndex> mIndex;

  PRUint16 mDirection;

  nsCOMPtr<nsIVariant> mCachedKey;
  jsval mCachedValue;
  bool mHaveCachedValue;
  JSRuntime* mJSRuntime;

  bool mContinueCalled;
  PRUint32 mDataIndex;

  Type mType;
  nsTArray<KeyValuePair> mData;
  nsTArray<KeyKeyPair> mKeyData;

  // Only touched on the main thread.
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbcursor_h__
