/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ObjectEmitter_h
#define frontend_ObjectEmitter_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE, MOZ_STACK_CLASS, MOZ_ALWAYS_INLINE, MOZ_RAII
#include "mozilla/Maybe.h"  // Maybe

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "frontend/BytecodeOffset.h"  // BytecodeOffset
#include "frontend/EmitterScope.h"    // EmitterScope
#include "frontend/NameOpEmitter.h"   // NameOpEmitter
#include "frontend/TDZCheckCache.h"   // TDZCheckCache
#include "js/RootingAPI.h"            // JS::Handle, JS::Rooted
#include "vm/BytecodeUtil.h"          // JSOp
#include "vm/JSAtom.h"                // JSAtom
#include "vm/NativeObject.h"          // PlainObject
#include "vm/Scope.h"                 // LexicalScope

namespace js {

namespace frontend {

struct BytecodeEmitter;
class SharedContext;

// Class for emitting bytecode for object and class properties.
// See ObjectEmitter and ClassEmitter for usage.
class MOZ_STACK_CLASS PropertyEmitter {
 public:
  enum class Kind {
    // Prototype property.
    Prototype,

    // Class static property.
    Static
  };

 protected:
  BytecodeEmitter* bce_;

  // True if the object is class.
  // Set by ClassEmitter.
  bool isClass_ = false;

  // True if the property is class static method.
  bool isStatic_ = false;

  // True if the property has computed or index key.
  bool isIndexOrComputed_ = false;

  // An object which keeps the shape of this object literal.
  // This fields is reset to nullptr whenever the object literal turns out to
  // have at least one numeric, computed, spread or __proto__ property, or
  // the object becomes dictionary mode.
  // This field is used only in ObjectEmitter.
  JS::Rooted<PlainObject*> obj_;

#ifdef DEBUG
  // The state of this emitter.
  //
  // +-------+
  // | Start |-+
  // +-------+ |
  //           |
  // +---------+
  // |
  // |  +------------------------------------------------------------+
  // |  |                                                            |
  // |  | [normal property/method/accessor]                          |
  // |  v   prepareForPropValue  +-----------+              +------+ |
  // +->+----------------------->| PropValue |-+         +->| Init |-+
  //    |                        +-----------+ |         |  +------+
  //    |                                      |         |
  //    |  +----------------------------------+          +-----------+
  //    |  |                                                         |
  //    |  +-+---------------------------------------+               |
  //    |    |                                       |               |
  //    |    | [method with super]                   |               |
  //    |    |   emitInitHomeObject +-------------+  v               |
  //    |    +--------------------->| InitHomeObj |->+               |
  //    |                           +-------------+  |               |
  //    |                                            |               |
  //    |    +-------------------------------------- +               |
  //    |    |                                                       |
  //    |    | emitInitProp                                          |
  //    |    | emitInitGetter                                        |
  //    |    | emitInitSetter                                        |
  //    |    +------------------------------------------------------>+
  //    |                                                            ^
  //    | [index property/method/accessor]                           |
  //    |   prepareForIndexPropKey  +----------+                     |
  //    +-------------------------->| IndexKey |-+                   |
  //    |                           +----------+ |                   |
  //    |                                        |                   |
  //    |  +-------------------------------------+                   |
  //    |  |                                                         |
  //    |  | prepareForIndexPropValue +------------+                 |
  //    |  +------------------------->| IndexValue |-+               |
  //    |                             +------------+ |               |
  //    |                                            |               |
  //    |    +---------------------------------------+               |
  //    |    |                                                       |
  //    |    +-+--------------------------------------------------+  |
  //    |      |                                                  |  |
  //    |      | [method with super]                              |  |
  //    |      |   emitInitHomeObject +---------------------+     v  |
  //    |      +--------------------->| InitHomeObjForIndex |---->+  |
  //    |                             +---------------------+     |  |
  //    |                                                         |  |
  //    |      +--------------------------------------------------+  |
  //    |      |                                                     |
  //    |      | emitInitIndexProp                                   |
  //    |      | emitInitIndexGetter                                 |
  //    |      | emitInitIndexSetter                                 |
  //    |      +---------------------------------------------------->+
  //    |                                                            |
  //    | [computed property/method/accessor]                        |
  //    |   prepareForComputedPropKey  +-------------+               |
  //    +----------------------------->| ComputedKey |-+             |
  //    |                              +-------------+ |             |
  //    |                                              |             |
  //    |  +-------------------------------------------+             |
  //    |  |                                                         |
  //    |  | prepareForComputedPropValue +---------------+           |
  //    |  +---------------------------->| ComputedValue |-+         |
  //    |                                +---------------+ |         |
  //    |                                                  |         |
  //    |    +---------------------------------------------+         |
  //    |    |                                                       |
  //    |    +-+--------------------------------------------------+  |
  //    |      |                                                  |  |
  //    |      | [method with super]                              |  |
  //    |      |   emitInitHomeObject +------------------------+  v  |
  //    |      +--------------------->| InitHomeObjForComputed |->+  |
  //    |                             +------------------------+  |  |
  //    |                                                         |  |
  //    |      +--------------------------------------------------+  |
  //    |      |                                                     |
  //    |      | emitInitComputedProp                                |
  //    |      | emitInitComputedGetter                              |
  //    |      | emitInitComputedSetter                              |
  //    |      +---------------------------------------------------->+
  //    |                                                            ^
  //    |                                                            |
  //    | [__proto__]                                                |
  //    |   prepareForProtoValue  +------------+ emitMutateProto     |
  //    +------------------------>| ProtoValue |-------------------->+
  //    |                         +------------+                     ^
  //    |                                                            |
  //    | [...prop]                                                  |
  //    |   prepareForSpreadOperand +---------------+ emitSpread     |
  //    +-------------------------->| SpreadOperand |----------------+
  //                                +---------------+
  enum class PropertyState {
    // The initial state.
    Start,

    // After calling prepareForPropValue.
    PropValue,

    // After calling emitInitHomeObject, from PropValue.
    InitHomeObj,

    // After calling prepareForIndexPropKey.
    IndexKey,

    // prepareForIndexPropValue.
    IndexValue,

    // After calling emitInitHomeObject, from IndexValue.
    InitHomeObjForIndex,

    // After calling prepareForComputedPropKey.
    ComputedKey,

    // prepareForComputedPropValue.
    ComputedValue,

    // After calling emitInitHomeObject, from ComputedValue.
    InitHomeObjForComputed,

    // After calling prepareForProtoValue.
    ProtoValue,

    // After calling prepareForSpreadOperand.
    SpreadOperand,

    // After calling one of emitInitProp, emitInitGetter, emitInitSetter,
    // emitInitIndexOrComputedProp, emitInitIndexOrComputedGetter,
    // emitInitIndexOrComputedSetter, emitMutateProto, or emitSpread.
    Init,
  };
  PropertyState propertyState_ = PropertyState::Start;
#endif

 public:
  explicit PropertyEmitter(BytecodeEmitter* bce);

  // Parameters are the offset in the source code for each character below:
  //
  // { __proto__: protoValue }
  //   ^
  //   |
  //   keyPos
  MOZ_MUST_USE bool prepareForProtoValue(
      const mozilla::Maybe<uint32_t>& keyPos);
  MOZ_MUST_USE bool emitMutateProto();

  // { ...obj }
  //   ^
  //   |
  //   spreadPos
  MOZ_MUST_USE bool prepareForSpreadOperand(
      const mozilla::Maybe<uint32_t>& spreadPos);
  MOZ_MUST_USE bool emitSpread();

  // { key: value }
  //   ^
  //   |
  //   keyPos
  MOZ_MUST_USE bool prepareForPropValue(const mozilla::Maybe<uint32_t>& keyPos,
                                        Kind kind = Kind::Prototype);

  // { 1: value }
  //   ^
  //   |
  //   keyPos
  MOZ_MUST_USE bool prepareForIndexPropKey(
      const mozilla::Maybe<uint32_t>& keyPos, Kind kind = Kind::Prototype);
  MOZ_MUST_USE bool prepareForIndexPropValue();

  // { [ key ]: value }
  //   ^
  //   |
  //   keyPos
  MOZ_MUST_USE bool prepareForComputedPropKey(
      const mozilla::Maybe<uint32_t>& keyPos, Kind kind = Kind::Prototype);
  MOZ_MUST_USE bool prepareForComputedPropValue();

  MOZ_MUST_USE bool emitInitHomeObject();

  // @param key
  //        Property key
  MOZ_MUST_USE bool emitInitProp(JS::Handle<JSAtom*> key);
  MOZ_MUST_USE bool emitInitGetter(JS::Handle<JSAtom*> key);
  MOZ_MUST_USE bool emitInitSetter(JS::Handle<JSAtom*> key);

  MOZ_MUST_USE bool emitInitIndexProp();
  MOZ_MUST_USE bool emitInitIndexGetter();
  MOZ_MUST_USE bool emitInitIndexSetter();

  MOZ_MUST_USE bool emitInitComputedProp();
  MOZ_MUST_USE bool emitInitComputedGetter();
  MOZ_MUST_USE bool emitInitComputedSetter();

 private:
  MOZ_MUST_USE MOZ_ALWAYS_INLINE bool prepareForProp(
      const mozilla::Maybe<uint32_t>& keyPos, bool isStatic, bool isComputed);

  // @param op
  //        Opcode for initializing property
  // @param key
  //        Atom of the property if the property key is not computed
  MOZ_MUST_USE bool emitInit(JSOp op, JS::Handle<JSAtom*> key);
  MOZ_MUST_USE bool emitInitIndexOrComputed(JSOp op);

  MOZ_MUST_USE bool emitPopClassConstructor();
};

// Class for emitting bytecode for object literal.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `{}`
//     ObjectEmitter oe(this);
//     oe.emitObject(0);
//     oe.emitEnd();
//
//   `{ prop: 10 }`
//     ObjectEmitter oe(this);
//     oe.emitObject(1);
//
//     oe.prepareForPropValue(Some(offset_of_prop));
//     emit(10);
//     oe.emitInitProp(atom_of_prop);
//
//     oe.emitEnd();
//
//   `{ prop: function() {} }`, when property value is anonymous function
//     ObjectEmitter oe(this);
//     oe.emitObject(1);
//
//     oe.prepareForPropValue(Some(offset_of_prop));
//     emit(function);
//     oe.emitInitProp(atom_of_prop);
//
//     oe.emitEnd();
//
//   `{ get prop() { ... }, set prop(v) { ... } }`
//     ObjectEmitter oe(this);
//     oe.emitObject(2);
//
//     oe.prepareForPropValue(Some(offset_of_prop));
//     emit(function_for_getter);
//     oe.emitInitGetter(atom_of_prop);
//
//     oe.prepareForPropValue(Some(offset_of_prop));
//     emit(function_for_setter);
//     oe.emitInitSetter(atom_of_prop);
//
//     oe.emitEnd();
//
//   `{ 1: 10, get 2() { ... }, set 3(v) { ... } }`
//     ObjectEmitter oe(this);
//     oe.emitObject(3);
//
//     oe.prepareForIndexPropKey(Some(offset_of_prop));
//     emit(1);
//     oe.prepareForIndexPropValue();
//     emit(10);
//     oe.emitInitIndexedProp();
//
//     oe.prepareForIndexPropKey(Some(offset_of_opening_bracket));
//     emit(2);
//     oe.prepareForIndexPropValue();
//     emit(function_for_getter);
//     oe.emitInitIndexGetter();
//
//     oe.prepareForIndexPropKey(Some(offset_of_opening_bracket));
//     emit(3);
//     oe.prepareForIndexPropValue();
//     emit(function_for_setter);
//     oe.emitInitIndexSetter();
//
//     oe.emitEnd();
//
//   `{ [prop1]: 10, get [prop2]() { ... }, set [prop3](v) { ... } }`
//     ObjectEmitter oe(this);
//     oe.emitObject(3);
//
//     oe.prepareForComputedPropKey(Some(offset_of_opening_bracket));
//     emit(prop1);
//     oe.prepareForComputedPropValue();
//     emit(10);
//     oe.emitInitComputedProp();
//
//     oe.prepareForComputedPropKey(Some(offset_of_opening_bracket));
//     emit(prop2);
//     oe.prepareForComputedPropValue();
//     emit(function_for_getter);
//     oe.emitInitComputedGetter();
//
//     oe.prepareForComputedPropKey(Some(offset_of_opening_bracket));
//     emit(prop3);
//     oe.prepareForComputedPropValue();
//     emit(function_for_setter);
//     oe.emitInitComputedSetter();
//
//     oe.emitEnd();
//
//   `{ __proto__: obj }`
//     ObjectEmitter oe(this);
//     oe.emitObject(1);
//     oe.prepareForProtoValue(Some(offset_of___proto__));
//     emit(obj);
//     oe.emitMutateProto();
//     oe.emitEnd();
//
//   `{ ...obj }`
//     ObjectEmitter oe(this);
//     oe.emitObject(1);
//     oe.prepareForSpreadOperand(Some(offset_of_triple_dots));
//     emit(obj);
//     oe.emitSpread();
//     oe.emitEnd();
//
class MOZ_STACK_CLASS ObjectEmitter : public PropertyEmitter {
 private:
  // The offset of JSOP_NEWINIT, which is replced by JSOP_NEWOBJECT later
  // when the object is known to have a fixed shape.
  BytecodeOffset top_;

#ifdef DEBUG
  // The state of this emitter.
  //
  // +-------+ emitObject +--------+
  // | Start |----------->| Object |-+
  // +-------+            +--------+ |
  //                                 |
  //   +-----------------------------+
  //   |
  //   | (do PropertyEmitter operation)  emitEnd  +-----+
  //   +-------------------------------+--------->| End |
  //                                              +-----+
  enum class ObjectState {
    // The initial state.
    Start,

    // After calling emitObject.
    Object,

    // After calling emitEnd.
    End,
  };
  ObjectState objectState_ = ObjectState::Start;
#endif

 public:
  explicit ObjectEmitter(BytecodeEmitter* bce);

  MOZ_MUST_USE bool emitObject(size_t propertyCount);
  MOZ_MUST_USE bool emitEnd();
};

// Save and restore the strictness.
// Used by class declaration/expression to temporarily enable strict mode.
class MOZ_RAII AutoSaveLocalStrictMode {
  SharedContext* sc_;
  bool savedStrictness_;

 public:
  explicit AutoSaveLocalStrictMode(SharedContext* sc);
  ~AutoSaveLocalStrictMode();

  // Force restore the strictness now.
  void restore();
};

// Class for emitting bytecode for JS class.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `class {}`
//     ClassEmitter ce(this);
//     ce.emitScope(scopeBindings);
//     ce.emitClass(nullptr, nullptr, false);
//
//     ce.emitInitDefaultConstructor(Some(offset_of_class),
//                                   Some(offset_of_closing_bracket));
//
//     ce.emitEnd(ClassEmitter::Kind::Expression);
//
//   `class { constructor() { ... } }`
//     ClassEmitter ce(this);
//     ce.emitScope(scopeBindings);
//     ce.emitClass(nullptr, nullptr, false);
//
//     emit(function_for_constructor);
//     ce.emitInitConstructor(/* needsHomeObject = */ false);
//
//     ce.emitEnd(ClassEmitter::Kind::Expression);
//
//   `class X { constructor() { ... } }`
//     ClassEmitter ce(this);
//     ce.emitScope(scopeBindings);
//     ce.emitClass(atom_of_X, nullptr, false);
//
//     ce.emitInitDefaultConstructor(Some(offset_of_class),
//                                   Some(offset_of_closing_bracket));
//
//     ce.emitEnd(ClassEmitter::Kind::Expression);
//
//   `class X { constructor() { ... } }`
//     ClassEmitter ce(this);
//     ce.emitScope(scopeBindings);
//     ce.emitClass(atom_of_X, nullptr, false);
//
//     emit(function_for_constructor);
//     ce.emitInitConstructor(/* needsHomeObject = */ false);
//
//     ce.emitEnd(ClassEmitter::Kind::Expression);
//
//   `class X extends Y { constructor() { ... } }`
//     ClassEmitter ce(this);
//     ce.emitScope(scopeBindings);
//
//     emit(Y);
//     ce.emitDerivedClass(atom_of_X, nullptr, false);
//
//     emit(function_for_constructor);
//     ce.emitInitConstructor(/* needsHomeObject = */ false);
//
//     ce.emitEnd(ClassEmitter::Kind::Expression);
//
//   `class X extends Y { constructor() { ... super.f(); ... } }`
//     ClassEmitter ce(this);
//     ce.emitScope(scopeBindings);
//
//     emit(Y);
//     ce.emitDerivedClass(atom_of_X, nullptr, false);
//
//     emit(function_for_constructor);
//     // pass true if constructor contains super.prop access
//     ce.emitInitConstructor(/* needsHomeObject = */ true);
//
//     ce.emitEnd(ClassEmitter::Kind::Expression);
//
//   `class X extends Y { field0 = expr0; ... }`
//     ClassEmitter ce(this);
//     ce.emitScope(scopeBindings);
//     emit(Y);
//     ce.emitDerivedClass(atom_of_X, nullptr, false);
//
//     ce.prepareForFieldInitializers(fields.length());
//     for (auto field : fields) {
//       emit(field.initializer_method());
//       ce.emitStoreFieldInitializer();
//     }
//     ce.emitFieldInitializersEnd();
//
//     emit(function_for_constructor);
//     ce.emitInitConstructor(/* needsHomeObject = */ false);
//     ce.emitEnd(ClassEmitter::Kind::Expression);
//
//   `class X { field0 = super.method(); ... }`
//     // after emitClass/emitDerivedClass
//     ce.prepareForFieldInitializers(1);
//     for (auto field : fields) {
//       emit(field.initializer_method());
//       if (field.initializer_contains_super_or_eval()) {
//         ce.emitFieldInitializerHomeObject();
//       }
//       ce.emitStoreFieldInitializer();
//     }
//     ce.emitFieldInitializersEnd();
//
//   `m() {}` in class
//     // after emitInitConstructor/emitInitDefaultConstructor
//     ce.prepareForPropValue(Some(offset_of_m));
//     emit(function_for_m);
//     ce.emitInitProp(atom_of_m);
//
//   `m() { super.f(); }` in class
//     // after emitInitConstructor/emitInitDefaultConstructor
//     ce.prepareForPropValue(Some(offset_of_m));
//     emit(function_for_m);
//     ce.emitInitHomeObject();
//     ce.emitInitProp(atom_of_m);
//
//   `async m() { super.f(); }` in class
//     // after emitInitConstructor/emitInitDefaultConstructor
//     ce.prepareForPropValue(Some(offset_of_m));
//     emit(function_for_m);
//     ce.emitInitHomeObject();
//     ce.emitInitProp(atom_of_m);
//
//   `get p() { super.f(); }` in class
//     // after emitInitConstructor/emitInitDefaultConstructor
//     ce.prepareForPropValue(Some(offset_of_p));
//     emit(function_for_p);
//     ce.emitInitHomeObject();
//     ce.emitInitGetter(atom_of_m);
//
//   `static m() {}` in class
//     // after emitInitConstructor/emitInitDefaultConstructor
//     ce.prepareForPropValue(Some(offset_of_m),
//                            PropertyEmitter::Kind::Static);
//     emit(function_for_m);
//     ce.emitInitProp(atom_of_m);
//
//   `static get [p]() { super.f(); }` in class
//     // after emitInitConstructor/emitInitDefaultConstructor
//     ce.prepareForComputedPropValue(Some(offset_of_m),
//                                    PropertyEmitter::Kind::Static);
//     emit(p);
//     ce.prepareForComputedPropValue();
//     emit(function_for_m);
//     ce.emitInitHomeObject();
//     ce.emitInitComputedGetter();
//
class MOZ_STACK_CLASS ClassEmitter : public PropertyEmitter {
 public:
  enum class Kind {
    // Class expression.
    Expression,

    // Class declaration.
    Declaration,
  };

 private:
  // Pseudocode for class declarations:
  //
  //     class extends BaseExpression {
  //       constructor() { ... }
  //       ...
  //       }
  //
  //
  //   if defined <BaseExpression> {
  //     let heritage = BaseExpression;
  //
  //     if (heritage !== null) {
  //       funProto = heritage;
  //       objProto = heritage.prototype;
  //     } else {
  //       funProto = %FunctionPrototype%;
  //       objProto = null;
  //     }
  //   } else {
  //     objProto = %ObjectPrototype%;
  //   }
  //
  //   let homeObject = ObjectCreate(objProto);
  //
  //   if defined <constructor> {
  //     if defined <BaseExpression> {
  //       cons = DefineMethod(<constructor>, proto=homeObject,
  //       funProto=funProto);
  //     } else {
  //       cons = DefineMethod(<constructor>, proto=homeObject);
  //     }
  //   } else {
  //     if defined <BaseExpression> {
  //       cons = DefaultDerivedConstructor(proto=homeObject,
  //       funProto=funProto);
  //     } else {
  //       cons = DefaultConstructor(proto=homeObject);
  //     }
  //   }
  //
  //   cons.prototype = homeObject;
  //   homeObject.constructor = cons;
  //
  //   EmitPropertyList(...)

  bool isDerived_ = false;

  mozilla::Maybe<TDZCheckCache> tdzCache_;
  mozilla::Maybe<EmitterScope> innerScope_;
  AutoSaveLocalStrictMode strictMode_;

#ifdef DEBUG
  // The state of this emitter.
  //
  // clang-format off
  // +-------+
  // | Start |-+------------------------>+-+
  // +-------+ |                         ^ |
  //           | [has scope]             | |
  //           |   emitScope   +-------+ | |
  //           +-------------->| Scope |-+ |
  //                           +-------+   |
  //                                       |
  //   +-----------------------------------+
  //   |
  //   |   emitClass           +-------+
  //   +-+----------------->+->| Class |-+
  //     |                  ^  +-------+ |
  //     | emitDerivedClass |            |
  //     +------------------+            |
  //                                     |
  //     +-------------------------------+
  //     |
  //     | prepareForFieldInitializers
  //     +-----------------------------+
  //     |                             |
  //     |                             |    +-------------------+
  //     |      +--------------------->+--->| FieldInitializers |-+
  //     |      |                           +-------------------+ |
  //     |      |                                                 |
  //     |      |      (emit initializer method)                  |
  //     |      |   +<--------------------------------------------+
  //     |      |   |
  //     |      |   | emitFieldInitializerHomeObject  +--------------------------------+
  //     |      |   +-------------------------------->| FieldInitializerWithHomeObject |-+
  //     |      |   |                                 +--------------------------------+ |
  //     |      |   |                                                                    |
  //     |      |   +------------------------------------------------------------------->+
  //     |      |                                                                        |
  //     |      |     emitStoreFieldInitializer                                          |
  //     |  +<--+<-----------------------------------------------------------------------+
  //     |  |
  //     |  | emitFieldInitializersEnd  +----------------------+
  //     |  +-------------------------->| FieldInitializersEnd |-+
  //     |                              +----------------------+ |
  //     |                                                       |
  //     |<------------------------------------------------------+
  //     |
  //     |
  //     |   emitInitConstructor           +-----------------+
  //     +-+--------------------------->+->| InitConstructor |-+
  //       |                            ^  +-----------------+ |
  //       | emitInitDefaultConstructor |                      |
  //       +----------------------------+                      |
  //                                                           |
  //       +---------------------------------------------------+
  //       |
  //       | (do PropertyEmitter operation)  emitEnd  +-----+
  //       +-------------------------------+--------->| End |
  //                                                  +-----+
  // clang-format on
  enum class ClassState {
    // The initial state.
    Start,

    // After calling emitScope.
    Scope,

    // After calling emitClass or emitDerivedClass.
    Class,

    // After calling emitInitConstructor or emitInitDefaultConstructor.
    InitConstructor,

    // After calling prepareForFieldInitializers
    // and 0 or more calls to emitStoreFieldInitializer.
    FieldInitializers,

    // After calling emitFieldInitializerHomeObject
    FieldInitializerWithHomeObject,

    // After calling emitFieldInitializersEnd.
    FieldInitializersEnd,

    // After calling emitEnd.
    End,
  };
  ClassState classState_ = ClassState::Start;
  size_t numFields_ = 0;
#endif

  JS::Rooted<JSAtom*> name_;
  JS::Rooted<JSAtom*> nameForAnonymousClass_;
  bool hasNameOnStack_ = false;
  mozilla::Maybe<NameOpEmitter> initializersAssignment_;
  size_t fieldIndex_ = 0;

 public:
  explicit ClassEmitter(BytecodeEmitter* bce);

  bool emitScope(JS::Handle<LexicalScope::Data*> scopeBindings);

  // @param name
  //        Name of the class (nullptr if this is anonymous class)
  // @param nameForAnonymousClass
  //        Statically inferred name of the class (only for anonymous classes)
  // @param hasNameOnStack
  //        If true the name is on the stack (only for anonymous classes)
  MOZ_MUST_USE bool emitClass(JS::Handle<JSAtom*> name,
                              JS::Handle<JSAtom*> nameForAnonymousClass,
                              bool hasNameOnStack);
  MOZ_MUST_USE bool emitDerivedClass(JS::Handle<JSAtom*> name,
                                     JS::Handle<JSAtom*> nameForAnonymousClass,
                                     bool hasNameOnStack);

  // @param needsHomeObject
  //        True if the constructor contains `super.foo`
  MOZ_MUST_USE bool emitInitConstructor(bool needsHomeObject);

  // Parameters are the offset in the source code for each character below:
  //
  //   class X { foo() {} }
  //   ^                  ^
  //   |                  |
  //   |                  classEnd
  //   |
  //   classStart
  //
  MOZ_MUST_USE bool emitInitDefaultConstructor(
      const mozilla::Maybe<uint32_t>& classStart,
      const mozilla::Maybe<uint32_t>& classEnd);

  MOZ_MUST_USE bool prepareForFieldInitializers(size_t numFields);
  MOZ_MUST_USE bool emitFieldInitializerHomeObject();
  MOZ_MUST_USE bool emitStoreFieldInitializer();
  MOZ_MUST_USE bool emitFieldInitializersEnd();

  MOZ_MUST_USE bool emitEnd(Kind kind);

 private:
  MOZ_MUST_USE bool initProtoAndCtor();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_ObjectEmitter_h */
