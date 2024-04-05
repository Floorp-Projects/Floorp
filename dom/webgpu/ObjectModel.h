/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_OBJECT_MODEL_H_
#define GPU_OBJECT_MODEL_H_

#include "nsWrapperCache.h"
#include "nsString.h"

class nsIGlobalObject;

namespace mozilla::webgpu {
class WebGPUChild;

template <typename T>
class ChildOf {
 protected:
  explicit ChildOf(T* const parent);
  virtual ~ChildOf();

  RefPtr<T> mParent;

 public:
  nsIGlobalObject* GetParentObject() const;
};

/// Most WebGPU DOM objects inherit from this class.
///
/// mValid should only be used in the destruction steps in Cleanup() to check
/// whether they have already run. This is because the destruction steps can be
/// triggered by either the object's destructor or the cycle collector
/// attempting to break a cycle. As a result, all methods accessible from JS can
/// assume that mValid is true.
///
/// Similarly, pointers to the device and the IPDL actor (bridge) can be assumed
/// to be non-null whenever the object is accessible from JS but not during
/// cleanup as they might have been snatched by cycle collection.
///
/// The general pattern is that all objects should implement Cleanup more or
/// less the same way. Cleanup should be the only function sending the
/// corresponding Drop message and cleanup should *never* be called by anything
/// other than the object destructor or the cycle collector.
///
/// These rules guarantee that:
/// - The Drop message is called only once and that no other IPC message
/// referring
///   to the same object is send after Drop.
/// - Any method outside of the destruction sequence can assume the pointers are
///   non-null. They only have to check that the IPDL actor can send messages
///   using `WebGPUChild::CanSend()`.
class ObjectBase : public nsWrapperCache {
 protected:
  virtual ~ObjectBase() = default;

  /// False during the destruction sequence of the object.
  bool mValid = true;

 public:
  void GetLabel(nsAString& aValue) const;
  void SetLabel(const nsAString& aLabel);

  auto CLabel() const { return NS_ConvertUTF16toUTF8(mLabel); }

 protected:
  // Object label, initialized from GPUObjectDescriptorBase.label.
  nsString mLabel;
};

}  // namespace mozilla::webgpu

#define GPU_DECL_JS_WRAP(T)                                             \
  JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) \
      override;

#define GPU_DECL_CYCLE_COLLECTION(T)                    \
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(T) \
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(T)

#define GPU_IMPL_JS_WRAP(T)                                                  \
  JSObject* T::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) { \
    return dom::GPU##T##_Binding::Wrap(cx, this, givenProto);                \
  }

// Note: we don't use `NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE` directly
// because there is a custom action we need to always do.
#define GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(T, ...) \
  NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(T)       \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(T)             \
    tmp->Cleanup();                                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)       \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER  \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                  \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(T)           \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)     \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(T, ...) \
  NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(T)                \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(T)                      \
    tmp->Cleanup();                                             \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER           \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR                    \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                           \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(T)                    \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)              \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(T, P, ...) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(T)                                 \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(T, P)             \
    tmp->Cleanup();                                                 \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                    \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER               \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR                        \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                               \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(T, P)           \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)                  \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define GPU_IMPL_CYCLE_COLLECTION(T, ...) \
  GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(T, __VA_ARGS__)

template <typename T>
void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                                 nsTArray<RefPtr<const T>>& field,
                                 const char* name, uint32_t flags) {
  for (auto& element : field) {
    CycleCollectionNoteChild(callback, const_cast<T*>(element.get()), name,
                             flags);
  }
}

template <typename T>
void ImplCycleCollectionUnlink(nsTArray<RefPtr<const T>>& field) {
  for (auto& element : field) {
    ImplCycleCollectionUnlink(element);
  }
  field.Clear();
}

#endif  // GPU_OBJECT_MODEL_H_
