/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbevents_h__
#define mozilla_dom_indexeddb_idbevents_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBVersionChangeEvent.h"
#include "nsIRunnable.h"

#include "nsDOMEvent.h"

#include "mozilla/dom/indexedDB/IDBObjectStore.h"

#define SUCCESS_EVT_STR "success"
#define ERROR_EVT_STR "error"
#define COMPLETE_EVT_STR "complete"
#define ABORT_EVT_STR "abort"
#define VERSIONCHANGE_EVT_STR "versionchange"
#define BLOCKED_EVT_STR "blocked"
#define UPGRADENEEDED_EVT_STR "upgradeneeded"

BEGIN_INDEXEDDB_NAMESPACE

enum Bubbles {
  eDoesNotBubble,
  eDoesBubble
};

enum Cancelable {
  eNotCancelable,
  eCancelable
};

already_AddRefed<nsDOMEvent>
CreateGenericEvent(const nsAString& aType,
                   Bubbles aBubbles,
                   Cancelable aCancelable);

class IDBVersionChangeEvent : public nsDOMEvent,
                              public nsIIDBVersionChangeEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIIDBVERSIONCHANGEEVENT

  inline static already_AddRefed<nsDOMEvent>
  Create(PRInt64 aOldVersion,
         PRInt64 aNewVersion)
  {
    return CreateInternal(NS_LITERAL_STRING(VERSIONCHANGE_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<nsDOMEvent>
  CreateBlocked(PRUint64 aOldVersion,
                PRUint64 aNewVersion)
  {
    return CreateInternal(NS_LITERAL_STRING(BLOCKED_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<nsDOMEvent>
  CreateUpgradeNeeded(PRUint64 aOldVersion,
                      PRUint64 aNewVersion)
  {
    return CreateInternal(NS_LITERAL_STRING(UPGRADENEEDED_EVT_STR),
                          aOldVersion, aNewVersion);
  }

  inline static already_AddRefed<nsIRunnable>
  CreateRunnable(PRUint64 aOldVersion,
                 PRUint64 aNewVersion,
                 nsIDOMEventTarget* aTarget)
  {
    return CreateRunnableInternal(NS_LITERAL_STRING(VERSIONCHANGE_EVT_STR),
                                  aOldVersion, aNewVersion, aTarget);
  }

  static already_AddRefed<nsIRunnable>
  CreateBlockedRunnable(PRUint64 aOldVersion,
                        PRUint64 aNewVersion,
                        nsIDOMEventTarget* aTarget)
  {
    return CreateRunnableInternal(NS_LITERAL_STRING(BLOCKED_EVT_STR),
                                  aOldVersion, aNewVersion, aTarget);
  }

protected:
  IDBVersionChangeEvent() : nsDOMEvent(nullptr, nullptr) { }
  virtual ~IDBVersionChangeEvent() { }

  static already_AddRefed<nsDOMEvent>
  CreateInternal(const nsAString& aType,
                 PRUint64 aOldVersion,
                 PRUint64 aNewVersion);

  static already_AddRefed<nsIRunnable>
  CreateRunnableInternal(const nsAString& aType,
                         PRUint64 aOldVersion,
                         PRUint64 aNewVersion,
                         nsIDOMEventTarget* aTarget);

  PRUint64 mOldVersion;
  PRUint64 mNewVersion;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbevents_h__
