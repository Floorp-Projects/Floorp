/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsPermissionManager_h__
#define nsPermissionManager_h__

#include "nsIPermissionManager.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsTHashtable.h"
#include "nsString.h"
#include "nsITimer.h"

class nsIPermission;

////////////////////////////////////////////////////////////////////////////////

// This allow us 8 types of permissions, with 256 values for each
// permission (some types might want to prompt, block 3rd party etc).
// Note that nsIPermissionManager.idl only allows 16 values for permission,
// and that nsPermissionManager::Write() can only deal with 26 values of
// permission safely. (We allow space for 256 here, since it's faster to
// deal with bytes than with bits).
// Note: When changing NUMBER_OF_TYPES, also update PermissionsAreEmpty()
// and the constructors.
// This should be a multiple of 4, to make PermissionsAreEmpty() fast
#define NUMBER_OF_TYPES       (8)
#define NUMBER_OF_PERMISSIONS (16)

class nsHostEntry : public PLDHashEntryHdr
{
public:
  // Hash methods
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  nsHostEntry(const char* aHost);
  nsHostEntry(const nsHostEntry& toCopy);

  ~nsHostEntry()
  {
  }

  KeyType GetKey() const
  {
    return mHost;
  }

  KeyTypePointer GetKeyPointer() const
  {
    return mHost;
  }

  PRBool KeyEquals(KeyTypePointer aKey) const
  {
    return !strcmp(mHost, aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return aKey;
  }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    // PL_DHashStringKey doesn't use the table parameter, so we can safely
    // pass nsnull
    return PL_DHashStringKey(nsnull, aKey);
  }

  enum { ALLOW_MEMMOVE = PR_TRUE };

  // Permissions methods
  inline const nsDependentCString GetHost() const
  {
    return nsDependentCString(mHost);
  }

  // Callers must do boundary checks
  void SetPermission(PRInt32 aTypeIndex, PRUint32 aPermission)
  {
    mPermissions[aTypeIndex] = (PRUint8)aPermission;
  }

  PRUint32 GetPermission(PRInt32 aTypeIndex) const
  {
    return (PRUint32)mPermissions[aTypeIndex];
  }

  PRBool PermissionsAreEmpty() const
  {
    // Cast to PRUint32, to make this faster. Only 2 checks instead of 8
    return (*NS_REINTERPRET_CAST(const PRUint32*, &mPermissions[0])==0 && 
            *NS_REINTERPRET_CAST(const PRUint32*, &mPermissions[4])==0 );
  }

private:
  const char *mHost;

  // This will contain the permissions for different types, in a bitmasked
  // form. The type itself defines the position.
  // One byte per permission. This is a small loss of memory (as 255 types
  // of action per type is a lot, 16 would be enough), but will improve speed
  // Bytemasking is around twice as fast (on one arch at least) than bitmasking
  PRUint8 mPermissions[NUMBER_OF_TYPES];
};


class nsPermissionManager : public nsIPermissionManager,
                            public nsIObserver,
                            public nsSupportsWeakReference
{
public:

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERMISSIONMANAGER
  NS_DECL_NSIOBSERVER

  nsPermissionManager();
  virtual ~nsPermissionManager();
  nsresult Init();

private:

  nsresult AddInternal(const nsAFlatCString &aHost,
                       PRInt32  aTypeIndex,
                       PRUint32 aPermission,
                       PRBool   aNotify);
  PRInt32 GetTypeIndex(const char *aTypeString,
                       PRBool      aAdd);

  nsHostEntry *GetHostEntry(const nsAFlatCString &aHost,
                            PRUint32              aType,
                            PRBool                aExactHostMatch);

  nsresult CommonTestPermission(nsIURI     *aURI,
                                const char *aType,
                                PRUint32   *aPermission,
                                PRBool      aExactHostMatch);

  // Use LazyWrite to save the permissions file on a timer. It will write
  // the file only once if repeatedly hammered quickly.
  void        LazyWrite();
  static void DoLazyWrite(nsITimer *aTimer, void *aClosure);
  nsresult    Write();

  nsresult Read();
  void     NotifyObserversWithPermission(const nsACString &aHost,
                                         const char       *aType,
                                         PRUint32          aPermission,
                                         const PRUnichar  *aData);
  void     NotifyObservers(nsIPermission *aPermission, const PRUnichar *aData);
  nsresult RemoveAllFromMemory();
  nsresult GetHost(nsIURI *aURI, nsACString &aResult);
  void     RemoveTypeStrings();

  nsCOMPtr<nsIObserverService> mObserverService;
  nsCOMPtr<nsIFile>            mPermissionsFile;
  nsCOMPtr<nsITimer>           mWriteTimer;
  nsTHashtable<nsHostEntry>    mHostTable;
  PRUint32                     mHostCount;
  PRPackedBool                 mChangedList;
  PRPackedBool                 mHasUnknownTypes;

  // An array to store the strings identifying the different types.
  char                        *mTypeArray[NUMBER_OF_TYPES];
};

// {4F6B5E00-0C36-11d5-A535-0010A401EB10}
#define NS_PERMISSIONMANAGER_CID \
{ 0x4f6b5e00, 0xc36, 0x11d5, { 0xa5, 0x35, 0x0, 0x10, 0xa4, 0x1, 0xeb, 0x10 } }

#endif /* nsPermissionManager_h__ */
