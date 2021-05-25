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

namespace js {

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

}  // namespace js

/* (higher flags are unused; add to JSPROP_FLAGS_MASK if ever defined) */

static constexpr unsigned JSPROP_FLAGS_MASK =
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT |
    JSPROP_CUSTOM_DATA_PROP | JSPROP_GETTER | JSPROP_SETTER | JSPROP_RESOLVING |
    js::JSPROP_IGNORE_ENUMERATE | js::JSPROP_IGNORE_READONLY |
    js::JSPROP_IGNORE_PERMANENT | js::JSPROP_IGNORE_VALUE;

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

class PropertyAttributes : public mozilla::EnumSet<PropertyAttribute> {
  // Re-use all EnumSet constructors.
  using mozilla::EnumSet<PropertyAttribute>::EnumSet;

 public:
  bool configurable() const {
    return contains(PropertyAttribute::Configurable);
  }
  bool enumerable() const { return contains(PropertyAttribute::Enumerable); }
  bool writable() const { return contains(PropertyAttribute::Writable); }
};

/**
 * A structure that represents a property on an object, or the absence of a
 * property.  Use {,Mutable}Handle<PropertyDescriptor> to interact with
 * instances of this structure rather than interacting directly with member
 * fields.
 */
class JS_PUBLIC_API PropertyDescriptor {
 private:
  bool hasConfigurable_ : 1;
  bool configurable_ : 1;

  bool hasEnumerable_ : 1;
  bool enumerable_ : 1;

  bool hasWritable_ : 1;
  bool writable_ : 1;

  bool hasValue_ : 1;
  bool hasGetter_ : 1;
  bool hasSetter_ : 1;

  bool resolving_ : 1;

  JSObject* getter_;
  JSObject* setter_;
  Value value_;

 public:
  PropertyDescriptor()
      : hasConfigurable_(false),
        configurable_(false),
        hasEnumerable_(false),
        enumerable_(false),
        hasWritable_(false),
        writable_(false),
        hasValue_(false),
        hasGetter_(false),
        hasSetter_(false),
        resolving_(false),
        getter_(nullptr),
        setter_(nullptr),
        value_(UndefinedValue()) {}

  void trace(JSTracer* trc);

  // Construct a new complete DataDescriptor.
  static PropertyDescriptor Data(const Value& value,
                                 PropertyAttributes attributes = {}) {
    PropertyDescriptor desc;
    desc.setConfigurable(attributes.configurable());
    desc.setEnumerable(attributes.enumerable());
    desc.setWritable(attributes.writable());
    desc.setValue(value);
    desc.assertComplete();
    return desc;
  }

  // This constructor is only provided for legacy code!
  static PropertyDescriptor Data(const Value& value, unsigned attrs) {
    MOZ_ASSERT((attrs & ~(JSPROP_PERMANENT | JSPROP_ENUMERATE |
                          JSPROP_READONLY | JSPROP_RESOLVING)) == 0);

    PropertyDescriptor desc;
    desc.setConfigurable(!(attrs & JSPROP_PERMANENT));
    desc.setEnumerable(attrs & JSPROP_ENUMERATE);
    desc.setWritable(!(attrs & JSPROP_READONLY));
    desc.setValue(value);
    desc.setResolving(attrs & JSPROP_RESOLVING);
    desc.assertComplete();
    return desc;
  }

  // Construct a new complete AccessorDescriptor.
  // Note: This means JSPROP_GETTER and JSPROP_SETTER are always set.
  static PropertyDescriptor Accessor(JSObject* getter, JSObject* setter,
                                     PropertyAttributes attributes = {}) {
    MOZ_ASSERT(!attributes.writable());

    PropertyDescriptor desc;
    desc.setConfigurable(attributes.configurable());
    desc.setEnumerable(attributes.enumerable());
    desc.setGetterObject(getter);
    desc.setSetterObject(setter);
    desc.assertComplete();
    return desc;
  }

  // This constructor is only provided for legacy code!
  static PropertyDescriptor Accessor(JSObject* getter, JSObject* setter,
                                     unsigned attrs) {
    MOZ_ASSERT((attrs & ~(JSPROP_PERMANENT | JSPROP_ENUMERATE |
                          JSPROP_RESOLVING)) == 0);

    PropertyDescriptor desc;
    desc.setConfigurable(!(attrs & JSPROP_PERMANENT));
    desc.setEnumerable(attrs & JSPROP_ENUMERATE);
    desc.setGetterObject(getter);
    desc.setSetterObject(setter);
    desc.setResolving(attrs & JSPROP_RESOLVING);
    desc.assertComplete();
    return desc;
  }

  static PropertyDescriptor Accessor(mozilla::Maybe<JSObject*> getter,
                                     mozilla::Maybe<JSObject*> setter,
                                     unsigned attrs) {
    MOZ_ASSERT((attrs & ~(JSPROP_PERMANENT | JSPROP_ENUMERATE |
                          JSPROP_RESOLVING)) == 0);

    PropertyDescriptor desc;
    desc.setConfigurable(!(attrs & JSPROP_PERMANENT));
    desc.setEnumerable(attrs & JSPROP_ENUMERATE);
    if (getter) {
      desc.setGetterObject(*getter);
    }
    if (setter) {
      desc.setSetterObject(*setter);
    }
    desc.setResolving(attrs & JSPROP_RESOLVING);
    desc.assertValid();
    return desc;
  }

  // Construct a new incomplete empty PropertyDescriptor.
  // Using the spec syntax this would be { }. Specific fields like [[Value]]
  // can be added with e.g., setValue.
  static PropertyDescriptor Empty() {
    PropertyDescriptor desc;
    desc.assertValid();
    MOZ_ASSERT(!desc.hasConfigurable() && !desc.hasEnumerable() &&
               !desc.hasWritable() && !desc.hasValue() &&
               !desc.hasGetterObject() && !desc.hasSetterObject());
    return desc;
  }

 public:
  bool isAccessorDescriptor() const {
    MOZ_ASSERT_IF(hasGetter_ || hasSetter_, !isDataDescriptor());
    return hasGetter_ || hasSetter_;
  }
  bool isGenericDescriptor() const {
    return !isAccessorDescriptor() && !isDataDescriptor();
  }
  bool isDataDescriptor() const {
    MOZ_ASSERT_IF(hasWritable_ || hasValue_, !isAccessorDescriptor());
    return hasWritable_ || hasValue_;
  }

  bool hasConfigurable() const { return hasConfigurable_; }
  bool configurable() const {
    MOZ_ASSERT(hasConfigurable());
    return configurable_;
  }
  void setConfigurable(bool configurable) {
    hasConfigurable_ = true;
    configurable_ = configurable;
  }

  bool hasEnumerable() const { return hasEnumerable_; }
  bool enumerable() const {
    MOZ_ASSERT(hasEnumerable());
    return enumerable_;
  }
  void setEnumerable(bool enumerable) {
    hasEnumerable_ = true;
    enumerable_ = enumerable;
  }

  bool hasValue() const { return hasValue_; }
  Value value() const {
    // TODO: Assert hasValue()
    return value_;
  }
  void setValue(const Value& v) {
    MOZ_ASSERT(!isAccessorDescriptor());
    hasValue_ = true;
    value_ = v;
  }

  bool hasWritable() const { return hasWritable_; }
  bool writable() const {
    MOZ_ASSERT(hasWritable());
    return writable_;
  }
  void setWritable(bool writable) {
    MOZ_ASSERT(!isAccessorDescriptor());
    hasWritable_ = true;
    writable_ = writable;
  }

  bool hasGetterObject() const { return hasGetter_; }
  JSObject* getterObject() const {
    MOZ_ASSERT(hasGetterObject());
    return getter_;
  }
  void setGetterObject(JSObject* obj) {
    MOZ_ASSERT(!isDataDescriptor());
    hasGetter_ = true;
    getter_ = obj;
  }

  bool hasSetterObject() const { return hasSetter_; }
  JSObject* setterObject() const {
    MOZ_ASSERT(hasSetterObject());
    return setter_;
  }
  void setSetterObject(JSObject* obj) {
    MOZ_ASSERT(!isDataDescriptor());
    hasSetter_ = true;
    setter_ = obj;
  }

  // Non-standard flag, which is set when defining properties in a resolve hook.
  bool resolving() const { return resolving_; }
  void setResolving(bool resolving) { resolving_ = resolving; }

  Value* valueDoNotUse() { return &value_; }
  Value const* valueDoNotUse() const { return &value_; }
  JSObject** getterDoNotUse() { return &getter_; }
  JSObject* const* getterDoNotUse() const { return &getter_; }
  void setGetterDoNotUse(JSObject* obj) { getter_ = obj; }
  JSObject** setterDoNotUse() { return &setter_; }
  JSObject* const* setterDoNotUse() const { return &setter_; }
  void setSetterDoNotUse(JSObject* obj) { setter_ = obj; }

  void assertValid() const {
#ifdef DEBUG
    if (isAccessorDescriptor()) {
      MOZ_ASSERT(!hasWritable_);
      MOZ_ASSERT(!hasValue_);
    } else {
      MOZ_ASSERT(isGenericDescriptor() || isDataDescriptor());
      MOZ_ASSERT(!hasGetter_);
      MOZ_ASSERT(!hasSetter_);
    }

    MOZ_ASSERT_IF(!hasConfigurable_, !configurable_);
    MOZ_ASSERT_IF(!hasEnumerable_, !enumerable_);
    MOZ_ASSERT_IF(!hasWritable_, !writable_);
    MOZ_ASSERT_IF(!hasValue_, value_.isUndefined());
    MOZ_ASSERT_IF(!hasGetter_, !getter_);
    MOZ_ASSERT_IF(!hasSetter_, !setter_);

    MOZ_ASSERT_IF(resolving_, !isGenericDescriptor());
#endif
  }

  void assertComplete() const {
#ifdef DEBUG
    assertValid();
    MOZ_ASSERT(hasConfigurable());
    MOZ_ASSERT(hasEnumerable());
    MOZ_ASSERT(!isGenericDescriptor());
    MOZ_ASSERT_IF(isDataDescriptor(), hasValue() && hasWritable());
    MOZ_ASSERT_IF(isAccessorDescriptor(),
                  hasGetterObject() && hasSetterObject());
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
  JS::Handle<JS::Value> value() const {
    return JS::Handle<JS::Value>::fromMarkedLocation(desc().valueDoNotUse());
  }

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

  bool resolving() const { return desc().resolving(); }

  void assertValid() const { desc().assertValid(); }
  void assertComplete() const { desc().assertComplete(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<JS::PropertyDescriptor, Wrapper>
    : public js::WrappedPtrOperations<JS::PropertyDescriptor, Wrapper> {
  JS::PropertyDescriptor& desc() { return static_cast<Wrapper*>(this)->get(); }

 public:
  JS::MutableHandle<JS::Value> value() {
    return JS::MutableHandle<JS::Value>::fromMarkedLocation(
        desc().valueDoNotUse());
  }
  void setValue(JS::Handle<JS::Value> v) { desc().setValue(v); }

  void setConfigurable(bool configurable) {
    desc().setConfigurable(configurable);
  }
  void setEnumerable(bool enumerable) { desc().setEnumerable(enumerable); }
  void setWritable(bool writable) { desc().setWritable(writable); }

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

  void setResolving(bool resolving) { desc().setResolving(resolving); }
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
