/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein Deinst@world.std.com
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

#define MOZ_PERSONALDICTIONARY_CONTRACTID "@mozilla.org/spellchecker/personaldictionary;1"
#define MOZ_PERSONALDICTIONARY_CID         \
{ /* 7EF52EAF-B7E1-462B-87E2-5D1DBACA9048 */  \
0X7EF52EAF, 0XB7E1, 0X462B, \
  { 0X87, 0XE2, 0X5D, 0X1D, 0XBA, 0XCA, 0X90, 0X48 } }

class nsUniCharEntry : public PLDHashEntryHdr
{
public:
  // Hash methods
  typedef const PRUnichar* KeyType;
  typedef const PRUnichar* KeyTypePointer;

  nsUniCharEntry(const PRUnichar* aKey) : mKey(nsCRT::strdup(aKey)) {}
  nsUniCharEntry(const nsUniCharEntry& toCopy)
  { 
    NS_NOTREACHED("ALLOW_MEMMOVE is set, so copy ctor shouldn't be called");
  }

  ~nsUniCharEntry()
  { 
    if (mKey)
      nsCRT::free(mKey);
  }
 
  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const { return !nsCRT::strcmp(mKey, aKey); }
  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) { return nsCRT::HashCode(aKey); }

  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  PRUnichar *mKey;
};


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
  nsTHashtable<nsUniCharEntry> mDictionaryTable;
  nsTHashtable<nsUniCharEntry> mIgnoreTable;
  nsCOMPtr<nsIUnicodeEncoder>  mEncoder; /*Encoder to use to compare with spellchecker word */
};

#endif
