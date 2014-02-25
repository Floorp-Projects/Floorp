/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QIcon>

#include <stdlib.h>
#include <unistd.h>

#include "mozilla/Endian.h"

#include "nsMimeTypes.h"
#include "nsIMIMEService.h"

#include "nsIStringBundle.h"

#include "nsNetUtil.h"
#include "nsIURL.h"

#include "nsIconChannel.h"
#include "nsGtkQtIconsConverter.h"

NS_IMPL_ISUPPORTS2(nsIconChannel,
                   nsIRequest,
                   nsIChannel)

static nsresult
moz_qicon_to_channel(QImage *image, nsIURI *aURI,
                     nsIChannel **aChannel)
{
  NS_ENSURE_ARG_POINTER(image);

  int width = image->width();
  int height = image->height();

  NS_ENSURE_TRUE(height < 256 && width < 256 && height > 0 && width > 0,
                 NS_ERROR_UNEXPECTED);

  const int n_channels = 4;
  long int buf_size = 2 + n_channels * height * width;
  uint8_t * const buf = (uint8_t*)NS_Alloc(buf_size);
  NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);
  uint8_t *out = buf;

  *(out++) = width;
  *(out++) = height;

  const uchar * const pixels = image->bits();
  int rowextra = image->bytesPerLine() - width * n_channels;

  // encode the RGB data and the A data
  const uchar * in = pixels;
  for (int y = 0; y < height; ++y, in += rowextra) {
    for (int x = 0; x < width; ++x) {
      uint8_t r = *(in++);
      uint8_t g = *(in++);
      uint8_t b = *(in++);
      uint8_t a = *(in++);
#define DO_PREMULTIPLY(c_) uint8_t(uint16_t(c_) * uint16_t(a) / uint16_t(255))
#if MOZ_LITTLE_ENDIAN
      *(out++) = DO_PREMULTIPLY(b);
      *(out++) = DO_PREMULTIPLY(g);
      *(out++) = DO_PREMULTIPLY(r);
      *(out++) = a;
#else
      *(out++) = a;
      *(out++) = DO_PREMULTIPLY(r);
      *(out++) = DO_PREMULTIPLY(g);
      *(out++) = DO_PREMULTIPLY(b);
#endif
#undef DO_PREMULTIPLY
    }
  }

  NS_ASSERTION(out == buf + buf_size, "size miscalculation");

  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream =
    do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->AdoptData((char*)buf, buf_size);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewInputStreamChannel(aChannel, aURI, stream,
                                  NS_LITERAL_CSTRING(IMAGE_ICON_MS));
}

nsresult
nsIconChannel::Init(nsIURI* aURI)
{

  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  nsAutoCString stockIcon;
  iconURI->GetStockIcon(stockIcon);

  nsAutoCString iconSizeString;
  iconURI->GetIconSize(iconSizeString);

  uint32_t desiredImageSize;
  iconURI->GetImageSize(&desiredImageSize);

  nsAutoCString iconStateString;
  iconURI->GetIconState(iconStateString);
  bool disabled = iconStateString.EqualsLiteral("disabled");

  // This is a workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=662299
  // Try to find corresponding freedesktop icon and fallback to empty QIcon if failed.
  QIcon icon = QIcon::fromTheme(QString(stockIcon.get()).replace("gtk-", "edit-"));
  QPixmap pixmap = icon.pixmap(desiredImageSize, desiredImageSize, disabled ? QIcon::Disabled : QIcon::Normal);

  QImage image = pixmap.toImage();

  return moz_qicon_to_channel(&image, iconURI,
                              getter_AddRefs(mRealChannel));
}
