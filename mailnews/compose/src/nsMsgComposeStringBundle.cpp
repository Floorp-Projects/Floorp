/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsMsgComposeStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsIIOService.h"
#include "nsIURI.h"

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define COMPOSE_BE_URL       "resource:/chrome/messengercompose/content/default/composebe_en.properties"

extern "C" 
char *
ComposeBEGetStringByIDREAL(PRInt32 stringID)
{
  nsresult    res;
  char*       propertyURL = NULL;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->CopyCharPref("mail.strings.compose_be", &propertyURL);

  if (!NS_SUCCEEDED(res) || !prefs)
    propertyURL = COMPOSE_BE_URL;

  NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &res);
  if (!NS_SUCCEEDED(res) || (nsnull == pNetService)) 
  {
      return PL_strdup("???");   // Don't I18N this string...failsafe return value
  }

  NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
  if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
  {
    nsCOMPtr<nsIURI>	url;
    nsILocale   *locale = nsnull;

#if 1
    nsIStringBundle* sBundle = nsnull;
    res = sBundleService->CreateBundle(propertyURL, locale, &sBundle);
#else
    res = pNetService->NewURI(propertyURL, nsnull, getter_AddRefs(url));
    // cleanup...if necessary
    if (propertyURL != COMPOSE_BE_URL)
      PR_FREEIF(propertyURL);

    // Cleanup property URL
    PR_FREEIF(propertyURL);

    if (NS_FAILED(res)) 
    {
      return PL_strdup("???");   // Don't I18N this string...failsafe return value
    }

    nsIStringBundle* sBundle = nsnull;
    res = sBundleService->CreateBundle(url, locale, &sBundle);
#endif
    if (NS_FAILED(res)) 
    {
      return PL_strdup("???");   // Don't I18N this string...failsafe return value
    }

    nsAutoString v("");
#if 1
    PRUnichar *ptrv = nsnull;
    res = sBundle->GetStringFromID(stringID, &ptrv);
    v = ptrv;
#else
    res = sBundle->GetStringFromID(stringID, v);
#endif
    if (NS_FAILED(res)) 
    {
      char    buf[128];

      PR_snprintf(buf, sizeof(buf), "[StringID %d?]", stringID);
      return PL_strdup(buf);
    }

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

  return PL_strdup("???");   // Don't I18N this string...failsafe return value
}

extern "C" 
char *
ComposeBEGetStringByID(PRInt32 stringID)
{
  if (-1000 == stringID) return PL_strdup("Application is out of memory.");
  if (-1001 == stringID) return PL_strdup("Unable to open the temporary file\n.\n%s\nCheck your `Temporary Directory' setting and try again.");
  if (-1002 == stringID) return PL_strdup("Error writing temporary file.");
  if (1000 == stringID) return PL_strdup("Subject");
  if (1001 == stringID) return PL_strdup("Resent-Comments");
  if (1002 == stringID) return PL_strdup("Resent-Date");
  if (1003 == stringID) return PL_strdup("Resent-Sender");
  if (1004 == stringID) return PL_strdup("Resent-From");
  if (1005 == stringID) return PL_strdup("Resent-To");
  if (1006 == stringID) return PL_strdup("Resent-CC");
  if (1007 == stringID) return PL_strdup("Date");
  if (1008 == stringID) return PL_strdup("Sender");
  if (1009 == stringID) return PL_strdup("From");
  if (1010 == stringID) return PL_strdup("Reply-To");
  if (1011 == stringID) return PL_strdup("Organization");
  if (1012 == stringID) return PL_strdup("To");
  if (1013 == stringID) return PL_strdup("CC");
  if (1014 == stringID) return PL_strdup("Newsgroups");
  if (1015 == stringID) return PL_strdup("Followup-To");
  if (1016 == stringID) return PL_strdup("References");
  if (1017 == stringID) return PL_strdup("Name");
  if (1018 == stringID) return PL_strdup("Type");
  if (1019 == stringID) return PL_strdup("Encoding");
  if (1020 == stringID) return PL_strdup("Description");
  if (1021 == stringID) return PL_strdup("Message-ID");
  if (1022 == stringID) return PL_strdup("Resent-Message-ID");
  if (1023 == stringID) return PL_strdup("BCC");
  if (1024 == stringID) return PL_strdup("Download Status");
  if (1025 == stringID) return PL_strdup("Not Downloaded Inline");
  if (1026 == stringID) return PL_strdup("Link to Document");
  if (1027 == stringID) return PL_strdup("<B>Document Info:</B>");
  if (1028 == stringID) return PL_strdup("Attachment");
  if (1029 == stringID) return PL_strdup("forward.msg");
  if (1030 == stringID) return PL_strdup("Add %s to your Address Book");
  if (1031 == stringID) return PL_strdup("<B><FONT COLOR=\042#808080\042>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Internal</FONT></B>");
  if (1032 == stringID) return PL_strdup("In message   wrote:<P>");
  if (1033 == stringID) return PL_strdup(" wrote:<P>");
  if (1034 == stringID) return PL_strdup("(no headers)");
  if (1035 == stringID) return PL_strdup("Toggle Attachment Pane");

  char    buf[128];
  
  PR_snprintf(buf, sizeof(buf), "[StringID %d?]", stringID);
  return PL_strdup(buf);
}
