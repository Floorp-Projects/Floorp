/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLMaybeCompiled_h__
#define nsXBLMaybeCompiled_h__

#include "js/RootingAPI.h"

/*
 * A union containing either a pointer representing uncompiled source or a
 * JSObject* representing the compiled result.  The class is templated on the
 * source object type.
 *
 * The purpose of abstracting this as a separate class is to allow it to be
 * wrapped in a JS::Heap<T> to correctly handle post-barriering of the JSObject
 * pointer, when present.
 *
 * No implementation of rootKind() is provided, which prevents
 * Root<nsXBLMaybeCompiled<UncompiledT>> from being used.
 */
template <class UncompiledT>
class nsXBLMaybeCompiled {
 public:
  nsXBLMaybeCompiled() : mUncompiled(BIT_UNCOMPILED) {}

  explicit nsXBLMaybeCompiled(UncompiledT* uncompiled)
      : mUncompiled(reinterpret_cast<uintptr_t>(uncompiled) | BIT_UNCOMPILED) {}

  explicit nsXBLMaybeCompiled(JSObject* compiled) : mCompiled(compiled) {}

  bool IsCompiled() const { return !(mUncompiled & BIT_UNCOMPILED); }

  UncompiledT* GetUncompiled() const {
    MOZ_ASSERT(!IsCompiled(), "Attempt to get compiled function as uncompiled");
    uintptr_t unmasked = mUncompiled & ~BIT_UNCOMPILED;
    return reinterpret_cast<UncompiledT*>(unmasked);
  }

  JSObject* GetJSFunction() const {
    MOZ_ASSERT(IsCompiled(), "Attempt to get uncompiled function as compiled");
    if (mCompiled) {
      JS::ExposeObjectToActiveJS(mCompiled);
    }
    return mCompiled;
  }

  // This is appropriate for use in tracing methods, etc.
  JSObject* GetJSFunctionPreserveColor() const {
    MOZ_ASSERT(IsCompiled(), "Attempt to get uncompiled function as compiled");
    return mCompiled;
  }

 private:
  JSObject*& UnsafeGetJSFunction() {
    MOZ_ASSERT(IsCompiled(), "Attempt to get uncompiled function as compiled");
    return mCompiled;
  }

  enum { BIT_UNCOMPILED = 1 << 0 };

  union {
    // An pointer that represents the function before being compiled, with
    // BIT_UNCOMPILED set.
    uintptr_t mUncompiled;

    // The JS object for the compiled result.
    JSObject* mCompiled;
  };

  friend struct js::BarrierMethods<nsXBLMaybeCompiled<UncompiledT>>;
};

namespace js {

template <class UncompiledT>
struct BarrierMethods<nsXBLMaybeCompiled<UncompiledT>> {
  typedef struct BarrierMethods<JSObject*> Base;

  static void writeBarriers(nsXBLMaybeCompiled<UncompiledT>* functionp,
                            nsXBLMaybeCompiled<UncompiledT> prev,
                            nsXBLMaybeCompiled<UncompiledT> next) {
    if (next.IsCompiled()) {
      Base::writeBarriers(
          &functionp->UnsafeGetJSFunction(),
          prev.IsCompiled() ? prev.UnsafeGetJSFunction() : nullptr,
          next.UnsafeGetJSFunction());
    } else if (prev.IsCompiled()) {
      Base::writeBarriers(&prev.UnsafeGetJSFunction(),
                          prev.UnsafeGetJSFunction(), nullptr);
    }
  }
  static void exposeToJS(nsXBLMaybeCompiled<UncompiledT> fun) {
    if (fun.IsCompiled()) {
      JS::ExposeObjectToActiveJS(fun.UnsafeGetJSFunction());
    }
  }
};

template <class T>
struct IsHeapConstructibleType<
    nsXBLMaybeCompiled<T>> {  // Yes, this is the exception to the rule. Sorry.
  static constexpr bool value = true;
};

template <class UncompiledT, class Wrapper>
class HeapBase<nsXBLMaybeCompiled<UncompiledT>, Wrapper> {
  const Wrapper& wrapper() const { return *static_cast<const Wrapper*>(this); }

  Wrapper& wrapper() { return *static_cast<Wrapper*>(this); }

  const nsXBLMaybeCompiled<UncompiledT>* extract() const {
    return wrapper().address();
  }

  nsXBLMaybeCompiled<UncompiledT>* extract() { return wrapper().unsafeGet(); }

 public:
  bool IsCompiled() const { return extract()->IsCompiled(); }
  UncompiledT* GetUncompiled() const { return extract()->GetUncompiled(); }
  JSObject* GetJSFunction() const { return extract()->GetJSFunction(); }
  JSObject* GetJSFunctionPreserveColor() const {
    return extract()->GetJSFunctionPreserveColor();
  }

  void SetUncompiled(UncompiledT* source) {
    wrapper() = nsXBLMaybeCompiled<UncompiledT>(source);
  }

  void SetJSFunction(JSObject* function) {
    wrapper() = nsXBLMaybeCompiled<UncompiledT>(function);
  }

  JS::Heap<JSObject*>& AsHeapObject() {
    MOZ_ASSERT(extract()->IsCompiled());
    return *reinterpret_cast<JS::Heap<JSObject*>*>(this);
  }
};

} /* namespace js */

#endif  // nsXBLMaybeCompiled_h__
