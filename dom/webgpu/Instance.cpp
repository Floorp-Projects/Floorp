/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Instance.h"

#include "Adapter.h"
#include "InstanceProvider.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(Instance, mParent)

/*static*/
RefPtr<Instance> Instance::Create(nsIGlobalObject* parent) {
  return new Instance(parent);
}

Instance::Instance(nsIGlobalObject* parent) : mParent(parent) {}

Instance::~Instance() = default;

JSObject* Instance::WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) {
  return dom::GPU_Binding::Wrap(cx, this, givenProto);
}

}  // namespace webgpu
}  // namespace mozilla
