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
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsXPTCUtils.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsServiceManagerUtils.h"
#include "nsAutoPtr.h"
#ifdef DEBUG
#include <stdio.h>
#endif

////////////////////////////////////////////////////////////////////////
// nsXTFWeakTearoff class

class nsXTFWeakTearoff : protected nsAutoXPTCStub
{
protected:
  ~nsXTFWeakTearoff();
  
public:
  nsXTFWeakTearoff(const nsIID& iid,
                   nsISupports* obj,
                   nsresult *rv);

  // nsISupports interface
  NS_DECL_ISUPPORTS
  
  NS_IMETHOD CallMethod(PRUint16 methodIndex,
                        const XPTMethodDescriptor* info,
                        nsXPTCMiniVariant* params);

private:
  nsISupports *mObj;
  nsIID mIID;
};

//----------------------------------------------------------------------
// implementation:

nsXTFWeakTearoff::nsXTFWeakTearoff(const nsIID& iid,
                                   nsISupports* obj,
                                   nsresult *rv)
  : mObj(obj), mIID(iid)
{
  MOZ_COUNT_CTOR(nsXTFWeakTearoff);

  *rv = InitStub(iid);
}

nsXTFWeakTearoff::~nsXTFWeakTearoff()
{
  MOZ_COUNT_DTOR(nsXTFWeakTearoff);
}

nsresult
NS_NewXTFWeakTearoff(const nsIID& iid,
                     nsISupports* obj,
                     nsISupports** aResult){
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  nsRefPtr<nsXTFWeakTearoff> result =
    new nsXTFWeakTearoff(iid, obj, &rv);
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(rv))
    return rv;

  return result->QueryInterface(iid, (void**) aResult);
}

//----------------------------------------------------------------------
// nsISupports implementation

NS_IMPL_ADDREF(nsXTFWeakTearoff)
NS_IMPL_RELEASE(nsXTFWeakTearoff)

NS_IMETHODIMP
nsXTFWeakTearoff::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_PRECONDITION(aInstancePtr, "null out param");

  if (aIID.Equals(mIID) || aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = mXPTCStub;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  // we can't map QI onto the obj, because the xpcom wrapper otherwise
  // QI-accumulates all interfaces defined on mObj
  //  else return mObj->QueryInterface(aIID, aInstancePtr); 
  *aInstancePtr = nsnull;
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsXTFWeakTearoff::CallMethod(PRUint16 methodIndex,
                             const XPTMethodDescriptor* info,
                             nsXPTCMiniVariant* params)
{
  NS_ASSERTION(methodIndex >= 3,
               "huh? indirect nsISupports method call unexpected");

  // prepare args:
  int paramCount = info->num_args;
  nsXPTCVariant* fullPars;
  if (!paramCount) {
    fullPars = nsnull;
  }
  else {
    fullPars = new nsXPTCVariant[paramCount];
    if (!fullPars)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  for (int i=0; i<paramCount; ++i) {
    const nsXPTParamInfo& paramInfo = info->params[i];
    uint8 flags = paramInfo.IsOut() ? nsXPTCVariant::PTR_IS_DATA : 0;
    fullPars[i].Init(params[i], paramInfo.GetType(), flags);
  }
  
  // make the call:
  nsresult rv = NS_InvokeByIndex(mObj, methodIndex, paramCount, fullPars);
  if (fullPars)
    delete []fullPars;
  return rv;
}
