/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderTextureHostOGL.h"

namespace mozilla {
namespace wr {

RenderTextureHostOGL::RenderTextureHostOGL()
{
  MOZ_COUNT_CTOR_INHERITED(RenderTextureHostOGL, RenderTextureHost);
}

RenderTextureHostOGL::~RenderTextureHostOGL()
{
  MOZ_COUNT_DTOR_INHERITED(RenderTextureHostOGL, RenderTextureHost);
}

} // namespace wr
} // namespace mozilla
