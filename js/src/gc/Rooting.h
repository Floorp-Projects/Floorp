/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Rooting_h
#define gc_Rooting_h

#include "gc/Allocator.h"
#include "gc/Policy.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

class JSLinearString;

namespace js {

class PropertyName;
class NativeObject;
class ArrayObject;
class GlobalObject;
class PlainObject;
class ScriptSourceObject;
class SavedFrame;
class Shape;
class ObjectGroup;
class DebuggerArguments;
class DebuggerEnvironment;
class DebuggerFrame;
class DebuggerObject;
class DebuggerScript;
class DebuggerSource;
class Scope;
class ModuleObject;

// These are internal counterparts to the public types such as HandleObject.

using HandleNativeObject = JS::Handle<NativeObject*>;
using HandleShape = JS::Handle<Shape*>;
using HandleObjectGroup = JS::Handle<ObjectGroup*>;
using HandleAtom = JS::Handle<JSAtom*>;
using HandleLinearString = JS::Handle<JSLinearString*>;
using HandlePropertyName = JS::Handle<PropertyName*>;
using HandleArrayObject = JS::Handle<ArrayObject*>;
using HandlePlainObject = JS::Handle<PlainObject*>;
using HandleSavedFrame = JS::Handle<SavedFrame*>;
using HandleScriptSourceObject = JS::Handle<ScriptSourceObject*>;
using HandleDebuggerArguments = JS::Handle<DebuggerArguments*>;
using HandleDebuggerEnvironment = JS::Handle<DebuggerEnvironment*>;
using HandleDebuggerFrame = JS::Handle<DebuggerFrame*>;
using HandleDebuggerObject = JS::Handle<DebuggerObject*>;
using HandleDebuggerScript = JS::Handle<DebuggerScript*>;
using HandleDebuggerSource = JS::Handle<DebuggerSource*>;
using HandleScope = JS::Handle<Scope*>;
using HandleModuleObject = JS::Handle<ModuleObject*>;

using MutableHandleShape = JS::MutableHandle<Shape*>;
using MutableHandleAtom = JS::MutableHandle<JSAtom*>;
using MutableHandleNativeObject = JS::MutableHandle<NativeObject*>;
using MutableHandlePlainObject = JS::MutableHandle<PlainObject*>;
using MutableHandleSavedFrame = JS::MutableHandle<SavedFrame*>;
using MutableHandleDebuggerArguments = JS::MutableHandle<DebuggerArguments*>;
using MutableHandleDebuggerEnvironment =
    JS::MutableHandle<DebuggerEnvironment*>;
using MutableHandleDebuggerFrame = JS::MutableHandle<DebuggerFrame*>;
using MutableHandleDebuggerObject = JS::MutableHandle<DebuggerObject*>;
using MutableHandleDebuggerScript = JS::MutableHandle<DebuggerScript*>;
using MutableHandleDebuggerSource = JS::MutableHandle<DebuggerSource*>;
using MutableHandleScope = JS::MutableHandle<Scope*>;
using MutableHandleModuleObject = JS::MutableHandle<ModuleObject*>;
using MutableHandleArrayObject = JS::MutableHandle<ArrayObject*>;

using RootedNativeObject = JS::Rooted<NativeObject*>;
using RootedShape = JS::Rooted<Shape*>;
using RootedObjectGroup = JS::Rooted<ObjectGroup*>;
using RootedAtom = JS::Rooted<JSAtom*>;
using RootedLinearString = JS::Rooted<JSLinearString*>;
using RootedPropertyName = JS::Rooted<PropertyName*>;
using RootedArrayObject = JS::Rooted<ArrayObject*>;
using RootedGlobalObject = JS::Rooted<GlobalObject*>;
using RootedPlainObject = JS::Rooted<PlainObject*>;
using RootedSavedFrame = JS::Rooted<SavedFrame*>;
using RootedScriptSourceObject = JS::Rooted<ScriptSourceObject*>;
using RootedDebuggerArguments = JS::Rooted<DebuggerArguments*>;
using RootedDebuggerEnvironment = JS::Rooted<DebuggerEnvironment*>;
using RootedDebuggerFrame = JS::Rooted<DebuggerFrame*>;
using RootedDebuggerObject = JS::Rooted<DebuggerObject*>;
using RootedDebuggerScript = JS::Rooted<DebuggerScript*>;
using RootedDebuggerSource = JS::Rooted<DebuggerSource*>;
using RootedScope = JS::Rooted<Scope*>;
using RootedModuleObject = JS::Rooted<ModuleObject*>;

using FunctionVector = JS::GCVector<JSFunction*>;
using PropertyNameVector = JS::GCVector<PropertyName*>;
using ShapeVector = JS::GCVector<Shape*>;
using StringVector = JS::GCVector<JSString*>;

/**
 * Interface substitute for Rooted<T> which does not root the variable's
 * memory.
 */
template <typename T>
class MOZ_RAII FakeRooted : public RootedBase<T, FakeRooted<T>> {
 public:
  using ElementType = T;

  template <typename CX>
  explicit FakeRooted(CX* cx) : ptr(JS::SafelyInitialized<T>()) {}

  template <typename CX>
  FakeRooted(CX* cx, T initial) : ptr(initial) {}

  DECLARE_POINTER_CONSTREF_OPS(T);
  DECLARE_POINTER_ASSIGN_OPS(FakeRooted, T);
  DECLARE_NONPOINTER_ACCESSOR_METHODS(ptr);
  DECLARE_NONPOINTER_MUTABLE_ACCESSOR_METHODS(ptr);

 private:
  T ptr;

  void set(const T& value) { ptr = value; }

  FakeRooted(const FakeRooted&) = delete;
};

/**
 * Interface substitute for MutableHandle<T> which is not required to point to
 * rooted memory.
 */
template <typename T>
class FakeMutableHandle
    : public js::MutableHandleBase<T, FakeMutableHandle<T>> {
 public:
  using ElementType = T;

  MOZ_IMPLICIT FakeMutableHandle(T* t) { ptr = t; }

  MOZ_IMPLICIT FakeMutableHandle(FakeRooted<T>* root) { ptr = root->address(); }

  void set(const T& v) { *ptr = v; }

  DECLARE_POINTER_CONSTREF_OPS(T);
  DECLARE_NONPOINTER_ACCESSOR_METHODS(*ptr);
  DECLARE_NONPOINTER_MUTABLE_ACCESSOR_METHODS(*ptr);

 private:
  FakeMutableHandle() : ptr(nullptr) {}
  DELETE_ASSIGNMENT_OPS(FakeMutableHandle, T);

  T* ptr;
};

/**
 * Types for a variable that either should or shouldn't be rooted, depending on
 * the template parameter allowGC. Used for implementing functions that can
 * operate on either rooted or unrooted data.
 *
 * The toHandle() and toMutableHandle() functions are for calling functions
 * which require handle types and are only called in the CanGC case. These
 * allow the calling code to type check.
 */

template <typename T, AllowGC allowGC>
class MaybeRooted {};

template <typename T>
class MaybeRooted<T, CanGC> {
 public:
  using HandleType = JS::Handle<T>;
  using RootType = JS::Rooted<T>;
  using MutableHandleType = JS::MutableHandle<T>;

  static inline JS::Handle<T> toHandle(HandleType v) { return v; }

  static inline JS::MutableHandle<T> toMutableHandle(MutableHandleType v) {
    return v;
  }

  template <typename T2>
  static inline JS::Handle<T2*> downcastHandle(HandleType v) {
    return v.template as<T2>();
  }
};

template <typename T>
class MaybeRooted<T, NoGC> {
 public:
  using HandleType = const T&;
  using RootType = FakeRooted<T>;
  using MutableHandleType = FakeMutableHandle<T>;

  static JS::Handle<T> toHandle(HandleType v) { MOZ_CRASH("Bad conversion"); }

  static JS::MutableHandle<T> toMutableHandle(MutableHandleType v) {
    MOZ_CRASH("Bad conversion");
  }

  template <typename T2>
  static inline T2* downcastHandle(HandleType v) {
    return &v->template as<T2>();
  }
};

} /* namespace js */

#endif /* gc_Rooting_h */
