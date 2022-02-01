/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_TupleType_h
#define vm_TupleType_h

#include <cstdint>
#include <functional>
#include "vm/JSContext.h"
#include "vm/NativeObject.h"

namespace JS {

class TupleType final : public js::NativeObject {
 public:
  static const js::ClassSpec classSpec_;
  static const JSClass class_;
  static const JSClass protoClass_;

 public:
  static TupleType* create(JSContext* cx, uint32_t length,
                           const Value* elements);

  static TupleType* createUninitialized(JSContext* cx, uint32_t initialLength);
  bool initializeNextElement(JSContext* cx, HandleValue elt);
  void finishInitialization(JSContext* cx);

  bool getOwnProperty(HandleId id, MutableHandleValue vp) const;
  inline uint32_t length() const { return getElementsHeader()->length; }

  // Methods defined on Tuple.prototype
  [[nodiscard]] static bool lengthAccessor(JSContext* cx, unsigned argc,
                                           Value* vp);

  // Comparison functions
  static bool sameValueZero(JSContext* cx, TupleType* lhs, TupleType* rhs,
                            bool* equal);
  static bool sameValue(JSContext* cx, TupleType* lhs, TupleType* rhs,
                        bool* equal);

  using ElementHasher = std::function<js::HashNumber(const Value& child)>;
  js::HashNumber hash(const ElementHasher& hasher) const;

  bool ensureAtomized(JSContext* cx);
  bool isAtomized() const { return getElementsHeader()->tupleIsAtomized(); }

  // This can be used to compare atomized tuples.
  static bool sameValueZero(TupleType* lhs, TupleType* rhs);

 private:
  template <bool Comparator(JSContext*, HandleValue, HandleValue, bool*)>
  static bool sameValueWith(JSContext* cx, TupleType* lhs, TupleType* rhs,
                            bool* equal);
};

}  // namespace JS

namespace js {

extern JSString* TupleToSource(JSContext* cx, TupleType* tup);
}

#endif
