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

//
// Hopefully, someone will write and XP call like this eventually!
//
#define     TPATH_LEN   1024

#ifdef WIN32
#include "windows.h"
#endif

char *
GetTheTempDirectoryOnTheSystem(void)
{
  char *retPath = (char *)PR_Malloc(TPATH_LEN);
  if (!retPath)
    return nsnull;

  retPath[0] = '\0';
#ifdef WIN32
  if (getenv("TEMP"))
    PL_strncpy(retPath, getenv("TEMP"), TPATH_LEN);  // environment variable
  else if (getenv("TMP"))
    PL_strncpy(retPath, getenv("TMP"), TPATH_LEN);   // How about this environment variable?
  else
    GetWindowsDirectory(retPath, TPATH_LEN);
#endif 

  // RICHIE - should do something better here!

#if defined(XP_UNIX) || defined(XP_BEOS)
  char *tPath = getenv("TMPDIR");
  if (!tPath)
    PL_strncpy(retPath, "/tmp/", TPATH_LEN);
  else
    PL_strncpy(retPath, tPath, TPATH_LEN);
#endif

#ifdef XP_MAC
  PL_strncpy(retPath, "", TPATH_LEN);
#endif

  return retPath;
}

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
//
nsFileSpec * 
nsMsgCreateTempFileSpec(char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  // Age old question, where to store temp files....ugh!
  char  *tDir = GetTheTempDirectoryOnTheSystem();
  if (!tDir)
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N

  nsFileSpec *tmpSpec = new nsFileSpec(tDir);
  if (!tmpSpec)
  {
    PR_FREEIF(tDir);
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N
  }

  *tmpSpec += tFileName;
  tmpSpec->MakeUnique();

  PR_FREEIF(tDir);
  return tmpSpec;
}

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
// returned
//
char * 
nsMsgCreateTempFileName(char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  // Age old question, where to store temp files....ugh!
  char  *tDir = GetTheTempDirectoryOnTheSystem();
  if (!tDir)
    return "mozmail.tmp";  // No need to I18N

  nsFileSpec tmpSpec(tDir);
  tmpSpec += tFileName;
  tmpSpec.MakeUnique();

  PR_FREEIF(tDir);
  char *tString = (char *)PL_strdup(tmpSpec.GetNativePathCString());
  if (!tString)
    return PL_strdup("mozmail.tmp");  // No need to I18N
  else
    return tString;
}

char * 
nsMimePlatformFileToURL (const char *name)
{
	char *prefix = "file:///";
	char *retVal = (char *)PR_Malloc(PL_strlen(name) + PL_strlen(prefix) + 1);
	if (retVal)
	{
		PL_strcpy(retVal, prefix);
		PL_strcat(retVal, name);
	}

  char *ptr = retVal;
  while (*ptr)
  {
    if (*ptr == '\\') *ptr = '/';
    if ( (*ptr == ':') && (ptr > (retVal+4)) )
      *ptr = '|';

    ++ptr;
  }
	return retVal;
}
