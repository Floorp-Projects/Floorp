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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Srilatha Moturi <srilatha@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
#include "nsCOMPtr.h"
#include "nsComm4xProfile.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsReadableUtils.h"
#include "nsNetCID.h"
#include "nsDirectoryServiceDefs.h"
#include "NSReg.h"

#if defined(XP_MACOSX)
#define PREF_FILE_NAME_IN_4x "Netscape Preferences"
#define OLDREG_NAME               "Netscape Registry"
#define OLDREG_DIR                NS_MAC_PREFS_DIR
#elif defined(XP_UNIX) && !defined(XP_MACOSX)
#define PREF_FILE_NAME_IN_4x "preferences.js"
#elif defined(XP_WIN) || defined(XP_OS2)
#define PREF_FILE_NAME_IN_4x "prefs.js"
#define OLDREG_NAME               "nsreg.dat"
#ifdef XP_WIN
#define OLDREG_DIR                NS_WIN_WINDOWS_DIR
#else
#define OLDREG_DIR                NS_OS2_DIR
#endif
#else
/* this will cause a failure at run time, as it should, since we don't know
   how to migrate platforms other than Mac, Windows and UNIX */
#define PREF_FILE_NAME_IN_4x ""
#endif

#ifndef MAXPATHLEN
#ifdef _MAX_PATH
#define MAXPATHLEN _MAX_PATH
#elif defined(CCHMAXPATH)
#define MAXPATHLEN CCHMAXPATH
#else
#define MAXPATHLEN 1024
#endif
#endif

nsComm4xProfile::nsComm4xProfile()
{
}

nsComm4xProfile::~nsComm4xProfile()
{
}

NS_IMPL_ISUPPORTS1(nsComm4xProfile, nsIComm4xProfile)

NS_IMETHODIMP
nsComm4xProfile::GetProfileList(PRUint32 *length, PRUnichar ***profileNames)
{
    nsresult rv;
// on win/mac/os2, NS4x uses a registry to determine profile locations
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_OS2)

    nsCOMPtr<nsIFile> regFile;
    rv = NS_GetSpecialDirectory(OLDREG_DIR, getter_AddRefs(regFile));
    NS_ENSURE_SUCCESS(rv, rv);
    regFile->AppendNative(NS_LITERAL_CSTRING(OLDREG_NAME));
  
    nsCAutoString path;
    rv = regFile->GetNativePath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    if (NR_StartupRegistry())
      return NS_ERROR_FAILURE;

    HREG reg = nsnull;
    if (NR_RegOpen(path.get(), &reg)) {
      NR_ShutdownRegistry();
      return NS_ERROR_FAILURE;
    }

    PRInt32 numProfileEntries = 0;
    REGENUM enumstate = 0;
    PRInt32 localLength = 0;

    char profileName[MAXREGNAMELEN];
    while (!NR_RegEnumSubkeys(reg, ROOTKEY_USERS, &enumstate,
                              profileName, MAXREGNAMELEN, REGENUM_CHILDREN))
        numProfileEntries++;

    // reset our enumerator
    enumstate = 0;

    PRUnichar **outArray, **next;
    next = outArray = (PRUnichar **)nsMemory::Alloc(numProfileEntries * sizeof(PRUnichar *));
    if (!outArray)
        return NS_ERROR_OUT_OF_MEMORY;

    while (!NR_RegEnumSubkeys(reg, ROOTKEY_USERS, &enumstate,
                              profileName, MAXREGNAMELEN, REGENUM_CHILDREN)) {
        *next = ToNewUnicode(NS_ConvertUTF8toUTF16(profileName));
        next++;
        localLength++;
    }

    *profileNames = outArray;
    *length = localLength;
    return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

#define PREF_NAME "user_pref(\"mail.directory\", \""
#define PREF_LENGTH 29
#define PREF_END "\")"
NS_IMETHODIMP
nsComm4xProfile::GetMailDir(const PRUnichar *aProfile, PRUnichar **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
    nsresult rv = NS_OK;
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_MACOSX)
    nsCOMPtr <nsILocalFile> resolvedLocation = do_CreateInstance("@mozilla.org/file/local;1");
    // on macos, registry entries are UTF8 encoded
    NS_ConvertUTF16toUTF8 profileName(aProfile);

    nsCOMPtr<nsIFile> regFile;
    rv = NS_GetSpecialDirectory(OLDREG_DIR, getter_AddRefs(regFile));
    if (NS_FAILED(rv)) return rv;

    regFile->AppendNative(NS_LITERAL_CSTRING(OLDREG_NAME));
  
    nsCAutoString path;
    rv = regFile->GetNativePath(path);
    if (NS_FAILED(rv)) return rv;

    if (NR_StartupRegistry())
        return NS_ERROR_FAILURE;

    HREG reg = nsnull;
    RKEY profile = nsnull;

    if (NR_RegOpen(path.get(), &reg))
      goto cleanup;

    if (NR_RegGetKey(reg, ROOTKEY_USERS, profileName.get(), &profile))
        goto cleanup;
  
    char profilePath[MAXPATHLEN];
    if (NR_RegGetEntryString(reg, profile, "ProfileLocation", profilePath, MAXPATHLEN))
        goto cleanup;

    resolvedLocation->InitWithPath(NS_ConvertUTF8toUTF16(profilePath));
    if (resolvedLocation) {
        nsCOMPtr <nsIFile> file;
        rv = resolvedLocation->Clone(getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr <nsILocalFile> profileLocation;
        profileLocation = do_QueryInterface(file);
        rv = profileLocation->AppendNative(NS_LITERAL_CSTRING(PREF_FILE_NAME_IN_4x));
        if (NS_FAILED(rv)) return rv;
        PRBool exists = PR_FALSE;
        rv = profileLocation->Exists(&exists);
        if (NS_FAILED(rv)) return rv;
        if (exists) {
            nsXPIDLString prefValue;
            rv = GetPrefValue(profileLocation, PREF_NAME, PREF_END, getter_Copies(prefValue));
            if (NS_FAILED(rv)) return rv;
            if (prefValue) {
#if defined(XP_MAC) || defined(XP_MACOSX)
                rv = profileLocation->SetPersistentDescriptor(NS_ConvertUCS2toUTF8(prefValue));
                if (NS_FAILED(rv)) return rv;
                nsAutoString path;
                rv = profileLocation->GetPath(path);
                if (NS_FAILED(rv)) return rv;
                *_retval = ToNewUnicode(path);
#else
                *_retval = ToNewUnicode(prefValue);
#endif
            }
#if defined(XP_WIN) || defined(XP_OS2) || defined(XP_MAC) || defined(XP_MACOSX)
            else {
                nsCOMPtr <nsIFile> mailLocation;
                rv =  resolvedLocation->Clone(getter_AddRefs(mailLocation));
                if (NS_FAILED(rv)) return rv;
                rv = mailLocation->AppendNative(NS_LITERAL_CSTRING("Mail"));
                if (NS_FAILED(rv)) return rv;
                nsAutoString path;
                rv = mailLocation->GetPath(path);
                if (NS_FAILED(rv)) return rv;
                *_retval = ToNewUnicode(path);
            }
#endif
        }
    }

cleanup:
    if (reg)
        NR_RegClose(reg);
    NR_ShutdownRegistry();
    return rv;
#else
    return NS_ERROR_FAILURE;
#endif
}

nsresult nsComm4xProfile::GetPrefValue(nsILocalFile *filePath, const char * prefName, const char * prefEnd, PRUnichar ** retval)
{
   nsString buffer;
   PRBool more = PR_TRUE;
   nsresult rv;
   nsCOMPtr<nsIFileInputStream> fileStream(do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
   if (NS_FAILED(rv))
     return rv;
   rv = fileStream->Init(filePath, -1, -1, PR_FALSE);
   if (NS_FAILED(rv))
     return rv;
 
   nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));
   if (NS_FAILED(rv)) {
     return rv;
   }
   PRBool found = PR_FALSE;
   PRInt32 offset;
   PRInt32 endOffset;
   while (!found && more) {
      nsCAutoString cLine;
       rv = lineStream->ReadLine(cLine, &more);
       if (NS_FAILED(rv))
           break;
       CopyASCIItoUTF16(cLine, buffer);
       offset = buffer.Find(prefName,PR_FALSE, 0, -1);
       if (offset != kNotFound) {
           endOffset = buffer.Find(prefEnd,PR_FALSE, 0, -1);
           if (endOffset != kNotFound) {
               nsAutoString prefValue;
               buffer.Mid(prefValue, offset + PREF_LENGTH, endOffset-(offset + PREF_LENGTH));
               found = PR_TRUE;
               *retval = ToNewUnicode(prefValue);
               break;
           }
       }
  }

  fileStream->Close(); 
  return rv; 
}

