/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is 
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsCOMPtr.h"
#include "xptcall.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#ifdef DEBUG
#include <stdio.h>
#endif

////////////////////////////////////////////////////////////////////////
// nsXTFInterfaceAggregator class

class nsXTFInterfaceAggregator : public nsXPTCStubBase
{
protected:
  friend nsresult
  NS_NewXTFInterfaceAggregator(const nsIID& iid,
                               nsISupports* inner,
                               nsISupports* outer,
                               nsISupports** result);

  nsXTFInterfaceAggregator(const nsIID& iid,
                           nsISupports* inner,
                           nsISupports* outer);
  ~nsXTFInterfaceAggregator();
  
public:
  // nsISupports interface
  NS_DECL_ISUPPORTS
  
  // nsXPTCStubBase
  NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);

  NS_IMETHOD CallMethod(PRUint16 methodIndex,
                        const nsXPTMethodInfo* info,
                        nsXPTCMiniVariant* params);

private:
  nsISupports *mInner;
  nsISupports *mOuter;
  nsIID mIID;
};

//----------------------------------------------------------------------
// implementation:

nsXTFInterfaceAggregator::nsXTFInterfaceAggregator(const nsIID& iid,
                                                   nsISupports* inner,
                                                   nsISupports* outer)
    : mOuter(outer), mIID(iid)
{
#ifdef DEBUG
//  printf("nsXTFInterfaceAggregator CTOR\n");
#endif
  inner->QueryInterface(iid, (void**)&mInner);
  mOuter->AddRef();
}

nsXTFInterfaceAggregator::~nsXTFInterfaceAggregator()
{
#ifdef DEBUG
//  printf("nsXTFInterfaceAggregator DTOR\n");
#endif
  mInner->Release();
  mOuter->Release();
}

nsresult
NS_NewXTFInterfaceAggregator(const nsIID& iid,
                             nsISupports* inner,
                             nsISupports* outer,
                             nsISupports** aResult){
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  nsXTFInterfaceAggregator* result = new nsXTFInterfaceAggregator(iid,inner,outer);
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports implementation

NS_IMPL_ADDREF(nsXTFInterfaceAggregator)
NS_IMPL_RELEASE(nsXTFInterfaceAggregator)

NS_IMETHODIMP
nsXTFInterfaceAggregator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if(aIID.Equals(mIID)) {
    *aInstancePtr = NS_STATIC_CAST(nsXPTCStubBase*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else return mOuter->QueryInterface(aIID, aInstancePtr);
}

//----------------------------------------------------------------------
// nsXPTCStubBase implementation

NS_IMETHODIMP
nsXTFInterfaceAggregator::GetInterfaceInfo(nsIInterfaceInfo** info)
{
  nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
  NS_ASSERTION(iim, "could not get interface info manager");
  return iim->GetInfoForIID( &mIID, info);
}

NS_IMETHODIMP
nsXTFInterfaceAggregator::CallMethod(PRUint16 methodIndex,
                                     const nsXPTMethodInfo* info,
                                     nsXPTCMiniVariant* params)
{
  if (methodIndex < 3) {
    NS_ERROR("huh? indirect nsISupports method call unexpected on nsXTFInterfaceAggregator.");
    return NS_ERROR_FAILURE;
  }

  // prepare args:
  int paramCount = info->GetParamCount();
  nsXPTCVariant* fullPars = paramCount ? new nsXPTCVariant[paramCount] : nsnull;

  for (int i=0; i<paramCount; ++i) {
    const nsXPTParamInfo& paramInfo = info->GetParam(i);
    PRUint8 flags = paramInfo.IsOut() ? nsXPTCVariant::PTR_IS_DATA : 0;
    fullPars[i].Init(params[i], paramInfo.GetType(), flags);
  }
  
  // make the call:
  nsresult rv = XPTC_InvokeByIndex(mInner,
                                   methodIndex,
                                   paramCount,
                                   fullPars);
  if (fullPars)
    delete []fullPars;
  return rv;
}
