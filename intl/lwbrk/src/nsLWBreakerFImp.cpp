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

#include "nsLWBreakerFImp.h"

#include "nsLWBRKDll.h"

#include "nsJISx4501LineBreaker.h"
#include "nsSampleWordBreaker.h"

nsLWBreakerFImp::nsLWBreakerFImp()
{
}
nsLWBreakerFImp::~nsLWBreakerFImp()
{
}

NS_IMPL_ISUPPORTS2(nsLWBreakerFImp,
                   nsILineBreakerFactory,
                   nsIWordBreakerFactory)

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

nsresult
nsLWBreakerFImp::GetBreaker(const nsAString& aParam, nsILineBreaker** oResult)
{
  nsJISx4051LineBreaker *result;
  if( aParam.EqualsLiteral("ja") ) 
  {
     result = new nsJISx4051LineBreaker (
           gJaNoBegin, sizeof(gJaNoBegin)/sizeof(PRUnichar), 
           gJaNoEnd, sizeof(gJaNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam.EqualsLiteral("ko")) 
  {
     result = new nsJISx4051LineBreaker (
           gKoNoBegin, sizeof(gKoNoBegin)/sizeof(PRUnichar), 
           gKoNoEnd, sizeof(gKoNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam.EqualsLiteral("tw")) 
  {
     result = new nsJISx4051LineBreaker (
           gTwNoBegin, sizeof(gTwNoBegin)/sizeof(PRUnichar), 
           gTwNoEnd, sizeof(gTwNoEnd)/sizeof(PRUnichar));
  } 
  else if(aParam.EqualsLiteral("cn")) 
  {
     result = new nsJISx4051LineBreaker (
           gCnNoBegin, sizeof(gCnNoBegin)/sizeof(PRUnichar), 
           gCnNoEnd, sizeof(gCnNoEnd)/sizeof(PRUnichar));
  } 
  else 
  {
     result = new nsJISx4051LineBreaker (nsnull, 0, nsnull, 0);
  }

  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *oResult = result;

  return NS_OK;
}

nsresult
nsLWBreakerFImp::GetBreaker(const nsAString& aParam, nsIWordBreaker** oResult)
{
  nsSampleWordBreaker *result = new nsSampleWordBreaker ();
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *oResult = result;

  return NS_OK;
}

