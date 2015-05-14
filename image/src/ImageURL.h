/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_src_ImageURL_h
#define mozilla_image_src_ImageURL_h

#include "nsIURI.h"
#include "MainThreadUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace image {

/** ImageURL
 *
 * nsStandardURL is not threadsafe, so this class is created to hold only the
 * necessary URL data required for image loading and decoding.
 *
 * Note: Although several APIs have the same or similar prototypes as those
 * found in nsIURI/nsStandardURL, the class does not implement nsIURI. This is
 * intentional; functionality is limited, and is only useful for imagelib code.
 * By not implementing nsIURI, external code cannot unintentionally be given an
 * nsIURI pointer with this limited class behind it; instead, conversion to a
 * fully implemented nsIURI is required (e.g. through NS_NewURI).
 */
class ImageURL
{
public:
  explicit ImageURL(nsIURI* aURI)
  {
    MOZ_ASSERT(NS_IsMainThread(), "Cannot use nsIURI off main thread!");
    aURI->GetSpec(mSpec);
    aURI->GetScheme(mScheme);
    aURI->GetRef(mRef);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageURL)

  nsresult GetSpec(nsACString& result)
  {
    result = mSpec;
    return NS_OK;
  }

  nsresult GetScheme(nsACString& result)
  {
    result = mScheme;
    return NS_OK;
  }

  nsresult SchemeIs(const char* scheme, bool* result)
  {
    NS_PRECONDITION(scheme, "scheme is null");
    NS_PRECONDITION(result, "result is null");

    *result = mScheme.Equals(scheme);
    return NS_OK;
  }

  nsresult GetRef(nsACString& result)
  {
    result = mRef;
    return NS_OK;
  }

  already_AddRefed<nsIURI> ToIURI()
  {
    MOZ_ASSERT(NS_IsMainThread(),
               "Convert to nsIURI on main thread only; it is not threadsafe.");
    nsCOMPtr<nsIURI> newURI;
    NS_NewURI(getter_AddRefs(newURI), mSpec);
    return newURI.forget();
  }

private:
  // Since this is a basic storage class, no duplication of spec parsing is
  // included in the functionality. Instead, the class depends upon the
  // parsing implementation in the nsIURI class used in object construction.
  // This means each field is stored separately, but since only a few are
  // required, this small memory tradeoff for threadsafe usage should be ok.
  nsAutoCString mSpec;
  nsAutoCString mScheme;
  nsAutoCString mRef;

  ~ImageURL() { }
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_src_ImageURL_h
