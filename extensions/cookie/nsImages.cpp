/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPermissions.h"
#include "nsUtils.h"
#include "plstr.h"

#include "nsVoidArray.h"
#include "prmem.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsComObsolete.h"

#define image_behaviorPref "network.image.imageBehavior"
#define image_warningPref "network.image.warnAboutImages"
#define image_blockerPref "imageblocker.enabled"
#define image_blockImageInMailPref "mailnews.message_display.disable_remote_image"

typedef struct _permission_HostStruct {
  char * host;
  nsVoidArray * permissionList;
} permission_HostStruct;

typedef struct _permission_TypeStruct {
  PRInt32 type;
  PRBool permission;
} permission_TypeStruct;

#define kBehaviorPrefDefault PERMISSION_Accept
#define kWarningPrefDefault PR_FALSE
#define kBlockerPrefDefault PR_FALSE
#define kBlockImageInMailPrefDefault PR_FALSE

static PERMISSION_BehaviorEnum gBehaviorPref = kBehaviorPrefDefault;
static PRBool gWarningPref = kWarningPrefDefault;
static PRBool gBlockerPref = kBlockerPrefDefault;
static PRBool gBlockImageInMailNewsPref = kBlockImageInMailPrefDefault;


PR_STATIC_CALLBACK(int)
image_BehaviorPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs || NS_FAILED(prefs->GetIntPref(image_behaviorPref, &n))) {
    gBehaviorPref = kBehaviorPrefDefault;
  } else {
    gBehaviorPref = (PERMISSION_BehaviorEnum)n;
  }
  return 0;
}

PR_STATIC_CALLBACK(int)
image_WarningPrefChanged(const char * newpref, void * data) {
  PRBool x;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs || NS_FAILED(prefs->GetBoolPref(image_warningPref, &x))) {
    x = kWarningPrefDefault;
  }
  gWarningPref = x;
  return 0;
}

PR_STATIC_CALLBACK(int)
image_BlockerPrefChanged(const char * newpref, void * data) {
  PRBool x;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs || NS_FAILED(prefs->GetBoolPref(image_blockerPref, &x))) {
    x = kBlockerPrefDefault;
  }
  gBlockerPref = x;
  return 0;
}

PR_STATIC_CALLBACK(int)
image_BlockedInMailPrefChanged(const char * newpref, void * data) {
  PRBool x;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs || NS_FAILED(prefs->GetBoolPref(image_blockImageInMailPref, &x))) {
    x = kBlockImageInMailPrefDefault;
  }
  gBlockImageInMailNewsPref = x;
  return 0;
}

PUBLIC void
IMAGE_RegisterPrefCallbacks(void) {
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
  if (!prefs) {
    NS_NOTREACHED("couldn't get prefs");
    // All the prefs should still have their default values from the
    // static initializers.
    return;
  }

  // Initialize for image_behaviorPref
  image_BehaviorPrefChanged(image_behaviorPref, NULL);
  prefs->RegisterCallback(image_behaviorPref, image_BehaviorPrefChanged, NULL);

  // Initialize for image_warningPref
  image_WarningPrefChanged(image_warningPref, NULL);
  prefs->RegisterCallback(image_warningPref, image_WarningPrefChanged, NULL);

  // Initialize for image_blockerPref
  image_BlockerPrefChanged(image_blockerPref, NULL);
  prefs->RegisterCallback(image_blockerPref, image_BlockerPrefChanged, NULL);

  //Initialize for image_blockImageInMailPref
  image_BlockedInMailPrefChanged(image_blockImageInMailPref, NULL);
  prefs->RegisterCallback(image_blockImageInMailPref, image_BlockedInMailPrefChanged, NULL);
}

PUBLIC nsresult
IMAGE_CheckForPermission
    (const char * hostname, const char * firstHostname, PRBool *permission) {

  /* exit if imageblocker is not enabled */
  if (!gBlockerPref) {
    *permission = (gBehaviorPref != PERMISSION_DontUse);
    return NS_OK;
  }

  /* try to make a decision based on pref settings */
  if ((gBehaviorPref == PERMISSION_DontUse)) {
    *permission = PR_FALSE;
    return NS_OK;
  }
  if (gBehaviorPref == PERMISSION_DontAcceptForeign) {
    /* compare tails of names checking to see if they have a common domain */
    /* we do this by comparing the tails of both names where each tail includes at least one dot */
    PRInt32 dotcount = 0;
    const char * tailHostname = hostname + PL_strlen(hostname) - 1;
    while (tailHostname > hostname) { 
      if (*tailHostname == '.') {
        dotcount++;
      }
      if (dotcount == 2) {
        tailHostname++;
        break;
      }
      tailHostname--;
    }
    dotcount = 0;
    const char * tailFirstHostname = firstHostname + PL_strlen(firstHostname) - 1;
    while (tailFirstHostname > firstHostname) { 
      if (*tailFirstHostname == '.') {
        dotcount++;
      }
      if (dotcount == 2) {
        tailFirstHostname++;
        break;
      }
      tailFirstHostname--;
    }
    if (PL_strcmp(tailFirstHostname, tailHostname)) {
      *permission = PR_FALSE;
      return NS_OK;
    }
  }

  /* use common routine to make decision */
  if (NS_SUCCEEDED(PERMISSION_Read())) {
    *permission = Permission_Check(0, hostname, IMAGEPERMISSION,
                                   PR_FALSE /* gWarningPref */, nsnull,
                                   "PermissionToAcceptImage", 0);
  } else {
    *permission = PR_TRUE;
  }
  return NS_OK;
}

PUBLIC PRBool
IMAGE_BlockedInMail()
{
  return gBlockImageInMailNewsPref;
}

PUBLIC nsresult
IMAGE_Block(nsIURI* imageURI)
{
  if (!imageURI) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCAutoString hostPort;
  imageURI->GetHostPort(hostPort);
  Permission_AddHost(hostPort, PR_FALSE, IMAGEPERMISSION, PR_TRUE);
  return NS_OK;
}
