/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
    _(Compare)                                                              \
    _(Phi)                                                                  \
    _(OsrValue)                                                             \
    _(OsrScopeChain)                                                        \
    _(CheckOverRecursed)                                                    \
    _(RecompileCheck)                                                       \
    _(DefVar)                                                               \
    _(CreateThis)                                                           \
    _(PrepareCall)                                                          \
    _(PassArg)                                                              \
    _(Call)                                                                 \
    _(BitNot)                                                               \
    _(TypeOf)                                                               \
    _(ToId)                                                                 \
    _(BitAnd)                                                               \
    _(BitOr)                                                                \
    _(BitXor)                                                               \
    _(Lsh)                                                                  \
    _(Rsh)                                                                  \
    _(Ursh)                                                                 \
    _(Abs)                                                                  \
    _(Sqrt)                                                                 \
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
    _(ToDouble)                                                             \
    _(ToInt32)                                                              \
    _(TruncateToInt32)                                                      \
    _(ToString)                                                             \
    _(NewArray)                                                             \
    _(NewObject)                                                            \
    _(InitProp)                                                             \
    _(Start)                                                                \
    _(OsrEntry)                                                             \
    _(RegExp)                                                               \
    _(Lambda)                                                               \
    _(ImplicitThis)                                                         \
    _(Slots)                                                                \
    _(Elements)                                                             \
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
    _(LoadElement)                                                          \
    _(LoadElementHole)                                                      \
    _(StoreElement)                                                         \
    _(StoreElementHole)                                                     \
    _(ArrayPopShift)                                                        \
    _(ArrayPush)                                                            \
    _(LoadTypedArrayElement)                                                \
    _(LoadTypedArrayElementHole)                                            \
    _(StoreTypedArrayElement)                                               \
    _(ClampToUint8)                                                         \
    _(LoadFixedSlot)                                                        \
    _(StoreFixedSlot)                                                       \
    _(CallGetProperty)                                                      \
    _(CallGetName)                                                          \
    _(CallGetNameTypeOf)                                                    \
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
    _(Floor)                                                                \
    _(Round)                                                                \
    _(InterruptCheck)

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

