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

#include "transactionManager.h"
#include "COM_auto_ptr.h"

nsTransactionManager::nsTransactionManager()
{
}

nsTransactionManager::~nsTransactionManager()
{
}

NS_IMPL_ADDREF(nsTransactionManager)
NS_IMPL_RELEASE(nsTransactionManager)

nsresult
nsTransactionManager::Execute(nsITransaction *tx)
{
  return NS_OK;
}

nsresult
nsTransactionManager::Undo(PRInt32 n)
{
  return NS_OK;
}

nsresult
nsTransactionManager::Redo(PRInt32 n)
{
  return NS_OK;
}

nsresult
nsTransactionManager::Write(nsIOutputStream *os)
{
  return NS_OK;
}

