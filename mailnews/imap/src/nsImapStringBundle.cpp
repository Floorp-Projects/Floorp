/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsImapStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define IMAP_MSGS_URL       "chrome://messenger/locale/imapMsgs.properties"

extern "C" 
nsresult
IMAPGetStringByID(PRInt32 stringID, PRUnichar **aString)
{
  nsresult res=NS_OK;
  nsCOMPtr <nsIStringBundle> sBundle;
  res = IMAPGetStringBundle(getter_AddRefs(sBundle));
  if (NS_SUCCEEDED(res) && sBundle)
    res = sBundle->GetStringFromID(stringID, aString);
  return res;
}

nsresult
IMAPGetStringBundle(nsIStringBundle **aBundle)
{
  nsresult rv=NS_OK;
  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!stringService) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIStringBundle> stringBundle;
  rv = stringService->CreateBundle(IMAP_MSGS_URL, getter_AddRefs(stringBundle));
  *aBundle = stringBundle;
  NS_IF_ADDREF(*aBundle);
  return rv;
}
