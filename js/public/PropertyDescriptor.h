/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Property descriptors and flags. */

#ifndef js_PropertyDescriptor_h
#define js_PropertyDescriptor_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT, MOZ_ASSERT_IF
#include "mozilla/EnumSet.h"     // mozilla::EnumSet
#include "mozilla/Maybe.h"       // mozilla::Maybe

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

/**
 * The property is exposed as a data property to JS code, but instead of an
 * object slot it uses custom get/set logic.
 *
 * This is used to implement the special array.length and ArgumentsObject
 * properties.
 *
 * This attribute is deprecated (we don't want to add more uses) and for
 * internal use only. This attribute never shows up in a PropertyDescriptor.
 */
static constexpr uint8_t JSPROP_CUSTOM_DATA_PROP = 0x08;

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
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT |
    JSPROP_CUSTOM_DATA_PROP | JSPROP_GETTER | JSPROP_SETTER | JSPROP_RESOLVING |
    JSPROP_IGNORE_ENUMERATE | JSPROP_IGNORE_READONLY | JSPROP_IGNORE_PERMANENT |
    JSPROP_IGNORE_VALUE;

namespace JS {

// 6.1.7.1 Property Attributes
enum class PropertyAttribute : uint8_t {
  // The descriptor is [[Configurable]] := true.
  Configurable,

  // The descriptor is [[Enumerable]] := true.
  Enumerable,

  // The descriptor is [[Writable]] := true. Only valid for data descriptors.
  Writable
};

using PropertyAttributes = mozilla::EnumSet<PropertyAttribute>;

/**
 * A structure that represents a property on an object, or the absence of a
 * property.  Use {,Mutable}Handle<PropertyDescriptor> to interact with
 * instances of this structure rather than interacting directly with member
 * fields.
 */
struct JS_PUBLIC_API PropertyDescriptor {
 private:
  unsigned attrs_ = 0;
  JSObject* getter_ = nullptr;
  JSObject* setter_ = nullptr;
  Value value_;

 public:
  PropertyDescriptor() = default;

  void trace(JSTracer* trc);

  // Construct a new complete DataDescriptor.
  static PropertyDescriptor Data(const Value& value,
                                 PropertyAttributes attributes = {}) {
    PropertyDescriptor desc;
    desc.setConfigurable(attributes.contains(PropertyAttribute::Configurable));
    desc.setEnumerable(attributes.contains(PropertyAttribute::Enumerable));
    desc.setWritable(attributes.contains(PropertyAttribute::Writable));
    desc.value_ = value;
    desc.assertComplete();
    return desc;
  }

  // This constructor is only provided for legacy code!
  static PropertyDescriptor Data(const Value& value, unsigned attrs) {
    MOZ_ASSERT((attrs &
                ~(JSPROP_PERMANENT | JSPROP_ENUMERATE | JSPROP_READONLY)) == 0);

    PropertyDescriptor desc;
    desc.attrs_ = attrs;
    desc.value_ = value;
    desc.assertComplete();
    return desc;
  }

  // Construct a new complete AccessorDescriptor.
  // Note: This means JSPROP_GETTER and JSPROP_SETTER are always set.
  static PropertyDescriptor Accessor(JSObject* getter, JSObject* setter,
                                     PropertyAttributes attributes = {}) {
    MOZ_ASSERT(!attributes.contains(PropertyAttribute::Writable));

    PropertyDescriptor desc;
    desc.setConfigurable(attributes.contains(PropertyAttribute::Configurable));
    desc.setEnumerable(attributes.contains(PropertyAttribute::Enumerable));
    desc.setGetterObject(getter);
    desc.setSetterObject(setter);
    desc.assertComplete();
    return desc;
  }

  // This constructor is only provided for legacy code!
  static PropertyDescriptor Accessor(JSObject* getter, JSObject* setter,
                                     unsigned attrs) {
    MOZ_ASSERT((attrs & ~(JSPROP_PERMANENT | JSPROP_ENUMERATE)) == 0);

    PropertyDescriptor desc;
    desc.attrs_ = attrs;
    desc.setGetterObject(getter);
    desc.setSetterObject(setter);
    desc.assertComplete();
    return desc;
  }

 private:
  bool has(unsigned bit) const {
    MOZ_ASSERT(bit != 0);
    MOZ_ASSERT((bit & (bit - 1)) == 0);  // only a single bit
    return (attrs_ & bit) != 0;
  }

  bool hasAny(unsigned bits) const { return (attrs_ & bits) != 0; }

  bool hasAll(unsigned bits) const { return (attrs_ & bits) == bits; }

 public:
  bool isAccessorDescriptor() const {
    return hasAny(JSPROP_GETTER | JSPROP_SETTER);
  }
  bool isGenericDescriptor() const {
    return (attrs_ & (JSPROP_GETTER | JSPROP_SETTER | JSPROP_IGNORE_READONLY |
                      JSPROP_IGNORE_VALUE)) ==
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
  void setConfigurable(bool configurable) {
    attrs_ = (attrs_ & ~(JSPROP_IGNORE_PERMANENT | JSPROP_PERMANENT)) |
             (configurable ? 0 : JSPROP_PERMANENT);
  }

  bool hasEnumerable() const { return !has(JSPROP_IGNORE_ENUMERATE); }
  bool enumerable() const {
    MOZ_ASSERT(hasEnumerable());
    return has(JSPROP_ENUMERATE);
  }
  void setEnumerable(bool enumerable) {
    attrs_ = (attrs_ & ~(JSPROP_IGNORE_ENUMERATE | JSPROP_ENUMERATE)) |
             (enumerable ? JSPROP_ENUMERATE : 0);
  }

  bool hasValue() const {
    return !isAccessorDescriptor() && !has(JSPROP_IGNORE_VALUE);
  }
  JS::Handle<JS::Value> value() const {
    return JS::Handle<JS::Value>::fromMarkedLocation(&value_);
  }
  JS::MutableHandle<JS::Value> value() {
    return JS::MutableHandle<JS::Value>::fromMarkedLocation(&value_);
  }
  void setValue(JS::Handle<JS::Value> v) {
    MOZ_ASSERT(!isAccessorDescriptor());
    attrs_ &= ~JSPROP_IGNORE_VALUE;
    value_ = v;
  }

  bool hasWritable() const {
    return !isAccessorDescriptor() && !has(JSPROP_IGNORE_READONLY);
  }
  bool writable() const {
    MOZ_ASSERT(hasWritable());
    return !has(JSPROP_READONLY);
  }
  void setWritable(bool writable) {
    MOZ_ASSERT(!isAccessorDescriptor());
    attrs_ = (attrs_ & ~(JSPROP_IGNORE_READONLY | JSPROP_READONLY)) |
             (writable ? 0 : JSPROP_READONLY);
  }

  bool hasGetterObject() const { return has(JSPROP_GETTER); }
  JSObject* getterObject() const {
    MOZ_ASSERT(hasGetterObject());
    return getter_;
  }
  void setGetterObject(JSObject* obj) {
    getter_ = obj;
    attrs_ &= ~(JSPROP_IGNORE_VALUE | JSPROP_IGNORE_READONLY | JSPROP_READONLY);
    attrs_ |= JSPROP_GETTER;
  }

  bool hasSetterObject() const { return has(JSPROP_SETTER); }
  JSObject* setterObject() const {
    MOZ_ASSERT(hasSetterObject());
    return setter_;
  }
  void setSetterObject(JSObject* obj) {
    setter_ = obj;
    attrs_ &= ~(JSPROP_IGNORE_VALUE | JSPROP_IGNORE_READONLY | JSPROP_READONLY);
    attrs_ |= JSPROP_SETTER;
  }

  bool hasGetterOrSetter() const { return getter_ || setter_; }

  unsigned attributes() const { return attrs_; }
  void setAttributesDoNotUse(unsigned attrs) { attrs_ = attrs; }

  JSObject** getterDoNotUse() { return &getter_; }
  JSObject* const* getterDoNotUse() const { return &getter_; }
  void setGetterDoNotUse(JSObject* obj) { getter_ = obj; }
  JSObject** setterDoNotUse() { return &setter_; }
  JSObject* const* setterDoNotUse() const { return &setter_; }
  void setSetterDoNotUse(JSObject* obj) { setter_ = obj; }

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
      MOZ_ASSERT_IF(!has(JSPROP_GETTER), !getter_);
      MOZ_ASSERT_IF(!has(JSPROP_SETTER), !setter_);
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
};

}  // namespace JS

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<JS::PropertyDescriptor, Wrapper> {
  const JS::PropertyDescriptor& desc() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool isAccessorDescriptor() const { return desc().isAccessorDescriptor(); }
  bool isGenericDescriptor() const { return desc().isGenericDescriptor(); }
  bool isDataDescriptor() const { return desc().isDataDescriptor(); }

  bool hasConfigurable() const { return desc().hasConfigurable(); }
  bool configurable() const { return desc().configurable(); }

  bool hasEnumerable() const { return desc().hasEnumerable(); }
  bool enumerable() const { return desc().enumerable(); }

  bool hasValue() const { return desc().hasValue(); }
  JS::HandleValue value() const { return desc().value(); }

  bool hasWritable() const { return desc().hasWritable(); }
  bool writable() const { return desc().writable(); }

  bool hasGetterObject() const { return desc().hasGetterObject(); }
  JS::Handle<JSObject*> getterObject() const {
    MOZ_ASSERT(hasGetterObject());
    return JS::Handle<JSObject*>::fromMarkedLocation(desc().getterDoNotUse());
  }
  bool hasSetterObject() const { return desc().hasSetterObject(); }
  JS::Handle<JSObject*> setterObject() const {
    MOZ_ASSERT(hasSetterObject());
    return JS::Handle<JSObject*>::fromMarkedLocation(desc().setterDoNotUse());
  }

  bool hasGetterOrSetter() const { return desc().hasGetterObject(); }

  unsigned attributes() const { return desc().attributes(); }

  void assertValid() const { desc().assertValid(); }

  void assertComplete() const { desc().assertComplete(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<JS::PropertyDescriptor, Wrapper>
    : public js::WrappedPtrOperations<JS::PropertyDescriptor, Wrapper> {
  JS::PropertyDescriptor& desc() { return static_cast<Wrapper*>(this)->get(); }

 public:
  void clear() {
    setAttributes(0);
    desc().setGetterDoNotUse(nullptr);
    desc().setSetterDoNotUse(nullptr);
    value().setUndefined();
  }

  void initFields(JS::Handle<JS::Value> v, unsigned attrs, JSObject* getter,
                  JSObject* setter) {
    value().set(v);
    setAttributes(attrs);
    desc().setGetterDoNotUse(getter);
    desc().setSetterDoNotUse(setter);
  }

  void assign(JS::PropertyDescriptor& other) {
    setAttributes(other.attributes());
    desc().setGetterDoNotUse(*other.getterDoNotUse());
    desc().setSetterDoNotUse(*other.setterDoNotUse());
    value().set(other.value());
  }

  void setDataDescriptor(JS::Handle<JS::Value> v, unsigned attrs) {
    MOZ_ASSERT((attrs & ~(JSPROP_ENUMERATE | JSPROP_PERMANENT |
                          JSPROP_READONLY | JSPROP_IGNORE_ENUMERATE |
                          JSPROP_IGNORE_PERMANENT | JSPROP_IGNORE_READONLY)) ==
               0);
    setAttributes(attrs);
    desc().setGetterDoNotUse(nullptr);
    desc().setSetterDoNotUse(nullptr);
    value().set(v);
  }

  JS::MutableHandle<JS::Value> value() { return desc().value(); }
  void setValue(JS::Handle<JS::Value> v) { desc().setValue(v); }

  void setConfigurable(bool configurable) {
    desc().setConfigurable(configurable);
  }
  void setEnumerable(bool enumerable) { desc().setEnumerable(enumerable); }
  void setWritable(bool writable) { desc().setWritable(writable); }
  void setAttributes(unsigned attrs) { desc().setAttributesDoNotUse(attrs); }

  void setGetterObject(JSObject* obj) { desc().setGetterObject(obj); }
  void setSetterObject(JSObject* obj) { desc().setSetterObject(obj); }

  JS::MutableHandle<JSObject*> getterObject() {
    MOZ_ASSERT(desc().hasGetterObject());
    return JS::MutableHandle<JSObject*>::fromMarkedLocation(
        desc().getterDoNotUse());
  }
  JS::MutableHandle<JSObject*> setterObject() {
    MOZ_ASSERT(desc().hasSetterObject());
    return JS::MutableHandle<JSObject*>::fromMarkedLocation(
        desc().setterDoNotUse());
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
 * If desc.isNothing(), then vp is set to undefined.
 */
extern JS_PUBLIC_API bool FromPropertyDescriptor(
    JSContext* cx, Handle<mozilla::Maybe<PropertyDescriptor>> desc,
    MutableHandle<Value> vp);

}  // namespace JS

#endif /* js_PropertyDescriptor_h */
