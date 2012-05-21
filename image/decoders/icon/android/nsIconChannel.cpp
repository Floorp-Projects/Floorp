/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "mozilla/dom/ContentChild.h"
#include "nsIURL.h"
#include "nsXULAppAPI.h"
#include "AndroidBridge.h"
#include "nsIconChannel.h"

NS_IMPL_ISUPPORTS2(nsIconChannel,
                   nsIRequest,
                   nsIChannel)

using namespace mozilla;
using mozilla::dom::ContentChild;

static nsresult
GetIconForExtension(const nsACString& aFileExt, PRUint32 aIconSize, PRUint8 * const aBuf)
{
    if (!AndroidBridge::Bridge())
        return NS_ERROR_FAILURE;

    AndroidBridge::Bridge()->GetIconForExtension(aFileExt, aIconSize, aBuf);

    return NS_OK;
}

static nsresult
CallRemoteGetIconForExtension(const nsACString& aFileExt, PRUint32 aIconSize, PRUint8 * const aBuf)
{
  NS_ENSURE_TRUE(aBuf != nsnull, NS_ERROR_NULL_POINTER);

  // An array has to be used to get data from remote process
  InfallibleTArray<PRUint8> bits;
  PRUint32 bufSize = aIconSize * aIconSize * 4;

  if (!ContentChild::GetSingleton()->SendGetIconForExtension(PromiseFlatCString(aFileExt), aIconSize, &bits))
    return NS_ERROR_FAILURE;

  NS_ASSERTION(bits.Length() == bufSize, "Pixels array is incomplete");
  if (bits.Length() != bufSize)
    return NS_ERROR_FAILURE;

  memcpy(aBuf, bits.Elements(), bufSize);

  return NS_OK;
}

static nsresult
moz_icon_to_channel(nsIURI *aURI, const nsACString& aFileExt, PRUint32 aIconSize, nsIChannel **aChannel)
{
  NS_ENSURE_TRUE(aIconSize < 256 && aIconSize > 0, NS_ERROR_UNEXPECTED);

  int width = aIconSize;
  int height = aIconSize;

  // moz-icon data should have two bytes for the size,
  // then the ARGB pixel values with pre-multiplied Alpha
  const int channels = 4;
  long int buf_size = 2 + channels * height * width;
  PRUint8 * const buf = (PRUint8*)NS_Alloc(buf_size);
  NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);
  PRUint8 * out = buf;

  *(out++) = width;
  *(out++) = height;

  nsresult rv;
  if (XRE_GetProcessType() == GeckoProcessType_Default)
    rv = GetIconForExtension(aFileExt, aIconSize, out);
  else
    rv = CallRemoteGetIconForExtension(aFileExt, aIconSize, out);
  NS_ENSURE_SUCCESS(rv, rv);

  // Encode the RGBA data
  const PRUint8 * in = out;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      PRUint8 r = *(in++);
      PRUint8 g = *(in++);
      PRUint8 b = *(in++);
      PRUint8 a = *(in++);
#define DO_PREMULTIPLY(c_) PRUint8(PRUint16(c_) * PRUint16(a) / PRUint16(255))
      *(out++) = DO_PREMULTIPLY(b);
      *(out++) = DO_PREMULTIPLY(g);
      *(out++) = DO_PREMULTIPLY(r);
      *(out++) = a;
#undef DO_PREMULTIPLY
    }
  }

  nsCOMPtr<nsIStringInputStream> stream =
    do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->AdoptData((char*)buf, buf_size);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewInputStreamChannel(aChannel, aURI, stream,
                                  NS_LITERAL_CSTRING("image/icon"));
}

nsresult
nsIconChannel::Init(nsIURI* aURI)
{
  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  nsCAutoString stockIcon;
  iconURI->GetStockIcon(stockIcon);

  PRUint32 desiredImageSize;
  iconURI->GetImageSize(&desiredImageSize);

  nsCAutoString iconFileExt;
  iconURI->GetFileExtension(iconFileExt);

  return moz_icon_to_channel(iconURI, iconFileExt, desiredImageSize, getter_AddRefs(mRealChannel));
}
