/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_INSTANCE_H_
#define WEBGPU_INSTANCE_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
struct WebGPUAdapterDescriptor;
} // namespace dom

namespace webgpu {
class Adapter;
class InstanceProvider;

class Instance final
    : public nsWrapperCache
{
public:
    WEBGPU_DECL_GOOP(Instance)

    const nsCOMPtr<nsIGlobalObject> mParent;

    static RefPtr<Instance> Create(nsIGlobalObject* parent);

private:
    explicit Instance(nsIGlobalObject* parent);
    virtual ~Instance();

public:
    nsIGlobalObject* GetParentObject() const { return mParent.get(); }

    already_AddRefed<Adapter> GetAdapter(const dom::WebGPUAdapterDescriptor& desc) const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_INSTANCE_H_
