/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InstanceProvider.h"

#include "Instance.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

InstanceProvider::InstanceProvider(nsIGlobalObject* const global)
    : mGlobal(global)
{ }

InstanceProvider::~InstanceProvider() = default;

already_AddRefed<Instance>
InstanceProvider::Webgpu() const
{
    if (!mInstance) {
        const auto inst = Instance::Create(mGlobal);
        mInstance = Some(inst);
    }
    auto ret = mInstance.value();
    return ret.forget();
}

void
InstanceProvider::CcTraverse(nsCycleCollectionTraversalCallback& callback) const
{
    if (mInstance) {
        CycleCollectionNoteChild(callback, mInstance.ref().get(),
                                 "webgpu::InstanceProvider::mInstance", 0);
    }
}

void
InstanceProvider::CcUnlink()
{
    mInstance = Some(nullptr);
}

} // namespace webgpu
} // namespace mozilla
