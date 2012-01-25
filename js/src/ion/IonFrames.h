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

#ifndef jsion_frames_h__
#define jsion_frames_h__

#include "jsfun.h"
#include "jstypes.h"
#include "jsutil.h"
#include "IonRegisters.h"
#include "IonCode.h"
#include "IonFrameIterator.h"

struct JSFunction;
struct JSScript;

namespace js {
namespace ion {

typedef void * CalleeToken;

enum CalleeTokenTag
{
    CalleeToken_Function = 0x0, // untagged
    CalleeToken_Script = 0x1,
    CalleeToken_InvalidationRecord = 0x2
};

static inline CalleeTokenTag
GetCalleeTokenTag(CalleeToken token)
{
    CalleeTokenTag tag = CalleeTokenTag(uintptr_t(token) & 0x3);
    JS_ASSERT(tag != 0x3);
    return tag;
}
static inline bool
CalleeTokenIsInvalidationRecord(CalleeToken token)
{
    return GetCalleeTokenTag(token) == CalleeToken_InvalidationRecord;
}
static inline CalleeToken
CalleeToToken(JSFunction *fun)
{
    return CalleeToken(uintptr_t(fun) | uintptr_t(CalleeToken_Function));
}
static inline CalleeToken
CalleeToToken(JSScript *script)
{
    return CalleeToken(uintptr_t(script) | uintptr_t(CalleeToken_Script));
}
static inline CalleeToken
InvalidationRecordToToken(InvalidationRecord *record)
{
    return CalleeToken(uintptr_t(record) | uintptr_t(CalleeToken_InvalidationRecord));
}
static inline bool
CalleeTokenIsFunction(CalleeToken token)
{
    return GetCalleeTokenTag(token) == CalleeToken_Function;
}
static inline JSFunction *
CalleeTokenToFunction(CalleeToken token)
{
    JS_ASSERT(CalleeTokenIsFunction(token));
    return (JSFunction *)token;
}
static inline JSScript *
CalleeTokenToScript(CalleeToken token)
{
    JS_ASSERT(GetCalleeTokenTag(token) == CalleeToken_Script);
    return (JSScript *)(uintptr_t(token) & ~uintptr_t(0x3));
}
static inline InvalidationRecord *
CalleeTokenToInvalidationRecord(CalleeToken token)
{
    JS_ASSERT(GetCalleeTokenTag(token) == CalleeToken_InvalidationRecord);
    return (InvalidationRecord *)(uintptr_t(token) & ~uintptr_t(0x3));
}
JSScript *
MaybeScriptFromCalleeToken(CalleeToken token);

// In between every two frames lies a small header describing both frames. This
// header, minimally, contains a returnAddress word and a descriptor word. The
// descriptor describes the size and type of the previous frame, whereas the
// returnAddress describes the address the newer frame (the callee) will return
// to. The exact mechanism in which frames are laid out is architecture
// dependent.
//
// Two special frame types exist. Entry frames begin an ion activation, and
// therefore there is exactly one per activation of ion::Cannon. Exit frames
// are necessary to leave JIT code and enter C++, and thus, C++ code will
// always begin iterating from the topmost exit frame.

// IonFrameInfo are stored separately from the stack frame.  It can be composed
// of any field which are computed at compile time only.  It is recovered by
// using the calleeToken and the returnAddress of the stack frame.
class IonFrameInfo
{
  private:
    // The displacement is the distance from the first byte of the JIT'd code
    // to the return address.
    uint32 displacement_;

    // Offset to entry in the safepoint stream.
    uint32 safepointOffset_;

    // If this instruction is effectful, a snapshot will be present to restore
    // the interpreter state in the case of a deep bailout.
    SnapshotOffset snapshotOffset_;

  public:
    IonFrameInfo(uint32 displacement, uint32 safepointOffset, SnapshotOffset snapshotOffset)
      : displacement_(displacement),
        safepointOffset_(safepointOffset),
        snapshotOffset_(snapshotOffset)
    { }

    uint32 displacement() const {
        return displacement_;
    }
    uint32 safepointOffset() const {
        return safepointOffset_;
    }
    inline SnapshotOffset snapshotOffset() const;
    inline bool hasSnapshotOffset() const;
};

struct InvalidationRecord
{
    void *calleeToken;
    uint8 *returnAddress;
    IonScript *ionScript;

    InvalidationRecord(void *calleeToken, uint8 *returnAddress);

    static InvalidationRecord *New(void *calleeToken, uint8 *returnAddress);
    static void Destroy(JSContext *cx, InvalidationRecord *record);
};

// The layout of an Ion frame on the C stack is roughly:
//      argN     _
//      ...       \ - These are jsvals
//      arg0      /
//   -3 this    _/
//   -2 callee
//   -1 descriptor
//    0 returnAddress
//   .. locals ..

static const uint32 FRAMETYPE_BITS = 3;

// Ion frames have a few important numbers associated with them:
//      Local depth:    The number of bytes required to spill local variables.
//      Argument depth: The number of bytes required to push arguments and make
//                      a function call.
//      Slack:          A frame may temporarily use extra stack to resolve cycles.
//
// The (local + argument) depth determines the "fixed frame size". The fixed
// frame size is the distance between the stack pointer and the frame header.
// Thus, fixed >= (local + argument).
//
// In order to compress guards, we create shared jump tables that recover the
// script from the stack and recover a snapshot pointer based on which jump was
// taken. Thus, we create a jump table for each fixed frame size.
//
// Jump tables are big. To control the amount of jump tables we generate, each
// platform chooses how to segregate stack size classes based on its
// architecture.
//
// On some architectures, these jump tables are not used at all, or frame
// size segregation is not needed. Thus, there is an option for a frame to not
// have any frame size class, and to be totally dynamic.
static const uint32 NO_FRAME_SIZE_CLASS_ID = uint32(-1);

class FrameSizeClass
{
    uint32 class_;

    explicit FrameSizeClass(uint32 class_) : class_(class_)
    { }
  
  public:
    FrameSizeClass()
    { }

    static FrameSizeClass None() {
        return FrameSizeClass(NO_FRAME_SIZE_CLASS_ID);
    }
    static FrameSizeClass FromClass(uint32 class_) {
        return FrameSizeClass(class_);
    }

    // These two functions are implemented in specific CodeGenerator-* files.
    static FrameSizeClass FromDepth(uint32 frameDepth);
    uint32 frameSize() const;

    uint32 classId() const {
        JS_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);
        return class_;
    }

    bool operator ==(const FrameSizeClass &other) const {
        return class_ == other.class_;
    }
    bool operator !=(const FrameSizeClass &other) const {
        return class_ != other.class_;
    }
};

// Information needed to recover machine register state.
class MachineState
{
    uintptr_t *regs_;
    double *fpregs_;

  public:
    MachineState()
      : regs_(NULL), fpregs_(NULL)
    { }
    MachineState(uintptr_t *regs, double *fpregs)
      : regs_(regs), fpregs_(fpregs)
    { }

    double readFloatReg(FloatRegister reg) const {
        return fpregs_[reg.code()];
    }
    uintptr_t readReg(Register reg) const {
        return regs_[reg.code()];
    }
};

class IonJSFrameLayout;

// Information needed to recover the content of the stack frame.
class FrameRecovery
{
    IonJSFrameLayout *fp_;
    uint8 *sp_;             // fp_ + frameSize

    MachineState machine_;
    uint32 snapshotOffset_;

    JSFunction *callee_;
    JSScript *script_;

  private:
    FrameRecovery(uint8 *fp, uint8 *sp, const MachineState &machine);

    void setSnapshotOffset(uint32 snapshotOffset) {
        snapshotOffset_ = snapshotOffset;
    }
    void setBailoutId(BailoutId bailoutId);

    void unpackCalleeToken(CalleeToken token);

    static int32 OffsetOfSlot(int32 slot);

  public:
    static FrameRecovery FromBailoutId(uint8 *fp, uint8 *sp, const MachineState &machine,
                                       BailoutId bailoutId);
    static FrameRecovery FromSnapshot(uint8 *fp, uint8 *sp, const MachineState &machine,
                                      SnapshotOffset offset);
    static FrameRecovery FromFrameIterator(const IonFrameIterator& it);

    MachineState &machine() {
        return machine_;
    }
    const MachineState &machine() const {
        return machine_;
    }
    uintptr_t readSlot(int32 slot) const {
        return *(uintptr_t *)((char *)fp_ + OffsetOfSlot(slot));
    }
    double readDoubleSlot(int32 slot) const {
        return *(double *)((char *)fp_ + OffsetOfSlot(slot));
    }
    JSFunction *callee() const {
        return callee_;
    }
    JSScript *script() const {
        return script_;
    }
    IonScript *ionScript() const;
    uint32 snapshotOffset() const {
        return snapshotOffset_;
    }
    uint32 frameSize() const {
        return ((uint8 *) fp_) - sp_;
    }
    IonJSFrameLayout *fp() {
        return fp_;
    }
};

// Data needed to recover from an exception.
struct ResumeFromException
{
    void *stackPointer;
};

void HandleException(ResumeFromException *rfe);

void MarkIonActivations(JSRuntime *rt, JSTracer *trc);

static inline uint32
MakeFrameDescriptor(uint32 frameSize, FrameType type)
{
    return (frameSize << FRAMETYPE_BITS) | type;
}

} // namespace ion
} // namespace js

#if defined(JS_CPU_X86) || defined (JS_CPU_X64)
# include "ion/shared/IonFrames-x86-shared.h"
#elif defined (JS_CPU_ARM)
# include "ion/arm/IonFrames-arm.h"
#else
# error "unsupported architecture"
#endif

namespace js {
namespace ion {

inline IonCommonFrameLayout *
IonFrameIterator::current() const
{
    return (IonCommonFrameLayout *)current_;
}

inline uint8 *
IonFrameIterator::returnAddress() const
{
    IonCommonFrameLayout *current = (IonCommonFrameLayout *) current_;
    return current->returnAddress();
}

inline size_t
IonFrameIterator::prevFrameLocalSize() const
{
    IonCommonFrameLayout *current = (IonCommonFrameLayout *) current_;
    return current->prevFrameLocalSize();
}

inline FrameType
IonFrameIterator::prevType() const
{
    IonCommonFrameLayout *current = (IonCommonFrameLayout *) current_;
    return current->prevType();
}

inline bool 
IonFrameIterator::more() const
{
    return type_ != IonFrame_Entry;
}

static inline IonScript *
GetTopIonFrame(JSContext *cx)
{
    IonFrameIterator iter(cx->runtime->ionTop);
    JS_ASSERT(iter.type() == IonFrame_Exit);
    ++iter;
    JS_ASSERT(iter.type() == IonFrame_JS);
    IonJSFrameLayout *frame = static_cast<IonJSFrameLayout*>(iter.current());
    switch (GetCalleeTokenTag(frame->calleeToken())) {
      case CalleeToken_Function: {
        JSFunction *fun = CalleeTokenToFunction(frame->calleeToken());
        return fun->script()->ion;
      }
      case CalleeToken_Script:
        return CalleeTokenToScript(frame->calleeToken())->ion;
      default:
        JS_NOT_REACHED("unexpected callee token kind");
        return NULL;
    }
}

void
GetPcScript(JSContext *cx, JSScript **scriptRes, jsbytecode **pcRes);

} /* namespace ion */
} /* namespace js */

#endif // jsion_frames_h__

