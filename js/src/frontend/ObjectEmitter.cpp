/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ObjectEmitter.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "frontend/BytecodeEmitter.h"  // BytecodeEmitter
#include "frontend/IfEmitter.h"        // IfEmitter
#include "frontend/SharedContext.h"    // SharedContext
#include "frontend/SourceNotes.h"      // SRC_*
#include "gc/AllocKind.h"              // AllocKind
#include "js/Id.h"                     // jsid
#include "js/Value.h"                  // UndefinedHandleValue
#include "vm/BytecodeUtil.h"           // IsHiddenInitOp
#include "vm/JSContext.h"              // JSContext
#include "vm/NativeObject.h"           // NativeDefineDataProperty
#include "vm/ObjectGroup.h"            // TenuredObject
#include "vm/Opcodes.h"                // JSOP_*
#include "vm/Runtime.h"                // JSAtomState (cx->names())

#include "gc/ObjectKind-inl.h"  // GetGCObjectKind
#include "vm/JSAtom-inl.h"      // AtomToId
#include "vm/JSObject-inl.h"    // NewBuiltinClassInstance

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

PropertyEmitter::PropertyEmitter(BytecodeEmitter* bce)
    : bce_(bce), obj_(bce->cx) {}

bool PropertyEmitter::prepareForProtoValue(const Maybe<uint32_t>& keyPos) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);

  //                [stack] CTOR? OBJ CTOR?

  if (keyPos) {
    if (!bce_->updateSourceCoordNotes(*keyPos)) {
      return false;
    }
  }

#ifdef DEBUG
  propertyState_ = PropertyState::ProtoValue;
#endif
  return true;
}

bool PropertyEmitter::emitMutateProto() {
  MOZ_ASSERT(propertyState_ == PropertyState::ProtoValue);

  //                [stack] OBJ PROTO

  if (!bce_->emit1(JSOP_MUTATEPROTO)) {
    //              [stack] OBJ
    return false;
  }

  obj_ = nullptr;
#ifdef DEBUG
  propertyState_ = PropertyState::Init;
#endif
  return true;
}

bool PropertyEmitter::prepareForSpreadOperand(
    const Maybe<uint32_t>& spreadPos) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);

  //                [stack] OBJ

  if (spreadPos) {
    if (!bce_->updateSourceCoordNotes(*spreadPos)) {
      return false;
    }
  }
  if (!bce_->emit1(JSOP_DUP)) {
    //              [stack] OBJ OBJ
    return false;
  }

#ifdef DEBUG
  propertyState_ = PropertyState::SpreadOperand;
#endif
  return true;
}

bool PropertyEmitter::emitSpread() {
  MOZ_ASSERT(propertyState_ == PropertyState::SpreadOperand);

  //                [stack] OBJ OBJ VAL

  if (!bce_->emitCopyDataProperties(BytecodeEmitter::CopyOption::Unfiltered)) {
    //              [stack] OBJ
    return false;
  }

  obj_ = nullptr;
#ifdef DEBUG
  propertyState_ = PropertyState::Init;
#endif
  return true;
}

MOZ_ALWAYS_INLINE bool PropertyEmitter::prepareForProp(
    const Maybe<uint32_t>& keyPos, bool isStatic, bool isIndexOrComputed) {
  isStatic_ = isStatic;
  isIndexOrComputed_ = isIndexOrComputed;

  //                [stack] CTOR? OBJ

  if (keyPos) {
    if (!bce_->updateSourceCoordNotes(*keyPos)) {
      return false;
    }
  }

  if (isStatic_) {
    if (!bce_->emit1(JSOP_DUP2)) {
      //            [stack] CTOR HOMEOBJ CTOR HOMEOBJ
      return false;
    }
    if (!bce_->emit1(JSOP_POP)) {
      //            [stack] CTOR HOMEOBJ CTOR
      return false;
    }
  }

  return true;
}

bool PropertyEmitter::prepareForPropValue(const Maybe<uint32_t>& keyPos,
                                          Kind kind /* = Kind::Prototype */) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);

  //                [stack] CTOR? OBJ

  if (!prepareForProp(keyPos,
                      /* isStatic_ = */ kind == Kind::Static,
                      /* isIndexOrComputed = */ false)) {
    //              [stack] CTOR? OBJ CTOR?
    return false;
  }

#ifdef DEBUG
  propertyState_ = PropertyState::PropValue;
#endif
  return true;
}

bool PropertyEmitter::prepareForIndexPropKey(
    const Maybe<uint32_t>& keyPos, Kind kind /* = Kind::Prototype */) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);

  //                [stack] CTOR? OBJ

  obj_ = nullptr;

  if (!prepareForProp(keyPos,
                      /* isStatic_ = */ kind == Kind::Static,
                      /* isIndexOrComputed = */ true)) {
    //              [stack] CTOR? OBJ CTOR?
    return false;
  }

#ifdef DEBUG
  propertyState_ = PropertyState::IndexKey;
#endif
  return true;
}

bool PropertyEmitter::prepareForIndexPropValue() {
  MOZ_ASSERT(propertyState_ == PropertyState::IndexKey);

  //                [stack] CTOR? OBJ CTOR? KEY

#ifdef DEBUG
  propertyState_ = PropertyState::IndexValue;
#endif
  return true;
}

bool PropertyEmitter::prepareForComputedPropKey(
    const Maybe<uint32_t>& keyPos, Kind kind /* = Kind::Prototype */) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);

  //                [stack] CTOR? OBJ

  obj_ = nullptr;

  if (!prepareForProp(keyPos,
                      /* isStatic_ = */ kind == Kind::Static,
                      /* isIndexOrComputed = */ true)) {
    //              [stack] CTOR? OBJ CTOR?
    return false;
  }

#ifdef DEBUG
  propertyState_ = PropertyState::ComputedKey;
#endif
  return true;
}

bool PropertyEmitter::prepareForComputedPropValue() {
  MOZ_ASSERT(propertyState_ == PropertyState::ComputedKey);

  //                [stack] CTOR? OBJ CTOR? KEY

  if (!bce_->emit1(JSOP_TOID)) {
    //              [stack] CTOR? OBJ CTOR? KEY
    return false;
  }

#ifdef DEBUG
  propertyState_ = PropertyState::ComputedValue;
#endif
  return true;
}

bool PropertyEmitter::emitInitHomeObject() {
  MOZ_ASSERT(propertyState_ == PropertyState::PropValue ||
             propertyState_ == PropertyState::IndexValue ||
             propertyState_ == PropertyState::ComputedValue);

  //                [stack] CTOR? HOMEOBJ CTOR? KEY? FUN

  // There are the following values on the stack conditionally, between
  // HOMEOBJ and FUN:
  //   * the 2nd CTOR if isStatic_
  //   * KEY if isIndexOrComputed_
  //
  // JSOP_INITHOMEOBJECT uses one of the following:
  //   * HOMEOBJ if !isStatic_
  //     (`super.foo` points the super prototype property)
  //   * the 2nd CTOR if isStatic_
  //     (`super.foo` points the super constructor property)
  if (!bce_->emitDupAt(1 + isIndexOrComputed_)) {
    //              [stack] # non-static method
    //              [stack] CTOR? HOMEOBJ CTOR KEY? FUN CTOR
    //              [stack] # static method
    //              [stack] CTOR? HOMEOBJ KEY? FUN HOMEOBJ
    return false;
  }
  if (!bce_->emit1(JSOP_INITHOMEOBJECT)) {
    //              [stack] CTOR? HOMEOBJ CTOR? KEY? FUN
    return false;
  }

#ifdef DEBUG
  if (propertyState_ == PropertyState::PropValue) {
    propertyState_ = PropertyState::InitHomeObj;
  } else if (propertyState_ == PropertyState::IndexValue) {
    propertyState_ = PropertyState::InitHomeObjForIndex;
  } else {
    propertyState_ = PropertyState::InitHomeObjForComputed;
  }
#endif
  return true;
}

bool PropertyEmitter::emitInitProp(JS::Handle<JSAtom*> key) {
  return emitInit(isClass_ ? JSOP_INITHIDDENPROP : JSOP_INITPROP, key);
}

bool PropertyEmitter::emitInitGetter(JS::Handle<JSAtom*> key) {
  obj_ = nullptr;
  return emitInit(isClass_ ? JSOP_INITHIDDENPROP_GETTER : JSOP_INITPROP_GETTER,
                  key);
}

bool PropertyEmitter::emitInitSetter(JS::Handle<JSAtom*> key) {
  obj_ = nullptr;
  return emitInit(isClass_ ? JSOP_INITHIDDENPROP_SETTER : JSOP_INITPROP_SETTER,
                  key);
}

bool PropertyEmitter::emitInitIndexProp() {
  return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM
                                          : JSOP_INITELEM);
}

bool PropertyEmitter::emitInitIndexGetter() {
  obj_ = nullptr;
  return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM_GETTER
                                          : JSOP_INITELEM_GETTER);
}

bool PropertyEmitter::emitInitIndexSetter() {
  obj_ = nullptr;
  return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM_SETTER
                                          : JSOP_INITELEM_SETTER);
}

bool PropertyEmitter::emitInitComputedProp() {
  return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM
                                          : JSOP_INITELEM);
}

bool PropertyEmitter::emitInitComputedGetter() {
  obj_ = nullptr;
  return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM_GETTER
                                          : JSOP_INITELEM_GETTER);
}

bool PropertyEmitter::emitInitComputedSetter() {
  obj_ = nullptr;
  return emitInitIndexOrComputed(isClass_ ? JSOP_INITHIDDENELEM_SETTER
                                          : JSOP_INITELEM_SETTER);
}

bool PropertyEmitter::emitInit(JSOp op, JS::Handle<JSAtom*> key) {
  MOZ_ASSERT(propertyState_ == PropertyState::PropValue ||
             propertyState_ == PropertyState::InitHomeObj);

  MOZ_ASSERT(op == JSOP_INITPROP || op == JSOP_INITHIDDENPROP ||
             op == JSOP_INITPROP_GETTER || op == JSOP_INITHIDDENPROP_GETTER ||
             op == JSOP_INITPROP_SETTER || op == JSOP_INITHIDDENPROP_SETTER);

  //                [stack] CTOR? OBJ CTOR? VAL

  uint32_t index;
  if (!bce_->makeAtomIndex(key, &index)) {
    return false;
  }

  if (obj_) {
    MOZ_ASSERT(!IsHiddenInitOp(op));
    MOZ_ASSERT(!obj_->inDictionaryMode());
    JS::Rooted<JS::PropertyKey> propKey(bce_->cx, AtomToId(key));
    if (!NativeDefineDataProperty(bce_->cx, obj_, propKey, UndefinedHandleValue,
                                  JSPROP_ENUMERATE)) {
      return false;
    }
    if (obj_->inDictionaryMode()) {
      obj_ = nullptr;
    }
  }

  if (!bce_->emitIndex32(op, index)) {
    //              [stack] CTOR? OBJ CTOR?
    return false;
  }

  if (!emitPopClassConstructor()) {
    return false;
  }

#ifdef DEBUG
  propertyState_ = PropertyState::Init;
#endif
  return true;
}

bool PropertyEmitter::emitInitIndexOrComputed(JSOp op) {
  MOZ_ASSERT(propertyState_ == PropertyState::IndexValue ||
             propertyState_ == PropertyState::InitHomeObjForIndex ||
             propertyState_ == PropertyState::ComputedValue ||
             propertyState_ == PropertyState::InitHomeObjForComputed);

  MOZ_ASSERT(op == JSOP_INITELEM || op == JSOP_INITHIDDENELEM ||
             op == JSOP_INITELEM_GETTER || op == JSOP_INITHIDDENELEM_GETTER ||
             op == JSOP_INITELEM_SETTER || op == JSOP_INITHIDDENELEM_SETTER);

  //                [stack] CTOR? OBJ CTOR? KEY VAL

  if (!bce_->emit1(op)) {
    //              [stack] CTOR? OBJ CTOR?
    return false;
  }

  if (!emitPopClassConstructor()) {
    return false;
  }

#ifdef DEBUG
  propertyState_ = PropertyState::Init;
#endif
  return true;
}

bool PropertyEmitter::emitPopClassConstructor() {
  if (isStatic_) {
    //              [stack] CTOR HOMEOBJ CTOR

    if (!bce_->emit1(JSOP_POP)) {
      //            [stack] CTOR HOMEOBJ
      return false;
    }
  }

  return true;
}

ObjectEmitter::ObjectEmitter(BytecodeEmitter* bce) : PropertyEmitter(bce) {}

bool ObjectEmitter::emitObject(size_t propertyCount) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(objectState_ == ObjectState::Start);

  //                [stack]

  // Emit code for {p:a, '%q':b, 2:c} that is equivalent to constructing
  // a new object and defining (in source order) each property on the object
  // (or mutating the object's [[Prototype]], in the case of __proto__).
  top_ = bce_->bytecodeSection().offset();
  if (!bce_->emitNewInit()) {
    //              [stack] OBJ
    return false;
  }

  // Try to construct the shape of the object as we go, so we can emit a
  // JSOP_NEWOBJECT with the final shape instead.
  // In the case of computed property names and indices, we cannot fix the
  // shape at bytecode compile time. When the shape cannot be determined,
  // |obj| is nulled out.

  // No need to do any guessing for the object kind, since we know the upper
  // bound of how many properties we plan to have.
  gc::AllocKind kind = gc::GetGCObjectKind(propertyCount);
  obj_ = NewBuiltinClassInstance<PlainObject>(bce_->cx, kind, TenuredObject);
  if (!obj_) {
    return false;
  }

#ifdef DEBUG
  objectState_ = ObjectState::Object;
#endif
  return true;
}

bool ObjectEmitter::emitEnd() {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);
  MOZ_ASSERT(objectState_ == ObjectState::Object);

  //                [stack] OBJ

  if (obj_) {
    // The object survived and has a predictable shape: update the original
    // bytecode.
    if (!bce_->replaceNewInitWithNewObject(obj_, top_)) {
      //            [stack] OBJ
      return false;
    }
  }

#ifdef DEBUG
  objectState_ = ObjectState::End;
#endif
  return true;
}

AutoSaveLocalStrictMode::AutoSaveLocalStrictMode(SharedContext* sc) : sc_(sc) {
  savedStrictness_ = sc_->setLocalStrictMode(true);
}

AutoSaveLocalStrictMode::~AutoSaveLocalStrictMode() {
  if (sc_) {
    restore();
  }
}

void AutoSaveLocalStrictMode::restore() {
  MOZ_ALWAYS_TRUE(sc_->setLocalStrictMode(savedStrictness_));
  sc_ = nullptr;
}

ClassEmitter::ClassEmitter(BytecodeEmitter* bce)
    : PropertyEmitter(bce),
      strictMode_(bce->sc),
      name_(bce->cx),
      nameForAnonymousClass_(bce->cx) {
  isClass_ = true;
}

bool ClassEmitter::emitScope(JS::Handle<LexicalScope::Data*> scopeBindings) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(classState_ == ClassState::Start);

  tdzCache_.emplace(bce_);

  innerScope_.emplace(bce_);
  if (!innerScope_->enterLexical(bce_, ScopeKind::Lexical, scopeBindings)) {
    return false;
  }

#ifdef DEBUG
  classState_ = ClassState::Scope;
#endif

  return true;
}

bool ClassEmitter::emitClass(JS::Handle<JSAtom*> name,
                             JS::Handle<JSAtom*> nameForAnonymousClass,
                             bool hasNameOnStack) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(classState_ == ClassState::Start ||
             classState_ == ClassState::Scope);
  MOZ_ASSERT_IF(nameForAnonymousClass || hasNameOnStack, !name);
  MOZ_ASSERT(!(nameForAnonymousClass && hasNameOnStack));

  //                [stack]

  name_ = name;
  nameForAnonymousClass_ = nameForAnonymousClass;
  hasNameOnStack_ = hasNameOnStack;
  isDerived_ = false;

  if (!bce_->emitNewInit()) {
    //              [stack] HOMEOBJ
    return false;
  }

#ifdef DEBUG
  classState_ = ClassState::Class;
#endif
  return true;
}

bool ClassEmitter::emitDerivedClass(JS::Handle<JSAtom*> name,
                                    JS::Handle<JSAtom*> nameForAnonymousClass,
                                    bool hasNameOnStack) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(classState_ == ClassState::Start ||
             classState_ == ClassState::Scope);
  MOZ_ASSERT_IF(nameForAnonymousClass || hasNameOnStack, !name);
  MOZ_ASSERT(!nameForAnonymousClass || !hasNameOnStack);

  //                [stack]

  name_ = name;
  nameForAnonymousClass_ = nameForAnonymousClass;
  hasNameOnStack_ = hasNameOnStack;
  isDerived_ = true;

  InternalIfEmitter ifThenElse(bce_);

  // Heritage must be null or a non-generator constructor
  if (!bce_->emit1(JSOP_CHECKCLASSHERITAGE)) {
    //              [stack] HERITAGE
    return false;
  }

  // [IF] (heritage !== null)
  if (!bce_->emit1(JSOP_DUP)) {
    //              [stack] HERITAGE HERITAGE
    return false;
  }
  if (!bce_->emit1(JSOP_NULL)) {
    //              [stack] HERITAGE HERITAGE NULL
    return false;
  }
  if (!bce_->emit1(JSOP_STRICTNE)) {
    //              [stack] HERITAGE NE
    return false;
  }

  // [THEN] funProto = heritage, objProto = heritage.prototype
  if (!ifThenElse.emitThenElse()) {
    return false;
  }
  if (!bce_->emit1(JSOP_DUP)) {
    //              [stack] HERITAGE HERITAGE
    return false;
  }
  if (!bce_->emitAtomOp(bce_->cx->names().prototype, JSOP_GETPROP)) {
    //              [stack] HERITAGE PROTO
    return false;
  }

  // [ELSE] funProto = %FunctionPrototype%, objProto = null
  if (!ifThenElse.emitElse()) {
    return false;
  }
  if (!bce_->emit1(JSOP_POP)) {
    //              [stack]
    return false;
  }
  if (!bce_->emit2(JSOP_BUILTINPROTO, JSProto_Function)) {
    //              [stack] PROTO
    return false;
  }
  if (!bce_->emit1(JSOP_NULL)) {
    //              [stack] PROTO NULL
    return false;
  }

  // [ENDIF]
  if (!ifThenElse.emitEnd()) {
    return false;
  }

  if (!bce_->emit1(JSOP_OBJWITHPROTO)) {
    //              [stack] HERITAGE HOMEOBJ
    return false;
  }
  if (!bce_->emit1(JSOP_SWAP)) {
    //              [stack] HOMEOBJ HERITAGE
    return false;
  }

#ifdef DEBUG
  classState_ = ClassState::Class;
#endif
  return true;
}

bool ClassEmitter::emitInitConstructor(bool needsHomeObject) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(classState_ == ClassState::Class ||
             classState_ == ClassState::FieldInitializersEnd);

  //                [stack] HOMEOBJ CTOR

  if (needsHomeObject) {
    if (!bce_->emitDupAt(1)) {
      //            [stack] HOMEOBJ CTOR HOMEOBJ
      return false;
    }
    if (!bce_->emit1(JSOP_INITHOMEOBJECT)) {
      //            [stack] HOMEOBJ CTOR
      return false;
    }
  }

  if (!initProtoAndCtor()) {
    //              [stack] CTOR HOMEOBJ
    return false;
  }

#ifdef DEBUG
  classState_ = ClassState::InitConstructor;
#endif
  return true;
}

bool ClassEmitter::emitInitDefaultConstructor(const Maybe<uint32_t>& classStart,
                                              const Maybe<uint32_t>& classEnd) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(classState_ == ClassState::Class ||
             classState_ == ClassState::FieldInitializersEnd);

  if (classStart && classEnd) {
    // In the case of default class constructors, emit the start and end
    // offsets in the source buffer as source notes so that when we
    // actually make the constructor during execution, we can give it the
    // correct toString output.
    if (!bce_->newSrcNote3(SRC_CLASS_SPAN, ptrdiff_t(*classStart),
                           ptrdiff_t(*classEnd))) {
      return false;
    }
  }

  RootedAtom className(bce_->cx, name_);
  if (!className) {
    if (nameForAnonymousClass_) {
      className = nameForAnonymousClass_;
    } else {
      className = bce_->cx->names().empty;
    }
  }

  if (isDerived_) {
    //              [stack] HERITAGE PROTO
    if (!bce_->emitAtomOp(className, JSOP_DERIVEDCONSTRUCTOR)) {
      //            [stack] HOMEOBJ CTOR
      return false;
    }
  } else {
    //              [stack] HOMEOBJ
    if (!bce_->emitAtomOp(className, JSOP_CLASSCONSTRUCTOR)) {
      //            [stack] HOMEOBJ CTOR
      return false;
    }
  }

  if (!initProtoAndCtor()) {
    //              [stack] CTOR HOMEOBJ
    return false;
  }

#ifdef DEBUG
  classState_ = ClassState::InitConstructor;
#endif
  return true;
}

bool ClassEmitter::initProtoAndCtor() {
  //                [stack] NAME? HOMEOBJ CTOR

  if (hasNameOnStack_) {
    if (!bce_->emitDupAt(2)) {
      //            [stack] NAME HOMEOBJ CTOR NAME
      return false;
    }
    if (!bce_->emit2(JSOP_SETFUNNAME, uint8_t(FunctionPrefixKind::None))) {
      //            [stack] NAME HOMEOBJ CTOR
      return false;
    }
  }

  if (!bce_->emit1(JSOP_SWAP)) {
    //              [stack] NAME? CTOR HOMEOBJ
    return false;
  }
  if (!bce_->emit1(JSOP_DUP2)) {
    //              [stack] NAME? CTOR HOMEOBJ CTOR HOMEOBJ
    return false;
  }
  if (!bce_->emitAtomOp(bce_->cx->names().prototype, JSOP_INITLOCKEDPROP)) {
    //              [stack] NAME? CTOR HOMEOBJ CTOR
    return false;
  }
  if (!bce_->emitAtomOp(bce_->cx->names().constructor, JSOP_INITHIDDENPROP)) {
    //              [stack] NAME? CTOR HOMEOBJ
    return false;
  }

  return true;
}

bool ClassEmitter::prepareForFieldInitializers(size_t numFields) {
  MOZ_ASSERT(classState_ == ClassState::Class);

  // .initializers is a variable that stores an array of lambdas containing
  // code (the initializer) for each field. Upon an object's construction,
  // these lambdas will be called, defining the values.
  initializersAssignment_.emplace(bce_,
                                  bce_->cx->names().dotInitializers.toHandle(),
                                  NameOpEmitter::Kind::Initialize);
  if (!initializersAssignment_->prepareForRhs()) {
    return false;
  }

  if (!bce_->emitUint32Operand(JSOP_NEWARRAY, numFields)) {
    //              [stack] HOMEOBJ HERITAGE? ARRAY
    return false;
  }

  MOZ_ASSERT(fieldIndex_ == 0);
#ifdef DEBUG
  classState_ = ClassState::FieldInitializers;
  numFields_ = numFields;
#endif
  return true;
}

bool ClassEmitter::emitFieldInitializerHomeObject() {
  MOZ_ASSERT(classState_ == ClassState::FieldInitializers);
  //          [stack] OBJ HERITAGE? ARRAY METHOD

  if (!bce_->emitDupAt(isDerived_ ? 3 : 2)) {
    //              [stack] OBJ HERITAGE? ARRAY METHOD OBJ
    return false;
  }
  if (!bce_->emit1(JSOP_INITHOMEOBJECT)) {
    //              [stack] OBJ HERITAGE? ARRAY METHOD
    return false;
  }

#ifdef DEBUG
  classState_ = ClassState::FieldInitializerWithHomeObject;
#endif
  return true;
}

bool ClassEmitter::emitStoreFieldInitializer() {
  MOZ_ASSERT(classState_ == ClassState::FieldInitializers ||
             classState_ == ClassState::FieldInitializerWithHomeObject);
  MOZ_ASSERT(fieldIndex_ < numFields_);
  //          [stack] HOMEOBJ HERITAGE? ARRAY METHOD

  if (!bce_->emitUint32Operand(JSOP_INITELEM_ARRAY, fieldIndex_)) {
    //          [stack] HOMEOBJ HERITAGE? ARRAY
    return false;
  }

  fieldIndex_++;
#ifdef DEBUG
  classState_ = ClassState::FieldInitializers;
#endif
  return true;
}

bool ClassEmitter::emitFieldInitializersEnd() {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);
  MOZ_ASSERT(classState_ == ClassState::FieldInitializers ||
             classState_ == ClassState::FieldInitializerWithHomeObject);
  MOZ_ASSERT(fieldIndex_ == numFields_);

  if (!initializersAssignment_->emitAssignment()) {
    //              [stack] HOMEOBJ HERITAGE? ARRAY
    return false;
  }
  initializersAssignment_.reset();

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack] HOMEOBJ HERITAGE?
    return false;
  }

#ifdef DEBUG
  classState_ = ClassState::FieldInitializersEnd;
#endif
  return true;
}

bool ClassEmitter::emitEnd(Kind kind) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);
  MOZ_ASSERT(classState_ == ClassState::InitConstructor ||
             classState_ == ClassState::FieldInitializersEnd);
  //                [stack] CTOR HOMEOBJ

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack] CTOR
    return false;
  }

  if (name_) {
    MOZ_ASSERT(tdzCache_.isSome());
    MOZ_ASSERT(innerScope_.isSome());

    if (!bce_->emitLexicalInitialization(name_)) {
      //            [stack] CTOR
      return false;
    }

    if (!innerScope_->leave(bce_)) {
      return false;
    }
    innerScope_.reset();

    if (kind == Kind::Declaration) {
      if (!bce_->emitLexicalInitialization(name_)) {
        //          [stack] CTOR
        return false;
      }
      // Only class statements make outer bindings, and they do not leave
      // themselves on the stack.
      if (!bce_->emit1(JSOP_POP)) {
        //          [stack]
        return false;
      }
    }

    tdzCache_.reset();
  } else if (innerScope_.isSome()) {
    //              [stack] CTOR
    MOZ_ASSERT(kind == Kind::Expression);
    MOZ_ASSERT(tdzCache_.isSome());

    if (!innerScope_->leave(bce_)) {
      return false;
    }
    innerScope_.reset();
    tdzCache_.reset();
  } else {
    //              [stack] CTOR
    MOZ_ASSERT(kind == Kind::Expression);
    MOZ_ASSERT(tdzCache_.isNothing());
  }

  //                [stack] # class declaration
  //                [stack]
  //                [stack] # class expression
  //                [stack] CTOR

  strictMode_.restore();

#ifdef DEBUG
  classState_ = ClassState::End;
#endif
  return true;
}
