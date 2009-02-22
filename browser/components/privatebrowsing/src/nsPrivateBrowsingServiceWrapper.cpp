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
 * The Original Code is Private Browsing.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari <ehsan.akhgari@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsPrivateBrowsingServiceWrapper.h"
#include "nsServiceManagerUtils.h"
#include "jsapi.h"
#include "nsIJSContextStack.h"

class JSStackGuard
{
public:
  JSStackGuard();
  ~JSStackGuard();

private:
  nsCOMPtr<nsIJSContextStack> mJSStack;
};

NS_IMPL_ISUPPORTS2(nsPrivateBrowsingServiceWrapper, nsIPrivateBrowsingService, nsIObserver)

nsresult
nsPrivateBrowsingServiceWrapper::Init()
{
  nsresult rv;
  mPBService = do_GetService("@mozilla.org/privatebrowsing;1", &rv);
  return rv;
}

JSStackGuard::JSStackGuard()
  : mJSStack(nsnull)
{
  nsresult rv;
  mJSStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);

  if (NS_SUCCEEDED(rv) && mJSStack) {
    rv = mJSStack->Push(nsnull);
    if (NS_FAILED(rv))
      mJSStack = nsnull;
  }
}

JSStackGuard::~JSStackGuard()
{
  if (mJSStack) {
    JSContext *cx;
    mJSStack->Pop(&cx);
    NS_ASSERTION(cx == nsnull, "JSContextStack mismatch");
  }
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::GetPrivateBrowsingEnabled(PRBool *aPrivateBrowsingEnabled)
{
  if (!aPrivateBrowsingEnabled)
    return NS_ERROR_NULL_POINTER;
  JSStackGuard guard;
  return mPBService->GetPrivateBrowsingEnabled(aPrivateBrowsingEnabled);
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::SetPrivateBrowsingEnabled(PRBool aPrivateBrowsingEnabled)
{
  JSStackGuard guard;
  return mPBService->SetPrivateBrowsingEnabled(aPrivateBrowsingEnabled);
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::GetAutoStarted(PRBool *aAutoStarted)
{
  if (!aAutoStarted)
    return NS_ERROR_NULL_POINTER;
  JSStackGuard guard;
  return mPBService->GetAutoStarted(aAutoStarted);
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::RemoveDataFromDomain(const nsACString & aDomain)
{
  JSStackGuard guard;
  return mPBService->RemoveDataFromDomain(aDomain);
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  JSStackGuard guard;
  nsCOMPtr<nsIObserver> observer(do_QueryInterface(mPBService));
  NS_ENSURE_TRUE(observer, NS_ERROR_FAILURE);
  return observer->Observe(aSubject, aTopic, aData);
}
