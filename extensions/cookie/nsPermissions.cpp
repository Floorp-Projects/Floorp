/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henrik Gemal <gemal@gemal.dk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#define alphabetize 1

#include "nsPermissions.h"
#include "nsUtils.h"
#include "nsXPIDLString.h"
#include "nsIFileSpec.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsVoidArray.h"
#include "xp_core.h"
#include "prmem.h"
#include "nsAppDirectoryServiceDefs.h"

static const char *kCookiesPermFileName = "cookperm.txt";

typedef struct _permission_HostStruct {
  char * host;
  nsVoidArray * permissionList;
} permission_HostStruct;

typedef struct _permission_TypeStruct {
  PRInt32 type;
  PRBool permission;
} permission_TypeStruct;

PRIVATE PRBool permission_changed = PR_FALSE;

PRIVATE PRBool cookie_rememberChecked;
PRIVATE PRBool image_rememberChecked;

PRIVATE nsVoidArray * permission_list=0;

PRBool
permission_CheckConfirmYN(nsIPrompt *aPrompter, PRUnichar * szMessage, PRUnichar * szCheckMessage, PRBool* checkValue) {

  nsresult res;
  nsCOMPtr<nsIPrompt> dialog;

  if (aPrompter)
    dialog = aPrompter;
  else {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch)
      wwatch->GetNewPrompter(0, getter_AddRefs(dialog));
  }
  if (!dialog) {
    *checkValue = 0;
    return PR_FALSE;
  }

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * confirm_string = CKutil_Localize(NS_LITERAL_STRING("Confirm").get());

  res = dialog->ConfirmEx(confirm_string, szMessage,
                          (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                          (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1),
                          nsnull, nsnull, nsnull, szCheckMessage, checkValue, &buttonPressed);

  if (NS_FAILED(res)) {
    *checkValue = 0;
  }
  if (*checkValue!=0 && *checkValue!=1) {
    NS_ASSERTION(PR_FALSE, "Bad result from checkbox");
    *checkValue = 0; /* this should never happen but it is happening!!! */
  }
  Recycle(confirm_string);
  return (buttonPressed == 0);
}

/*
 * search if permission already exists
 */
nsresult
permission_CheckFromList(const char * hostname, PRBool &permission, PRInt32 type) {
  permission_HostStruct * hostStruct;
  permission_TypeStruct * typeStruct;

  /* ignore leading period in host name */
  while (hostname && (*hostname == '.')) {
    hostname++;
  }

  /* return if permission_list does not exist */
  if (permission_list == nsnull) {
    return NS_ERROR_FAILURE;
  }

  /* find host name within list */
  PRInt32 hostCount = permission_list->Count();
  for (PRInt32 i = 0; i < hostCount; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(i));
    if (hostStruct) {
      if(hostname && hostStruct->host && !PL_strcasecmp(hostname, hostStruct->host)) {

        /* search for type in the permission list for this host */
        PRInt32 typeCount = hostStruct->permissionList->Count();
        for (PRInt32 typeIndex=0; typeIndex<typeCount; typeIndex++) {
          typeStruct = NS_STATIC_CAST
            (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
          if (typeStruct->type == type) {

            /* type found.  Obtain the corresponding permission */
            permission = typeStruct->permission;
            return NS_OK;
          }
        }

        /* type not found, return failure */
        return NS_ERROR_FAILURE;
      }
    }
  }

  /* not found, return failure */
  return NS_ERROR_FAILURE;
}

PRBool
permission_GetRememberChecked(PRInt32 type) {
  if (type == COOKIEPERMISSION) {
    return cookie_rememberChecked;
  } else if (type == IMAGEPERMISSION) {
    return image_rememberChecked;
  } else {
    return PR_FALSE;
  }
}

void
permission_SetRememberChecked(PRInt32 type, PRBool value) {
  if (type == COOKIEPERMISSION) {
    cookie_rememberChecked = value;
  } else if (type == IMAGEPERMISSION) {
    image_rememberChecked = value;
  }
}

PUBLIC PRBool
Permission_Check(
     nsIPrompt *aPrompter,
     const char * hostname,
     PRInt32 type,
     PRBool warningPref,
     PRUnichar * message)
{
  PRBool permission;

  /* try to make decision based on saved permissions */
  if (NS_SUCCEEDED(permission_CheckFromList(hostname, permission, type))) {
    return permission;
  }

  /* see if we need to prompt */
  if(!warningPref) {
    return PR_TRUE;
  }

  /* we need to prompt */
  PRBool rememberChecked = permission_GetRememberChecked(type);
  PRUnichar * remember_string = CKutil_Localize(NS_LITERAL_STRING("RememberThisDecision").get());
  permission = permission_CheckConfirmYN(aPrompter, message, remember_string, &rememberChecked);

  /* see if we need to remember this decision */
  if (rememberChecked) {
    char * hostname2 = NULL;
    /* ignore leading periods in host name */
    const char * hostnameAfterDot = hostname;
    while (hostnameAfterDot && (*hostnameAfterDot == '.')) {
      hostnameAfterDot++;
    }
    CKutil_StrAllocCopy(hostname2, hostnameAfterDot);
    Permission_AddHost(hostname2, permission, type, PR_TRUE);
  }
  if (rememberChecked != permission_GetRememberChecked(type)) {
    permission_SetRememberChecked(type, rememberChecked);
    permission_changed = PR_TRUE;
    Permission_Save();
  }
  return permission;
}

PUBLIC nsresult
Permission_AddHost(char * host, PRBool permission, PRInt32 type, PRBool save) {
  /* create permission list if it does not yet exist */
  if(!permission_list) {
    permission_list = new nsVoidArray();
    if(!permission_list) {
      Recycle(host);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  /* find existing entry for host */
  permission_HostStruct * hostStruct;
  PRBool HostFound = PR_FALSE;
  PRInt32 hostCount = permission_list->Count();
  PRInt32 i;
  for (i = 0; i < hostCount; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(i));
    if (hostStruct) {
      if (PL_strcasecmp(host,hostStruct->host)==0) {

        /* host found in list */
        Recycle(host);
        HostFound = PR_TRUE;
        break;
#ifdef alphabetize
      } else if (PL_strcasecmp(host, hostStruct->host) < 0) {

        /* need to insert new entry here */
        break;
#endif
      }
    }
  }

  if (!HostFound) {

    /* create a host structure for the host */
    hostStruct = PR_NEW(permission_HostStruct);
    if (!hostStruct) {
      Recycle(host);
      return NS_ERROR_FAILURE;
    }
    hostStruct->host = host;
    hostStruct->permissionList = new nsVoidArray();
    if(!hostStruct->permissionList) {
      PR_Free(hostStruct);
      Recycle(host);
      return NS_ERROR_FAILURE;
    }

    /* insert host structure into the list */
    if (i == permission_list->Count()) {
      permission_list->AppendElement(hostStruct);
    } else {
      permission_list->InsertElementAt(hostStruct, i);
    }
  }

  /* see if host already has an entry for this type */
  permission_TypeStruct * typeStruct;
  PRBool typeFound = PR_FALSE;
  PRInt32 typeCount = hostStruct->permissionList->Count();
  for (PRInt32 typeIndex=0; typeIndex<typeCount; typeIndex++) {
    typeStruct = NS_STATIC_CAST
      (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
    if (typeStruct->type == type) {

      /* type found.  Modify the corresponding permission */
      typeStruct->permission = permission;
      typeFound = PR_TRUE;
      break;
    }
  }

  /* create a type structure and attach it to the host structure */
  if (!typeFound) {
    typeStruct = PR_NEW(permission_TypeStruct);
    typeStruct->type = type;
    typeStruct->permission = permission;
    hostStruct->permissionList->AppendElement(typeStruct);
  }

  /* write the changes out to a file */
  if (save) {
    permission_changed = PR_TRUE;
    Permission_Save();
  }
  return NS_OK;
}

PRIVATE void
permission_Unblock(char * host, PRInt32 type) {

  /* nothing to do if permission list does not exist */
  if(!permission_list) {
    return;
  }

  /* find existing entry for host */
  permission_HostStruct * hostStruct;
  PRInt32 hostCount = permission_list->Count();
  for (PRInt32 hostIndex = 0; hostIndex < hostCount; ++hostIndex) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(hostIndex));
    if (hostStruct && PL_strcasecmp(host, hostStruct->host)==0) {
      /* host found in list, see if it has an entry for this type */
      permission_TypeStruct * typeStruct;
      PRInt32 typeCount = hostStruct->permissionList->Count();
      for (PRInt32 typeIndex=0; typeIndex<typeCount; typeIndex++) {
        typeStruct = NS_STATIC_CAST
          (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
        if (typeStruct && typeStruct->type == type) {
          /* type found.  Remove the permission if it is PR_FALSE */
          if (typeStruct->permission == PR_FALSE) {
            hostStruct->permissionList->RemoveElementAt(typeIndex);
            /* if no more types are present, remove the entry */
            typeCount = hostStruct->permissionList->Count();
            if (typeCount == 0) {
              PR_FREEIF(hostStruct->permissionList);
              permission_list->RemoveElementAt(hostIndex);
              PR_FREEIF(hostStruct->host);
              PR_Free(hostStruct);
            }
            permission_changed = PR_TRUE;
            Permission_Save();
            return;
          }
          break;
        }
      }
      break;
    }
  }
  return;
}

/* saves the permissions to disk */
PUBLIC void
Permission_Save() {
  permission_HostStruct * hostStruct;
  permission_TypeStruct * typeStruct;

  if (!permission_changed) {
    return;
  }
  if (permission_list == nsnull) {
    return;
  }
  nsFileSpec dirSpec;
  nsresult rval = CKutil_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    return;
  }
  nsOutputFileStream strm(dirSpec + kCookiesPermFileName);
  if (!strm.is_open()) {
    return;
  }

#define PERMISSIONFILE_LINE1 "# HTTP Permission File\n"
#define PERMISSIONFILE_LINE2 "# http://www.netscape.com/newsref/std/cookie_spec.html\n"
#define PERMISSIONFILE_LINE3 "# This is a generated file!  Do not edit.\n\n"

  strm.write(PERMISSIONFILE_LINE1, PL_strlen(PERMISSIONFILE_LINE1));
  strm.write(PERMISSIONFILE_LINE2, PL_strlen(PERMISSIONFILE_LINE2));
  strm.write(PERMISSIONFILE_LINE3, PL_strlen(PERMISSIONFILE_LINE3));

  /* format shall be:
   * host \t permission \t permission ... \n
   */

  
  PRInt32 hostCount = permission_list->Count();
  for (PRInt32 i = 0; i < hostCount; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(i));
    if (hostStruct) {
      strm.write(hostStruct->host, nsCRT::strlen(hostStruct->host));

      PRInt32 typeCount = hostStruct->permissionList->Count();
      for (PRInt32 typeIndex=0; typeIndex<typeCount; typeIndex++) {
        typeStruct = NS_STATIC_CAST
          (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
        strm.write("\t", 1);
        nsCAutoString tmp; tmp.AppendInt(typeStruct->type);
        strm.write(tmp.get(), tmp.Length());
        if (typeStruct->permission) {
          strm.write("T", 1);
        } else {
          strm.write("F", 1);
        }
      }
      strm.write("\n", 1);
    }
  }

  /* save current state of nag boxs' checkmarks */
  strm.write("@@@@", 4);
  for (PRInt32 type = 0; type < NUMBER_OF_PERMISSIONS; type++) {
    strm.write("\t", 1);
    nsCAutoString tmp; tmp.AppendInt(type);
    strm.write(tmp.get(), tmp.Length());
    if (permission_GetRememberChecked(type)) {
      strm.write("T", 1);
    } else {
      strm.write("F", 1);
    }
  }

  strm.write("\n", 1);

  permission_changed = PR_FALSE;
  strm.flush();
  strm.close();
}

/* reads the permissions from disk */
PUBLIC nsresult
PERMISSION_Read() {
  if (permission_list) {
    return NS_OK;
  }
  // create permission list to avoid continually attempting to re-read
  // cookperm.txt if it doesn't exist
  permission_list = new nsVoidArray();
  if(!permission_list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsAutoString buffer;
  nsFileSpec dirSpec;
  nsresult rv = CKutil_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsInputFileStream strm(dirSpec + kCookiesPermFileName);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    for (PRInt32 type=0; type<NUMBER_OF_PERMISSIONS; type++) {
      permission_SetRememberChecked(type, PR_FALSE);
    }
    return NS_OK;
  }

  /* format is:
   * host \t number permission \t number permission ... \n
   * if this format isn't respected we move onto the next line in the file.
   */
  while(CKutil_GetLine(strm,buffer) != -1) {
    if ( !buffer.IsEmpty() ) {
      PRUnichar firstChar = buffer.CharAt(0);
      if (firstChar == '#' || firstChar == nsCRT::CR ||
          firstChar == nsCRT::LF || firstChar == 0) {
        continue;
      }
    }

    int hostIndex, permissionIndex;
    PRUint32 nextPermissionIndex = 0;
    hostIndex = 0;

    if ((permissionIndex=buffer.FindChar('\t', PR_FALSE, hostIndex)+1) == 0) {
      continue;      
    }

    /* ignore leading periods in host name */
    while (hostIndex < permissionIndex && (buffer.CharAt(hostIndex) == '.')) {
      hostIndex++;
    }

    nsAutoString host;
    buffer.Mid(host, hostIndex, permissionIndex-hostIndex-1);

    nsAutoString permissionString;
    for (;;) {
      if (nextPermissionIndex == buffer.Length()+1) {
        break;
      }
      if ((nextPermissionIndex=buffer.FindChar('\t', PR_FALSE, permissionIndex)+1) == 0) {
        nextPermissionIndex = buffer.Length()+1;
      }
      buffer.Mid(permissionString, permissionIndex, nextPermissionIndex-permissionIndex-1);
      permissionIndex = nextPermissionIndex;

      PRInt32 type = 0;
      PRUint32 index = 0;

      if (permissionString.IsEmpty()) {
        continue; /* empty permission entry -- should never happen */
      }
      char c = (char)permissionString.CharAt(index);
      while (index < permissionString.Length() && c >= '0' && c <= '9') {
        type = 10*type + (c-'0');
        c = (char)permissionString.CharAt(++index);
      }
      if (index >= permissionString.Length()) {
        continue; /* bad format for this permission entry */
      }
      PRBool permission = (permissionString.CharAt(index) == 'T');

      /*
       * a host value of "@@@@" is a special code designating the
       * state of the nag-box's checkmark
       */
      if (host.EqualsWithConversion("@@@@")) {
        if (!permissionString.IsEmpty()) {
          permission_SetRememberChecked(type, permission);
        }
      } else {
        if (!permissionString.IsEmpty()) {
          rv = Permission_AddHost(host.ToNewCString(), permission, type, PR_FALSE);
          if (NS_FAILED(rv)) {
            strm.close();
            return rv;
          }
        }
      }
    }
  }

  strm.close();
  permission_changed = PR_FALSE;
  return NS_OK;
}

PUBLIC PRInt32
PERMISSION_HostCount() {
  if (!permission_list) {
    return 0;
  }
  return permission_list->Count();
}

PUBLIC PRInt32
PERMISSION_TypeCount(PRInt32 host) {
  if (!permission_list) {
    return 0;
  }

  permission_HostStruct *hostStruct;
  hostStruct = NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(host));
  NS_ASSERTION(hostStruct, "corrupt permission list");
  return hostStruct->permissionList->Count();
}

PUBLIC nsresult
PERMISSION_Enumerate
    (PRInt32 hostNumber, PRInt32 typeNumber, char **host, PRInt32 *type, PRBool *capability) {
  if (hostNumber > PERMISSION_HostCount()  || typeNumber > PERMISSION_TypeCount(hostNumber)) {
    return NS_ERROR_FAILURE;
  }
  permission_HostStruct *hostStruct;
  permission_TypeStruct * typeStruct;

  hostStruct = NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(hostNumber));
  NS_ASSERTION(hostStruct, "corrupt permission list");
  char * copyOfHost = NULL;
  CKutil_StrAllocCopy(copyOfHost, hostStruct->host);
  *host = copyOfHost;

  typeStruct = NS_STATIC_CAST
    (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeNumber));
  *capability = typeStruct->permission;
  *type = typeStruct->type;
  return NS_OK;
}

PRIVATE void
permission_remove (PRInt32 hostNumber, PRInt32 type) {
  if (!permission_list) {
    return;
  }
  permission_HostStruct * hostStruct;
  hostStruct =
    NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(hostNumber));
  if (!hostStruct) {
    return;
  }
  permission_TypeStruct * typeStruct;
  typeStruct =
    NS_STATIC_CAST(permission_TypeStruct*, hostStruct->permissionList->ElementAt(type));
  if (!typeStruct) {
    return;
  }
  hostStruct->permissionList->RemoveElementAt(type);
  permission_changed = PR_TRUE;

  /* if no types are present, remove the entry */
  PRInt32 typeCount = hostStruct->permissionList->Count();
  if (typeCount == 0) {
    PR_FREEIF(hostStruct->permissionList);
    permission_list->RemoveElementAt(hostNumber);
    PR_FREEIF(hostStruct->host);
    PR_Free(hostStruct);
  }
}

PUBLIC void
PERMISSION_Remove(const char* host, PRInt32 type) {

  /* get to the indicated host in the list */
  if (permission_list) {
    PRInt32 hostCount = permission_list->Count();
    permission_HostStruct * hostStruct;
    while (hostCount>0) {
      hostCount--;
      hostStruct =
        NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(hostCount));
      NS_ASSERTION(hostStruct, "corrupt permission list");
      if ((PL_strcmp(hostStruct->host, host) == 0)) {

        /* get to the indicated permission in the list */
        PRInt32 typeCount = hostStruct->permissionList->Count();
        permission_TypeStruct * typeStruct;
        while (typeCount>0) {
          typeCount--;
          typeStruct =
            NS_STATIC_CAST
              (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeCount));
          NS_ASSERTION(typeStruct, "corrupt permission list");
          if (typeStruct->type == type) {
            permission_remove(hostCount, typeCount);
            permission_changed = PR_TRUE;
            Permission_Save();
            break;
          }
        }
        break;
      }
    }
  }
}

/* blows away all permissions currently in the list */
PUBLIC void
PERMISSION_RemoveAll() {
 
  if (permission_list) {
    permission_HostStruct * hostStruct;
    PRInt32 hostCount = permission_list->Count();
    for (PRInt32 i = hostCount-1; i >=0; i--) {
      hostStruct = NS_STATIC_CAST(permission_HostStruct*, permission_list->ElementAt(i));
      PRInt32 typeCount = hostStruct->permissionList->Count();
      for (PRInt32 typeIndex = typeCount-1; typeIndex >=0; typeIndex--) {
        permission_remove(hostCount, typeCount);
      }
    }
    delete permission_list;
    permission_list = NULL;
  }
}

PUBLIC void
PERMISSION_DeletePersistentUserData(void)
{
  nsresult res;
  
  nsCOMPtr<nsIFile> cookiesPermFile;
  res = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(cookiesPermFile));
  if (NS_SUCCEEDED(res)) {
    res = cookiesPermFile->Append(kCookiesPermFileName);
    if (NS_SUCCEEDED(res))
        (void) cookiesPermFile->Remove(PR_FALSE);
  }
}

PUBLIC void
PERMISSION_Add(const char * objectURL, PRBool permission, PRInt32 type) {
  if (!objectURL) {
    return;
  }
  char *host = CKutil_ParseURL(objectURL, GET_HOST_PART);

  /*
   * if permission is false, it will be added to the permission list
   * if permission is true, any false permissions will be removed rather than a
   *    true permission being added
   */
  if (permission) {
    char * hostPtr = host;
    while (PR_TRUE) {
      permission_Unblock(hostPtr, type);
      hostPtr = PL_strchr(hostPtr, '.');
      if (!hostPtr) {
        break;
      }
      hostPtr++; /* get passed the period */
    }
    Recycle(host);
    return;
  }
  Permission_AddHost(host, permission, type, PR_TRUE);
}
