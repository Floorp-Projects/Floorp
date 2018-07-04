/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_Texture_H_
#define WEBGPU_Texture_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
struct WebGPUTextureViewDescriptor;
} // namespace dom

namespace webgpu {

class Device;
class TextureView;

class Texture final
    : public ChildOf<Device>
{
public:
    WEBGPU_DECL_GOOP(Texture)

private:
    Texture() = delete;
    virtual ~Texture();

public:
    already_AddRefed<TextureView> CreateTextureView(const dom::WebGPUTextureViewDescriptor&) const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_Texture_H_
