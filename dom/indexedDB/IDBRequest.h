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
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#ifndef mozilla_dom_indexeddb_idbrequest_h__
#define mozilla_dom_indexeddb_idbrequest_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBRequest.h"
#include "nsIVariant.h"

#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class IDBFactory;
class IDBDatabase;

class IDBRequest : public nsDOMEventTargetHelper,
                   public nsIIDBRequest
{
  friend class AsyncConnectionHelper;

public:
  class Generator : public nsISupports
  {
    protected:
      friend class IDBRequest;

      Generator() { }

      virtual ~Generator() {
        NS_ASSERTION(mLiveRequests.IsEmpty(), "Huh?!");
      }

      IDBRequest* GenerateRequest() {
        IDBRequest* request = new IDBRequest(this, false);
        if (!mLiveRequests.AppendElement(request)) {
          NS_ERROR("Append failed!");
        }
        return request;
      }

      IDBRequest* GenerateWriteRequest() {
        IDBRequest* request = new IDBRequest(this, true);
        if (!mLiveRequests.AppendElement(request)) {
          NS_ERROR("Append failed!");
        }
        return request;
      }

      void NoteDyingRequest(IDBRequest* aRequest) {
        NS_ASSERTION(mLiveRequests.Contains(aRequest), "Unknown request!");
        mLiveRequests.RemoveElement(aRequest);
      }

    private:
      // XXXbent Assuming infallible nsTArray here, make sure it lands!
      nsAutoTArray<IDBRequest*, 1> mLiveRequests;
  };

  friend class IDBRequestGenerator;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBREQUEST
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBRequest,
                                           nsDOMEventTargetHelper)

  already_AddRefed<nsISupports> GetGenerator()
  {
    nsCOMPtr<nsISupports> generator(mGenerator);
    return generator.forget();
  }

private:
  // Only called by IDBRequestGenerator::Generate().
  IDBRequest(Generator* aGenerator,
             bool aWriteRequest);

  nsRefPtr<Generator> mGenerator;

protected:
  // Called by Release().
  ~IDBRequest();

  PRUint16 mReadyState;
  PRBool mAborted;
  PRBool mWriteRequest;
  nsRefPtr<nsDOMEventListenerWrapper> mOnSuccessListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbrequest_h__
