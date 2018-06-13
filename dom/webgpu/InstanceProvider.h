/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_INSTANCE_PROVIDER_H_
#define WEBGPU_INSTANCE_PROVIDER_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"

class nsCycleCollectionTraversalCallback;
class nsIGlobalObject;

namespace mozilla {
namespace webgpu {
class Instance;

class InstanceProvider
{
private:
    nsIGlobalObject* const mGlobal;
    mutable Maybe<RefPtr<Instance>> mInstance;

protected:
    explicit InstanceProvider(nsIGlobalObject* global);
    virtual ~InstanceProvider();

public:
    already_AddRefed<Instance> Webgpu() const;

    nsIGlobalObject* GetParentObject() const { return mGlobal; }

    void CcTraverse(nsCycleCollectionTraversalCallback&) const;
    void CcUnlink();
};

template<typename T>
void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            const Maybe<T>& field,
                            const char* name, uint32_t flags)
{
    if (field) {
        CycleCollectionNoteChild(callback, field.value(), name, flags);
    }
}

template<typename T>
void
ImplCycleCollectionUnlink(Maybe<T>& field)
{
    field = Nothing();
}

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_INSTANCE_PROVIDER_H_
