/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBlocker.h"
#include "nsIPermissionManager.h"
#include "nsContentUtils.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::image;

bool ImageBlocker::sInitialized = false;
int32_t ImageBlocker::sImagePermission = nsIPermissionManager::ALLOW_ACTION;

NS_IMPL_ISUPPORTS(ImageBlocker, nsIContentPolicy)

ImageBlocker::ImageBlocker()
{
  if (!sInitialized) {
    Preferences::AddIntVarCache(&sImagePermission,
                                "permissions.default.image");
    sInitialized = true;
  }
}

NS_IMETHODIMP
ImageBlocker::ShouldLoad(uint32_t aContentType,
                         nsIURI* aContentLocation,
                         nsIURI* aRequestOrigin,
                         nsISupports* aContext,
                         const nsACString& aMimeTypeGuess,
                         nsISupports* aExtra,
                         nsIPrincipal* aRequestPrincipal,
                         int16_t* aShouldLoad)
{
  MOZ_ASSERT(aContentType == nsContentUtils::InternalContentPolicyTypeToExternal(aContentType),
             "We should only see external content policy types here.");

  *aShouldLoad = nsIContentPolicy::ACCEPT;
  if (!aContentLocation) {
    return NS_OK;
  }

  nsAutoCString scheme;
  Unused << aContentLocation->GetScheme(scheme);
  if (!scheme.LowerCaseEqualsLiteral("ftp") &&
      !scheme.LowerCaseEqualsLiteral("http") &&
      !scheme.LowerCaseEqualsLiteral("https")) {
    return NS_OK;
  }


  // Block loading images depending on the permissions.default.image pref.
  if (aContentType == nsIContentPolicy::TYPE_IMAGE ||
      aContentType == nsIContentPolicy::TYPE_IMAGESET) {
    *aShouldLoad = (sImagePermission == nsIPermissionManager::ALLOW_ACTION) ?
                     nsIContentPolicy::ACCEPT :
                     nsIContentPolicy::REJECT_TYPE;
  }
  return NS_OK;
}

NS_IMETHODIMP
ImageBlocker::ShouldProcess(uint32_t aContentType,
                            nsIURI* aContentLocation,
                            nsIURI* aRequestOrigin,
                            nsISupports* aRequestingContext,
                            const nsACString& aMimeTypeGuess,
                            nsISupports* aExtra,
                            nsIPrincipal* aRequestPrincipal,
                            int16_t* aShouldProcess)
{
  MOZ_ASSERT(aContentType == nsContentUtils::InternalContentPolicyTypeToExternal(aContentType),
             "We should only see external content policy types here.");

  return ShouldLoad(aContentType, aContentLocation, aRequestOrigin,
                    aRequestingContext, aMimeTypeGuess, aExtra,
		    aRequestPrincipal, aShouldProcess);
}
