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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"    // precompiled header...

#include "nsMsgRDFFolder.h"
#include "nsIRDFResourceFactory.h"
#include "prmem.h"
#include "plstr.h"

nsMsgRDFFolder::nsMsgRDFFolder(const char* uri)
  : nsRDFResource(uri), mFolder(nsnull)
{
}

nsMsgRDFFolder::~nsMsgRDFFolder()
{
  NS_IF_RELEASE(mFolder);
}

NS_IMPL_ADDREF(nsMsgRDFFolder)
NS_IMPL_RELEASE(nsMsgRDFFolder)

NS_IMETHODIMP
nsMsgRDFFolder::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(nsIMsgRDFFolder::IID())) {
    *result = NS_STATIC_CAST(nsIMsgRDFFolder*, this);
		AddRef();
		return NS_OK;
	}
	return nsRDFResource::QueryInterface(iid, result);
}

NS_IMETHODIMP nsMsgRDFFolder::GetFolder(nsIMsgFolder * *aFolder)
{
	if(!aFolder)
		return NS_ERROR_NULL_POINTER;

	NS_ADDREF(mFolder);
	*aFolder = mFolder;
	return NS_OK;
}

NS_IMETHODIMP nsMsgRDFFolder::SetFolder(nsIMsgFolder * aFolder)
{
	if(aFolder)
	{
		NS_ADDREF(aFolder);
		if(mFolder)	
			NS_RELEASE(mFolder);
		mFolder = aFolder;
	}
	return NS_OK;
}

/**
 * This class creates resources for message folder URIs. It should be
 * registered for the "mailnewsfolder:" prefix.
 */
class nsMsgFolderResourceFactoryImpl : public nsIRDFResourceFactory
{
public:
  nsMsgFolderResourceFactoryImpl(void);
  virtual ~nsMsgFolderResourceFactoryImpl(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateResource(const char* aURI, nsIRDFResource** aResult);
};

nsMsgFolderResourceFactoryImpl::nsMsgFolderResourceFactoryImpl(void)
{
  NS_INIT_REFCNT();
}

nsMsgFolderResourceFactoryImpl::~nsMsgFolderResourceFactoryImpl(void)
{
}

NS_IMPL_ISUPPORTS(nsMsgFolderResourceFactoryImpl, nsIRDFResourceFactory::IID());

NS_IMETHODIMP
nsMsgFolderResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsMsgRDFFolder *folder = new nsMsgRDFFolder(aURI);
  if (! folder)
    return NS_ERROR_OUT_OF_MEMORY;

	folder->QueryInterface(nsIRDFResource::IID(), (void**)aResult);
    return NS_OK;
}

nsresult
NS_NewRDFMsgFolderResourceFactory(nsIRDFResourceFactory** aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsMsgFolderResourceFactoryImpl* factory =
		new nsMsgFolderResourceFactoryImpl();

  if (! factory)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(factory);
  *aResult = factory;
  return NS_OK;
}

