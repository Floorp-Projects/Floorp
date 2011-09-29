/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
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
 * The Original Code is Mozilla DOM Storage.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (original author)
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

#ifndef mozilla_dom_StorageParent_h
#define mozilla_dom_StorageParent_h

#include "mozilla/dom/PStorageParent.h"
#include "nsDOMStorage.h"

namespace mozilla {
namespace dom {

class StorageConstructData;

class StorageParent : public PStorageParent
{
public:
  StorageParent(const StorageConstructData& aData);

private:
  bool RecvGetKeys(const bool& aCallerSecure, InfallibleTArray<nsString>* aKeys);
  bool RecvGetLength(const bool& aCallerSecure, const bool& aSessionOnly,
                     PRUint32* aLength, nsresult* rv);
  bool RecvGetKey(const bool& aCallerSecure, const bool& aSessionOnly,
                  const PRUint32& aIndex,nsString* aKey, nsresult* rv);
  bool RecvGetValue(const bool& aCallerSecure, const bool& aSessionOnly,
                    const nsString& aKey, StorageItem* aItem, nsresult* rv);
  bool RecvSetValue(const bool& aCallerSecure, const bool& aSessionOnly,
                    const nsString& aKey, const nsString& aData,
                    nsString* aOldValue, nsresult* rv);
  bool RecvRemoveValue(const bool& aCallerSecure, const bool& aSessionOnly,
                       const nsString& aKey, nsString* aOldData, nsresult* rv);
  bool RecvClear(const bool& aCallerSecure, const bool& aSessionOnly,
                 PRInt32* aOldCount, nsresult* rv);

  bool RecvGetDBValue(const nsString& aKey, nsString* aValue, bool* aSecure,
                      nsresult* rv);
  bool RecvSetDBValue(const nsString& aKey, const nsString& aValue,
                      const bool& aSecure, nsresult* rv);
  bool RecvSetSecure(const nsString& aKey, const bool& aSecure, nsresult* rv);

  bool RecvInit(const bool& aUseDB,
                const bool& aCanUseChromePersist,
                const bool& aSessionOnly,
                const nsCString& aDomain,
                const nsCString& aScopeDBKey,
                const nsCString& aQuotaDomainDBKey,
                const nsCString& aQuotaETLDplus1DomainDBKey,
                const PRUint32& aStorageType);

  nsRefPtr<DOMStorageImpl> mStorage;
};

}
}

#endif
