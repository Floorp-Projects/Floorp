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

#include "nsPrivilege.h"
#include "xp.h"

static nsPrivilege *thePrivilegeCache[nsPermissionState_NumberOfPermissions][nsDurationState_NumberOfDurations];


//
// 			PUBLIC METHODS 
//

nsPrivilege::nsPrivilege(nsPermissionState perm, nsDurationState duration)
{
  itsPerm=perm;
  itsDuration=duration;
  itsString = NULL;
}

nsPrivilege::~nsPrivilege(void)
{
  if (itsString)
    delete []itsString;
}

nsPrivilege * nsPrivilege::findPrivilege(nsPermissionState permission, nsDurationState duration)
{
  return thePrivilegeCache[permission][duration];
}

PRBool nsPrivilege::samePermission(nsPrivilege *p)
{
  if (p->itsPerm == itsPerm)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::samePermission(nsPermissionState perm)
{
  if (itsPerm == perm)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::sameDuration(nsPrivilege *p)
{
  if (p->itsDuration == itsDuration)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::sameDuration(nsDurationState duration)
{
  if (itsDuration == duration)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::isAllowed(void)
{
  if (itsPerm == nsPermissionState_Allowed)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::isAllowedForever(void)
{
  if ((itsPerm == nsPermissionState_Allowed) &&
      (itsDuration == nsDurationState_Forever))
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::isForbidden(void)
{
  if (itsPerm == nsPermissionState_Forbidden)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::isForbiddenForever(void)
{
  if ((itsPerm == nsPermissionState_Forbidden) &&
      (itsDuration == nsDurationState_Forever))
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsPrivilege::isBlank(void)
{
  if (itsPerm == nsPermissionState_Blank)
    return PR_TRUE;
  return PR_FALSE;
}

nsPermissionState nsPrivilege::getPermission(void)
{
  return itsPerm;
}

nsDurationState nsPrivilege::getDuration(void)
{
  return itsDuration;
}

/* The following function is used to restore the privilege from persistent store.
 * This does the reverse of toString method.
 */
nsPrivilege * nsPrivilege::findPrivilege(char *privStr)
{
  nsPermissionState permission; 
  nsDurationState duration;
  if (XP_STRCMP(privStr, "allowed in scope") == 0) {
    permission = nsPermissionState_Allowed;
    duration = nsDurationState_Scope;
  } else if (XP_STRCMP(privStr, "allowed in session") == 0) {
    permission = nsPermissionState_Allowed;
    duration = nsDurationState_Session;
  } else if (XP_STRCMP(privStr, "allowed forever") == 0) {
    permission = nsPermissionState_Allowed;
    duration = nsDurationState_Forever;
  } else if (XP_STRCMP(privStr, "forbidden forever") == 0) {
    permission = nsPermissionState_Forbidden;
    duration = nsDurationState_Forever;
  } else if (XP_STRCMP(privStr, "forbidden in session") == 0) {
    permission = nsPermissionState_Forbidden;
    duration = nsDurationState_Session;
  } else if (XP_STRCMP(privStr, "forbidden in scope") == 0) {
    permission = nsPermissionState_Forbidden;
    duration = nsDurationState_Scope;
  } else if (XP_STRCMP(privStr, "blank forever") == 0) {
    permission = nsPermissionState_Blank;
    duration = nsDurationState_Forever;
  } else if (XP_STRCMP(privStr, "blank in session") == 0) {
    permission = nsPermissionState_Blank;
    duration = nsDurationState_Session;
  } else if (XP_STRCMP(privStr, "blank in scope") == 0) {
    permission = nsPermissionState_Blank;
    duration = nsDurationState_Scope;
  } else {
    permission = nsPermissionState_Blank;
    duration = nsDurationState_Scope;
  }
  return findPrivilege(permission, duration);
}


char * nsPrivilege::toString(void) 
{
  char *permStr=NULL;
  char *durStr=NULL;
  if (itsString != NULL)
    return itsString;

  switch(itsPerm) {
  case nsPermissionState_Allowed:
    permStr = "allowed";
    break;
  case nsPermissionState_Forbidden:
    permStr = "forbidden";
    break;
  case nsPermissionState_Blank:
    permStr = "blank";
    break;
  default:
    PR_ASSERT(FALSE);
    permStr = "blank";
    break;
  }

  switch(itsDuration) {
  case nsDurationState_Scope:
    durStr = " in scope";
    break;
  case nsDurationState_Session:
    durStr = " in session";
    break;
  case nsDurationState_Forever:
    durStr = " forever";
    break;
  default:
    PR_ASSERT(FALSE);
    permStr = "blank";
    break;
  }

  itsString = new char[strlen(permStr) + strlen(durStr) + 1];
  XP_STRCPY(itsString, permStr);
  XP_STRCAT(itsString, durStr);
  return itsString;
}



//
// 			PRIVATE METHODS 
//

PRBool nsPrivilegeInitialize(void)
{
  nsPermissionState perm;
  nsDurationState duration;
  for (int i = 0; i < nsPermissionState_NumberOfPermissions; i++)
    for(int j = 0; j < nsDurationState_NumberOfDurations; j++) {
      /* XXX: hack fix it */
      perm = (nsPermissionState)i;
      duration = (nsDurationState)j;
      thePrivilegeCache[i][j] = new nsPrivilege(perm, duration);
    }
  return PR_TRUE;
}

PRBool nsPrivilege::theInited = nsPrivilegeInitialize();
