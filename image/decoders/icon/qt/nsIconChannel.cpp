/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QIcon>
#include <QStyle>
#include <QApplication>

#include <stdlib.h>
#include <unistd.h>

#include "nsIMIMEService.h"

#include "nsIStringBundle.h"

#include "nsNetUtil.h"
#include "nsIURL.h"
#include "prlink.h"

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
  PRUint8 * const buf = (PRUint8*)NS_Alloc(buf_size);
  NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);
  PRUint8 *out = buf;

  *(out++) = width;
  *(out++) = height;

  const uchar * const pixels = image->bits();
  int rowextra = image->bytesPerLine() - width * n_channels;

  // encode the RGB data and the A data
  const uchar * in = pixels;
  for (int y = 0; y < height; ++y, in += rowextra) {
    for (int x = 0; x < width; ++x) {
      PRUint8 r = *(in++);
      PRUint8 g = *(in++);
      PRUint8 b = *(in++);
      PRUint8 a = *(in++);
#define DO_PREMULTIPLY(c_) PRUint8(PRUint16(c_) * PRUint16(a) / PRUint16(255))
#ifdef IS_LITTLE_ENDIAN
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
                                  NS_LITERAL_CSTRING("image/icon"));
}

nsresult
nsIconChannel::Init(nsIURI* aURI)
{

  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  nsCAutoString stockIcon;
  iconURI->GetStockIcon(stockIcon);

  nsCAutoString iconSizeString;
  iconURI->GetIconSize(iconSizeString);

  PRUint32 desiredImageSize;
  iconURI->GetImageSize(&desiredImageSize);

  nsCAutoString iconStateString;
  iconURI->GetIconState(iconStateString);
  bool disabled = iconStateString.EqualsLiteral("disabled");

  QStyle::StandardPixmap sp_icon = (QStyle::StandardPixmap)0;
  nsCOMPtr <nsIGtkQtIconsConverter> converter = do_GetService("@mozilla.org/gtkqticonsconverter;1");
  if (converter) {
    PRInt32 res = 0;
    stockIcon.Cut(0,4);
    converter->Convert(stockIcon.get(), &res);
    sp_icon = (QStyle::StandardPixmap)res;
    // printf("ConvertIcon: icon:'%s' -> res:%i\n", stockIcon.get(), res);
  }
  if (!sp_icon)
    return NS_ERROR_FAILURE;

  QStyle *style = qApp->style();
  NS_ENSURE_TRUE(style, NS_ERROR_NULL_POINTER);
  QPixmap pixmap = style->standardIcon(sp_icon).pixmap(desiredImageSize, desiredImageSize, disabled?QIcon::Disabled:QIcon::Normal);
  QImage image = pixmap.toImage();

  return moz_qicon_to_channel(&image, iconURI,
                              getter_AddRefs(mRealChannel));
}
