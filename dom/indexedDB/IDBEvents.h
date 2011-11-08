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

already_AddRefed<nsDOMEvent>
CreateGenericEvent(const nsAString& aType,
                   bool aBubblesAndCancelable = false);

already_AddRefed<nsIRunnable>
CreateGenericEventRunnable(const nsAString& aType,
                           nsIDOMEventTarget* aTarget);

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
  IDBVersionChangeEvent() : nsDOMEvent(nsnull, nsnull) { }
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
