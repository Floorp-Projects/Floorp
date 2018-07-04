/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_Queue_H_
#define WEBGPU_Queue_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
template<typename T> class Sequence;
} // namespace dom

namespace webgpu {

class CommandBuffer;
class Device;
class Fence;

class Queue final
    : public ChildOf<Device>
{
public:
    WEBGPU_DECL_GOOP(Queue)

private:
    Queue() = delete;
    virtual ~Queue();

public:
    void Submit(const dom::Sequence<OwningNonNull<CommandBuffer>>& buffers) const;
    already_AddRefed<Fence> InsertFence() const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_Queue_H_
