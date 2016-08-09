/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibilityService.h"

#include "nsAccessiblePivot.h"
#include "nsAccessibilityService.h"

#ifdef A11Y_LOG
#include "Logging.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

xpcAccessibilityService *xpcAccessibilityService::gXPCAccessibilityService = nullptr;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

void
xpcAccessibilityService::ShutdownCallback(nsITimer* aTimer, void* aClosure)
{
  if (CanShutdownAccService()) {
    GetAccService()->Shutdown();
  }

  xpcAccessibilityService* xpcAccService =
    reinterpret_cast<xpcAccessibilityService*>(aClosure);

  if (xpcAccService->mShutdownTimer) {
    xpcAccService->mShutdownTimer->Cancel();
    xpcAccService->mShutdownTimer = nullptr;
  }
}

NS_IMETHODIMP_(MozExternalRefCountType)
xpcAccessibilityService::AddRef(void)
{
  MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(xpcAccessibilityService)
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  if (!mRefCnt.isThreadSafe)
    NS_ASSERT_OWNINGTHREAD(xpcAccessibilityService);
  nsrefcnt count = ++mRefCnt;
  NS_LOG_ADDREF(this, count, "xpcAccessibilityService", sizeof(*this));

  if (mRefCnt > 1) {
    GetOrCreateAccService(false);
  }

  return count;
}

NS_IMETHODIMP_(MozExternalRefCountType)
xpcAccessibilityService::Release(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");

  if (!mRefCnt.isThreadSafe) {
    NS_ASSERT_OWNINGTHREAD(xpcAccessibilityService);
  }

  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "xpcAccessibilityService");

  if (count == 0) {
    if (!mRefCnt.isThreadSafe) {
      NS_ASSERT_OWNINGTHREAD(xpcAccessibilityService);
    }

    mRefCnt = 1; /* stabilize */
    delete (this);
    return 0;
  }

  // When ref count goes down to 1 (held internally as a static reference),
  // it means that there are no more external references to the
  // xpcAccessibilityService and we can attempt to shut down acceessiblity
  // service.
  if (count == 1 && !mShutdownTimer) {
    mShutdownTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (mShutdownTimer) {
      mShutdownTimer->InitWithFuncCallback(ShutdownCallback, this, 100,
                                           nsITimer::TYPE_ONE_SHOT);
    }
  }

  return count;
}

NS_IMPL_QUERY_INTERFACE(xpcAccessibilityService, nsIAccessibilityService,
                                                 nsIAccessibleRetrieval)

NS_IMETHODIMP
xpcAccessibilityService::GetApplicationAccessible(nsIAccessible** aAccessibleApplication)
{
  NS_ENSURE_ARG_POINTER(aAccessibleApplication);

  NS_IF_ADDREF(*aAccessibleApplication = XPCApplicationAcc());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode,
                                          nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;
  if (!aNode) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> node(do_QueryInterface(aNode));
  if (!node) {
    return NS_ERROR_INVALID_ARG;
  }

  DocAccessible* document = GetAccService()->GetDocAccessible(node->OwnerDoc());
  if (document) {
    NS_IF_ADDREF(*aAccessible = ToXPC(document->GetAccessible(node)));
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::GetStringRole(uint32_t aRole, nsAString& aString)
{
  GetAccService()->GetStringRole(aRole, aString);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::GetStringStates(uint32_t aState, uint32_t aExtraState,
                                         nsISupports **aStringStates)
{
  GetAccService()->GetStringStates(aState, aExtraState, aStringStates);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::GetStringEventType(uint32_t aEventType,
                                            nsAString& aString)
{
  GetAccService()->GetStringEventType(aEventType, aString);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::GetStringRelationType(uint32_t aRelationType,
                                               nsAString& aString)
{
  GetAccService()->GetStringRelationType(aRelationType, aString);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::GetAccessibleFromCache(nsIDOMNode* aNode,
                                                nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;
  if (!aNode) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> node(do_QueryInterface(aNode));
  if (!node) {
    return NS_ERROR_INVALID_ARG;
  }

  // Search for an accessible in each of our per document accessible object
  // caches. If we don't find it, and the given node is itself a document, check
  // our cache of document accessibles (document cache). Note usually shutdown
  // document accessibles are not stored in the document cache, however an
  // "unofficially" shutdown document (i.e. not from DocManager) can still
  // exist in the document cache.
  Accessible* accessible = GetAccService()->FindAccessibleInCache(node);
  if (!accessible) {
    nsCOMPtr<nsIDocument> document(do_QueryInterface(node));
    if (document) {
      accessible = mozilla::a11y::GetExistingDocAccessible(document);
    }
  }

  NS_IF_ADDREF(*aAccessible = ToXPC(accessible));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::CreateAccessiblePivot(nsIAccessible* aRoot,
                                               nsIAccessiblePivot** aPivot)
{
  NS_ENSURE_ARG_POINTER(aPivot);
  NS_ENSURE_ARG(aRoot);
  *aPivot = nullptr;

  Accessible* accessibleRoot = aRoot->ToInternalAccessible();
  NS_ENSURE_TRUE(accessibleRoot, NS_ERROR_INVALID_ARG);

  nsAccessiblePivot* pivot = new nsAccessiblePivot(accessibleRoot);
  NS_ADDREF(*aPivot = pivot);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::SetLogging(const nsACString& aModules)
{
#ifdef A11Y_LOG
  logging::Enable(PromiseFlatCString(aModules));
#endif
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibilityService::IsLogged(const nsAString& aModule, bool* aIsLogged)
{
  NS_ENSURE_ARG_POINTER(aIsLogged);
  *aIsLogged = false;

#ifdef A11Y_LOG
  *aIsLogged = logging::IsEnabled(aModule);
#endif

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// NS_GetAccessibilityService
////////////////////////////////////////////////////////////////////////////////

nsresult
NS_GetAccessibilityService(nsIAccessibilityService** aResult)
{
  NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
  *aResult = nullptr;

  GetOrCreateAccService(false);

  xpcAccessibilityService* service = new xpcAccessibilityService();
  NS_ENSURE_TRUE(service, NS_ERROR_OUT_OF_MEMORY);
  xpcAccessibilityService::gXPCAccessibilityService = service;
  NS_ADDREF(*aResult = service);

  return NS_OK;
}
