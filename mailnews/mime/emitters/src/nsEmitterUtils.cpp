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
#include "prmem.h"
#include "plstr.h"
#include "nsMailHeaders.h"
#include "nsIMimeEmitter.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "prprf.h"

extern "C" PRBool
EmitThisHeaderForPrefSetting(PRInt32 dispType, const char *header)
{
	if (nsMimeHeaderDisplayTypes::AllHeaders == dispType)
    return PR_TRUE;

  if ((!header) || (!*header))
    return PR_FALSE;

  if (nsMimeHeaderDisplayTypes::MicroHeaders == dispType)
  {
    if (
          (!PL_strcmp(header, HEADER_SUBJECT)) ||
          (!PL_strcmp(header, HEADER_FROM)) ||
          (!PL_strcmp(header, HEADER_DATE))
       )
      return PR_TRUE;
    else
      return PR_FALSE;
  }

  if (nsMimeHeaderDisplayTypes::NormalHeaders == dispType)
  {
    if (
        (!PL_strcmp(header, HEADER_DATE)) ||
        (!PL_strcmp(header, HEADER_TO)) ||
        (!PL_strcmp(header, HEADER_SUBJECT)) ||
        (!PL_strcmp(header, HEADER_SENDER)) ||
        (!PL_strcmp(header, HEADER_RESENT_TO)) ||
        (!PL_strcmp(header, HEADER_RESENT_SENDER)) ||
        (!PL_strcmp(header, HEADER_RESENT_FROM)) ||
        (!PL_strcmp(header, HEADER_RESENT_CC)) ||
        (!PL_strcmp(header, HEADER_REPLY_TO)) ||
        (!PL_strcmp(header, HEADER_REFERENCES)) ||
        (!PL_strcmp(header, HEADER_NEWSGROUPS)) ||
        (!PL_strcmp(header, HEADER_MESSAGE_ID)) ||
        (!PL_strcmp(header, HEADER_FROM)) ||
        (!PL_strcmp(header, HEADER_FOLLOWUP_TO)) ||
        (!PL_strcmp(header, HEADER_CC)) ||
        (!PL_strcmp(header, HEADER_BCC))
       )
       return PR_TRUE;
    else
      return PR_FALSE;
  }

  return PR_TRUE;
}

// 
// This will search a string bundle (eventually) to find a descriptive header 
// name to match what was found in the mail message. aHeaderName is passed in
// in all caps and a dropback default name is provided. The caller needs to free
// the memory returned by this function.
//
extern "C" char *
LocalizeHeaderName(const char *aHeaderName, const char *aDefaultName)
{


  return PL_strdup(aDefaultName);
}

/* This is the next generation string retrieval call */
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#define MIME_URL "resource:/res/mailnews/messenger/mimeheader_en.properties"

extern "C" 
char *
MimeGetStringByNameREAL(const char *aHeaderName)
{
  nsresult    res = NS_OK;
  char*       propertyURL;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->CopyCharPref("mail.headerstrings.mime", &propertyURL);

  if (!NS_SUCCEEDED(res) || !prefs)
    propertyURL = MIME_URL;

  nsCOMPtr<nsIURI> pURI;
  NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &res);
  if (NS_FAILED(res)) 
  {
    if (propertyURL != MIME_URL)
      PR_FREEIF(propertyURL);
    return nsnull;
  }

  res = pService->NewURI(propertyURL, nsnull, getter_AddRefs(pURI));
  if (NS_FAILED(res))
  {
    if (propertyURL != MIME_URL)
      PR_FREEIF(propertyURL);
    return nsnull;
  }

  if (propertyURL != MIME_URL)
  {
    PR_FREEIF(propertyURL);
    propertyURL = nsnull;
  }

  NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
  if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
  {
    nsILocale   *locale = nsnull;
    nsIStringBundle* sBundle = nsnull;
    res = sBundleService->CreateBundle(propertyURL, locale, &sBundle);
    if (NS_FAILED(res)) 
    {
      return nsnull;
    }

    nsAutoString v("");
	PRUnichar * ptrv;
	nsString uniStr(aHeaderName);
	res = sBundle->GetStringFromName(uniStr.GetUnicode(), &ptrv);
	v = ptrv;
    if (NS_FAILED(res)) 
      return nsnull;

    // Here we need to return a new copy of the string
    char      *returnBuffer = NULL;
    PRInt32   bufferLen = v.Length() + 1;

    returnBuffer = (char *)PR_MALLOC(bufferLen);
    if (returnBuffer)
    {
      v.ToCString(returnBuffer, bufferLen);
      return returnBuffer;
    }
  }

  return nsnull;
}

extern "C" 
char *
MimeGetStringByName(const char *aHeaderName)
{
  if (!PL_strcasecmp(aHeaderName, "From")) return PL_strdup("From");

  return nsnull;
}
