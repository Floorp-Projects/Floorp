/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_OBJECT_MODEL_H_
#define WEBGPU_OBJECT_MODEL_H_

#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
namespace webgpu {

template<typename T>
class ChildOf
    : public nsWrapperCache
{
public:
    const RefPtr<T> mParent;

    explicit ChildOf(T* parent = nullptr); // TODO: This can't be nullptr eventually.
protected:
    virtual ~ChildOf();

public:
    nsIGlobalObject* GetParentObject() const;
};

} // namespace webgpu
} // namespace mozilla

#define WEBGPU_DECL_GOOP(T) \
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(T) \
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(T) \
    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

#define WEBGPU_IMPL_GOOP_INTERNAL(T) \
    JSObject* \
    T::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) \
    { \
        return dom::WebGPU ## T ## _Binding::Wrap(cx, this, givenProto); \
    } \
    NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(T, AddRef) \
    NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(T, Release)

#define WEBGPU_IMPL_GOOP(T,...) \
    WEBGPU_IMPL_GOOP_INTERNAL(T) \
    NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(T, mParent, __VA_ARGS__)

#define WEBGPU_IMPL_GOOP_0(T) \
    WEBGPU_IMPL_GOOP_INTERNAL(T) \
    NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(T, mParent)

template<typename T>
void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            const RefPtr<T>& field,
                            const char* name, uint32_t flags)
{
    CycleCollectionNoteChild(callback, field.get(), name, flags);
}

template<typename T>
void
ImplCycleCollectionUnlink(const RefPtr<T>& field)
{
    const auto mutPtr = const_cast< RefPtr<T>* >(&field);
    ImplCycleCollectionUnlink(*mutPtr);
}

#endif // WEBGPU_OBJECT_MODEL_H_
