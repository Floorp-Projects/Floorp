/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgITools.h"

#define NS_IMGTOOLS_CID \
{ /* fd9a9e8a-a77b-496a-b7bb-263df9715149 */         \
     0xfd9a9e8a,                                     \
     0xa77b,                                         \
     0x496a,                                         \
    {0xb7, 0xbb, 0x26, 0x3d, 0xf9, 0x71, 0x51, 0x49} \
}

class imgTools : public imgITools
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGITOOLS

  imgTools();
  virtual ~imgTools();
};
