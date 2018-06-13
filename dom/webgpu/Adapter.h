/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_Adapter_H_
#define WEBGPU_Adapter_H_

#include "mozilla/AlreadyAddRefed.h"
#include "nsString.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
struct WebGPUDeviceDescriptor;
struct WebGPUExtensions;
struct WebGPUFeatures;
} // namespace dom

namespace webgpu {
class Device;
class Instance;

class Adapter final
    : public ChildOf<Instance>
{
public:
    WEBGPU_DECL_GOOP(Adapter)

    const nsString mName;

private:
    Adapter() = delete;
    virtual ~Adapter();

public:
    void GetName(nsString& out) const {
        out = mName;
    }

    void Extensions(dom::WebGPUExtensions& out) const;
    void Features(dom::WebGPUFeatures& out) const;
    already_AddRefed<Device> CreateDevice(const dom::WebGPUDeviceDescriptor& desc) const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_Adapter_H_
