/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Bienvenu <bienvenu@mozilla.org>
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

#include "msgCore.h"
#include "nsMsgFolderNotificationService.h"


//
//  nsMsgFolderNotificationService
//
NS_IMPL_ISUPPORTS1(nsMsgFolderNotificationService, nsIMsgFolderNotificationService)

nsMsgFolderNotificationService::nsMsgFolderNotificationService()
{
}

nsMsgFolderNotificationService::~nsMsgFolderNotificationService()
{
  /* destructor code */
}

NS_IMETHODIMP nsMsgFolderNotificationService::GetHasListeners(PRBool *aHasListeners)
{
  NS_ENSURE_ARG_POINTER(aHasListeners);
  *aHasListeners = m_listeners.Count() > 0;
  return NS_OK;
}


/* void addListener (in nsIMsgFolderListener aListener); */
NS_IMETHODIMP nsMsgFolderNotificationService::AddListener(nsIMsgFolderListener *aListener)
{
  return m_listeners.AppendObject(aListener);
}

/* void removeListener (in nsIMsgFolderListener aListener); */
NS_IMETHODIMP nsMsgFolderNotificationService::RemoveListener(nsIMsgFolderListener *aListener)
{
  PRInt32 index = m_listeners.IndexOf(aListener);
  NS_ASSERTION(index >= 0, "removing non-existent listener");
  if (index >= 0)
  {
    m_listeners.RemoveObjectAt(index);
  }
  return NS_OK;
}

/* void notifItemAdded (in nsISupports aItem); */
NS_IMETHODIMP nsMsgFolderNotificationService::NotifyItemAdded(nsISupports *aItem)
{
  PRInt32 count = m_listeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgFolderListener> listener = m_listeners[i];
    NS_ASSERTION(listener, "listener is null");
    if (!listener) 
      return NS_ERROR_FAILURE;
    listener->ItemAdded(aItem);
  }
  
  return NS_OK;
}

/* void notifyItemDeleted (in nsISupports aItem); */
NS_IMETHODIMP nsMsgFolderNotificationService::NotifyItemDeleted(nsISupports *aItem)
{
  PRInt32 count = m_listeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgFolderListener> listener = m_listeners[i];
    NS_ASSERTION(listener, "listener is null");
    if (!listener) 
      return NS_ERROR_FAILURE;
    listener->ItemDeleted(aItem);
  }
  
  return NS_OK;
}

/* void notifyItemMoveCopyCompleted (in boolean aMove, in nsISupportsArray aSrcItems, in nsIMsgFolder aDestFolder); */
NS_IMETHODIMP nsMsgFolderNotificationService::NotifyItemMoveCopyCompleted(PRBool aMove, nsISupportsArray *aSrcItems, nsIMsgFolder *aDestFolder)
{
  PRInt32 count = m_listeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgFolderListener> listener = m_listeners[i];
    NS_ASSERTION(listener, "listener is null");
    if (!listener) 
      return NS_ERROR_FAILURE;
    listener->ItemMoveCopyCompleted(aMove, aSrcItems, aDestFolder);
  }
  
  return NS_OK;
}

/* void notifyFolderRenamed (in nsIMsgFolder aOrigFolder, in nsIMsgFolder aNewFolder); */
NS_IMETHODIMP nsMsgFolderNotificationService::NotifyFolderRenamed(nsIMsgFolder *aOrigFolder, nsIMsgFolder *aNewFolder)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void notifyItemEvent (in nsISupports aItem, in string aEvent, in nsISupports aData); */
NS_IMETHODIMP nsMsgFolderNotificationService::NotifyItemEvent(nsISupports *aItem, const nsACString &aEvent, nsISupports *aData)
{
  PRInt32 count = m_listeners.Count();
  
  for(PRInt32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgFolderListener> listener = m_listeners[i];
    NS_ASSERTION(listener, "listener is null");
    if (!listener) 
      return NS_ERROR_FAILURE;
    listener->ItemEvent(aItem, aEvent, aData);
  }
  
  return NS_OK;
}
