/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_BUFFER_H_
#define WEBGPU_BUFFER_H_

#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/TypedArray.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class Buffer final
    : public ChildOf<Device>
{
public:
    JS::Heap<JSObject*> mMapping;

    WEBGPU_DECL_GOOP(Buffer)

private:
    explicit Buffer(Device* parent);
    virtual ~Buffer();

public:
    void GetMapping(JSContext* cx, JS::MutableHandle<JSObject*> out) const;
    void Unmap() const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_BUFFER_H_
