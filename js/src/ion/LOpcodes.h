/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_lir_opcodes_common_h__
#define jsion_lir_opcodes_common_h__

#define LIR_COMMON_OPCODE_LIST(_)   \
    _(Label)                        \
    _(Nop)                          \
    _(OsiPoint)                     \
    _(MoveGroup)                    \
    _(Integer)                      \
    _(Pointer)                      \
    _(Double)                       \
    _(Value)                        \
    _(Parameter)                    \
    _(Callee)                       \
    _(TableSwitch)                  \
    _(TableSwitchV)                 \
    _(Goto)                         \
    _(NewArray)                     \
    _(NewObject)                    \
    _(NewSlots)                     \
    _(NewDeclEnvObject)             \
    _(NewCallObject)                \
    _(NewStringObject)              \
    _(InitProp)                     \
    _(CheckOverRecursed)            \
    _(RecompileCheck)               \
    _(DefVar)                       \
    _(DefFun)                       \
    _(CallKnown)                    \
    _(CallGeneric)                  \
    _(CallNative)                   \
    _(ApplyArgsGeneric)             \
    _(StackArgT)                    \
    _(StackArgV)                    \
    _(CreateThisV)                  \
    _(CreateThisO)                  \
    _(CreateThisWithTemplate)       \
    _(ReturnFromCtor)               \
    _(BitNotI)                      \
    _(BitNotV)                      \
    _(BitOpI)                       \
    _(BitOpV)                       \
    _(ShiftI)                       \
    _(UrshD)                        \
    _(Return)                       \
    _(Throw)                        \
    _(Phi)                          \
    _(TestIAndBranch)               \
    _(TestDAndBranch)               \
    _(TestVAndBranch)               \
    _(TestOAndBranch)               \
    _(PolyInlineDispatch)           \
    _(Compare)                      \
    _(CompareAndBranch)             \
    _(CompareD)                     \
    _(CompareDAndBranch)            \
    _(CompareS)                     \
    _(CompareB)                     \
    _(CompareBAndBranch)            \
    _(CompareV)                     \
    _(CompareVAndBranch)            \
    _(CompareVM)                    \
    _(IsNullOrLikeUndefined)        \
    _(IsNullOrLikeUndefinedAndBranch)\
    _(EmulatesUndefined)            \
    _(EmulatesUndefinedAndBranch)   \
    _(MinMaxI)                      \
    _(MinMaxD)                      \
    _(NegD)                         \
    _(AbsI)                         \
    _(AbsD)                         \
    _(SqrtD)                        \
    _(PowI)                         \
    _(PowD)                         \
    _(Random)                       \
    _(MathFunctionD)                \
    _(NotI)                         \
    _(NotD)                         \
    _(NotO)                         \
    _(NotV)                         \
    _(AddI)                         \
    _(SubI)                         \
    _(MulI)                         \
    _(MathD)                        \
    _(ModD)                         \
    _(BinaryV)                      \
    _(Concat)                       \
    _(CharCodeAt)                   \
    _(FromCharCode)                 \
    _(Int32ToDouble)                \
    _(ValueToDouble)                \
    _(ValueToInt32)                 \
    _(DoubleToInt32)                \
    _(TruncateDToInt32)             \
    _(IntToString)                  \
    _(Start)                        \
    _(OsrEntry)                     \
    _(OsrValue)                     \
    _(OsrScopeChain)                \
    _(RegExp)                       \
    _(RegExpTest)                   \
    _(Lambda)                       \
    _(LambdaForSingleton)           \
    _(ImplicitThis)                 \
    _(Slots)                        \
    _(Elements)                     \
    _(LoadSlotV)                    \
    _(LoadSlotT)                    \
    _(StoreSlotV)                   \
    _(StoreSlotT)                   \
    _(GuardShape)                   \
    _(GuardClass)                   \
    _(TypeBarrier)                  \
    _(MonitorTypes)                 \
    _(InitializedLength)            \
    _(SetInitializedLength)         \
    _(BoundsCheck)                  \
    _(BoundsCheckRange)             \
    _(BoundsCheckLower)             \
    _(LoadElementV)                 \
    _(LoadElementT)                 \
    _(LoadElementHole)              \
    _(StoreElementV)                \
    _(StoreElementT)                \
    _(ArrayPopShiftV)               \
    _(ArrayPopShiftT)               \
    _(ArrayPushV)                   \
    _(ArrayPushT)                   \
    _(ArrayConcat)                  \
    _(StoreElementHoleV)            \
    _(StoreElementHoleT)            \
    _(LoadTypedArrayElement)        \
    _(LoadTypedArrayElementHole)    \
    _(StoreTypedArrayElement)       \
    _(ClampIToUint8)                \
    _(ClampDToUint8)                \
    _(ClampVToUint8)                \
    _(LoadFixedSlotV)               \
    _(LoadFixedSlotT)               \
    _(StoreFixedSlotV)              \
    _(StoreFixedSlotT)              \
    _(FunctionEnvironment)          \
    _(GetPropertyCacheV)            \
    _(GetPropertyCacheT)            \
    _(GetElementCacheV)             \
    _(BindNameCache)                \
    _(CallGetProperty)              \
    _(GetNameCache)                 \
    _(CallGetIntrinsicValue)        \
    _(CallGetElement)               \
    _(CallSetElement)               \
    _(CallSetProperty)              \
    _(CallDeleteProperty)           \
    _(SetPropertyCacheV)            \
    _(SetPropertyCacheT)            \
    _(CallIteratorStart)            \
    _(IteratorStart)                \
    _(IteratorNext)                 \
    _(IteratorMore)                 \
    _(IteratorEnd)                  \
    _(ArrayLength)                  \
    _(TypedArrayLength)             \
    _(TypedArrayElements)           \
    _(StringLength)                 \
    _(ArgumentsLength)              \
    _(GetArgument)                  \
    _(TypeOfV)                      \
    _(ToIdV)                        \
    _(Floor)                        \
    _(Round)                        \
    _(In)                           \
    _(InArray)                      \
    _(InstanceOfO)                  \
    _(InstanceOfV)                  \
    _(CallInstanceOf)               \
    _(InterruptCheck)               \
    _(FunctionBoundary)             \
    _(GetDOMProperty)               \
    _(SetDOMProperty)               \
    _(CallDOMNative)

#if defined(JS_CPU_X86)
# include "x86/LOpcodes-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/LOpcodes-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/LOpcodes-arm.h"
#endif

#define LIR_OPCODE_LIST(_)          \
    LIR_COMMON_OPCODE_LIST(_)       \
    LIR_CPU_OPCODE_LIST(_)

#endif // jsion_lir_opcodes_common_h__

