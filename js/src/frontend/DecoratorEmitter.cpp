/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/DecoratorEmitter.h"

#include "mozilla/Assertions.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/CallOrNewEmitter.h"
#include "frontend/IfEmitter.h"
#include "frontend/NameAnalysisTypes.h"
#include "frontend/ObjectEmitter.h"
#include "frontend/ParseNode.h"
#include "frontend/ParserAtom.h"
#include "frontend/WhileEmitter.h"
#include "vm/ThrowMsgKind.h"

using namespace js;
using namespace js::frontend;

DecoratorEmitter::DecoratorEmitter(BytecodeEmitter* bce) : bce_(bce) {}

// A helper function to read the decorators in reverse order to how they were
// parsed.
bool DecoratorEmitter::reverseDecoratorsToApplicationOrder(
    const ListNode* decorators, DecoratorsVector& vec) {
  if (!vec.resize(decorators->count())) {
    ReportOutOfMemory(bce_->fc);
    return false;
  }
  int end = decorators->count() - 1;
  for (ParseNode* decorator : decorators->contents()) {
    vec[end--] = decorator;
  }
  return true;
}

bool DecoratorEmitter::emitApplyDecoratorsToElementDefinition(
    DecoratorEmitter::Kind kind, ParseNode* key, ListNode* decorators,
    bool isStatic) {
  MOZ_ASSERT(kind != Kind::Field && kind != Kind::Accessor);

  // The DecoratorEmitter expects the value to be decorated to be at the top
  // of the stack prior to this call. It will apply the decorators to this
  // value, possibly replacing the value with a value returned by a decorator.
  //          [stack] VAL

  // Decorators Proposal
  // https://arai-a.github.io/ecma262-compare/?pr=2417&id=sec-applydecoratorstoelementdefinition.
  // Step 1. Let decorators be elementRecord.[[Decorators]].
  // Step 2. If decorators is empty, return unused.
  // This is checked by the caller.
  MOZ_ASSERT(!decorators->empty());

  DecoratorsVector dec_vecs;
  if (!reverseDecoratorsToApplicationOrder(decorators, dec_vecs)) {
    return false;
  }

  // Step 3. Let key be elementRecord.[[Key]].
  // Step 4. Let kind be elementRecord.[[Kind]].
  // Step 5. For each element decorator of decorators, do
  for (auto it = dec_vecs.begin(); it != dec_vecs.end(); it++) {
    ParseNode* decorator = *it;
    // Step 5.a. Let decorationState be the Record { [[Finished]]: false }.
    if (!emitDecorationState()) {
      return false;
    }

    if (!emitCallDecoratorForElement(kind, key, isStatic, decorator)) {
      return false;
    }

    // Step 5.i. Set decorationState.[[Finished]] to true.
    if (!emitUpdateDecorationState()) {
      return false;
    }

    // We need to check if the decorator returned undefined, a callable value,
    // or any other value.
    if (!emitCheckIsUndefined()) {
      //          [stack] VAL RETVAL ISUNDEFINED
      return false;
    }

    InternalIfEmitter ie(bce_);
    if (!ie.emitThenElse()) {
      //          [stack] VAL RETVAL
      return false;
    }

    // Pop the undefined RETVAL from the stack, leaving the original value in
    // place.
    if (!bce_->emitPopN(1)) {
      //          [stack] VAL
      return false;
    }

    if (!ie.emitElseIf(mozilla::Nothing())) {
      return false;
    }

    // Step 5.l.i. If IsCallable(newValue) is true, then
    if (!emitCheckIsCallable()) {
      //              [stack] VAL RETVAL ISCALLABLE_RESULT
      return false;
    }

    if (!ie.emitThenElse()) {
      //          [stack] VAL RETVAL
      return false;
    }
    // Step 5.l. Else,
    // Step 5.l.i.1. Perform MakeMethod(newValue, homeObject).
    // MakeMethod occurs in the caller, here we just drop the original method
    // which was an argument to the decorator, and leave the new method
    // returned by the decorator on the stack.
    if (!bce_->emit1(JSOp::Swap)) {
      //          [stack] RETVAL VAL
      return false;
    }
    if (!bce_->emitPopN(1)) {
      //          [stack] RETVAL
      return false;
    }
    // Step 5.j.ii. Else if initializer is not undefined, throw a TypeError
    //              exception.
    // Step 5.l.ii. Else if newValue is not undefined, throw a
    //              TypeError exception.
    if (!ie.emitElse()) {
      return false;
    }

    if (!bce_->emitPopN(1)) {
      //          [stack] RETVAL
      return false;
    }

    if (!bce_->emit2(JSOp::ThrowMsg,
                     uint8_t(ThrowMsgKind::DecoratorInvalidReturnType))) {
      return false;
    }

    if (!ie.emitEnd()) {
      return false;
    }
  }

  return true;
}

bool DecoratorEmitter::emitApplyDecoratorsToFieldDefinition(
    ParseNode* key, ListNode* decorators, bool isStatic) {
  // This method creates a new array to contain initializers added by decorators
  // to the stack. start:
  //          [stack]
  // end:
  //          [stack] ARRAY

  // Decorators Proposal
  // https://arai-a.github.io/ecma262-compare/?pr=2417&id=sec-applydecoratorstoelementdefinition.
  // Step 1. Let decorators be elementRecord.[[Decorators]].
  // Step 2. If decorators is empty, return unused.
  // This is checked by the caller.
  MOZ_ASSERT(!decorators->empty());

  // If we're apply decorators to a field, we'll push a new array to the stack
  // to hold newly created initializers.
  if (!bce_->emitUint32Operand(JSOp::NewArray, 1)) {
    //          [stack] ARRAY
    return false;
  }

  if (!emitPropertyKey(key)) {
    //          [stack] ARRAY NAME
    return false;
  }

  if (!bce_->emitUint32Operand(JSOp::InitElemArray, 0)) {
    //          [stack] ARRAY
    return false;
  }

  if (!bce_->emit1(JSOp::One)) {
    //          [stack] ARRAY INDEX
    return false;
  }

  DecoratorsVector dec_vecs;
  if (!reverseDecoratorsToApplicationOrder(decorators, dec_vecs)) {
    return false;
  }

  // Step 3. Let key be elementRecord.[[Key]].
  // Step 4. Let kind be elementRecord.[[Kind]].
  // Step 5. For each element decorator of decorators, do
  for (auto it = dec_vecs.begin(); it != dec_vecs.end(); it++) {
    ParseNode* decorator = *it;
    // Step 5.a. Let decorationState be the Record { [[Finished]]: false }.
    if (!emitDecorationState()) {
      return false;
    }

    if (!emitCallDecoratorForElement(Kind::Field, key, isStatic, decorator)) {
      //          [stack] ARRAY INDEX RETVAL
      return false;
    }

    // Step 5.i. Set decorationState.[[Finished]] to true.
    if (!emitUpdateDecorationState()) {
      //          [stack] ARRAY INDEX RETVAL
      return false;
    }

    // We need to check if the decorator returned undefined, a callable value,
    // or any other value.
    if (!emitCheckIsUndefined()) {
      //          [stack] ARRAY INDEX RETVAL ISUNDEFINED
      return false;
    }

    InternalIfEmitter ie(bce_);
    if (!ie.emitThenElse()) {
      //          [stack] ARRAY INDEX RETVAL
      return false;
    }

    // Pop the undefined RETVAL from the stack, leaving the original value in
    // place.
    if (!bce_->emitPopN(1)) {
      //          [stack] ARRAY INDEX
      return false;
    }

    if (!ie.emitElseIf(mozilla::Nothing())) {
      return false;
    }

    // Step 5.l.i. If IsCallable(newValue) is true, then
    if (!emitCheckIsCallable()) {
      //              [stack] ARRAY INDEX RETVAL ISCALLABLE_RESULT
      return false;
    }

    if (!ie.emitThenElse()) {
      //          [stack] ARRAY INDEX RETVAL
      return false;
    }

    // Step 5.j. If kind is field, then
    // Step 5.j.i. If IsCallable(initializer) is true, append initializer to
    //             elementRecord.[[Initializers]].
    if (!bce_->emit1(JSOp::InitElemInc)) {
      //          [stack] ARRAY INDEX
      return false;
    }

    // Step 5.j.ii. Else if initializer is not undefined, throw a TypeError
    //              exception.
    // Step 5.l.ii. Else if newValue is not undefined, throw a
    //              TypeError exception.
    if (!ie.emitElse()) {
      return false;
    }

    if (!bce_->emitPopN(1)) {
      //          [stack] ARRAY INDEX
      return false;
    }

    if (!bce_->emit2(JSOp::ThrowMsg,
                     uint8_t(ThrowMsgKind::DecoratorInvalidReturnType))) {
      return false;
    }

    if (!ie.emitEnd()) {
      return false;
    }
  }

  // Pop INDEX
  return bce_->emitPopN(1);
  //          [stack] ARRAY
}

bool DecoratorEmitter::emitApplyDecoratorsToAccessorDefinition(
    ParseNode* key, ListNode* decorators, bool isStatic) {
  // This method creates a new array to contain initializers added by decorators
  // to the stack. start:
  //          [stack] GETTER SETTER
  // end:
  //          [stack] GETTER SETTER ARRAY
  MOZ_ASSERT(key->is<NameNode>());

  // Decorators Proposal
  // https://arai-a.github.io/ecma262-compare/?pr=2417&id=sec-applydecoratorstoelementdefinition.
  // Step 1. Let decorators be elementRecord.[[Decorators]].
  // Step 2. If decorators is empty, return unused.
  // This is checked by the caller.
  MOZ_ASSERT(!decorators->empty());

  // If we're applying decorators to a field, we'll push a new array to the
  // stack to hold newly created initializers.
  if (!bce_->emitUint32Operand(JSOp::NewArray, 1)) {
    //          [stack] GETTER SETTER ARRAY
    return false;
  }

  if (!bce_->emitGetPrivateName(&key->as<NameNode>())) {
    //          [stack] GETTER SETTER ARRAY NAME
    return false;
  }

  if (!bce_->emitUint32Operand(JSOp::InitElemArray, 0)) {
    //          [stack] GETTER SETTER ARRAY
    return false;
  }

  if (!bce_->emit1(JSOp::One)) {
    //          [stack] GETTER SETTER ARRAY INDEX
    return false;
  }

  DecoratorsVector dec_vecs;
  if (!reverseDecoratorsToApplicationOrder(decorators, dec_vecs)) {
    return false;
  }

  // Step 3. Let key be elementRecord.[[Key]].
  // Step 4. Let kind be elementRecord.[[Kind]].
  // Step 5. For each element decorator of decorators, do
  for (auto it = dec_vecs.begin(); it != dec_vecs.end(); it++) {
    ParseNode* decorator = *it;
    // 5.a. Let decorationState be the Record { [[Finished]]: false }.
    if (!emitDecorationState()) {
      return false;
    }

    // Step 5.g.i. Set value to OrdinaryObjectCreate(%Object.prototype%).
    ObjectEmitter oe(bce_);
    if (!oe.emitObject(2)) {
      //          [stack] GETTER SETTER ARRAY INDEX VALUE
      return false;
    }

    // Step 5.g.ii. Perform ! CreateDataPropertyOrThrow(value, "get",
    //              elementRecord.[[Get]]).
    if (!oe.prepareForPropValue(decorator->pn_pos.begin,
                                PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (!bce_->emitDupAt(4)) {
      //          [stack] GETTER SETTER ARRAY INDEX VALUE GETTER
      return false;
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::get())) {
      //          [stack] GETTER SETTER ARRAY INDEX VALUE
      return false;
    }

    // Step 5.g.iii. Perform ! CreateDataPropertyOrThrow(value, "set",
    //               elementRecord.[[Set]]).
    if (!oe.prepareForPropValue(decorator->pn_pos.begin,
                                PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (!bce_->emitDupAt(3)) {
      //          [stack] GETTER SETTER ARRAY INDEX VALUE SETTER
      return false;
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::set())) {
      //          [stack] GETTER SETTER ARRAY INDEX VALUE
      return false;
    }

    if (!oe.emitEnd()) {
      //          [stack] GETTER SETTER ARRAY INDEX VALUE
      return false;
    }

    // Step 5.j. Let newValue be ? Call(decorator, decoratorReceiver,
    //           « value, context »).
    if (!emitCallDecoratorForElement(Kind::Accessor, key, isStatic,
                                     decorator)) {
      //          [stack] GETTER SETTER ARRAY INDEX RETVAL
      return false;
    }

    // Step 5.k. Set decorationState.[[Finished]] to true.
    if (!emitUpdateDecorationState()) {
      //          [stack] GETTER SETTER ARRAY INDEX RETVAL
      return false;
    }

    // We need to check if the decorator returned undefined, a callable value,
    // or any other value.
    if (!emitCheckIsUndefined()) {
      //          [stack] GETTER SETTER ARRAY INDEX RETVAL ISUNDEFINED
      return false;
    }

    InternalIfEmitter ie(bce_);
    if (!ie.emitThenElse()) {
      //          [stack] GETTER SETTER ARRAY INDEX RETVAL
      return false;
    }

    // Pop the undefined RETVAL from the stack, leaving the original values in
    // place.
    if (!bce_->emitPopN(1)) {
      //          [stack] GETTER SETTER ARRAY INDEX
      return false;
    }

    if (!ie.emitElse()) {
      return false;
    }

    // Step 5.k. Else if kind is accessor, then
    // Step 5.k.ii. Else if newValue is not undefined, throw a TypeError
    //              exception. (Reordered)
    if (!bce_->emit2(JSOp::CheckIsObj,
                     uint8_t(CheckIsObjectKind::DecoratorReturn))) {
      //          [stack] GETTER SETTER ARRAY INDEX RETVAL
      return false;
    }

    // Step 5.k.i. If newValue is an Object, then
    // Step 5.k.i.1. Let newGetter be ? Get(newValue, "get").
    // Step 5.k.i.2. If IsCallable(newGetter) is true, set
    //               elementRecord.[[Get]] to newGetter.
    // Step 5.k.i.3. Else if newGetter is not undefined, throw a
    //               TypeError exception.
    if (!emitHandleNewValueField(
            frontend::TaggedParserAtomIndex::WellKnown::get(), 5)) {
      return false;
    }

    // Step 5.k.i.4. Let newSetter be ? Get(newValue, "set").
    // Step 5.k.i.5. If IsCallable(newSetter) is true, set
    //               elementRecord.[[Set]] to newSetter.
    // Step 5.k.i.6. Else if newSetter is not undefined, throw a
    //               TypeError exception.
    if (!emitHandleNewValueField(
            frontend::TaggedParserAtomIndex::WellKnown::set(), 4)) {
      return false;
    }

    // Step 5.k.i.7. Let initializer be ? Get(newValue, "init").
    // Step 5.k.i.8. If IsCallable(initializer) is true, append
    //               initializer to elementRecord.[[Initializers]].
    // Step 5.k.i.9. Else if initializer is not undefined, throw a
    //               TypeError exception.
    if (!emitHandleNewValueField(
            frontend::TaggedParserAtomIndex::WellKnown::init(), 0)) {
      return false;
    }

    // Pop RETVAL from stack
    if (!bce_->emitPopN(1)) {
      //          [stack] GETTER SETTER ARRAY INDEX
      return false;
    }

    if (!ie.emitEnd()) {
      return false;
    }
  }

  // Pop INDEX
  return bce_->emitPopN(1);
  //          [stack] GETTER SETTER ARRAY
}

bool DecoratorEmitter::emitApplyDecoratorsToClassDefinition(
    ParseNode* key, ListNode* decorators) {
  // This function expects a class constructor to already be on the stack. It
  // applies each decorator to the class constructor, possibly replacing it with
  // the return value of the decorator.
  //          [stack] CTOR

  DecoratorsVector dec_vecs;
  if (!reverseDecoratorsToApplicationOrder(decorators, dec_vecs)) {
    return false;
  }

  // https://arai-a.github.io/ecma262-compare/?pr=2417&id=sec-applydecoratorstoclassdefinition
  // Step 1. For each element decoratorRecord of decorators, do
  for (auto it = dec_vecs.begin(); it != dec_vecs.end(); it++) {
    ParseNode* decorator = *it;
    // Step 1.a. Let decorator be decoratorRecord.[[Decorator]].
    // Step 1.b. Let decoratorReceiver be decoratorRecord.[[Receiver]].
    // Step 1.c. Let decorationState be the Record { [[Finished]]: false }.
    if (!emitDecorationState()) {
      return false;
    }

    CallOrNewEmitter cone(bce_, JSOp::Call,
                          CallOrNewEmitter::ArgumentsKind::Other,
                          ValueUsage::WantValue);

    if (!bce_->emitCalleeAndThis(decorator, nullptr, cone)) {
      //          [stack] VAL? CALLEE THIS
      return false;
    }

    if (!cone.prepareForNonSpreadArguments()) {
      return false;
    }

    // Duplicate the class definition to pass it as an argument
    // to the decorator.
    if (!bce_->emitDupAt(2)) {
      //          [stack] CTOR CALLEE THIS CTOR
      return false;
    }

    // Step 1.d. Let context be CreateDecoratorContextObject(class, className,
    //           extraInitializers, decorationState).
    if (!emitCreateDecoratorContextObject(Kind::Class, key, false,
                                          decorator->pn_pos)) {
      //          [stack] CTOR CALLEE THIS CTOR context
      return false;
    }

    // Step 1.e. Let newDef be ? Call(decorator, decoratorReceiver, « classDef,
    //           context »).
    if (!cone.emitEnd(2, decorator->pn_pos.begin)) {
      //          [stack] CTOR NEWCTOR
      return false;
    }

    // Step 1.f. Set decorationState.[[Finished]] to true.
    if (!emitUpdateDecorationState()) {
      return false;
    }

    if (!emitCheckIsUndefined()) {
      //          [stack] CTOR NEWCTOR ISUNDEFINED
      return false;
    }

    InternalIfEmitter ie(bce_);
    if (!ie.emitThenElse()) {
      //          [stack] CTOR NEWCTOR
      return false;
    }

    // Pop the undefined NEWDEF from the stack, leaving the original value in
    // place.
    if (!bce_->emitPopN(1)) {
      //          [stack] CTOR
      return false;
    }

    if (!ie.emitElseIf(mozilla::Nothing())) {
      return false;
    }

    // Step 1.g. If IsCallable(newDef) is true, then
    // Step 1.g.i. Set classDef to newDef.
    if (!emitCheckIsCallable()) {
      //              [stack] CTOR NEWCTOR ISCALLABLE_RESULT
      return false;
    }

    if (!ie.emitThenElse()) {
      //          [stack] CTOR NEWCTOR
      return false;
    }

    if (!bce_->emit1(JSOp::Swap)) {
      //          [stack] NEWCTOR CTOR
      return false;
    }
    if (!bce_->emitPopN(1)) {
      //          [stack] NEWCTOR
      return false;
    }

    // Step 1.h. Else if newDef is not undefined, then
    // Step 1.h.i. Throw a TypeError exception.
    if (!ie.emitElse()) {
      return false;
    }

    if (!bce_->emitPopN(1)) {
      //          [stack] CTOR
      return false;
    }
    if (!bce_->emit2(JSOp::ThrowMsg,
                     uint8_t(ThrowMsgKind::DecoratorInvalidReturnType))) {
      return false;
    }

    if (!ie.emitEnd()) {
      return false;
    }
  }

  // Step 2. Return classDef.
  return true;
}

bool DecoratorEmitter::emitInitializeFieldOrAccessor() {
  //          [stack] THIS INITIALIZERS

  // Decorators Proposal
  // https://arai-a.github.io/ecma262-compare/?pr=2417&id=sec-applydecoratorstoelementdefinition.
  //
  // Step 1. Assert: elementRecord.[[Kind]] is field or accessor.
  // Step 2. If elementRecord.[[BackingStorageKey]] is present, let fieldName be
  //         elementRecord.[[BackingStorageKey]].
  // Step 3. Else, let fieldName be elementRecord.[[Key]].
  // We've stored the fieldname in the first element of the initializers array.
  if (!bce_->emit1(JSOp::Dup)) {
    //          [stack] THIS INITIALIZERS INITIALIZERS
    return false;
  }

  if (!bce_->emit1(JSOp::Zero)) {
    //          [stack] THIS INITIALIZERS INITIALIZERS INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::GetElem)) {
    //          [stack] THIS INITIALIZERS FIELDNAME
    return false;
  }

  // Retrieve initial value of the field
  if (!bce_->emit1(JSOp::Dup)) {
    //          [stack] THIS INITIALIZERS FIELDNAME FIELDNAME
    return false;
  }

  if (!bce_->emitDupAt(3)) {
    //          [stack] THIS INITIALIZERS FIELDNAME FIELDNAME THIS
    return false;
  }

  if (!bce_->emit1(JSOp::Swap)) {
    //          [stack] THIS INITIALIZERS FIELDNAME THIS FIELDNAME
    return false;
  }

  // Step 4. Let initValue be undefined.
  // TODO: (See Bug 1817993) At the moment, we're applying the initialization
  // logic in two steps. The pre-decorator initialization code runs, stores
  // the initial value, and then we retrieve it here and apply the initializers
  // added by decorators. We should unify these two steps.
  if (!bce_->emit1(JSOp::GetElem)) {
    //          [stack] THIS INITIALIZERS FIELDNAME VALUE
    return false;
  }

  if (!bce_->emit2(JSOp::Pick, 2)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS
    return false;
  }

  // Retrieve the length of the initializers array.
  if (!bce_->emit1(JSOp::Dup)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS INITIALIZERS
    return false;
  }

  if (!bce_->emitAtomOp(JSOp::GetProp,
                        TaggedParserAtomIndex::WellKnown::length())) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH
    return false;
  }

  if (!bce_->emit1(JSOp::One)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    return false;
  }

  // Step 5. For each element initializer of elementRecord.[[Initializers]], do
  InternalWhileEmitter wh(bce_);
  // At this point, we have no context to determine offsets in the
  // code for this while statement. Ideally, it would correspond to
  // the field we're initializing.
  if (!wh.emitCond()) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::Dup)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX INDEX
    return false;
  }

  if (!bce_->emitDupAt(2)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX INDEX
    //                  LENGTH
    return false;
  }

  if (!bce_->emit1(JSOp::Lt)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX BOOL
    return false;
  }

  // Step 5.a. Set initValue to ? Call(initializer, receiver, « initValue»).
  if (!wh.emitBody()) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    return false;
  }

  if (!bce_->emitDupAt(2)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    //                  INITIALIZERS
    return false;
  }

  if (!bce_->emitDupAt(1)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    //                  INITIALIZERS INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::GetElem)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX FUNC
    return false;
  }

  if (!bce_->emitDupAt(6)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX FUNC THIS
    return false;
  }

  // Pass value in as argument to the initializer
  if (!bce_->emit2(JSOp::Pick, 5)) {
    //          [stack] THIS FIELDNAME INITIALIZERS LENGTH INDEX FUNC THIS VALUE
    return false;
  }

  // Callee is always internal function.
  if (!bce_->emitCall(JSOp::Call, 1)) {
    //          [stack] THIS FIELDNAME INITIALIZERS LENGTH INDEX RVAL
    return false;
  }

  // Store returned value for next iteration
  if (!bce_->emit2(JSOp::Unpick, 3)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    return false;
  }

  if (!bce_->emit1(JSOp::Inc)) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    return false;
  }

  if (!wh.emitEnd()) {
    //          [stack] THIS FIELDNAME VALUE INITIALIZERS LENGTH INDEX
    return false;
  }

  // Step 6. If fieldName is a Private Name, then
  // Step 6.a. Perform ? PrivateFieldAdd(receiver, fieldName, initValue).
  // Step 7. Else,
  // Step 7.a. Assert: IsPropertyKey(fieldName) is true.
  // Step 7.b. Perform ? CreateDataPropertyOrThrow(receiver, fieldName,
  //           initValue).
  // TODO: (See Bug 1817993) Because the field already exists, we just store the
  // updated value here.
  if (!bce_->emitPopN(3)) {
    //          [stack] THIS FIELDNAME VALUE
    return false;
  }

  if (!bce_->emit1(JSOp::InitElem)) {
    //          [stack] THIS
    return false;
  }

  // Step 8. Return unused.
  return bce_->emitPopN(1);
  //          [stack]
}

bool DecoratorEmitter::emitPropertyKey(ParseNode* key) {
  if (key->is<NameNode>()) {
    NameNode* keyAsNameNode = &key->as<NameNode>();
    if (keyAsNameNode->privateNameKind() == PrivateNameKind::None) {
      if (!bce_->emitStringOp(JSOp::String, keyAsNameNode->atom())) {
        //          [stack] NAME
        return false;
      }
    } else {
      MOZ_ASSERT(keyAsNameNode->privateNameKind() == PrivateNameKind::Field);
      if (!bce_->emitGetPrivateName(keyAsNameNode)) {
        //          [stack] NAME
        return false;
      }
    }
  } else if (key->isKind(ParseNodeKind::NumberExpr)) {
    if (!bce_->emitNumberOp(key->as<NumericLiteral>().value())) {
      //      [stack] NAME
      return false;
    }
  } else {
    // Otherwise this is a computed property name. BigInt keys are parsed
    // as (synthetic) computed property names, too.
    MOZ_ASSERT(key->isKind(ParseNodeKind::ComputedName));

    if (!bce_->emitComputedPropertyName(&key->as<UnaryNode>())) {
      //      [stack] NAME
      return false;
    }
  }

  return true;
}

bool DecoratorEmitter::emitDecorationState() {
  // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1800724.
  return true;
}

bool DecoratorEmitter::emitUpdateDecorationState() {
  // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1800724.
  return true;
}

bool DecoratorEmitter::emitCallDecoratorForElement(Kind kind, ParseNode* key,
                                                   bool isStatic,
                                                   ParseNode* decorator) {
  MOZ_ASSERT(kind != Kind::Class);
  // Except for fields, this method expects the value to be passed
  // to the decorator to be on top of the stack. For methods, getters and
  // setters this is the method itself. For accessors it is an object
  // containing the getter and setter associated with the accessor.
  //          [stack] VAL?
  // Prepare to call decorator
  CallOrNewEmitter cone(bce_, JSOp::Call,
                        CallOrNewEmitter::ArgumentsKind::Other,
                        ValueUsage::WantValue);

  if (!bce_->emitCalleeAndThis(decorator, nullptr, cone)) {
    //          [stack] VAL? CALLEE THIS
    return false;
  }

  if (!cone.prepareForNonSpreadArguments()) {
    return false;
  }

  if (kind == Kind::Field) {
    // Step 5.c. Let value be undefined.
    if (!bce_->emit1(JSOp::Undefined)) {
      //          [stack] CALLEE THIS undefined
      return false;
    }
  } else if (kind == Kind::Getter || kind == Kind::Method ||
             kind == Kind::Setter) {
    // Step 5.d. If kind is method, set value to elementRecord.[[Value]].
    // Step 5.e. Else if kind is getter, set value to elementRecord.[[Get]].
    // Step 5.f. Else if kind is setter, set value to elementRecord.[[Set]].
    // The DecoratorEmitter expects the method to already be on the stack.
    // We dup the value here so we can use it as an argument to the decorator.
    if (!bce_->emitDupAt(2)) {
      //          [stack] VAL CALLEE THIS VAL
      return false;
    }
  } else {
    // Step 5.g. Else if kind is accessor, then
    // Step 5.g.i. Set value to OrdinaryObjectCreate(%Object.prototype%).
    // For accessor decorators, we've already created the value object prior
    // to calling this method.
    MOZ_ASSERT(kind == Kind::Accessor);
    if (!bce_->emitPickN(2)) {
      //          [stack] CALLEE THIS VAL
      return false;
    }
  }
  // Step 5.b. Let context be CreateDecoratorContextObject(kind, key,
  //           extraInitializers, decorationState, isStatic).
  if (!emitCreateDecoratorContextObject(kind, key, isStatic,
                                        decorator->pn_pos)) {
    //          [stack] VAL? CALLEE THIS VAL context
    return false;
  }

  // Step 5.h. Let newValue be ? Call(decorator, undefined, « value, context»).
  return cone.emitEnd(2, decorator->pn_pos.begin);
  //          [stack] VAL? RETVAL
}

bool DecoratorEmitter::emitCreateDecoratorAccessObject() {
  // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1800725.
  ObjectEmitter oe(bce_);
  if (!oe.emitObject(0)) {
    return false;
  }
  return oe.emitEnd();
}

bool DecoratorEmitter::emitCheckIsUndefined() {
  // This emits code to check if the value at the top of the stack is
  // undefined. The value is left on the stack.
  //          [stack] VAL
  if (!bce_->emit1(JSOp::Dup)) {
    //          [stack] VAL VAL
    return false;
  }
  if (!bce_->emit1(JSOp::Undefined)) {
    //          [stack] VAL VAL undefined
    return false;
  }
  return bce_->emit1(JSOp::Eq);
  //          [stack] VAL ISUNDEFINED
}

bool DecoratorEmitter::emitCheckIsCallable() {
  // This emits code to check if the value at the top of the stack is
  // callable. The value is left on the stack.
  //            [stack] VAL
  if (!bce_->emitAtomOp(JSOp::GetIntrinsic,
                        TaggedParserAtomIndex::WellKnown::IsCallable())) {
    //            [stack] VAL ISCALLABLE
    return false;
  }
  if (!bce_->emit1(JSOp::Undefined)) {
    //            [stack] VAL ISCALLABLE UNDEFINED
    return false;
  }
  if (!bce_->emitDupAt(2)) {
    //            [stack] VAL ISCALLABLE UNDEFINED VAL
    return false;
  }
  return bce_->emitCall(JSOp::Call, 1);
  //              [stack] VAL ISCALLABLE_RESULT
}

bool DecoratorEmitter::emitCreateAddInitializerFunction() {
  // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1800724.
  ObjectEmitter oe(bce_);
  if (!oe.emitObject(0)) {
    return false;
  }
  return oe.emitEnd();
}

bool DecoratorEmitter::emitCreateDecoratorContextObject(Kind kind,
                                                        ParseNode* key,
                                                        bool isStatic,
                                                        TokenPos pos) {
  // Step 1. Let contextObj be OrdinaryObjectCreate(%Object.prototype%).
  ObjectEmitter oe(bce_);
  size_t propertyCount = kind == Kind::Class ? 3 : 6;
  if (!oe.emitObject(propertyCount)) {
    //          [stack] context
    return false;
  }
  if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
    return false;
  }

  TaggedParserAtomIndex kindStr;
  switch (kind) {
    case Kind::Method:
      // Step 2. If kind is method, let kindStr be "method".
      kindStr = frontend::TaggedParserAtomIndex::WellKnown::method();
      break;
    case Kind::Getter:
      // Step 3. Else if kind is getter, let kindStr be "getter".
      kindStr = frontend::TaggedParserAtomIndex::WellKnown::getter();
      break;
    case Kind::Setter:
      // Step 4. Else if kind is setter, let kindStr be "setter".
      kindStr = frontend::TaggedParserAtomIndex::WellKnown::setter();
      break;
    case Kind::Accessor:
      // Step 5. Else if kind is accessor, let kindStr be "accessor".
      kindStr = frontend::TaggedParserAtomIndex::WellKnown::accessor();
      break;
    case Kind::Field:
      // Step 6. Else if kind is field, let kindStr be "field".
      kindStr = frontend::TaggedParserAtomIndex::WellKnown::field();
      break;
    case Kind::Class:
      // Step 7. Else,
      // Step 7.a. Assert: kind is class.
      // Step 7.b. Let kindStr be "class".
      kindStr = frontend::TaggedParserAtomIndex::WellKnown::class_();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown kind");
      break;
  }
  if (!bce_->emitStringOp(JSOp::String, kindStr)) {
    //          [stack] context kindStr
    return false;
  }

  // Step 8. Perform ! CreateDataPropertyOrThrow(contextObj, "kind", kindStr).
  if (!oe.emitInit(frontend::AccessorType::None,
                   frontend::TaggedParserAtomIndex::WellKnown::kind())) {
    //          [stack] context
    return false;
  }
  // Step 9. If kind is not class, then
  if (kind != Kind::Class) {
    MOZ_ASSERT(key != nullptr, "Expect key to be present except for classes");

    // Step 9.a. Perform ! CreateDataPropertyOrThrow(contextObj, "access",
    //           CreateDecoratorAccessObject(kind, name)).
    if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (!emitCreateDecoratorAccessObject()) {
      return false;
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::access())) {
      //          [stack] context
      return false;
    }
    // Step 9.b. If isStatic is present, perform
    //           ! CreateDataPropertyOrThrow(contextObj, "static", isStatic).
    if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (!bce_->emit1(isStatic ? JSOp::True : JSOp::False)) {
      //          [stack] context isStatic
      return false;
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::static_())) {
      //          [stack] context
      return false;
    }
    // Step 9.c. If name is a Private Name, then
    // Step 9.c.i. Perform ! CreateDataPropertyOrThrow(contextObj, "private",
    //             true).
    // Step 9.d. Else, Step 9.d.i. Perform
    //           ! CreateDataPropertyOrThrow(contextObj, "private", false).
    if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (!bce_->emit1(key->isKind(ParseNodeKind::PrivateName) ? JSOp::True
                                                             : JSOp::False)) {
      //          [stack] context private
      return false;
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::private_())) {
      //          [stack] context
      return false;
    }
    // Step 9.c.ii. Perform ! CreateDataPropertyOrThrow(contextObj,
    //              "name", name.[[Description]]).
    //
    // Step 9.d.ii. Perform ! CreateDataPropertyOrThrow(contextObj,
    //              "name", name.[[Description]]).)
    if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (key->is<NameNode>()) {
      if (!bce_->emitStringOp(JSOp::String, key->as<NameNode>().atom())) {
        return false;
      }
    } else {
      if (!emitPropertyKey(key)) {
        return false;
      }
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::name())) {
      //          [stack] context
      return false;
    }
  } else {
    // Step 10. Else,
    // Step 10.a. Perform ! CreateDataPropertyOrThrow(contextObj, "name", name).
    if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (key != nullptr) {
      if (!bce_->emitStringOp(JSOp::String, key->as<NameNode>().atom())) {
        return false;
      }
    } else {
      if (!bce_->emit1(JSOp::Undefined)) {
        return false;
      }
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::name())) {
      //          [stack] context
      return false;
    }
  }
  // Step 11. Let addInitializer be CreateAddInitializerFunction(initializers,
  //          decorationState).
  if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
    return false;
  }
  if (!emitCreateAddInitializerFunction()) {
    //          [stack] context addInitializer
    return false;
  }
  // Step 12. Perform ! CreateDataPropertyOrThrow(contextObj, "addInitializer",
  //          addInitializer).
  if (!oe.emitInit(
          frontend::AccessorType::None,
          frontend::TaggedParserAtomIndex::WellKnown::addInitializer())) {
    //          [stack] context
    return false;
  }
  // Step 13. Return contextObj.
  return oe.emitEnd();
}

bool DecoratorEmitter::emitHandleNewValueField(TaggedParserAtomIndex atom,
                                               int8_t offset) {
  // This function handles retrieving the new value from a field in the RETVAL
  // object returned by the decorator. The `atom` is the atom of the field to be
  // examined. The offset is the offset of the existing value on the stack,
  // which will be replaced by the new value. If the offset is zero, we're
  // handling the initializer which will be added to the array of initializers
  // already on the stack.
  //            [stack] GETTER SETTER ARRAY INDEX RETVAL

  if (!bce_->emit1(JSOp::Dup)) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL RETVAL
    return false;
  }
  if (!bce_->emitStringOp(JSOp::String, atom)) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL RETVAL ATOM
    return false;
  }
  if (!bce_->emit1(JSOp::GetElem)) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL
    //          NEW_VALUE
    return false;
  }

  if (!emitCheckIsUndefined()) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL
    //                  NEW_VALUE ISUNDEFINED
    return false;
  }

  InternalIfEmitter ifCallable(bce_);
  if (!ifCallable.emitThenElse()) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL
    //                  NEW_VALUE
    return false;
  }

  // Pop the undefined getter or setter from the stack, leaving the original
  // values in place.
  if (!bce_->emitPopN(1)) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL
    return false;
  }

  if (!ifCallable.emitElseIf(mozilla::Nothing())) {
    return false;
  }
  if (!emitCheckIsCallable()) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL
    //                  NEW_VALUE ISCALLABLE_RESULT
    return false;
  }
  if (!ifCallable.emitThenElse()) {
    //          [stack] GETTER SETTER ARRAY INDEX RETVAL
    //                  NEW_VALUE
    return false;
  }
  if (offset != 0) {
    if (!bce_->emitPickN(offset)) {
      //          [stack] GETTER? SETTER? ARRAY INDEX RETVAL
      //                  NEW_VALUE GETTER_OR_SETTER
      return false;
    }
    if (!bce_->emitPopN(1)) {
      //          [stack] GETTER? SETTER? ARRAY INDEX RETVAL
      //                  NEW_VALUE
      return false;
    }
    if (!bce_->emitUnpickN(offset - 1)) {
      //          [stack] GETTER SETTER ARRAY INDEX RETVAL
      return false;
    }
  } else {
    // Offset == 0 means we're retrieving the initializer, this is
    // stored in the initializer array on the stack.
    if (!bce_->emit1(JSOp::Swap)) {
      //          [stack] GETTER SETTER ARRAY INDEX NEW_VALUE RETVAL
      return false;
    }

    if (!bce_->emitUnpickN(3)) {
      //          [stack] GETTER SETTER RETVAL ARRAY INDEX NEW_VALUE
      return false;
    }

    if (!bce_->emit1(JSOp::InitElemInc)) {
      //          [stack] GETTER SETTER RETVAL ARRAY INDEX
      return false;
    }

    if (!bce_->emitPickN(2)) {
      //          [stack] GETTER SETTER ARRAY INDEX RETVAL
      return false;
    }
  }

  if (!ifCallable.emitElse()) {
    return false;
  }

  if (!bce_->emitPopN(1)) {
    //          [stack] GETTER SETTER ARRAY INDEX
    return false;
  }

  if (!bce_->emit2(JSOp::ThrowMsg,
                   uint8_t(ThrowMsgKind::DecoratorInvalidReturnType))) {
    return false;
  }

  return ifCallable.emitEnd();
}
