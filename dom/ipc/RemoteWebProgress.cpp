/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWebProgress.h"

namespace mozilla::dom {

NS_IMPL_ADDREF(RemoteWebProgress)
NS_IMPL_RELEASE(RemoteWebProgress)

NS_INTERFACE_MAP_BEGIN(RemoteWebProgress)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
NS_INTERFACE_MAP_END

NS_IMETHODIMP RemoteWebProgress::AddProgressListener(
    nsIWebProgressListener* aListener, uint32_t aNotifyMask) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgress::RemoveProgressListener(
    nsIWebProgressListener* aListener) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgress::GetDOMWindow(mozIDOMWindowProxy** aDOMWindow) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP RemoteWebProgress::GetIsTopLevel(bool* aIsTopLevel) {
  NS_ENSURE_ARG_POINTER(aIsTopLevel);
  *aIsTopLevel = mIsTopLevel;
  return NS_OK;
}

NS_IMETHODIMP RemoteWebProgress::GetIsLoadingDocument(
    bool* aIsLoadingDocument) {
  NS_ENSURE_ARG_POINTER(aIsLoadingDocument);
  *aIsLoadingDocument = mIsLoadingDocument;
  return NS_OK;
}

NS_IMETHODIMP RemoteWebProgress::GetLoadType(uint32_t* aLoadType) {
  NS_ENSURE_ARG_POINTER(aLoadType);
  *aLoadType = mLoadType;
  return NS_OK;
}

NS_IMETHODIMP RemoteWebProgress::GetTarget(nsIEventTarget** aTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgress::SetTarget(nsIEventTarget* aTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla::dom
