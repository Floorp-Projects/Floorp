/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xp.h"
#include "nsUserTarget.h"
#include "nsPrivilegeManager.h"

#include "prtypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jpermission.h"

static PRBool displayUI=PR_FALSE;

static nsPermState 
displayPermissionDialog(char *prinStr, char *targetStr, char *riskStr, PRBool isCert, void *cert)
{
  return nsJSJavaDisplayDialog(prinStr, targetStr, riskStr, isCert, cert);

}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


//
// 			PUBLIC METHODS 
//

nsUserTarget::~nsUserTarget(void)
{
}

#define OPTION "<option>"
nsPrivilege * nsUserTarget::enablePrivilege(nsPrincipal *prin, void *data)
{
  char *riskStr = getRisk();
  char *desc = getDescription();
  char *prinStr = prin->toString();
  char *targetStr = new char[strlen(OPTION) + strlen(desc) + 1];
  XP_STRCPY(targetStr, OPTION);
  XP_STRCAT(targetStr, desc);
  PRBool isCert = (prin->isCodebase()) ? PR_FALSE : PR_TRUE;
  void *cert = prin->getCertificate();
  nsPermState permState = nsPermState_AllowedSession;

  /* 
   * Check Registration Mode flag and the url code base 
   * to set permission state 
   */
  if ((nsCapsGetRegistrationModeFlag()) && (prin != NULL)) {
	  if (prin->isFileCodeBase()) {
		permState = nsPermState_AllowedSession;
	  }
  } else if (displayUI) {
	/* set displayUI to TRUE, to enable UI */
    nsCaps_lock();
    permState = displayPermissionDialog(prinStr, targetStr, riskStr, isCert, cert); 
    nsCaps_unlock();
  }

  nsPermissionState permVal; 
  nsDurationState durationVal;

  if (permState == nsPermState_AllowedForever) {
    permVal = nsPermissionState_Allowed;
    durationVal = nsDurationState_Forever;
  } else if (permState == nsPermState_AllowedSession) {
    permVal = nsPermissionState_Allowed;
    durationVal = nsDurationState_Session;
  } else if (permState == nsPermState_ForbiddenForever) {
    permVal = nsPermissionState_Forbidden;
    durationVal = nsDurationState_Forever;
  } else {
    permVal = nsPermissionState_Blank;
    durationVal = nsDurationState_Session;
  }
  delete []targetStr;
  return nsPrivilege::findPrivilege(permVal, durationVal);
}


