/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_SwapChain_H_
#define WEBGPU_SwapChain_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
struct WebGPUSwapChainDescriptor;
} // namespace dom

namespace webgpu {

class Device;
class Texture;

class SwapChain final
    : public ChildOf<Device>
{
public:
    WEBGPU_DECL_GOOP(SwapChain)

private:
    SwapChain() = delete;
    virtual ~SwapChain();

public:
    void Configure(const dom::WebGPUSwapChainDescriptor& desc) const;
    already_AddRefed<Texture> GetNextTexture() const;
    void Present() const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_SwapChain_H_
