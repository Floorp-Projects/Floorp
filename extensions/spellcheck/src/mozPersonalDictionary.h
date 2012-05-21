/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozPersonalDictionary_h__
#define mozPersonalDictionary_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "mozIPersonalDictionary.h"
#include "nsIUnicodeEncoder.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsTHashtable.h"
#include "nsCRT.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"

#define MOZ_PERSONALDICTIONARY_CONTRACTID "@mozilla.org/spellchecker/personaldictionary;1"
#define MOZ_PERSONALDICTIONARY_CID         \
{ /* 7EF52EAF-B7E1-462B-87E2-5D1DBACA9048 */  \
0X7EF52EAF, 0XB7E1, 0X462B, \
  { 0X87, 0XE2, 0X5D, 0X1D, 0XBA, 0XCA, 0X90, 0X48 } }

class mozPersonalDictionary : public mozIPersonalDictionary,
                              public nsIObserver,
                              public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_MOZIPERSONALDICTIONARY
  NS_DECL_NSIOBSERVER
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(mozPersonalDictionary, mozIPersonalDictionary)

  mozPersonalDictionary();
  virtual ~mozPersonalDictionary();

  nsresult Init();

protected:
  bool           mDirty;       /* has the dictionary been modified */
  nsTHashtable<nsUnicharPtrHashKey> mDictionaryTable;
  nsTHashtable<nsUnicharPtrHashKey> mIgnoreTable;
  nsCOMPtr<nsIUnicodeEncoder>  mEncoder; /*Encoder to use to compare with spellchecker word */
};

#endif
