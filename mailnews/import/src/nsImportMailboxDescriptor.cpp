/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


#include "nscore.h"
#include "nsImportMailboxDescriptor.h"

////////////////////////////////////////////////////////////////////////



NS_METHOD nsImportMailboxDescriptor::Create( nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsImportMailboxDescriptor *it = new nsImportMailboxDescriptor();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF( it);
  nsresult rv = it->QueryInterface( aIID, aResult);
  NS_RELEASE( it);
  return rv;
}

NS_IMPL_ISUPPORTS(nsImportMailboxDescriptor, nsIImportMailboxDescriptor::GetIID());

nsImportMailboxDescriptor::nsImportMailboxDescriptor() 
{ 
	NS_INIT_ISUPPORTS(); 
	m_import = PR_TRUE;
	m_pFileSpec = nsnull;
	m_size = 0;
	m_depth = 0;
	m_id = 0;
	NS_NewFileSpec( &m_pFileSpec);
}
