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

#include "nsLWBreakerFImp.h"

#include "nsLWIMP.h"

#include "pratom.h"
#include "nsLWBRKDll.h"
nsLWBreakerFImp::nsLWBreakerFImp()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
nsLWBreakerFImp::~nsLWBreakerFImp()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_DEFINE_IID(kILineBreakerFactoryIID, NS_ILINEBREAKERFACTORY_IID);
NS_DEFINE_IID(kIWordBreakerFactoryIID, NS_ILINEBREAKERFACTORY_IID);
NS_DEFINE_IID(kILineBreakerIID, NS_ILINEBREAKER_IID);
NS_DEFINE_IID(kIWordBreakerIID, NS_ILINEBREAKER_IID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


NS_IMPL_ADDREF  (  nsLWBreakerFImp );
NS_IMPL_RELEASE (  nsLWBreakerFImp );

nsresult nsLWBreakerFImp::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  
  if( NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  *aInstancePtr = NULL;

  if( aIID.Equals ( kILineBreakerFactoryIID )) {
    *aInstancePtr = (void*) ((nsILineBreakerFactory*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if( aIID.Equals ( kIWordBreakerFactoryIID )) {
    *aInstancePtr = (void*) ((nsIWordBreakerFactory*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if( aIID.Equals ( kISupportsIID )) {
    *aInstancePtr = (void*) (this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult nsLWBreakerFImp::GetBreaker(nsString& aParam, nsILineBreaker** oResult)
{
  if( NULL == oResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *oResult = new LINEBREAKER ();
  return (*oResult)->AddRef();
}

nsresult nsLWBreakerFImp::GetBreaker(nsString& aParam, nsIWordBreaker** oResult)
{
  if( NULL == oResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *oResult = new WORDBREAKER ();
  return (*oResult)->AddRef();
}

