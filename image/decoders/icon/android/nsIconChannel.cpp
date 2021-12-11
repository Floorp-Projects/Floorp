/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/NullPrincipal.h"
#include "nsMimeTypes.h"
#include "nsXULAppAPI.h"
#include "AndroidBridge.h"
#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsIStringStream.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"

NS_IMPL_ISUPPORTS(nsIconChannel, nsIRequest, nsIChannel)

using namespace mozilla;
using mozilla::dom::ContentChild;

static nsresult GetIconForExtension(const nsACString& aFileExt,
                                    uint32_t aIconSize, uint8_t* const aBuf) {
  if (!AndroidBridge::Bridge()) {
    return NS_ERROR_FAILURE;
  }

  AndroidBridge::Bridge()->GetIconForExtension(aFileExt, aIconSize, aBuf);

  return NS_OK;
}

static nsresult CallRemoteGetIconForExtension(const nsACString& aFileExt,
                                              uint32_t aIconSize,
                                              uint8_t* const aBuf) {
  NS_ENSURE_TRUE(aBuf != nullptr, NS_ERROR_NULL_POINTER);

  // An array has to be used to get data from remote process
  nsTArray<uint8_t> bits;
  uint32_t bufSize = aIconSize * aIconSize * 4;

  if (!ContentChild::GetSingleton()->SendGetIconForExtension(
          PromiseFlatCString(aFileExt), aIconSize, &bits)) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(bits.Length() == bufSize, "Pixels array is incomplete");
  if (bits.Length() != bufSize) {
    return NS_ERROR_FAILURE;
  }

  memcpy(aBuf, bits.Elements(), bufSize);

  return NS_OK;
}

static nsresult moz_icon_to_channel(nsIURI* aURI, const nsACString& aFileExt,
                                    uint32_t aIconSize, nsIChannel** aChannel) {
  NS_ENSURE_TRUE(aIconSize < 256 && aIconSize > 0, NS_ERROR_UNEXPECTED);

  int width = aIconSize;
  int height = aIconSize;

  // moz-icon data should have two bytes for the size,
  // then the ARGB pixel values with pre-multiplied Alpha
  const int channels = 4;
  CheckedInt32 buf_size =
      4 + channels * CheckedInt32(height) * CheckedInt32(width);
  if (!buf_size.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint8_t* const buf = (uint8_t*)moz_xmalloc(buf_size.value());
  uint8_t* out = buf;

  *(out++) = width;
  *(out++) = height;
  *(out++) = uint8_t(mozilla::gfx::SurfaceFormat::R8G8B8A8);

  // Set all bits to ensure in nsIconDecoder we color manage and premultiply.
  *(out++) = 0xFF;

  nsresult rv;
  if (XRE_IsParentProcess()) {
    rv = GetIconForExtension(aFileExt, aIconSize, out);
  } else {
    rv = CallRemoteGetIconForExtension(aFileExt, aIconSize, out);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringInputStream> stream =
      do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->AdoptData((char*)buf, buf_size.value());
  NS_ENSURE_SUCCESS(rv, rv);

  // nsIconProtocolHandler::NewChannel will provide the correct loadInfo for
  // this iconChannel. Use the most restrictive security settings for the
  // temporary loadInfo to make sure the channel can not be opened.
  nsCOMPtr<nsIPrincipal> nullPrincipal =
      NullPrincipal::CreateWithoutOriginAttributes();
  return NS_NewInputStreamChannel(
      aChannel, aURI, stream.forget(), nullPrincipal,
      nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
      nsIContentPolicy::TYPE_INTERNAL_IMAGE, nsLiteralCString(IMAGE_ICON_MS));
}

nsresult nsIconChannel::Init(nsIURI* aURI) {
  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  nsAutoCString stockIcon;
  iconURI->GetStockIcon(stockIcon);

  uint32_t desiredImageSize;
  iconURI->GetImageSize(&desiredImageSize);

  nsAutoCString iconFileExt;
  iconURI->GetFileExtension(iconFileExt);

  return moz_icon_to_channel(iconURI, iconFileExt, desiredImageSize,
                             getter_AddRefs(mRealChannel));
}
