/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBlocker.h"
#include "nsIPermissionManager.h"
#include "nsContentUtils.h"
#include "mozilla/StaticPrefs_permissions.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::image;

NS_IMPL_ISUPPORTS(ImageBlocker, nsIContentPolicy)

NS_IMETHODIMP
ImageBlocker::ShouldLoad(nsIURI* aContentLocation, nsILoadInfo* aLoadInfo,
                         int16_t* aShouldLoad) {
  *aShouldLoad = nsIContentPolicy::ACCEPT;

  if (!aContentLocation) {
    // Bug 1720280: Ideally we should block the load, but to avoid a potential
    // null pointer deref, we return early in this case. Please note that
    // the ImageBlocker only applies about http/https loads anyway.
    return NS_OK;
  }

  ExtContentPolicyType contentType = aLoadInfo->GetExternalContentPolicyType();
  if (contentType != ExtContentPolicy::TYPE_IMAGE &&
      contentType != ExtContentPolicy::TYPE_IMAGESET) {
    return NS_OK;
  }

  if (ImageBlocker::ShouldBlock(aContentLocation)) {
    NS_SetRequestBlockingReason(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_CONTENT_BLOCKED);
    *aShouldLoad = nsIContentPolicy::REJECT_TYPE;
  }

  return NS_OK;
}

NS_IMETHODIMP
ImageBlocker::ShouldProcess(nsIURI* aContentLocation, nsILoadInfo* aLoadInfo,
                            int16_t* aShouldProcess) {
  // We block images at load level already, so those should not end up here.
  *aShouldProcess = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

/* static */
bool ImageBlocker::ShouldBlock(nsIURI* aContentLocation) {
  // Block loading images depending on the permissions.default.image pref.
  if (StaticPrefs::permissions_default_image() !=
      nsIPermissionManager::DENY_ACTION) {
    return false;
  }

  // we only want to check http, https
  // for chrome:// and resources and others, no need to check.
  return aContentLocation->SchemeIs("http") ||
         aContentLocation->SchemeIs("https");
}
