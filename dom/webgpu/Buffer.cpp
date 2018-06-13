/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Buffer.h"

#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

Buffer::Buffer(Device* const parent)
    : ChildOf(parent)
{
    mozilla::HoldJSObjects(this); // Mimed from PushSubscriptionOptions
}

Buffer::~Buffer()
{
    mMapping = nullptr;
    mozilla::DropJSObjects(this);
}

void
Buffer::GetMapping(JSContext*, JS::MutableHandle<JSObject*> out) const
{
    out.set(mMapping);
}

void
Buffer::Unmap() const
{
    MOZ_CRASH("todo");
}

JSObject*
webgpu::Buffer::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGPUBuffer_Binding::Wrap(cx, this, givenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Buffer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Buffer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mMapping = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Buffer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Buffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMapping)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Buffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Buffer, Release)

} // namespace webgpu
} // namespace mozilla
