/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
nsPrivateBrowsingServiceWrapper::GetPrivateBrowsingEnabled(bool *aPrivateBrowsingEnabled)
{
  if (!aPrivateBrowsingEnabled)
    return NS_ERROR_NULL_POINTER;
  JSStackGuard guard;
  return mPBService->GetPrivateBrowsingEnabled(aPrivateBrowsingEnabled);
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::SetPrivateBrowsingEnabled(bool aPrivateBrowsingEnabled)
{
  JSStackGuard guard;
  return mPBService->SetPrivateBrowsingEnabled(aPrivateBrowsingEnabled);
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::GetAutoStarted(bool *aAutoStarted)
{
  if (!aAutoStarted)
    return NS_ERROR_NULL_POINTER;
  JSStackGuard guard;
  return mPBService->GetAutoStarted(aAutoStarted);
}

NS_IMETHODIMP
nsPrivateBrowsingServiceWrapper::GetLastChangedByCommandLine(bool *aReason)
{
  if (!aReason)
    return NS_ERROR_NULL_POINTER;
  JSStackGuard guard;
  return mPBService->GetLastChangedByCommandLine(aReason);
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
