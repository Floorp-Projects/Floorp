/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Seth Spitzer <sspitzer@netscape.com>
 * Bhuvan Racham <racham@netscape.com>
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

#define INCL_DOSMEMMGR
#define INCL_DOSERRORS
#include <os2.h>

#include "nsMessengerOS2Integration.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"

nsMessengerOS2Integration::nsMessengerOS2Integration()
{
  APIRET rc;
  PVOID pvObject = NULL;
  PULONG pUnreadCount = NULL;

  rc = DosGetNamedSharedMem((PVOID *)&pUnreadCount, "\\sharemem\\inbox.mem", PAG_READ | PAG_WRITE);

  if (rc != NO_ERROR) {
     rc = DosAllocSharedMem(&pvObject, "\\sharemem\\inbox.mem", sizeof(ULONG), PAG_COMMIT | PAG_WRITE);
     pUnreadCount = (PULONG)pvObject;
  }
  *pUnreadCount = 0;
}

nsMessengerOS2Integration::~nsMessengerOS2Integration()
{
  APIRET rc;
  PVOID pvObject = NULL;
  PULONG pUnreadCount = NULL;

  rc = DosGetNamedSharedMem((PVOID *)&pUnreadCount, "\\sharemem\\inbox.mem", PAG_READ | PAG_WRITE);

  if (rc != NO_ERROR) {
     rc = DosFreeMem(pUnreadCount);
  }
}

NS_IMPL_ADDREF(nsMessengerOS2Integration);
NS_IMPL_RELEASE(nsMessengerOS2Integration);

NS_INTERFACE_MAP_BEGIN(nsMessengerOS2Integration)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIFolderListener)
NS_INTERFACE_MAP_END

nsresult
nsMessengerOS2Integration::Init()
{
  nsresult rv;

  nsCOMPtr <nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the default server changes
  rv = accountManager->AddRootFolderListener(this);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the unread total count changes
  rv = mailSession->AddFolderListener(this, nsIFolderListener::boolPropertyChanged | nsIFolderListener::intPropertyChanged);
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemPropertyChanged(nsISupports *, nsIAtom *, char const *, char const *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemUnicharPropertyChanged(nsISupports *, nsIAtom *, const PRUnichar *, const PRUnichar *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemRemoved(nsISupports *, nsISupports *, const char *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemPropertyFlagChanged(nsISupports *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
  if (newFlag == nsIMsgFolder::nsMsgBiffState_NewMail) 
  {
     APIRET rc;
     PULONG pUnreadCount = NULL;
     printf("Change icon to newmail\n");
     rc = DosGetNamedSharedMem((PVOID *)&pUnreadCount, "\\sharemem\\inbox.mem", PAG_READ | PAG_WRITE);
     *pUnreadCount = 1;
  }
  else if (newFlag == nsIMsgFolder::nsMsgBiffState_NoMail)
  {
     APIRET rc;
     PULONG pUnreadCount = NULL;
     printf("Change icon to newmail\n");
     rc = DosGetNamedSharedMem((PVOID *)&pUnreadCount, "\\sharemem\\inbox.mem", PAG_READ | PAG_WRITE);
     *pUnreadCount = 0;
     printf("Change icon to nomail\n");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemAdded(nsISupports *, nsISupports *, const char *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemBoolPropertyChanged(nsISupports *aItem,
                                                         nsIAtom *aProperty,
                                                         PRBool aOldValue,
                                                         PRBool aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemEvent(nsIFolder *, nsIAtom *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemIntPropertyChanged(nsISupports *aItem, nsIAtom *aProperty, PRInt32 aOldValue, PRInt32 aNewValue)
{
  return NS_OK;
}


