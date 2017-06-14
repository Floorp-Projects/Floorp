/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozPersonalDictionary_h__
#define mozPersonalDictionary_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozIPersonalDictionary.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsTHashtable.h"
#include "nsCRT.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include <mozilla/Monitor.h>

#define MOZ_PERSONALDICTIONARY_CONTRACTID "@mozilla.org/spellchecker/personaldictionary;1"
#define MOZ_PERSONALDICTIONARY_CID         \
{ /* 7EF52EAF-B7E1-462B-87E2-5D1DBACA9048 */  \
0X7EF52EAF, 0XB7E1, 0X462B, \
  { 0X87, 0XE2, 0X5D, 0X1D, 0XBA, 0XCA, 0X90, 0X48 } }

class mozPersonalDictionaryLoader;
class mozPersonalDictionarySave;

class mozPersonalDictionary final : public mozIPersonalDictionary,
                                    public nsIObserver,
                                    public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIPERSONALDICTIONARY
  NS_DECL_NSIOBSERVER

  mozPersonalDictionary();

  nsresult Init();

protected:
  virtual ~mozPersonalDictionary();

  /* true if the dictionary has been loaded from disk */
  bool mIsLoaded;

  /* true if a dictionary save is pending */
  bool mSavePending;

  nsCOMPtr<nsIFile> mFile;
  mozilla::Monitor mMonitor;
  mozilla::Monitor mMonitorSave;
  nsTHashtable<nsUnicharPtrHashKey> mDictionaryTable;
  nsTHashtable<nsUnicharPtrHashKey> mIgnoreTable;

private:
  /* wait for the asynchronous load of the dictionary to be completed */
  void WaitForLoad();

  /* enter the monitor before starting a synchronous load off the main-thread */
  void SyncLoad();

  /* launch an asynchrounous load of the dictionary from the main-thread
   * after successfully initializing mFile with the path of the dictionary */
  nsresult LoadInternal();

  /* perform a synchronous load of the dictionary from disk */
  void SyncLoadInternal();

  /* wait for the asynchronous save of the dictionary to be completed */
  void WaitForSave();

  friend class mozPersonalDictionaryLoader;
  friend class mozPersonalDictionarySave;
};

#endif
