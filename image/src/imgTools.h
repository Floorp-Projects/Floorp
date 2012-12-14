/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgITools.h"
#include "gfxContext.h"

#define NS_IMGTOOLS_CID \
{ /* 4c2383a4-931c-484d-8c4a-973590f66e3f */         \
     0x4c2383a4,                                     \
     0x931c,                                         \
     0x484d,                                         \
    {0x8c, 0x4a, 0x97, 0x35, 0x90, 0xf6, 0x6e, 0x3f} \
}

class imgTools : public imgITools
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGITOOLS

  imgTools();
  virtual ~imgTools();

private:
  NS_IMETHODIMP EncodeImageData(gfxImageSurface *aSurface,
                                const nsACString& aMimeType,
                                const nsAString& aOutputOptions,
                                nsIInputStream **aStream);

  NS_IMETHODIMP GetFirstImageFrame(imgIContainer *aContainer,
                                   gfxImageSurface **aSurface);
};
