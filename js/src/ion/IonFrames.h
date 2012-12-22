/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_frames_h__
#define jsion_frames_h__

#include "mozilla/DebugOnly.h"

#include "jsfun.h"
#include "jstypes.h"
#include "jsutil.h"
#include "Registers.h"
#include "IonCode.h"
#include "IonFrameIterator.h"

class JSFunction;
class JSScript;

namespace js {
namespace ion {

typedef void * CalleeToken;

enum CalleeTokenTag
{
    CalleeToken_Function = 0x0, // untagged
    CalleeToken_Script = 0x1
};

static inline CalleeTokenTag
GetCalleeTokenTag(CalleeToken token)
{
    CalleeTokenTag tag = CalleeTokenTag(uintptr_t(token) & 0x3);
    JS_ASSERT(tag <= CalleeToken_Script);
    return tag;
}
static inline CalleeToken
CalleeToToken(JSFunction *fun)
{
    return CalleeToken(uintptr_t(fun) | uintptr_t(CalleeToken_Function));
}
static inline CalleeToken
CalleeToToken(RawScript script)
{
    return CalleeToken(uintptr_t(script) | uintptr_t(CalleeToken_Script));
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
static inline UnrootedScript
CalleeTokenToScript(CalleeToken token)
{
    JS_ASSERT(GetCalleeTokenTag(token) == CalleeToken_Script);
    return (RawScript)(uintptr_t(token) & ~uintptr_t(0x3));
}

static inline UnrootedScript
ScriptFromCalleeToken(CalleeToken token)
{
    AutoAssertNoGC nogc;
    switch (GetCalleeTokenTag(token)) {
      case CalleeToken_Script:
        return CalleeTokenToScript(token);
      case CalleeToken_Function:
        return CalleeTokenToFunction(token)->nonLazyScript();
    }
    JS_NOT_REACHED("invalid callee token tag");
    return UnrootedScript(NULL);
}

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

class LSafepoint;

// Two-tuple that lets you look up the safepoint entry given the
// displacement of a call instruction within the JIT code.
class SafepointIndex
{
    // The displacement is the distance from the first byte of the JIT'd code
    // to the return address (of the call that the safepoint was generated for).
    uint32_t displacement_;

    union {
        LSafepoint *safepoint_;

        // Offset to the start of the encoded safepoint in the safepoint stream.
        uint32_t safepointOffset_;
    };

    mozilla::DebugOnly<bool> resolved;

  public:
    SafepointIndex(uint32_t displacement, LSafepoint *safepoint)
      : displacement_(displacement),
        safepoint_(safepoint),
        resolved(false)
    { }

    void resolve();

    LSafepoint *safepoint() {
        JS_ASSERT(!resolved);
        return safepoint_;
    }
    uint32_t displacement() const {
        return displacement_;
    }
    uint32_t safepointOffset() const {
        return safepointOffset_;
    }
    void adjustDisplacement(uint32_t offset) {
        JS_ASSERT(offset >= displacement_);
        displacement_ = offset;
    }
    inline SnapshotOffset snapshotOffset() const;
    inline bool hasSnapshotOffset() const;
};

class MacroAssembler;
// The OSI point is patched to a call instruction. Therefore, the
// returnPoint for an OSI call is the address immediately following that
// call instruction. The displacement of that point within the assembly
// buffer is the |returnPointDisplacement|.
class OsiIndex
{
    uint32_t callPointDisplacement_;
    uint32_t snapshotOffset_;

  public:
    OsiIndex(uint32_t callPointDisplacement, uint32_t snapshotOffset)
      : callPointDisplacement_(callPointDisplacement),
        snapshotOffset_(snapshotOffset)
    { }

    uint32_t returnPointDisplacement() const;
    uint32_t callPointDisplacement() const {
        return callPointDisplacement_;
    }
    uint32_t snapshotOffset() const {
        return snapshotOffset_;
    }
    void fixUpOffset(MacroAssembler &masm);
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

// The descriptor is organized into three sections:
// [ frame size | constructing bit | frame type ]
// < highest - - - - - - - - - - - - - - lowest >
static const uintptr_t FRAMESIZE_SHIFT = 3;
static const uintptr_t FRAMETYPE_BITS = 3;
static const uintptr_t FRAMETYPE_MASK = (1 << FRAMETYPE_BITS) - 1;

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
static const uint32_t NO_FRAME_SIZE_CLASS_ID = uint32_t(-1);

class FrameSizeClass
{
    uint32_t class_;

    explicit FrameSizeClass(uint32_t class_) : class_(class_)
    { }
  
  public:
    FrameSizeClass()
    { }

    static FrameSizeClass None() {
        return FrameSizeClass(NO_FRAME_SIZE_CLASS_ID);
    }
    static FrameSizeClass FromClass(uint32_t class_) {
        return FrameSizeClass(class_);
    }

    // These functions are implemented in specific CodeGenerator-* files.
    static FrameSizeClass FromDepth(uint32_t frameDepth);
    static FrameSizeClass ClassLimit();
    uint32_t frameSize() const;

    uint32_t classId() const {
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

// Data needed to recover from an exception.
struct ResumeFromException
{
    void *stackPointer;
};

void HandleException(ResumeFromException *rfe);

void MarkIonActivations(JSRuntime *rt, JSTracer *trc);
void MarkIonCompilerRoots(JSTracer *trc);

static inline uint32_t
MakeFrameDescriptor(uint32_t frameSize, FrameType type)
{
    return (frameSize << FRAMESIZE_SHIFT) | type;
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

UnrootedScript
GetTopIonJSScript(JSContext *cx,
                  const SafepointIndex **safepointIndexOut = NULL,
                  void **returnAddrOut = NULL);

void
GetPcScript(JSContext *cx, MutableHandleScript scriptRes, jsbytecode **pcRes);

// Given a slot index, returns the offset, in bytes, of that slot from an
// IonJSFrameLayout. Slot distances are uniform across architectures, however,
// the distance does depend on the size of the frame header.
static inline int32_t
OffsetOfFrameSlot(int32_t slot)
{
    if (slot <= 0)
        return -slot;
    return -(slot * STACK_SLOT_SIZE);
}

static inline uintptr_t
ReadFrameSlot(IonJSFrameLayout *fp, int32_t slot)
{
    return *(uintptr_t *)((char *)fp + OffsetOfFrameSlot(slot));
}

static inline double
ReadFrameDoubleSlot(IonJSFrameLayout *fp, int32_t slot)
{
    return *(double *)((char *)fp + OffsetOfFrameSlot(slot));
}

} /* namespace ion */
} /* namespace js */

#endif // jsion_frames_h__

