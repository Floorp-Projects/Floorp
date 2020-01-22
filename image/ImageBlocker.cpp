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
                         const nsACString& aMimeGuess, int16_t* aShouldLoad) {
  uint32_t contentType = aLoadInfo->GetExternalContentPolicyType();
  MOZ_ASSERT(contentType == nsContentUtils::InternalContentPolicyTypeToExternal(
                                contentType),
             "We should only see external content policy types here.");

  *aShouldLoad = nsIContentPolicy::ACCEPT;

  // we only want to check http, https, ftp
  // for chrome:// and resources and others, no need to check.
  nsAutoCString scheme;
  aContentLocation->GetScheme(scheme);
  if (!scheme.LowerCaseEqualsLiteral("ftp") &&
      !scheme.LowerCaseEqualsLiteral("http") &&
      !scheme.LowerCaseEqualsLiteral("https")) {
    return NS_OK;
  }

  // Block loading images depending on the permissions.default.image pref.
  if ((contentType == nsIContentPolicy::TYPE_IMAGE ||
       contentType == nsIContentPolicy::TYPE_IMAGESET) &&
      StaticPrefs::permissions_default_image() ==
          nsIPermissionManager::DENY_ACTION) {
    NS_SetRequestBlockingReason(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_CONTENT_BLOCKED);
    *aShouldLoad = nsIContentPolicy::REJECT_TYPE;
  }

  return NS_OK;
}

NS_IMETHODIMP
ImageBlocker::ShouldProcess(nsIURI* aContentLocation, nsILoadInfo* aLoadInfo,
                            const nsACString& aMimeGuess,
                            int16_t* aShouldProcess) {
#ifdef DEBUG
  uint32_t contentType = aLoadInfo->GetExternalContentPolicyType();
  MOZ_ASSERT(contentType == nsContentUtils::InternalContentPolicyTypeToExternal(
                                contentType),
             "We should only see external content policy types here.");
#endif

  // We block images at load level already, so those should not end up here.
  *aShouldProcess = nsIContentPolicy::ACCEPT;
  return NS_OK;
}
