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
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
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

/* Central object for session roaming. Controls program sequence and provides
   common functions. */

#ifndef _Core_H_
#define _Core_H_


#include "nsISessionRoaming.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIRegistry.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsVoidArray.h"

#define NS_SESSIONROAMING_CID                          \
  { /* {ab62465c-494c-446e-b671-930bb98a7bc4} */       \
    0xab62465c,                                        \
    0x494c,                                            \
    0x446e,                                            \
    { 0xb6, 0x71, 0x93, 0x0b, 0xb9, 0x8a, 0x7b, 0xc4 } \
  }

#define NS_SESSIONROAMING_CONTRACTID	\
          "@mozilla.org/profile/session-roaming;1"

class Protocol;

class Core: public nsISessionRoaming
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISESSIONROAMING

    Core();

    // Is a roaming profile (if not, then nothing to do)
    inline PRBool IsRoaming() { return mIsRoaming; };

    // Which profile files should be stored on the server
    // @return pointer to the internal nsDeque object. Do not free it.
    inline const nsCStringArray* GetFilesToRoam()  { return &mFiles; };

    // Local profile directory
    nsresult GetProfileDir(nsIFile** result);

    // Method used to store remote files
    // 0=unknown, 1=HTTP, 2=Copy
    inline PRInt32 GetMethod() { return mMethod; };

    /* If we'd normally overwrite a newer file. Ask user, which file to keep.
     * @param download  direction: true = download, false = upload
     * @param files     conflicting files
     * @param result    Files for which conflicts should be ignored.
     *                  This is a subset of the files param and those should
     *                  be uploaded / downloaded. The rest of files in the
     *                  files param should *not* be uploaded / downloaded.
     *                  Wants an array passed in, items will be added to that.
     * @return NS_ERROR_ABORT, if the user clicked Cancel.
     */
    nsresult ConflictResolveUI(PRBool download, const nsCStringArray& files,
                               nsCStringArray* result);

    nsresult GetRegistry(nsCOMPtr<nsIRegistry>& result);
    nsresult GetRegistryTree(nsRegistryKey& result);

    /* At the time we attempt to upload, the network lib has already been
       shut down. So, temporarily restore it and then close it down again.
       Of course, this is a hack, until we (me, ccarlen, darin) have found a
       better solution and it has been tested thoroughly for regressions.
       This problem currently doesn't appear during download (at startup).

       @param restore  if true, then restore, otherwise close
       @param topic  the notification topic.
                     either "profile-change-net-restore"
                     or "profile-change-net-teardown"
    */
    inline nsresult RestoreNet() { return RestoreCloseNet(PR_TRUE); };
    inline nsresult CloseNet() { return RestoreCloseNet(PR_FALSE); };

protected:
    // Data (see getters above)
    PRBool mIsRoaming;
    PRInt32 mMethod;
    nsCStringArray mFiles;

    // Cache
    nsCOMPtr<nsIRegistry> mRegistry;

    // Reads liprefs: is roaming profile?, files, server info etc.
    nsresult ReadRoamingPrefs();

    // Factory method for a new method handler that can handle this method
    // We'll use a new object for down-/upload respectively
    // @return new object. you have to free it with delete.
    Protocol* CreateMethodHandler();

    nsresult RestoreCloseNet(PRBool restore);
};

#endif
