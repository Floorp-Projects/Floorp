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


/*
 * This is a c++ header, which includes other c++ headers.  It needs to
 * be complied as c++ source.
 *
 * If everything in this page needs to be compiled as C code, then why
 * isnt this file a .c file ?
 *
 */
#include "nsUserDialogHelper.h"
#include "xp.h"
#include "xpgetstr.h"
#include "prprf.h"
#include "nsTarget.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef XXX
extern int CAPS_TARGET_RISK_STR_LOW;
extern int CAPS_TARGET_RISK_STR_MEDIUM;
extern int CAPS_TARGET_RISK_STR_HIGH;
extern int CAPS_TARGET_HELP_URL;
#endif

//
// resources for user targets
//
char * JavaSecUI_targetRiskStr(int risk) 
{
#ifdef XXX
  char * str;
  if (risk <= nsRiskType_LowRisk) {
    str = XP_GetString(CAPS_TARGET_RISK_STR_LOW);
  } else if (risk <= nsRiskType_MediumRisk) {
    str = XP_GetString(CAPS_TARGET_RISK_STR_MEDIUM);
  } else {
    str =  XP_GetString(CAPS_TARGET_RISK_STR_HIGH);
  }
  return str;
#else
  return "High";
#endif
}

int JavaSecUI_targetRiskLow(void) 
{
  return nsRiskType_LowRisk;
}

int JavaSecUI_targetRiskMedium(void) 
{
  return nsRiskType_MediumRisk;
}

int JavaSecUI_targetRiskHigh(void) 
{
  return nsRiskType_HighRisk;
}

char * JavaSecUI_getHelpURL(int id) 
{
  char * java_sec_help_url = "";
  char * tag = JavaSecUI_getString(id);
#ifdef XXX 
  java_sec_help_url = XP_GetString(CAPS_TARGET_HELP_URL);
#endif

  PR_ASSERT(tag != NULL);
  PR_ASSERT(java_sec_help_url != NULL);

  char *helpURL = (char *)XP_ALLOC(strlen(java_sec_help_url) + strlen(tag) + 1);
  XP_STRCPY(helpURL, java_sec_help_url);
  XP_STRCAT(helpURL, tag);
  return helpURL;
}

char * JavaSecUI_getString(int id) 
{
#ifdef XXX
  char *str = XP_GetString(id);
#endif
  char *str = capsGetString(id);
  PR_ASSERT(str != NULL);
  return str;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
