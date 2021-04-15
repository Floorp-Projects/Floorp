/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PropertyResult_h
#define vm_PropertyResult_h

#include "mozilla/Assertions.h"

#include "js/HeapAPI.h"
#include "js/RootingAPI.h"

#include "vm/Shape.h"

namespace js {

class PropertyResult {
  enum class Kind : uint8_t {
    NotFound,
    NativeProperty,
    NonNativeProperty,
    DenseElement,
    TypedArrayElement,
  };
  union {
    // Set if kind is NativeProperty.
    ShapeProperty shapeProp_;
    // Set if kind is DenseElement.
    uint32_t denseIndex_;
    // Set if kind is TypedArrayElement.
    size_t typedArrayIndex_;
  };
  Kind kind_ = Kind::NotFound;
  bool ignoreProtoChain_ = false;

 public:
  // Note: because ShapeProperty does not have a default constructor, we can't
  // use |= default| here.
  PropertyResult() {}

  // When a property is not found, we may additionally indicate that the
  // prototype chain should be ignored. This occurs for:
  //  - An out-of-range numeric property on a TypedArrayObject.
  //  - A resolve hook recursively calling itself as it sets the property.
  bool isNotFound() const { return kind_ == Kind::NotFound; }
  bool shouldIgnoreProtoChain() const {
    MOZ_ASSERT(isNotFound());
    return ignoreProtoChain_;
  }

  bool isFound() const { return kind_ != Kind::NotFound; }
  bool isNonNativeProperty() const { return kind_ == Kind::NonNativeProperty; }
  bool isDenseElement() const { return kind_ == Kind::DenseElement; }
  bool isTypedArrayElement() const { return kind_ == Kind::TypedArrayElement; }
  bool isNativeProperty() const { return kind_ == Kind::NativeProperty; }

  ShapeProperty shapeProperty() const {
    MOZ_ASSERT(isNativeProperty());
    return shapeProp_;
  }

  uint32_t denseElementIndex() const {
    MOZ_ASSERT(isDenseElement());
    return denseIndex_;
  }

  size_t typedArrayElementIndex() const {
    MOZ_ASSERT(isTypedArrayElement());
    return typedArrayIndex_;
  }

  void setNotFound() { kind_ = Kind::NotFound; }

  void setNativeProperty(ShapeProperty prop) {
    kind_ = Kind::NativeProperty;
    shapeProp_ = prop;
  }

  void setTypedObjectProperty() { kind_ = Kind::NonNativeProperty; }
  void setProxyProperty() { kind_ = Kind::NonNativeProperty; }

  void setDenseElement(uint32_t index) {
    kind_ = Kind::DenseElement;
    denseIndex_ = index;
  }

  void setTypedArrayElement(size_t index) {
    kind_ = Kind::TypedArrayElement;
    typedArrayIndex_ = index;
  }

  void setTypedArrayOutOfRange() {
    kind_ = Kind::NotFound;
    ignoreProtoChain_ = true;
  }
  void setRecursiveResolve() {
    kind_ = Kind::NotFound;
    ignoreProtoChain_ = true;
  }

  void trace(JSTracer* trc);
};

template <class Wrapper>
class WrappedPtrOperations<PropertyResult, Wrapper> {
  const PropertyResult& value() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool isNotFound() const { return value().isNotFound(); }
  bool isFound() const { return value().isFound(); }
  ShapeProperty shapeProperty() const { return value().shapeProperty(); }
  uint32_t denseElementIndex() const { return value().denseElementIndex(); }
  size_t typedArrayElementIndex() const {
    return value().typedArrayElementIndex();
  }
  bool isNativeProperty() const { return value().isNativeProperty(); }
  bool isNonNativeProperty() const { return value().isNonNativeProperty(); }
  bool isDenseElement() const { return value().isDenseElement(); }
  bool isTypedArrayElement() const { return value().isTypedArrayElement(); }

  bool shouldIgnoreProtoChain() const {
    return value().shouldIgnoreProtoChain();
  }
};

template <class Wrapper>
class MutableWrappedPtrOperations<PropertyResult, Wrapper>
    : public WrappedPtrOperations<PropertyResult, Wrapper> {
  PropertyResult& value() { return static_cast<Wrapper*>(this)->get(); }

 public:
  void setNotFound() { value().setNotFound(); }
  void setNativeProperty(ShapeProperty prop) {
    value().setNativeProperty(prop);
  }
  void setTypedObjectProperty() { value().setTypedObjectProperty(); }
  void setProxyProperty() { value().setProxyProperty(); }
  void setDenseElement(uint32_t index) { value().setDenseElement(index); }
  void setTypedArrayElement(size_t index) {
    value().setTypedArrayElement(index);
  }
  void setTypedArrayOutOfRange() { value().setTypedArrayOutOfRange(); }
  void setRecursiveResolve() { value().setRecursiveResolve(); }
};

}  // namespace js

#endif /* vm_PropertyResult_h */
