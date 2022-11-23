/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/DecoratorEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/CallOrNewEmitter.h"
#include "frontend/IfEmitter.h"
#include "frontend/ObjectEmitter.h"
#include "frontend/ParseNode.h"
#include "frontend/ParserAtom.h"
#include "vm/ThrowMsgKind.h"

using namespace js;
using namespace js::frontend;

DecoratorEmitter::DecoratorEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool DecoratorEmitter::emitApplyDecoratorsToElementDefinition(
    DecoratorEmitter::Kind kind, ParseNode* key, ListNode* decorators,
    bool isStatic) {
  // The DecoratorEmitter expects the value to be decorated to be at the top
  // of the stack prior to this call. It will apply the decorators to this
  // value, possibly replacing the value with a value returned by a decorator.
  //          [stack] CTOR? OBJ CTOR? VAL

  // Decorators Proposal
  // https://arai-a.github.io/ecma262-compare/?pr=2417&id=sec-applydecoratorstoelementdefinition.
  // 1. Let decorators be elementRecord.[[Decorators]].
  // 2. If decorators is empty, return unused.
  if (decorators->empty()) {
    return true;
  }
  // 3. Let key be elementRecord.[[Key]].
  // 4. Let kind be elementRecord.[[Kind]].
  // 5. For each element decorator of decorators, do
  for (ParseNode* decorator : decorators->contents()) {
    //     5.a. Let decorationState be the Record { [[Finished]]: false }.
    if (!emitDecorationState()) {
      return false;
    }

    // Prepare to call decorator
    CallOrNewEmitter cone(bce_, JSOp::Call,
                          CallOrNewEmitter::ArgumentsKind::Other,
                          ValueUsage::WantValue);

    // DecoratorMemberExpression: IdentifierReference e.g. @dec
    if (decorator->is<NameNode>()) {
      if (!cone.emitNameCallee(decorator->as<NameNode>().name())) {
        //          [stack] CTOR? OBJ CTOR? VAL CALLEE THIS?
        return false;
      }
    } else if (decorator->is<ListNode>()) {
      // DecoratorMemberExpression: DecoratorMemberExpression . IdentifierName
      // e.g. @decorators.nested.dec
      PropOpEmitter& poe = cone.prepareForPropCallee(false);
      if (!poe.prepareForObj()) {
        return false;
      }

      ListNode* ln = &decorator->as<ListNode>();
      bool first = true;
      for (ParseNode* node : ln->contentsTo(ln->last())) {
        // We should have only placed NameNode instances in this list while
        // parsing.
        MOZ_ASSERT(node->is<NameNode>());

        if (first) {
          NameNode* obj = &node->as<NameNode>();
          if (!bce_->emitGetName(obj)) {
            return false;
          }
          first = false;
        } else {
          NameNode* prop = &node->as<NameNode>();
          GCThingIndex propAtomIndex;

          if (!bce_->makeAtomIndex(prop->atom(), ParserAtom::Atomize::Yes,
                                   &propAtomIndex)) {
            return false;
          }

          if (!bce_->emitAtomOp(JSOp::GetProp, propAtomIndex)) {
            return false;
          }
        }
      }

      NameNode* prop = &ln->last()->as<NameNode>();
      if (!poe.emitGet(prop->atom())) {
        return false;
      }
    } else {
      // DecoratorCallExpression | DecoratorParenthesizedExpression,
      // e.g. @dec('argument') | @((value, context) => value)
      if (!cone.prepareForFunctionCallee()) {
        return false;
      }

      if (!bce_->emitTree(decorator)) {
        return false;
      }
    }

    if (!cone.emitThis()) {
      //          [stack] CTOR? OBJ CTOR? VAL CALLEE THIS
      return false;
    }

    if (!cone.prepareForNonSpreadArguments()) {
      return false;
    }

    //     5.c. Let value be undefined.
    //     5.d. If kind is method, set value to elementRecord.[[Value]].
    if (kind == Kind::Method) {
      // The DecoratorEmitter expects the method to already be on the stack.
      // We dup the value here so we can use it as an argument to the decorator.
      if (!bce_->emitDupAt(2)) {
        //          [stack] CTOR? OBJ CTOR? VAL CALLEE THIS VAL
        return false;
      }
    }
    //     5.e. Else if kind is getter, set value to elementRecord.[[Get]].
    //     5.f. Else if kind is setter, set value to elementRecord.[[Set]].
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793960.
    //     5.g. Else if kind is accessor, then
    //         5.g.i. Set value to OrdinaryObjectCreate(%Object.prototype%).
    //         5.g.ii. Perform ! CreateDataPropertyOrThrow(accessor, "get",
    //         elementRecord.[[Get]]). 5.g.iii. Perform
    //         ! CreateDataPropertyOrThrow(accessor, "set",
    //         elementRecord.[[Set]]).
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793961.
    //     5.b. Let context be CreateDecoratorContextObject(kind, key,
    //     extraInitializers, decorationState, isStatic).
    if (!emitCreateDecoratorContextObject(kind, key, isStatic,
                                          decorator->pn_pos)) {
      //          [stack] CTOR? OBJ CTOR? VAL CALLEE THIS VAL
      //          context
      return false;
    }

    //     5.h. Let newValue be ? Call(decorator, undefined, « value, context
    //     »).
    if (!cone.emitEnd(2, decorator->pn_pos.begin)) {
      //          [stack] CTOR? OBJ CTOR? VAL RETVAL
      return false;
    }

    //     5.i. Set decorationState.[[Finished]] to true.
    if (!emitUpdateDecorationState()) {
      return false;
    }
    // clang-format off
    //     5.j. If kind is field, then
    //         5.j.i. If IsCallable(initializer) is true, append initializer to elementRecord.[[Initializers]].
    //         5.j.ii. Else if initializer is not undefined, throw a TypeError exception.
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793962.
    //     5.k. Else if kind is accessor, then
    //         5.k.i. If newValue is an Object, then
    //             5.k.i.1. Let newGetter be ? Get(newValue, "get").
    //             5.k.i.2. If IsCallable(newGetter) is true, set elementRecord.[[Get]] to newGetter.
    //             5.k.i.3. Else if newGetter is not undefined, throw a TypeError exception.
    //             5.k.i.4. Let newSetter be ? Get(newValue, "set").
    //             5.k.i.5. If IsCallable(newSetter) is true, set elementRecord.[[Set]] to newSetter.
    //             5.k.i.6. Else if newSetter is not undefined, throw a TypeError exception.
    //             5.k.i.7. Let initializer be ? Get(newValue, "init").
    //             5.k.i.8. If IsCallable(initializer) is true, append initializer to elementRecord.[[Initializers]].
    //             5.k.i.9. Else if initializer is not undefined, throw a TypeError exception.
    //         5.k.ii. Else if newValue is not undefined, throw a TypeError exception.
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793961.
    //     5.l. Else,
    // clang-format on

    // We need to check if the decorator returned undefined, a callable value,
    // or any other value. If it returns undefined, we ignore it, if returns a
    // callable value, it replaces the previously defined value. Otherwise, we
    // throw an exception.
    IfEmitter ie(bce_);
    if (!ie.emitIf(mozilla::Nothing())) {
      return false;
    }
    if (!bce_->emit1(JSOp::Dup)) {
      //          [stack] CTOR? OBJ CTOR? VAL RETVAL RETVAL
      return false;
    }
    if (!bce_->emit1(JSOp::Undefined)) {
      //          [stack] CTOR? OBJ CTOR? VAL RETVAL undefined
      return false;
    }
    if (!bce_->emit1(JSOp::Eq)) {
      //          [stack] CTOR? OBJ CTOR? VAL RETVAL ISUNDEFINED
      return false;
    }

    if (!ie.emitThenElse()) {
      //          [stack] CTOR? OBJ CTOR? VAL RETVAL
      return false;
    }
    // Pop the undefined RETVAL from the stack, leaving the original value in
    // place.
    if (!bce_->emitPopN(1)) {
      //          [stack] CTOR? OBJ CTOR? VAL
      return false;
    }

    if (!ie.emitElseIf(mozilla::Nothing())) {
      return false;
    }
    //         5.l.i. If IsCallable(newValue) is true, then
    if (!bce_->emitAtomOp(JSOp::GetIntrinsic,
                          TaggedParserAtomIndex::WellKnown::IsCallable())) {
      //            [stack] RETVAL ISCALLABLE
      return false;
    }
    if (!bce_->emit1(JSOp::Undefined)) {
      //            [stack] RETVAL ISCALLABLE UNDEFINED
      return false;
    }
    if (!bce_->emitDupAt(2)) {
      //            [stack] RETVAL ISCALLABLE UNDEFINED RETVAL
      return false;
    }
    if (!bce_->emitCall(JSOp::Call, 1)) {
      //              [stack] RETVAL ISCALLABLE_RESULT
      return false;
    }

    if (!ie.emitThenElse()) {
      //          [stack] CTOR? OBJ CTOR? VAL RETVAL
      return false;
    }
    //             5.l.i.1. Perform MakeMethod(newValue, homeObject).
    // MakeMethod occurs in the caller, here we just drop the original method
    // which was an argument to the decorator, and leave the new method
    // returned by the decorator on the stack.
    if (!bce_->emit1(JSOp::Swap)) {
      //          [stack] CTOR? OBJ CTOR? RETVAL VAL
      return false;
    }
    if (!bce_->emitPopN(1)) {
      //          [stack] CTOR? OBJ CTOR? RETVAL
      return false;
    }
    //         5.l.ii. Else if newValue is not undefined, throw a TypeError
    //         exception.
    if (!ie.emitElse()) {
      return false;
    }
    // Pop RETVAL from the stack
    if (!bce_->emitPopN(1)) {
      //          [stack] CTOR? OBJ CTOR? VAL
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

bool DecoratorEmitter::emitDecorationState() {
  // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1800724.
  return true;
}

bool DecoratorEmitter::emitUpdateDecorationState() {
  // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1800724.
  return true;
}

bool DecoratorEmitter::emitCreateDecoratorAccessObject() {
  // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1800725.
  ObjectEmitter oe(bce_);
  if (!oe.emitObject(0)) {
    return false;
  }
  return oe.emitEnd();
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
  MOZ_ASSERT(key->is<NameNode>());

  // 1. Let contextObj be OrdinaryObjectCreate(%Object.prototype%).
  ObjectEmitter oe(bce_);
  if (!oe.emitObject(/* propertyCount */ 6)) {
    //          [stack] context
    return false;
  }
  if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
    return false;
  }

  if (kind == Kind::Method) {
    // 2. If kind is method, let kindStr be "method".
    if (!bce_->emitStringOp(
            JSOp::String,
            frontend::TaggedParserAtomIndex::WellKnown::method())) {
      //          [stack] context "method"
      return false;
    }
  } else {
    // clang-format off
    // 3. Else if kind is getter, let kindStr be "getter".
    // 4. Else if kind is setter, let kindStr be "setter".
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793960.
    // 5. Else if kind is accessor, let kindStr be "accessor".
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793961.
    // 6. Else if kind is field, let kindStr be "field".
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793962.
    // 7. Else,
    //     a. Assert: kind is class.
    //     b. Let kindStr be "class".
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793963.
    // clang-format on
    return false;
  }

  // 8. Perform ! CreateDataPropertyOrThrow(contextObj, "kind", kindStr).
  if (!oe.emitInit(frontend::AccessorType::None,
                   frontend::TaggedParserAtomIndex::WellKnown::kind())) {
    //          [stack] context
    return false;
  }
  // 9. If kind is not class, then
  if (kind != Kind::Class) {
    //     9.a. Perform ! CreateDataPropertyOrThrow(contextObj, "access",
    //     CreateDecoratorAccessObject(kind, name)).
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
    //     9.b. If isStatic is present, perform
    //     ! CreateDataPropertyOrThrow(contextObj, "static", isStatic).
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
    //     9.c. If name is a Private Name, then
    //         9.c.i. Perform ! CreateDataPropertyOrThrow(contextObj, "private",
    //         true).
    //     9.d. Else,
    //         9.d.i. Perform ! CreateDataPropertyOrThrow(contextObj, "private",
    //         false).
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
    //         9.c.ii. Perform ! CreateDataPropertyOrThrow(contextObj, "name",
    //         name.[[Description]]).
    //
    //         9.d.ii. Perform
    //         ! CreateDataPropertyOrThrow(contextObj, "name", name).)
    if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
      return false;
    }
    if (!bce_->emitStringOp(JSOp::String, key->as<NameNode>().atom())) {
      return false;
    }
    if (!oe.emitInit(frontend::AccessorType::None,
                     frontend::TaggedParserAtomIndex::WellKnown::name())) {
      //          [stack] context
      return false;
    }
  } else {
    // 10. Else,
    //     10.a. Perform ! CreateDataPropertyOrThrow(contextObj, "name", name).
    // TODO: See https://bugzilla.mozilla.org/show_bug.cgi?id=1793963
    return false;
  }
  // 11. Let addInitializer be CreateAddInitializerFunction(initializers,
  // decorationState).
  if (!oe.prepareForPropValue(pos.begin, PropertyEmitter::Kind::Prototype)) {
    return false;
  }
  if (!emitCreateAddInitializerFunction()) {
    //          [stack] context addInitializer
    return false;
  }
  // 12. Perform ! CreateDataPropertyOrThrow(contextObj, "addInitializer",
  // addInitializer).
  if (!oe.emitInit(
          frontend::AccessorType::None,
          frontend::TaggedParserAtomIndex::WellKnown::addInitializer())) {
    //          [stack] context
    return false;
  }
  // 13. Return contextObj.
  return oe.emitEnd();
}
