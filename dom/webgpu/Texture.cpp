/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Texture.h"

#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "TextureView.h"

namespace mozilla {
namespace webgpu {

Texture::~Texture() = default;

already_AddRefed<TextureView>
Texture::CreateTextureView(const dom::WebGPUTextureViewDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

WEBGPU_IMPL_GOOP_0(Texture)

} // namespace webgpu
} // namespace mozilla
