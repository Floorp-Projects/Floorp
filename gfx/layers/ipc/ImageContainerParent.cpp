/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageContainerParent.h"

#include "nsThreadUtils.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace layers {

ImageContainerParent::~ImageContainerParent()
{
  while (!mImageHosts.IsEmpty()) {
    mImageHosts[mImageHosts.Length() - 1]->SetImageContainer(nullptr);
  }
}

bool ImageContainerParent::RecvAsyncDelete()
{
  Unused << PImageContainerParent::Send__delete__(this);
  return true;
}

} // namespace layers
} // namespace mozilla
