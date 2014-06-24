/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgITools.h"

#define NS_IMGTOOLS_CID \
{ /* 3d8fa16d-c9e1-4b50-bdef-2c7ae249967a */         \
     0x3d8fa16d,                                     \
     0xc9e1,                                         \
     0x4b50,                                         \
    {0xbd, 0xef, 0x2c, 0x7a, 0xe2, 0x49, 0x96, 0x7a} \
}

class imgTools : public imgITools
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGITOOLS

  imgTools();

private:
  virtual ~imgTools();
};
