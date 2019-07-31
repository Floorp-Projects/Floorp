/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TIOracle_h
#define jit_TIOracle_h

#include "jit/Ion.h"
#include "vm/TypeInference.h"

namespace js {
namespace jit {

// [SMDOC] TI Oracle
//
// The SpiderMonkey VM collects runtime type inference data as it proceeds that
// describe the possible set of types that may exist at different locations.
// During IonMonkey compilation, the compiler may decide to depend on this TI
// data. To ensure that this is safe, even if the TI data changes in the future,
// IonMonkey registers invalidation hooks on the data it depends on.
//
// The TIOracle interface makes sure that we attach the right constraints by
// providing a layer of abstraction between IonBuilder and TI. Instead of having
// direct access to TI and being responsible for attaching the right
// constraints, IonBuilder queries the TIOracle. The TIOracle is responsible for
// attaching the correct constraints so that the results of queries continue to
// be true for the lifetime of the compiled script.

class IonBuilder;
class MDefinition;

class TIOracle {
 public:
  class Object {
    JSObject* raw_;
    explicit Object(JSObject* raw) : raw_(raw) {}

    friend class TIOracle;

   public:
    JSObject* toRaw() { return raw_; }

    // You must be certain the object is guaranteed through TI and/or
    // precise guards.
    static Object unsafeFromRaw(JSObject* raw) { return Object(raw); }

    explicit operator bool() { return !!raw_; }
  };

  // Note: this is called TIOracle::Group even though it is currently
  // wrapping an ObjectKey. This is because, conceptually, ObjectKey
  // is nothing more than "ObjectGroup plus the ability to retrieve
  // the lone object in a singleton group".
  class Group {
    using ObjectKey = js::TypeSet::ObjectKey;

    ObjectKey* raw_;
    explicit Group(ObjectKey* raw) : raw_(raw) {}
    explicit Group(JSObject* raw);
    explicit Group(ObjectGroup* raw);

    friend class TIOracle;

   public:
    ObjectKey* toRaw() { return raw_; }

    template <typename T>
    static Group unsafeFromRaw(T raw) {
      return Group(raw);
    }

    explicit operator bool() { return !!raw_; }
  };

  class TypeSet {
    TemporaryTypeSet* raw_;
    explicit TypeSet(TemporaryTypeSet* raw) : raw_(raw) {}

    friend class TIOracle;

   public:
    TemporaryTypeSet* toRaw() { return raw_; }

    static TypeSet unsafeFromRaw(TemporaryTypeSet* raw) { return TypeSet(raw); }

    explicit operator bool() { return !!raw_; }
  };

  explicit TIOracle(IonBuilder* builder, CompilerConstraintList* constraints);

  IonBuilder* builder() { return builder_; }
  CompilerConstraintList* constraints() { return constraints_; }

  AbortReasonOr<Ok> ensureBallast();

  TypeSet resultTypeSet(MDefinition* def);

 private:
  Object wrapObject(JSObject* object);

  bool hasStableClass(Object obj);
  bool hasStableProto(Object obj);

  IonBuilder* builder_;
  CompilerConstraintList* constraints_;
};

}  // namespace jit
}  // namespace js

#endif /* jit_TIOracle_h */
