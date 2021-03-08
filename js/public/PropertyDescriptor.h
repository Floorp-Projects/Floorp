/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Property descriptors and flags. */

#ifndef js_PropertyDescriptor_h
#define js_PropertyDescriptor_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT, MOZ_ASSERT_IF

#include <stdint.h>  // uint8_t

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/Class.h"       // JS{Getter,Setter}Op
#include "js/RootingAPI.h"  // JS::Handle, js::{,Mutable}WrappedPtrOperations
#include "js/Value.h"       // JS::Value

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSObject;
class JS_PUBLIC_API JSTracer;

/* Property attributes, set in JSPropertySpec and passed to API functions.
 *
 * The data structure in which some of these values are stored only uses a
 * uint8_t to store the relevant information.  Proceed with caution if trying to
 * reorder or change the the first byte worth of flags.
 */

/** The property is visible in for/in loops. */
static constexpr uint8_t JSPROP_ENUMERATE = 0x01;

/**
 * The property is non-writable.  This flag is only valid when neither
 * JSPROP_GETTER nor JSPROP_SETTER is set.
 */
static constexpr uint8_t JSPROP_READONLY = 0x02;

/**
 * The property is non-configurable: it can't be deleted, and if it's an
 * accessor descriptor, its getter and setter can't be changed.
 */
static constexpr uint8_t JSPROP_PERMANENT = 0x04;

/* (0x08 is unused; add to JSPROP_FLAGS_MASK if ever defined) */

/** The property has a getter function. */
static constexpr uint8_t JSPROP_GETTER = 0x10;

/** The property has a setter function. */
static constexpr uint8_t JSPROP_SETTER = 0x20;

/* (0x40 and 0x80 are unused; add to JSPROP_FLAGS_MASK if ever defined) */

/* (0x1000 is unused; add to JSPROP_FLAGS_MASK if ever defined) */

/**
 * Resolve hooks and enumerate hooks must pass this flag when calling
 * JS_Define* APIs to reify lazily-defined properties.
 *
 * JSPROP_RESOLVING is used only with property-defining APIs. It tells the
 * engine to skip the resolve hook when performing the lookup at the beginning
 * of property definition. This keeps the resolve hook from accidentally
 * triggering itself: unchecked recursion.
 *
 * For enumerate hooks, triggering the resolve hook would be merely silly, not
 * fatal, except in some cases involving non-configurable properties.
 */
static constexpr unsigned JSPROP_RESOLVING = 0x2000;

/**
 * When redefining an existing property, ignore the value of the
 * JSPROP_ENUMERATE flag.  This flag is ignored in other situations.
 */
static constexpr unsigned JSPROP_IGNORE_ENUMERATE = 0x4000;

/**
 * When redefining an existing property, ignore the value of the JSPROP_READONLY
 * flag.  This flag is ignored in other situations.
 */
static constexpr unsigned JSPROP_IGNORE_READONLY = 0x8000;

/**
 * When redefining an existing property, ignore the value of the
 * JSPROP_PERMANENT flag.  This flag is ignored in other situations.
 */
static constexpr unsigned JSPROP_IGNORE_PERMANENT = 0x10000;

/**
 * When redefining an existing property, ignore the Value in the descriptor.
 * This flag is ignored in other situations.
 */
static constexpr unsigned JSPROP_IGNORE_VALUE = 0x20000;

/* (higher flags are unused; add to JSPROP_FLAGS_MASK if ever defined) */

static constexpr unsigned JSPROP_FLAGS_MASK =
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_GETTER |
    JSPROP_SETTER | JSPROP_RESOLVING | JSPROP_IGNORE_ENUMERATE |
    JSPROP_IGNORE_READONLY | JSPROP_IGNORE_PERMANENT | JSPROP_IGNORE_VALUE;

namespace JS {

/**
 * A structure that represents a property on an object, or the absence of a
 * property.  Use {,Mutable}Handle<PropertyDescriptor> to interact with
 * instances of this structure rather than interacting directly with member
 * fields.
 */
struct JS_PUBLIC_API PropertyDescriptor {
  JSObject* obj = nullptr;
  unsigned attrs = 0;
  JSGetterOp getter = nullptr;
  JSSetterOp setter = nullptr;
  Value value;

  PropertyDescriptor() = default;

  void trace(JSTracer* trc);
};

}  // namespace JS

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<JS::PropertyDescriptor, Wrapper> {
  const JS::PropertyDescriptor& desc() const {
    return static_cast<const Wrapper*>(this)->get();
  }

  bool has(unsigned bit) const {
    MOZ_ASSERT(bit != 0);
    MOZ_ASSERT((bit & (bit - 1)) == 0);  // only a single bit
    return (desc().attrs & bit) != 0;
  }

  bool hasAny(unsigned bits) const { return (desc().attrs & bits) != 0; }

  bool hasAll(unsigned bits) const { return (desc().attrs & bits) == bits; }

 public:
  // Descriptors with JSGetterOp/JSSetterOp are considered data
  // descriptors. It's complicated.
  bool isAccessorDescriptor() const {
    return hasAny(JSPROP_GETTER | JSPROP_SETTER);
  }
  bool isGenericDescriptor() const {
    return (desc().attrs & (JSPROP_GETTER | JSPROP_SETTER |
                            JSPROP_IGNORE_READONLY | JSPROP_IGNORE_VALUE)) ==
           (JSPROP_IGNORE_READONLY | JSPROP_IGNORE_VALUE);
  }
  bool isDataDescriptor() const {
    return !isAccessorDescriptor() && !isGenericDescriptor();
  }

  bool hasConfigurable() const { return !has(JSPROP_IGNORE_PERMANENT); }
  bool configurable() const {
    MOZ_ASSERT(hasConfigurable());
    return !has(JSPROP_PERMANENT);
  }

  bool hasEnumerable() const { return !has(JSPROP_IGNORE_ENUMERATE); }
  bool enumerable() const {
    MOZ_ASSERT(hasEnumerable());
    return has(JSPROP_ENUMERATE);
  }

  bool hasValue() const {
    return !isAccessorDescriptor() && !has(JSPROP_IGNORE_VALUE);
  }
  JS::HandleValue value() const {
    return JS::Handle<JS::Value>::fromMarkedLocation(&desc().value);
  }

  bool hasWritable() const {
    return !isAccessorDescriptor() && !has(JSPROP_IGNORE_READONLY);
  }
  bool writable() const {
    MOZ_ASSERT(hasWritable());
    return !has(JSPROP_READONLY);
  }

  bool hasGetterObject() const { return has(JSPROP_GETTER); }
  JS::Handle<JSObject*> getterObject() const {
    MOZ_ASSERT(hasGetterObject());
    return JS::Handle<JSObject*>::fromMarkedLocation(
        reinterpret_cast<JSObject* const*>(&desc().getter));
  }
  bool hasSetterObject() const { return has(JSPROP_SETTER); }
  JS::Handle<JSObject*> setterObject() const {
    MOZ_ASSERT(hasSetterObject());
    return JS::Handle<JSObject*>::fromMarkedLocation(
        reinterpret_cast<JSObject* const*>(&desc().setter));
  }

  bool hasGetterOrSetter() const { return desc().getter || desc().setter; }

  JS::Handle<JSObject*> object() const {
    return JS::Handle<JSObject*>::fromMarkedLocation(&desc().obj);
  }
  unsigned attributes() const { return desc().attrs; }
  JSGetterOp getter() const { return desc().getter; }
  JSSetterOp setter() const { return desc().setter; }

  void assertValid() const {
#ifdef DEBUG
    MOZ_ASSERT((attributes() &
                ~(JSPROP_ENUMERATE | JSPROP_IGNORE_ENUMERATE |
                  JSPROP_PERMANENT | JSPROP_IGNORE_PERMANENT | JSPROP_READONLY |
                  JSPROP_IGNORE_READONLY | JSPROP_IGNORE_VALUE | JSPROP_GETTER |
                  JSPROP_SETTER | JSPROP_RESOLVING)) == 0);
    MOZ_ASSERT(!hasAll(JSPROP_IGNORE_ENUMERATE | JSPROP_ENUMERATE));
    MOZ_ASSERT(!hasAll(JSPROP_IGNORE_PERMANENT | JSPROP_PERMANENT));
    if (isAccessorDescriptor()) {
      MOZ_ASSERT(!has(JSPROP_READONLY));
      MOZ_ASSERT(!has(JSPROP_IGNORE_READONLY));
      MOZ_ASSERT(!has(JSPROP_IGNORE_VALUE));
      MOZ_ASSERT(value().isUndefined());
      MOZ_ASSERT_IF(!has(JSPROP_GETTER), !getter());
      MOZ_ASSERT_IF(!has(JSPROP_SETTER), !setter());
    } else {
      MOZ_ASSERT(!hasAll(JSPROP_IGNORE_READONLY | JSPROP_READONLY));
      MOZ_ASSERT_IF(has(JSPROP_IGNORE_VALUE), value().isUndefined());
    }

    MOZ_ASSERT_IF(has(JSPROP_RESOLVING), !has(JSPROP_IGNORE_ENUMERATE));
    MOZ_ASSERT_IF(has(JSPROP_RESOLVING), !has(JSPROP_IGNORE_PERMANENT));
    MOZ_ASSERT_IF(has(JSPROP_RESOLVING), !has(JSPROP_IGNORE_READONLY));
    MOZ_ASSERT_IF(has(JSPROP_RESOLVING), !has(JSPROP_IGNORE_VALUE));
#endif
  }

  void assertComplete() const {
#ifdef DEBUG
    assertValid();
    MOZ_ASSERT((attributes() &
                ~(JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY |
                  JSPROP_GETTER | JSPROP_SETTER | JSPROP_RESOLVING)) == 0);
    MOZ_ASSERT_IF(isAccessorDescriptor(),
                  has(JSPROP_GETTER) && has(JSPROP_SETTER));
#endif
  }

  void assertCompleteIfFound() const {
#ifdef DEBUG
    if (object()) {
      assertComplete();
    }
#endif
  }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<JS::PropertyDescriptor, Wrapper>
    : public js::WrappedPtrOperations<JS::PropertyDescriptor, Wrapper> {
  JS::PropertyDescriptor& desc() { return static_cast<Wrapper*>(this)->get(); }

 public:
  void clear() {
    object().set(nullptr);
    setAttributes(0);
    setGetter(nullptr);
    setSetter(nullptr);
    value().setUndefined();
  }

  void initFields(JS::Handle<JSObject*> obj, JS::Handle<JS::Value> v,
                  unsigned attrs, JSGetterOp getterOp, JSSetterOp setterOp) {
    object().set(obj);
    value().set(v);
    setAttributes(attrs);
    setGetter(getterOp);
    setSetter(setterOp);
  }

  void assign(JS::PropertyDescriptor& other) {
    object().set(other.obj);
    setAttributes(other.attrs);
    setGetter(other.getter);
    setSetter(other.setter);
    value().set(other.value);
  }

  void setDataDescriptor(JS::Handle<JS::Value> v, unsigned attrs) {
    MOZ_ASSERT((attrs & ~(JSPROP_ENUMERATE | JSPROP_PERMANENT |
                          JSPROP_READONLY | JSPROP_IGNORE_ENUMERATE |
                          JSPROP_IGNORE_PERMANENT | JSPROP_IGNORE_READONLY)) ==
               0);
    object().set(nullptr);
    setAttributes(attrs);
    setGetter(nullptr);
    setSetter(nullptr);
    value().set(v);
  }

  JS::MutableHandle<JSObject*> object() {
    return JS::MutableHandle<JSObject*>::fromMarkedLocation(&desc().obj);
  }
  unsigned& attributesRef() { return desc().attrs; }
  JSGetterOp& getter() { return desc().getter; }
  JSSetterOp& setter() { return desc().setter; }
  JS::MutableHandle<JS::Value> value() {
    return JS::MutableHandle<JS::Value>::fromMarkedLocation(&desc().value);
  }
  void setValue(JS::Handle<JS::Value> v) {
    MOZ_ASSERT(!(desc().attrs & (JSPROP_GETTER | JSPROP_SETTER)));
    attributesRef() &= ~JSPROP_IGNORE_VALUE;
    value().set(v);
  }

  void setConfigurable(bool configurable) {
    setAttributes(
        (desc().attrs & ~(JSPROP_IGNORE_PERMANENT | JSPROP_PERMANENT)) |
        (configurable ? 0 : JSPROP_PERMANENT));
  }
  void setEnumerable(bool enumerable) {
    setAttributes(
        (desc().attrs & ~(JSPROP_IGNORE_ENUMERATE | JSPROP_ENUMERATE)) |
        (enumerable ? JSPROP_ENUMERATE : 0));
  }
  void setWritable(bool writable) {
    MOZ_ASSERT(!(desc().attrs & (JSPROP_GETTER | JSPROP_SETTER)));
    setAttributes((desc().attrs & ~(JSPROP_IGNORE_READONLY | JSPROP_READONLY)) |
                  (writable ? 0 : JSPROP_READONLY));
  }
  void setAttributes(unsigned attrs) { desc().attrs = attrs; }

  void setGetter(JSGetterOp op) { desc().getter = op; }
  void setSetter(JSSetterOp op) { desc().setter = op; }
  void setGetterObject(JSObject* obj) {
    desc().getter = reinterpret_cast<JSGetterOp>(obj);
    desc().attrs &=
        ~(JSPROP_IGNORE_VALUE | JSPROP_IGNORE_READONLY | JSPROP_READONLY);
    desc().attrs |= JSPROP_GETTER;
  }
  void setSetterObject(JSObject* obj) {
    desc().setter = reinterpret_cast<JSSetterOp>(obj);
    desc().attrs &=
        ~(JSPROP_IGNORE_VALUE | JSPROP_IGNORE_READONLY | JSPROP_READONLY);
    desc().attrs |= JSPROP_SETTER;
  }

  JS::MutableHandle<JSObject*> getterObject() {
    MOZ_ASSERT(this->hasGetterObject());
    return JS::MutableHandle<JSObject*>::fromMarkedLocation(
        reinterpret_cast<JSObject**>(&desc().getter));
  }
  JS::MutableHandle<JSObject*> setterObject() {
    MOZ_ASSERT(this->hasSetterObject());
    return JS::MutableHandle<JSObject*>::fromMarkedLocation(
        reinterpret_cast<JSObject**>(&desc().setter));
  }
};

}  // namespace js

namespace JS {

extern JS_PUBLIC_API bool ObjectToCompletePropertyDescriptor(
    JSContext* cx, Handle<JSObject*> obj, Handle<Value> descriptor,
    MutableHandle<PropertyDescriptor> desc);

/*
 * ES6 draft rev 32 (2015 Feb 2) 6.2.4.4 FromPropertyDescriptor(Desc).
 *
 * If desc.object() is null, then vp is set to undefined.
 */
extern JS_PUBLIC_API bool FromPropertyDescriptor(
    JSContext* cx, Handle<PropertyDescriptor> desc, MutableHandle<Value> vp);

}  // namespace JS

#endif /* js_PropertyDescriptor_h */
