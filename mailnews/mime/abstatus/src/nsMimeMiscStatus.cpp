/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#include "nsCOMPtr.h"
#include "stdio.h"
#include "msgCore.h"
#include "nsMimeMiscStatus.h"
#include "nsIPref.h"
#include "nsCRT.h"

// For the new pref API's
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsresult NS_NewMimeMiscStatus(const nsIID& iid, void **result)
{
	nsMimeMiscStatus *obj = new nsMimeMiscStatus();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ADDREF(nsMimeMiscStatus)
NS_IMPL_RELEASE(nsMimeMiscStatus)
NS_IMPL_QUERY_INTERFACE(nsMimeMiscStatus, nsIMimeMiscStatus::GetIID()); /* we need to pass in the interface ID of this interface */

/*
 * nsMimeMiscStatus definitions....
 */
nsMimeMiscStatus::nsMimeMiscStatus()
{
  NS_INIT_REFCNT();
}

nsMimeMiscStatus::~nsMimeMiscStatus(void)
{
}

nsresult
nsMimeMiscStatus::GetMiscStatus(const char *aName, const char *aEmail, PRInt32 *_retval)
{
  return NS_OK;
}

nsresult
nsMimeMiscStatus::GetImageURL(PRInt32 aStatus, char **_retval)
{
  
  return NS_OK;
}

nsresult
nsMimeMiscStatus::GetWindowXULandJS(char **_retval)
{
  return NS_ERROR_FAILURE;
}

nsresult
nsMimeMiscStatus::GetGlobalXULandJS(char **_retval)
{
char  *headerLine = "<html:script language=\"javascript\" src=\"chrome://messenger/content/abookstat.js\"/>";

  *_retval = PL_strdup(headerLine);
  if (*_retval) 
    return NS_OK;
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

const char *XUL = "\
<titledbutton src=\"%s\" class=\"%s\" onclick=\"AddToAB('%s', '%s');\" style=\"vertical-align: middle;\"/>";

nsresult
nsMimeMiscStatus::GetIndividualXUL(const char *aHeader, const char *aName, 
                                   const char *aEmail, char **_retval)
{
  static PRBool   count = -1;
  char *retXUL = nsnull;
  char *className = PR_smprintf("ABOOK-%s", aEmail);

  retXUL = PR_smprintf(XUL,
                      "chrome://messenger/skin/abookstat.gif",
                       className,
                       aName, aEmail);

  PR_FREEIF(className);
  if (retXUL)
  {
    *_retval = retXUL;
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE; 
}
