/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef jsion_mir_opcodes_h__
#define jsion_mir_opcodes_h__

namespace js {
namespace ion {

#define MIR_OPCODE_LIST(_)                                                  \
    _(Constant)                                                             \
    _(Parameter)                                                            \
    _(Callee)                                                               \
    _(TableSwitch)                                                          \
    _(Goto)                                                                 \
    _(Test)                                                                 \
    _(PolyInlineDispatch)                                                   \
    _(Compare)                                                              \
    _(Phi)                                                                  \
    _(Beta)                                                                 \
    _(OsrValue)                                                             \
    _(OsrScopeChain)                                                        \
    _(ReturnFromCtor)                                                       \
    _(CheckOverRecursed)                                                    \
    _(RecompileCheck)                                                       \
    _(DefVar)                                                               \
    _(DefFun)                                                               \
    _(CreateThis)                                                           \
    _(CreateThisWithTemplate)                                               \
    _(PrepareCall)                                                          \
    _(PassArg)                                                              \
    _(Call)                                                                 \
    _(ApplyArgs)                                                            \
    _(BitNot)                                                               \
    _(TypeOf)                                                               \
    _(ToId)                                                                 \
    _(BitAnd)                                                               \
    _(BitOr)                                                                \
    _(BitXor)                                                               \
    _(Lsh)                                                                  \
    _(Rsh)                                                                  \
    _(Ursh)                                                                 \
    _(MinMax)                                                               \
    _(Abs)                                                                  \
    _(Sqrt)                                                                 \
    _(Pow)                                                                  \
    _(PowHalf)                                                              \
    _(Random)                                                               \
    _(MathFunction)                                                         \
    _(Add)                                                                  \
    _(Sub)                                                                  \
    _(Mul)                                                                  \
    _(Div)                                                                  \
    _(Mod)                                                                  \
    _(Concat)                                                               \
    _(CharCodeAt)                                                           \
    _(FromCharCode)                                                         \
    _(Return)                                                               \
    _(Throw)                                                                \
    _(Box)                                                                  \
    _(Unbox)                                                                \
    _(GuardObject)                                                          \
    _(GuardString)                                                          \
    _(ToDouble)                                                             \
    _(ToInt32)                                                              \
    _(TruncateToInt32)                                                      \
    _(ToString)                                                             \
    _(NewSlots)                                                             \
    _(NewArray)                                                             \
    _(NewObject)                                                            \
    _(NewDeclEnvObject)                                                     \
    _(NewCallObject)                                                        \
    _(NewStringObject)                                                      \
    _(InitProp)                                                             \
    _(Start)                                                                \
    _(OsrEntry)                                                             \
    _(Nop)                                                                  \
    _(RegExp)                                                               \
    _(RegExpTest)                                                           \
    _(Lambda)                                                               \
    _(ImplicitThis)                                                         \
    _(Slots)                                                                \
    _(Elements)                                                             \
    _(ConstantElements)                                                     \
    _(LoadSlot)                                                             \
    _(StoreSlot)                                                            \
    _(FunctionEnvironment)                                                  \
    _(TypeBarrier)                                                          \
    _(MonitorTypes)                                                         \
    _(GetPropertyCache)                                                     \
    _(GetElementCache)                                                      \
    _(BindNameCache)                                                        \
    _(GuardShape)                                                           \
    _(GuardClass)                                                           \
    _(ArrayLength)                                                          \
    _(TypedArrayLength)                                                     \
    _(TypedArrayElements)                                                   \
    _(InitializedLength)                                                    \
    _(SetInitializedLength)                                                 \
    _(Not)                                                                  \
    _(BoundsCheck)                                                          \
    _(BoundsCheckLower)                                                     \
    _(InArray)                                                              \
    _(LoadElement)                                                          \
    _(LoadElementHole)                                                      \
    _(StoreElement)                                                         \
    _(StoreElementHole)                                                     \
    _(ArrayPopShift)                                                        \
    _(ArrayPush)                                                            \
    _(ArrayConcat)                                                          \
    _(LoadTypedArrayElement)                                                \
    _(LoadTypedArrayElementHole)                                            \
    _(StoreTypedArrayElement)                                               \
    _(ClampToUint8)                                                         \
    _(LoadFixedSlot)                                                        \
    _(StoreFixedSlot)                                                       \
    _(CallGetProperty)                                                      \
    _(GetNameCache)                                                         \
    _(CallGetIntrinsicValue)                                                \
    _(CallGetElement)                                                       \
    _(CallSetElement)                                                       \
    _(CallSetProperty)                                                      \
    _(DeleteProperty)                                                       \
    _(SetPropertyCache)                                                     \
    _(IteratorStart)                                                        \
    _(IteratorNext)                                                         \
    _(IteratorMore)                                                         \
    _(IteratorEnd)                                                          \
    _(StringLength)                                                         \
    _(ArgumentsLength)                                                      \
    _(GetArgument)                                                          \
    _(Floor)                                                                \
    _(Round)                                                                \
    _(In)                                                                   \
    _(InstanceOf)                                                           \
    _(CallInstanceOf)                                                       \
    _(InterruptCheck)                                                       \
    _(FunctionBoundary)                                                     \
    _(GetDOMProperty)                                                       \
    _(SetDOMProperty)

// Forward declarations of MIR types.
#define FORWARD_DECLARE(op) class M##op;
 MIR_OPCODE_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

class MInstructionVisitor
{
  public:
#define VISIT_INS(op) virtual bool visit##op(M##op *) { JS_NOT_REACHED("NYI: " #op); return false; }
    MIR_OPCODE_LIST(VISIT_INS)
#undef VISIT_INS
};

} // namespace ion
} // namespace js

#endif // jsion_mir_opcodes_h__

