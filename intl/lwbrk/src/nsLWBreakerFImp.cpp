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

#include "nsLWBRKDll.h"

#include "pratom.h"
#include "nsJISx4501LineBreaker.h"
#include "nsSampleWordBreaker.h"
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

static const PRUnichar gJaNoBegin[] =
{
  0xfffd // to be changed
};
static const PRUnichar gJaNoEnd[] =
{
  0xfffd // to be changed
};
static const PRUnichar gKoNoBegin[] =
{
  0xfffd // to be changed
};
static const PRUnichar gKoNoEnd[] =
{
  0xfffd // to be changed
};
static const PRUnichar gTwNoBegin[] =
{
  0xfffd // to be changed
};
static const PRUnichar gTwNoEnd[] =
{
  0xfffd // to be changed
};
static const PRUnichar gCnNoBegin[] =
{
  0xfffd // to be changed
};
static const PRUnichar gCnNoEnd[] =
{
  0xfffd // to be changed
};
nsresult nsLWBreakerFImp::GetBreaker(nsString& aParam, nsILineBreaker** oResult)
{
  if( NULL == oResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if( aParam == "ja" ) 
  {
     *oResult = new nsJISx4501LineBreaker (
           gJaNoBegin, sizeof(gJaNoBegin)/sizeof(PRUnichar), 
           gJaNoEnd, sizeof(gJaNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam =="ko") 
  {
     *oResult = new nsJISx4501LineBreaker (
           gKoNoBegin, sizeof(gKoNoBegin)/sizeof(PRUnichar), 
           gKoNoEnd, sizeof(gKoNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam =="tw") 
  {
     *oResult = new nsJISx4501LineBreaker (
           gTwNoBegin, sizeof(gTwNoBegin)/sizeof(PRUnichar), 
           gTwNoEnd, sizeof(gTwNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam =="cn") 
  {
     *oResult = new nsJISx4501LineBreaker (
           gCnNoBegin, sizeof(gCnNoBegin)/sizeof(PRUnichar), 
           gCnNoEnd, sizeof(gCnNoEnd)/sizeof(PRUnichar));
  } 
  else 
  {
     *oResult = new nsJISx4501LineBreaker (nsnull, 0, nsnull, 0);
  }
  return (*oResult)->AddRef();
}

nsresult nsLWBreakerFImp::GetBreaker(nsString& aParam, nsIWordBreaker** oResult)
{
  if( NULL == oResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *oResult = new nsSampleWordBreaker ();
  return (*oResult)->AddRef();
}

