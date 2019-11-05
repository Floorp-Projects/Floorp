/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Device.h"

#include "Adapter.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION_INHERITED(Device, DOMEventTargetHelper)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(Device, DOMEventTargetHelper)
GPU_IMPL_JS_WRAP(Device)

Device::Device(nsIGlobalObject* aGlobal) : DOMEventTargetHelper(aGlobal) {}

Device::~Device() = default;

void Device::GetLabel(nsAString& aValue) const { aValue = mLabel; }
void Device::SetLabel(const nsAString& aLabel) { mLabel = aLabel; }

}  // namespace webgpu
}  // namespace mozilla
