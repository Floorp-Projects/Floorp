/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Bhuvan Racham <racham@netscape.com>
 *   Peter Weilbacher <mozilla@Weilbacher.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define INCL_DOSMEMMGR
#define INCL_DOSERRORS
#include <os2.h>

#include "nsMessengerOS2Integration.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"

#define WARPCENTER_SHAREDMEM "\\sharemem\\inbox.mem"

nsMessengerOS2Integration::nsMessengerOS2Integration()
{
  PVOID pvObject = NULL;
  PULONG pUnreadState = NULL;
  APIRET rc = DosGetNamedSharedMem((PVOID *)&pUnreadState, WARPCENTER_SHAREDMEM,
                                   PAG_READ | PAG_WRITE);

  if (rc != NO_ERROR) {
    rc = DosAllocSharedMem(&pvObject, WARPCENTER_SHAREDMEM, sizeof(ULONG),
                           PAG_COMMIT | PAG_WRITE);
    pUnreadState = (PULONG)pvObject;
  }
  *pUnreadState = 0;

  mBiffStateAtom = do_GetAtom("BiffState");
  mTotalUnreadMessagesAtom = do_GetAtom("TotalUnreadMessages");
}

nsMessengerOS2Integration::~nsMessengerOS2Integration()
{
  PULONG pUnreadState = NULL;
  APIRET rc = DosGetNamedSharedMem((PVOID *)&pUnreadState, WARPCENTER_SHAREDMEM,
                                   PAG_READ | PAG_WRITE);

  if (rc != NO_ERROR) {
    rc = DosFreeMem(pUnreadState);
  }
}

NS_IMPL_ISUPPORTS2(nsMessengerOS2Integration, nsIMessengerOSIntegration, nsIFolderListener)

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

  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemPropertyChanged(nsIRDFResource *, nsIAtom *, char const *, char const *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemUnicharPropertyChanged(nsIRDFResource *, nsIAtom *, const PRUnichar *, const PRUnichar *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemRemoved(nsIRDFResource *, nsISupports *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemPropertyFlagChanged(nsIMsgDBHdr *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemAdded(nsIRDFResource *, nsISupports *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemBoolPropertyChanged(nsIRDFResource *aItem, nsIAtom *aProperty, PRBool aOldValue, PRBool aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemEvent(nsIMsgFolder *, nsIAtom *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerOS2Integration::OnItemIntPropertyChanged(nsIRDFResource *aItem, nsIAtom *aProperty, PRInt32 aOldValue, PRInt32 aNewValue)
{
  PULONG pUnreadState = NULL;
  APIRET rc = DosGetNamedSharedMem((PVOID *)&pUnreadState, WARPCENTER_SHAREDMEM,
                                   PAG_READ | PAG_WRITE);
  if (rc != NO_ERROR)
    return NS_OK;

  if (aProperty == mBiffStateAtom) {
    if (aNewValue == nsIMsgFolder::nsMsgBiffState_NewMail) {
      *pUnreadState = 1;
    } else if (aNewValue == nsIMsgFolder::nsMsgBiffState_NoMail) {
      *pUnreadState = 0;
    } else {
      // setting nothing, unknown state (nsIMsgFolder::nsMsgBiffState_Unknown)
    }
  } else if (aProperty == mTotalUnreadMessagesAtom) {
    // do nothing for now
    // (we just want to reflect the statusbar mail biff in the system)
  }

  return NS_OK;
}
