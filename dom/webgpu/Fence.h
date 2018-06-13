/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_Fence_H_
#define WEBGPU_Fence_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
class Promise;
} // namespace dom
namespace webgpu {

class Device;

class Fence final
    : public ChildOf<Device>
{
public:
    WEBGPU_DECL_GOOP(Fence)

private:
    Fence() = delete;
    virtual ~Fence();

public:
    bool Wait(double milliseconds) const;
    already_AddRefed<dom::Promise> Promise() const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_Fence_H_
