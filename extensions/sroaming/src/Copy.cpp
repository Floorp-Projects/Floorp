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

#include "Copy.h"
#include "nsIComponentManager.h"
#include "nsILocalFile.h"
#include "nsXPIDLString.h"

#define kRegTreeCopy (NS_LITERAL_STRING("Copy"))
#define kRegKeyRemote (NS_LITERAL_STRING("RemoteDir"))

// Internal helper functions unrelated to class

/* @param fileSubPath  works for subpaths or just filenames?
                       doesn't really matter for us. */
static nsresult CopyFile(nsCOMPtr<nsIFile> fromDir,
                         nsCOMPtr<nsIFile> toDir,
                         nsAString& fileSubPath)
{
  nsresult rv;

  nsCOMPtr<nsIFile> fromFile;
  rv = fromDir->Clone(getter_AddRefs(fromFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = fromFile->Append(fileSubPath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> toFileOld;
  rv = toDir->Clone(getter_AddRefs(toFileOld));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = toFileOld->Append(fileSubPath);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = fromFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists)
  {
    rv = toFileOld->Remove(PR_FALSE);
    rv = fromFile->CopyTo(toDir, fileSubPath);
  }
  else
  {
    rv = NS_ERROR_FILE_NOT_FOUND;
  }
  return rv;
}

static void AppendElementsToStrArray(nsCStringArray& target, nsCStringArray& source)
{
  for (PRInt32 i = source.Count() - 1; i >= 0; i--)
    target.AppendCString(*source.CStringAt(i));
}

/*
 * nsSRoamingProtocol implementation
 */

nsresult Copy::Init(Core* aController)
{
  nsresult rv;
  mController = aController;
  if (!mController)
    return NS_ERROR_INVALID_ARG;

  // Get prefs
  nsCOMPtr<nsIRegistry> registry;
  rv = mController->GetRegistry(registry);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRegistryKey regkey;
  rv = mController->GetRegistryTree(regkey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = registry->GetKey(regkey,
                        kRegTreeCopy.get(),
                        &regkey);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString remoteDirPref;
  rv = registry->GetString(regkey, kRegKeyRemote.get(),
                           getter_Copies(remoteDirPref));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> lf;
  rv = NS_NewLocalFile(remoteDirPref, PR_FALSE,
                       getter_AddRefs(lf));
  NS_ENSURE_SUCCESS(rv, rv);
  
  mRemoteDir = lf;

  rv = mController->GetProfileDir(getter_AddRefs(mProfileDir));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mProfileDir)
    return NS_ERROR_FILE_NOT_FOUND;

  return NS_OK;
}

nsresult Copy::Download()
{
  return DownUpLoad(PR_TRUE);
}

nsresult Copy::Upload()
{
  return DownUpLoad(PR_FALSE);
}

nsresult Copy::DownUpLoad(PRBool download)
{
  nsresult rv = NS_OK;

  const nsCStringArray* files = mController->GetFilesToRoam();

  // Check for conflicts
  nsCStringArray conflicts(10);
  nsCStringArray copyfiles(10);
  PRInt32 i;
  for (i = files->Count() - 1; i >= 0; i--)
  {
    nsCString& file = *files->CStringAt(i);
    NS_ConvertASCIItoUTF16 fileL(file);

    nsCOMPtr<nsIFile> profileFile;
    rv = mProfileDir->Clone(getter_AddRefs(profileFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = profileFile->Append(fileL);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> remoteFile;
    rv = mRemoteDir->Clone(getter_AddRefs(remoteFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = remoteFile->Append(fileL);
    NS_ENSURE_SUCCESS(rv, rv);

    // avoid conflicts for missing files
    PRBool remoteExists = PR_TRUE;
    PRBool profileExists = PR_TRUE;
    remoteFile->Exists(&remoteExists);
    profileFile->Exists(&profileExists);
    if (download)
    {
      if (!remoteExists)
        continue;

      if (!profileExists) {
        copyfiles.AppendCString(file);
        continue;
        /* actually, this code is not needed given how the last modified
           code below works, but for readability and just in case... */
      }
    }
    else
    {
      if (!profileExists)
        continue;

      if (!remoteExists) {
        copyfiles.AppendCString(file);
        continue;
      }
    }

    PRInt64 profileTime = 0;
    PRInt64 remoteTime = 0;
    profileFile->GetLastModifiedTime(&profileTime);
    remoteFile->GetLastModifiedTime(&remoteTime);

    // do we have a conflict?
    if (download
        ? profileTime > remoteTime
        : profileTime < remoteTime )
      conflicts.AppendCString(file);
    else
      copyfiles.AppendCString(file);
  }

  // Ask user about conflicts
  nsCStringArray copyfiles_conflicts(10);
  rv = mController->ConflictResolveUI(download, conflicts,
                                      &copyfiles_conflicts);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendElementsToStrArray(copyfiles, copyfiles_conflicts);

  // Copy
  for (i = copyfiles.Count() - 1; i >= 0; i--)
  {
    const nsCString& file = *copyfiles.CStringAt(i);
    NS_ConvertASCIItoUTF16 fileL(file);
    if (download)
      rv = CopyFile(mRemoteDir, mProfileDir, fileL);
    else
      rv = CopyFile(mProfileDir, mRemoteDir, fileL);
  }

  return rv;
}
