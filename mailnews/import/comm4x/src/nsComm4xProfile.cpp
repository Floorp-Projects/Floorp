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
#include "nsIProfileInternal.h"
#include "nsILineInputStream.h"
#include "nsReadableUtils.h"
#include "nsNetCID.h"

#if defined(XP_MACOSX)
#define PREF_FILE_NAME_IN_4x "Netscape Preferences"
#elif defined(XP_UNIX) && !defined(XP_MACOSX)
#define PREF_FILE_NAME_IN_4x "preferences.js"
#elif defined(XP_WIN) || defined(XP_OS2)
#define PREF_FILE_NAME_IN_4x "prefs.js"
#else
/* this will cause a failure at run time, as it should, since we don't know
   how to migrate platforms other than Mac, Windows and UNIX */
#define PREF_FILE_NAME_IN_4x ""
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
    nsresult rv = NS_OK;
    nsCOMPtr<nsIProfileInternal> profile (do_GetService(NS_PROFILE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) 
        return rv;
    rv = profile->GetProfileListX(nsIProfileInternal::LIST_FOR_IMPORT, length, profileNames);
    return rv;
}

#define PREF_NAME "user_pref(\"mail.directory\", \""
#define PREF_LENGTH 29
#define PREF_END "\")"
NS_IMETHODIMP
nsComm4xProfile::GetMailDir(const PRUnichar *profileName, PRUnichar **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
    nsresult rv = NS_OK;
    nsCOMPtr <nsILocalFile> resolvedLocation;

    nsCOMPtr<nsIProfileInternal> profile (do_GetService(NS_PROFILE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) 
        return rv;
    rv = profile->GetOriginalProfileDir(profileName, getter_AddRefs(resolvedLocation));
    if (NS_FAILED(rv))
        return rv;
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
    return rv;
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

