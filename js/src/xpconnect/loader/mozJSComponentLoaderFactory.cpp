/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prlog.h"
#include "nsIComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "mozJSComponentLoader.h"

class mozJSComponentLoaderFactory : public nsIFactory
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY

    mozJSComponentLoaderFactory() : mRefCnt(0) { };
    virtual ~mozJSComponentLoaderFactory() {
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
    };
    
private:
    nsrefcnt mRefCnt;
}

NS_IMPL_ISUPPORTS(mozJSComponentLoaderFactory, NS_GET_IID(nsIFactory))

nsresult
mozJSComponentLoaderFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID,
					    void **aResult)
{
}

nsresult
mozJSComponentLoaderFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}

