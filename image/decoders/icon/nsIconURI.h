/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_icon_nsIconURI_h
#define mozilla_image_decoders_icon_nsIconURI_h

#include "nsIIconURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIIPCSerializableURI.h"
#include "nsINestedURI.h"

class nsMozIconURI : public nsIMozIconURI
                   , public nsIIPCSerializableURI
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIMOZICONURI
  NS_DECL_NSIIPCSERIALIZABLEURI

  // nsMozIconURI
  nsMozIconURI();

protected:
  virtual ~nsMozIconURI();
  nsCOMPtr<nsIURL> mIconURL; // a URL that we want the icon for
  uint32_t mSize; // the # of pixels in a row that we want for this image.
                  // Typically 16, 32, 128, etc.
  nsCString mContentType; // optional field explicitly specifying the content
                          // type
  nsCString mFileName; // for if we don't have an actual file path, we're just
                       // given a filename with an extension
  nsCString mStockIcon;
  int32_t mIconSize;   // -1 if not specified, otherwise index into
                       // kSizeStrings
  int32_t mIconState;  // -1 if not specified, otherwise index into
                       // kStateStrings
};

// For moz-icon URIs that point to an actual file on disk and are
// therefore nested URIs
class nsNestedMozIconURI final : public nsMozIconURI
                               , public nsINestedURI
{
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIURI(nsMozIconURI::)
  NS_FORWARD_NSIMOZICONURI(nsMozIconURI::)
  NS_FORWARD_NSIIPCSERIALIZABLEURI(nsMozIconURI::)

  NS_DECL_NSINESTEDURI

  nsNestedMozIconURI();

protected:
  virtual ~nsNestedMozIconURI();

};

#endif // mozilla_image_decoders_icon_nsIconURI_h
