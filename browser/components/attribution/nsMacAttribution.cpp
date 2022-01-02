/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMacAttribution.h"

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

#include "../../../xpcom/io/CocoaFileUtils.h"
#include "nsCocoaFeatures.h"
#include "nsString.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsMacAttributionService, nsIMacAttributionService)

NS_IMETHODIMP
nsMacAttributionService::SetReferrerUrl(const nsACString& aFilePath,
                                        const nsACString& aReferrerUrl,
                                        const bool aCreate) {
  const nsCString& flat = PromiseFlatCString(aFilePath);
  CFStringRef filePath = ::CFStringCreateWithCString(
      kCFAllocatorDefault, flat.get(), kCFStringEncodingUTF8);

  if (!filePath) {
    return NS_ERROR_UNEXPECTED;
  }

  const nsCString& flatReferrer = PromiseFlatCString(aReferrerUrl);
  CFStringRef referrer = ::CFStringCreateWithCString(
      kCFAllocatorDefault, flatReferrer.get(), kCFStringEncodingUTF8);
  if (!referrer) {
    ::CFRelease(filePath);
    return NS_ERROR_UNEXPECTED;
  }
  CFURLRef referrerURL =
      ::CFURLCreateWithString(kCFAllocatorDefault, referrer, nullptr);

  CocoaFileUtils::AddQuarantineMetadataToFile(filePath, NULL, referrerURL, true,
                                              aCreate);

  ::CFRelease(filePath);
  ::CFRelease(referrer);
  if (referrerURL) {
    ::CFRelease(referrerURL);
  }

  return NS_OK;
}
