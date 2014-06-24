/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMozIconURI_h__
#define nsMozIconURI_h__

#include "nsIIconURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_MOZICONURI_CID                            \
{                                                    \
    0x43a88e0e,                                      \
    0x2d37,                                          \
    0x11d5,                                          \
    { 0x99, 0x7, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b }   \
}

class nsMozIconURI : public nsIMozIconURI
{
public:    
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIMOZICONURI

  // nsMozIconURI
  nsMozIconURI();

protected:
  virtual ~nsMozIconURI();
  nsCOMPtr<nsIURL> mIconURL; // a URL that we want the icon for
  uint32_t mSize; // the # of pixels in a row that we want for this image. Typically 16, 32, 128, etc.
  nsCString mContentType; // optional field explicitly specifying the content type
  nsCString mFileName; // for if we don't have an actual file path, we're just given a filename with an extension
  nsCString mStockIcon;
  int32_t mIconSize;     // -1 if not specified, otherwise index into kSizeStrings
  int32_t mIconState;    // -1 if not specified, otherwise index into kStateStrings
};

#endif // nsMozIconURI_h__
