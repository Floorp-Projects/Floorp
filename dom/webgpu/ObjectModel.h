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

class ObjectBase : public nsWrapperCache {
 private:
  nsString mLabel;

 protected:
  virtual ~ObjectBase() = default;

  // False if this object is definitely invalid.
  //
  // See WebGPU §3.2, "Invalid Internal Objects & Contagious Invalidity".
  //
  // There could also be state in the GPU process indicating that our
  // counterpart object there is invalid; certain GPU process operations will
  // report an error back to use if we try to use it. But if it's useful to know
  // whether the object is "definitely invalid", this should suffice.
  bool mValid = true;

 public:
  // Return true if this WebGPU object may be valid.
  //
  // This is used by methods that want to know whether somebody other than
  // `this` is valid. Generally, WebGPU object methods check `this->mValid`
  // directly.
  bool IsValid() const { return mValid; }

  void GetLabel(nsAString& aValue) const;
  void SetLabel(const nsAString& aLabel);
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

#define GPU_IMPL_CYCLE_COLLECTION(T, ...)            \
  NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(T, AddRef)    \
  NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(T, Release) \
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
