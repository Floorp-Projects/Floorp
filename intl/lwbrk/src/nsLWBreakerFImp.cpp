/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *
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

#include "nsLWBreakerFImp.h"

#include "nsLWBRKDll.h"

#include "pratom.h"
#include "nsJISx4501LineBreaker.h"
#include "nsSampleWordBreaker.h"
nsLWBreakerFImp::nsLWBreakerFImp()
{
  NS_INIT_REFCNT();
}
nsLWBreakerFImp::~nsLWBreakerFImp()
{
}

NS_DEFINE_IID(kILineBreakerFactoryIID, NS_ILINEBREAKERFACTORY_IID);
NS_DEFINE_IID(kIWordBreakerFactoryIID, NS_IWORDBREAKERFACTORY_IID);
NS_DEFINE_IID(kILineBreakerIID, NS_ILINEBREAKER_IID);
NS_DEFINE_IID(kIWordBreakerIID, NS_IWORDBREAKER_IID);
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
  if( aParam.EqualsWithConversion("ja") ) 
  {
     *oResult = new nsJISx4501LineBreaker (
           gJaNoBegin, sizeof(gJaNoBegin)/sizeof(PRUnichar), 
           gJaNoEnd, sizeof(gJaNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam.EqualsWithConversion("ko")) 
  {
     *oResult = new nsJISx4501LineBreaker (
           gKoNoBegin, sizeof(gKoNoBegin)/sizeof(PRUnichar), 
           gKoNoEnd, sizeof(gKoNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam.EqualsWithConversion("tw")) 
  {
     *oResult = new nsJISx4501LineBreaker (
           gTwNoBegin, sizeof(gTwNoBegin)/sizeof(PRUnichar), 
           gTwNoEnd, sizeof(gTwNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam.EqualsWithConversion("cn")) 
  {
     *oResult = new nsJISx4501LineBreaker (
           gCnNoBegin, sizeof(gCnNoBegin)/sizeof(PRUnichar), 
           gCnNoEnd, sizeof(gCnNoEnd)/sizeof(PRUnichar));
  } 
  else 
  {
     *oResult = new nsJISx4501LineBreaker (nsnull, 0, nsnull, 0);
  }

  if (*oResult == NULL) return NS_ERROR_OUT_OF_MEMORY;
  (*oResult)->AddRef();
  return NS_OK;
}

nsresult nsLWBreakerFImp::GetBreaker(nsString& aParam, nsIWordBreaker** oResult)
{
  if( NULL == oResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *oResult = new nsSampleWordBreaker ();
  if (*oResult == NULL) return NS_ERROR_OUT_OF_MEMORY;
  (*oResult)->AddRef();
  return NS_OK;
}

