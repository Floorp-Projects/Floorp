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

#include "nsTransactionManager.h"
#include "COM_auto_ptr.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);

nsTransactionManager::nsTransactionManager()
{
}

nsTransactionManager::~nsTransactionManager()
{
}

NS_IMPL_ADDREF(nsTransactionManager)
NS_IMPL_RELEASE(nsTransactionManager)

nsresult
nsTransactionManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITransactionManagerIID)) {
    *aInstancePtr = (void*)(nsITransactionManager*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

nsresult
nsTransactionManager::Do(nsITransaction *aTransaction)
{
  return NS_OK;
}

nsresult
nsTransactionManager::Undo()
{
  return NS_OK;
}

nsresult
nsTransactionManager::Redo()
{
  return NS_OK;
}

nsresult
nsTransactionManager::GetNumberOfUndoItems(PRInt32 *aNumItems)
{
  if (aNumItems)
    *aNumItems = 0;

  return NS_OK;
}

nsresult
nsTransactionManager::GetNumberOfRedoItems(PRInt32 *aNumItems)
{
  if (aNumItems)
    *aNumItems = 0;

  return NS_OK;
}

nsresult
nsTransactionManager::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult
nsTransactionManager::AddListener(nsITransactionListener *aListener)
{
  return NS_OK;
}

nsresult
nsTransactionManager::RemoveListener(nsITransactionListener *aListener)
{
  return NS_OK;
}

