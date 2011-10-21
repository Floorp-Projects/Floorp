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
 *   David Anderson <dvander@alliedmods.net>
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

#ifndef jsion_macro_assembler_h__
#define jsion_macro_assembler_h__

#if defined(JS_CPU_X86)
# include "ion/x86/MacroAssembler-x86.h"
#elif defined(JS_CPU_X64)
# include "ion/x64/MacroAssembler-x64.h"
#elif defined(JS_CPU_ARM)
# include "ion/arm/MacroAssembler-arm.h"
#endif
#include "MoveResolver.h"

namespace js {
namespace ion {

class MacroAssembler : public MacroAssemblerSpecific
{
    typedef MoveResolver::MoveOperand MoveOperand;
    typedef MoveResolver::Move Move;

    MacroAssembler *thisFromCtor() {
        return this;
    }

  public:
    class AutoRooter : public AutoGCRooter
    {
        MacroAssembler *masm_;

      public:
        AutoRooter(JSContext *cx, MacroAssembler *masm)
          : AutoGCRooter(cx, IONMASM),
            masm_(masm)
        {
        }

        MacroAssembler *masm() const {
            return masm_;
        }
    };

    AutoRooter autoRooter_;
    MoveResolver moveResolver_;

    // Number of bytes the stack is adjusted inside a call to C. Calls to C may
    // not be nested.
    uint32 stackAdjust_;
    bool dynamicAlignment_;
    bool inCall_;

    // Size of the returned value.
    enum CallProperty {
        LargeReturnValue = 1 << 0,
        ReturnArgConsumeStack = 1 << 1,
        StackAllocated = 1 << 2,
        HasGetRes = 1 << 3,
        None = 0
    };

    // CallProperty flags.
    uint32 callProperties_;

    bool enoughMemory_;

    // Compute space needed for the function call and set the properties of the
    // callee.  It returns the space which has to be allocated for calling the
    // function.
    //
    // arg            Number of arguments of the function.
    // returnSize     Size in bytes of the result.
    // returnOperand  MoveOperand of the result allocated space. (only when
    //                returnSize > sizeof(void *) )
    uint32 setupABICall(uint32 arg, uint32 returnSize, const MoveOperand *returnOperand);

    // This function set the argument without any consideration for its purpose.
    // This implies that 0th argument could either be a pointer to the returned
    // value of the callee or the first argument of the callee.  Users should
    // use setABIArg which take care of this.
    void setAnyABIArg(uint32 arg, const MoveOperand &from);

  public:
    MacroAssembler()
      : autoRooter_(GetIonContext()->cx, thisFromCtor()),
        stackAdjust_(0),
        inCall_(false),
        callProperties_(None),
        enoughMemory_(true)
    {
    }

    MacroAssembler(JSContext *cx)
      : autoRooter_(cx, thisFromCtor()),
        stackAdjust_(0),
        inCall_(false),
        callProperties_(None),
        enoughMemory_(true)
    {
    }

    MoveResolver &moveResolver() {
        return moveResolver_;
    }

    size_t instructionsSize() const {
        return size();
    }

    bool oom() const {
        return MacroAssemblerSpecific::oom() || !enoughMemory_;
    }


    // Setup a call to C/C++ code, given the number of general arguments it
    // takes. Note that this only supports cdecl.
    //
    // In order for alignment to work correctly, the MacroAssembler must have a
    // consistent view of the stack displacement. It is okay to call "push"
    // manually, however, if the stack alignment were to change, the macro
    // assembler should be notified before starting a call.
    void setupAlignedABICall(uint32 args, uint32 returnSize = sizeof(void *),
                             const MoveOperand *returnOperand = NULL);

    // Sets up an ABI call for when the alignment is not known. This may need a
    // scratch register.
    void setupUnalignedABICall(uint32 args, const Register &scratch,
                               uint32 returnSize = sizeof(void *),
                               const MoveOperand *returnOperand = NULL);

    // Arguments can be assigned to a C/C++ call in any order. They are moved
    // in parallel immediately before performing the call. This process may
    // temporarily use more stack, in which case esp-relative addresses will be
    // automatically adjusted. It is extremely important that esp-relative
    // addresses are computed *after* setupABICall().
    inline void setABIArg(uint32 arg, const MoveOperand &from) {
        arg += callProperties_ & LargeReturnValue ? 1 : 0;
        setAnyABIArg(arg, from);
    }

    inline void setABIArg(uint32 arg, const Register &reg) {
        setABIArg(arg, MoveOperand(reg));
    }

    // Copy returned value from the memory in which it has been stored
    // temporary.  They are moved in parallel immediately before unwinding the
    // stack. This process may temporarily use more stack, in which case
    // esp-relative addresses will be automatically adjusted.
    void getABIRes(uint32 offset, const MoveOperand &to);


    // Emits a call to a C/C++ function, resolving all argument moves.
    void callWithABI(void *fun);

    // Free space reserved for the return value and for the arguments.
    //
    // This function restore the stack as it was before setupABICall.  This
    // function has to be called after callWithABI.  The return value must be
    // saved before this call.
    void finishABICall();

    // Emits a test of a value against all types in a TypeSet. A scratch
    // register is required.
    template <typename T>
    void guardTypeSet(const T &address, types::TypeSet *types, Register scratch,
                      Label *mismatched);
};

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_h__

