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

#include "prtypes.h"
#include "nspr.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"

#include "structs.h"
#include "proto.h"
#include "secnav.h"

#include "jpermission.h"

static char *userTargetErrMsg;
static nsPermState gPermState;
void *gPrincipalCert;

static void
nsUserTargetHandleMonitorError(int rv)
{
  if (rv == PR_FAILURE) {
    userTargetErrMsg = "IllegalMonitorStateException current thread not owner";
  }
  else if (PR_GetError() == PR_PENDING_INTERRUPT_ERROR) {
    userTargetErrMsg = "ThreadDeath: thread tried to proceed after being stopped";
  } else {
    userTargetErrMsg = NULL;
  }
}


PR_PUBLIC_API(void)
java_netscape_security_savePrivilege(nsPermState permState)
{
  PR_CEnterMonitor((void *)&gPermState);
  gPermState = permState;
  nsUserTargetHandleMonitorError(PR_CNotifyAll((void*)&gPermState));
  PR_CExitMonitor((void *)&gPermState);
}

PR_PUBLIC_API(void *)
java_netscape_security_getCert(char *prinStr)
{
  return gPrincipalCert;
}


PR_PUBLIC_API(nsPermState) 
nsJSJavaDisplayDialog(char *prinStr, char *targetStr, char *riskStr, PRBool isCert, void*cert)
{
  void * context = XP_FindSomeContext(); 
  PRIntervalTime sleep = (PRIntervalTime)PR_INTERVAL_NO_TIMEOUT;
  nsPermState ret_val=nsPermState_NotSet;

  PR_CEnterMonitor((void *)&gPermState);
  /* XXX: The following is a hack, we should passs gPrincipalCert to SECNAV_... code,
   * but all this code will change real soon in the new world order
   */
  gPrincipalCert = cert;
  SECNAV_signedAppletPrivileges(context, prinStr, targetStr, 
                                riskStr, isCert);
  nsUserTargetHandleMonitorError(PR_CWait((void*)&gPermState, sleep));
  nsUserTargetHandleMonitorError(PR_CNotifyAll((void*)&gPermState));
  ret_val = gPermState;
  PR_CExitMonitor((void *)&gPermState);

  PR_Sleep(500000);
  return ret_val;
}
