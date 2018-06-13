/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Instance.h"

#include "Adapter.h"
#include "InstanceProvider.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace webgpu {

/*static*/ RefPtr<Instance>
Instance::Create(nsIGlobalObject* parent)
{
    return new Instance(parent);
}

Instance::Instance(nsIGlobalObject* parent)
    : mParent(parent)
{
}

Instance::~Instance() = default;

already_AddRefed<Adapter>
Instance::GetAdapter(const dom::WebGPUAdapterDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

template<typename T>
void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            const nsCOMPtr<T>& field,
                            const char* name, uint32_t flags)
{
    CycleCollectionNoteChild(callback, field.get(), name, flags);
}

template<typename T>
void
ImplCycleCollectionUnlink(const nsCOMPtr<T>& field)
{
    const auto mut = const_cast<nsCOMPtr<T>*>(&field);
    *mut = nullptr;
}

JSObject*
Instance::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGPU_Binding::Wrap(cx, this, givenProto);
}
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Instance, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Instance, Release)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Instance, mParent)

} // namespace webgpu
} // namespace mozilla
