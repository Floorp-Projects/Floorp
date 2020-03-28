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
#include "gc/AllocKind.h"              // AllocKind
#include "js/Id.h"                     // jsid
#include "js/Value.h"                  // UndefinedHandleValue
#include "vm/BytecodeUtil.h"           // IsHiddenInitOp
#include "vm/JSContext.h"              // JSContext
#include "vm/NativeObject.h"           // NativeDefineDataProperty
#include "vm/ObjectGroup.h"            // TenuredObject
#include "vm/Opcodes.h"                // JSOp
#include "vm/Runtime.h"                // JSAtomState (cx->names())

#include "gc/ObjectKind-inl.h"  // GetGCObjectKind
#include "vm/JSAtom-inl.h"      // AtomToId
#include "vm/JSObject-inl.h"    // NewBuiltinClassInstance

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

PropertyEmitter::PropertyEmitter(BytecodeEmitter* bce) : bce_(bce) {}

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

  if (!bce_->emit1(JSOp::MutateProto)) {
    //              [stack] OBJ
    return false;
  }

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
  if (!bce_->emit1(JSOp::Dup)) {
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
    if (!bce_->emit1(JSOp::Dup2)) {
      //            [stack] CTOR HOMEOBJ CTOR HOMEOBJ
      return false;
    }
    if (!bce_->emit1(JSOp::Pop)) {
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

  if (!bce_->emit1(JSOp::ToId)) {
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
  // JSOp::InitHomeObject uses one of the following:
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
  if (!bce_->emit1(JSOp::InitHomeObject)) {
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
  return emitInit(isClass_ ? JSOp::InitHiddenProp : JSOp::InitProp, key);
}

bool PropertyEmitter::emitInitGetter(JS::Handle<JSAtom*> key) {
  return emitInit(isClass_ ? JSOp::InitHiddenPropGetter : JSOp::InitPropGetter,
                  key);
}

bool PropertyEmitter::emitInitSetter(JS::Handle<JSAtom*> key) {
  return emitInit(isClass_ ? JSOp::InitHiddenPropSetter : JSOp::InitPropSetter,
                  key);
}

bool PropertyEmitter::emitInitIndexProp() {
  return emitInitIndexOrComputed(isClass_ ? JSOp::InitHiddenElem
                                          : JSOp::InitElem);
}

bool PropertyEmitter::emitInitIndexGetter() {
  return emitInitIndexOrComputed(isClass_ ? JSOp::InitHiddenElemGetter
                                          : JSOp::InitElemGetter);
}

bool PropertyEmitter::emitInitIndexSetter() {
  return emitInitIndexOrComputed(isClass_ ? JSOp::InitHiddenElemSetter
                                          : JSOp::InitElemSetter);
}

bool PropertyEmitter::emitInitComputedProp() {
  return emitInitIndexOrComputed(isClass_ ? JSOp::InitHiddenElem
                                          : JSOp::InitElem);
}

bool PropertyEmitter::emitInitComputedGetter() {
  return emitInitIndexOrComputed(isClass_ ? JSOp::InitHiddenElemGetter
                                          : JSOp::InitElemGetter);
}

bool PropertyEmitter::emitInitComputedSetter() {
  return emitInitIndexOrComputed(isClass_ ? JSOp::InitHiddenElemSetter
                                          : JSOp::InitElemSetter);
}

bool PropertyEmitter::emitInit(JSOp op, JS::Handle<JSAtom*> key) {
  MOZ_ASSERT(propertyState_ == PropertyState::PropValue ||
             propertyState_ == PropertyState::InitHomeObj);

  MOZ_ASSERT(op == JSOp::InitProp || op == JSOp::InitHiddenProp ||
             op == JSOp::InitPropGetter || op == JSOp::InitHiddenPropGetter ||
             op == JSOp::InitPropSetter || op == JSOp::InitHiddenPropSetter);

  //                [stack] CTOR? OBJ CTOR? VAL

  if (!bce_->emitAtomOp(op, key)) {
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

  MOZ_ASSERT(op == JSOp::InitElem || op == JSOp::InitHiddenElem ||
             op == JSOp::InitElemGetter || op == JSOp::InitHiddenElemGetter ||
             op == JSOp::InitElemSetter || op == JSOp::InitHiddenElemSetter);

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

    if (!bce_->emit1(JSOp::Pop)) {
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
  if (!bce_->emit1(JSOp::NewInit)) {
    //              [stack] OBJ
    return false;
  }

#ifdef DEBUG
  objectState_ = ObjectState::Object;
#endif
  return true;
}

bool ObjectEmitter::emitObjectWithTemplateOnStack() {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(objectState_ == ObjectState::Start);

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

  if (!bce_->emit1(JSOp::NewInit)) {
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
  if (!bce_->emit1(JSOp::CheckClassHeritage)) {
    //              [stack] HERITAGE
    return false;
  }

  // [IF] (heritage !== null)
  if (!bce_->emit1(JSOp::Dup)) {
    //              [stack] HERITAGE HERITAGE
    return false;
  }
  if (!bce_->emit1(JSOp::Null)) {
    //              [stack] HERITAGE HERITAGE NULL
    return false;
  }
  if (!bce_->emit1(JSOp::StrictNe)) {
    //              [stack] HERITAGE NE
    return false;
  }

  // [THEN] funProto = heritage, objProto = heritage.prototype
  if (!ifThenElse.emitThenElse()) {
    return false;
  }
  if (!bce_->emit1(JSOp::Dup)) {
    //              [stack] HERITAGE HERITAGE
    return false;
  }
  if (!bce_->emitAtomOp(JSOp::GetProp, bce_->cx->names().prototype)) {
    //              [stack] HERITAGE PROTO
    return false;
  }

  // [ELSE] funProto = %FunctionPrototype%, objProto = null
  if (!ifThenElse.emitElse()) {
    return false;
  }
  if (!bce_->emit1(JSOp::Pop)) {
    //              [stack]
    return false;
  }
  if (!bce_->emit2(JSOp::BuiltinProto, JSProto_Function)) {
    //              [stack] PROTO
    return false;
  }
  if (!bce_->emit1(JSOp::Null)) {
    //              [stack] PROTO NULL
    return false;
  }

  // [ENDIF]
  if (!ifThenElse.emitEnd()) {
    return false;
  }

  if (!bce_->emit1(JSOp::ObjWithProto)) {
    //              [stack] HERITAGE HOMEOBJ
    return false;
  }
  if (!bce_->emit1(JSOp::Swap)) {
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
             classState_ == ClassState::InstanceFieldInitializersEnd);

  //                [stack] HOMEOBJ CTOR

  if (needsHomeObject) {
    if (!bce_->emitDupAt(1)) {
      //            [stack] HOMEOBJ CTOR HOMEOBJ
      return false;
    }
    if (!bce_->emit1(JSOp::InitHomeObject)) {
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

bool ClassEmitter::emitInitDefaultConstructor(uint32_t classStart,
                                              uint32_t classEnd) {
  MOZ_ASSERT(propertyState_ == PropertyState::Start);
  MOZ_ASSERT(classState_ == ClassState::Class);

  RootedAtom className(bce_->cx, name_);
  if (!className) {
    if (nameForAnonymousClass_) {
      className = nameForAnonymousClass_;
    } else {
      className = bce_->cx->names().empty;
    }
  }

  uint32_t atomIndex;
  if (!bce_->makeAtomIndex(className, &atomIndex)) {
    return false;
  }

  // In the case of default class constructors, emit the start and end
  // offsets in the source buffer as source notes so that when we
  // actually make the constructor during execution, we can give it the
  // correct toString output.
  BytecodeOffset off;
  if (isDerived_) {
    //              [stack] HERITAGE PROTO
    if (!bce_->emitN(JSOp::DerivedConstructor, 12, &off)) {
      //            [stack] HOMEOBJ CTOR
      return false;
    }
  } else {
    //              [stack] HOMEOBJ
    if (!bce_->emitN(JSOp::ClassConstructor, 12, &off)) {
      //            [stack] HOMEOBJ CTOR
      return false;
    }
  }
  SetClassConstructorOperands(bce_->bytecodeSection().code(off), atomIndex,
                              classStart, classEnd);

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
    if (!bce_->emit2(JSOp::SetFunName, uint8_t(FunctionPrefixKind::None))) {
      //            [stack] NAME HOMEOBJ CTOR
      return false;
    }
  }

  if (!bce_->emit1(JSOp::Swap)) {
    //              [stack] NAME? CTOR HOMEOBJ
    return false;
  }
  if (!bce_->emit1(JSOp::Dup2)) {
    //              [stack] NAME? CTOR HOMEOBJ CTOR HOMEOBJ
    return false;
  }
  if (!bce_->emitAtomOp(JSOp::InitLockedProp, bce_->cx->names().prototype)) {
    //              [stack] NAME? CTOR HOMEOBJ CTOR
    return false;
  }
  if (!bce_->emitAtomOp(JSOp::InitHiddenProp, bce_->cx->names().constructor)) {
    //              [stack] NAME? CTOR HOMEOBJ
    return false;
  }

  return true;
}

bool ClassEmitter::prepareForFieldInitializers(size_t numFields,
                                               bool isStatic) {
  MOZ_ASSERT_IF(!isStatic, classState_ == ClassState::Class);
  MOZ_ASSERT_IF(isStatic, classState_ == ClassState::InitConstructor);
  MOZ_ASSERT(fieldState_ == FieldState::Start);

  // .initializers is a variable that stores an array of lambdas containing
  // code (the initializer) for each field. Upon an object's construction,
  // these lambdas will be called, defining the values.
  auto initializersName = isStatic ? &JSAtomState::dotStaticInitializers
                                   : &JSAtomState::dotInitializers;
  HandlePropertyName initializers = bce_->cx->names().*initializersName;
  initializersAssignment_.emplace(bce_, initializers,
                                  NameOpEmitter::Kind::Initialize);
  if (!initializersAssignment_->prepareForRhs()) {
    return false;
  }

  if (!bce_->emitUint32Operand(JSOp::NewArray, numFields)) {
    //              [stack] ARRAY
    return false;
  }

  fieldIndex_ = 0;
#ifdef DEBUG
  if (isStatic) {
    classState_ = ClassState::StaticFieldInitializers;
  } else {
    classState_ = ClassState::InstanceFieldInitializers;
  }
  numFields_ = numFields;
#endif
  return true;
}

bool ClassEmitter::prepareForFieldInitializer() {
  MOZ_ASSERT(classState_ == ClassState::InstanceFieldInitializers ||
             classState_ == ClassState::StaticFieldInitializers);
  MOZ_ASSERT(fieldState_ == FieldState::Start);

#ifdef DEBUG
  fieldState_ = FieldState::Initializer;
#endif
  return true;
}

bool ClassEmitter::emitFieldInitializerHomeObject(bool isStatic) {
  MOZ_ASSERT(fieldState_ == FieldState::Initializer);
  //                [stack] OBJ HERITAGE? ARRAY METHOD
  // or:
  //                [stack] CTOR HOMEOBJ ARRAY METHOD

  if (isStatic) {
    if (!bce_->emitDupAt(3)) {
      //            [stack] CTOR HOMEOBJ ARRAY METHOD CTOR
      return false;
    }
  } else {
    if (!bce_->emitDupAt(isDerived_ ? 3 : 2)) {
      //            [stack] OBJ HERITAGE? ARRAY METHOD OBJ
      return false;
    }
  }
  if (!bce_->emit1(JSOp::InitHomeObject)) {
    //              [stack] OBJ HERITAGE? ARRAY METHOD
    // or:
    //              [stack] CTOR HOMEOBJ ARRAY METHOD
    return false;
  }

#ifdef DEBUG
  fieldState_ = FieldState::InitializerWithHomeObject;
#endif
  return true;
}

bool ClassEmitter::emitStoreFieldInitializer() {
  MOZ_ASSERT(fieldState_ == FieldState::Initializer ||
             fieldState_ == FieldState::InitializerWithHomeObject);
  MOZ_ASSERT(fieldIndex_ < numFields_);
  //          [stack] HOMEOBJ HERITAGE? ARRAY METHOD

  if (!bce_->emitUint32Operand(JSOp::InitElemArray, fieldIndex_)) {
    //          [stack] HOMEOBJ HERITAGE? ARRAY
    return false;
  }

  fieldIndex_++;
#ifdef DEBUG
  fieldState_ = FieldState::Start;
#endif
  return true;
}

bool ClassEmitter::emitFieldInitializersEnd() {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);
  MOZ_ASSERT(classState_ == ClassState::InstanceFieldInitializers ||
             classState_ == ClassState::StaticFieldInitializers);
  MOZ_ASSERT(fieldState_ == FieldState::Start);
  MOZ_ASSERT(fieldIndex_ == numFields_);

  if (!initializersAssignment_->emitAssignment()) {
    //              [stack] HOMEOBJ HERITAGE? ARRAY
    return false;
  }
  initializersAssignment_.reset();

  if (!bce_->emit1(JSOp::Pop)) {
    //              [stack] HOMEOBJ HERITAGE?
    return false;
  }

#ifdef DEBUG
  if (classState_ == ClassState::InstanceFieldInitializers) {
    classState_ = ClassState::InstanceFieldInitializersEnd;
  } else {
    classState_ = ClassState::StaticFieldInitializersEnd;
  }
#endif
  return true;
}

bool ClassEmitter::emitBinding() {
  MOZ_ASSERT(propertyState_ == PropertyState::Start ||
             propertyState_ == PropertyState::Init);
  MOZ_ASSERT(classState_ == ClassState::InitConstructor ||
             classState_ == ClassState::InstanceFieldInitializersEnd ||
             classState_ == ClassState::StaticFieldInitializersEnd);
  //                [stack] CTOR HOMEOBJ

  if (!bce_->emit1(JSOp::Pop)) {
    //              [stack] CTOR
    return false;
  }

  if (name_) {
    MOZ_ASSERT(innerScope_.isSome());

    if (!bce_->emitLexicalInitialization(name_)) {
      //            [stack] CTOR
      return false;
    }
  }

  //                [stack] CTOR

#ifdef DEBUG
  classState_ = ClassState::BoundName;
#endif
  return true;
}

bool ClassEmitter::emitEnd(Kind kind) {
  MOZ_ASSERT(classState_ == ClassState::BoundName);
  //                [stack] CTOR

  if (innerScope_.isSome()) {
    MOZ_ASSERT(tdzCache_.isSome());

    if (!innerScope_->leave(bce_)) {
      return false;
    }
    innerScope_.reset();
    tdzCache_.reset();
  } else {
    MOZ_ASSERT(kind == Kind::Expression);
    MOZ_ASSERT(tdzCache_.isNothing());
  }

  if (kind == Kind::Declaration) {
    MOZ_ASSERT(name_);

    if (!bce_->emitLexicalInitialization(name_)) {
      //            [stack] CTOR
      return false;
    }
    // Only class statements make outer bindings, and they do not leave
    // themselves on the stack.
    if (!bce_->emit1(JSOp::Pop)) {
      //            [stack]
      return false;
    }
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
