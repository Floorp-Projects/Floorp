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

#include "jstypes.h"
#include "jsutil.h"

struct JSFunction;
struct JSScript;

namespace js {
namespace ion {

// The layout of an Ion frame on the C stack:
//   argN     _ 
//   ...       \ - These are jsvals
//   arg0      /
//   this    _/
//   calleeToken - Encodes script or JSFunction
//   descriptor  - Size of the parent frame 
//   returnAddr - Return address, entering into the next call.
//   .. locals ..

// Layout of the frame prefix. This assumes the stack architecture grows down.
// If this is ever not the case, we'll have to refactor.
struct IonFrameData
{
    void *returnAddress_;
    uintptr_t sizeDescriptor_;
    void *calleeToken_;
};

class IonFramePrefix : protected IonFrameData
{
  public:
    // True if this is the frame passed into EnterIonCode.
    bool isEntryFrame() const {
        return !(sizeDescriptor_ & 1);
    }
    // The depth of the parent frame.
    size_t prevFrameDepth() const {
        JS_ASSERT(!isEntryFrame());
        return sizeDescriptor_ >> 1;
    }
    IonFramePrefix *prev() const {
        JS_ASSERT(!isEntryFrame());
        return (IonFramePrefix *)((uint8 *)this - prevFrameDepth());
    }
    void *calleeToken() const {
        return calleeToken_;
    }
    void setReturnAddress(void *address) {
        returnAddress_ = address;
    }
};

static const uint32 ION_FRAME_PREFIX_SIZE = sizeof(IonFramePrefix);

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

typedef void * CalleeToken;

static inline CalleeToken
CalleeToToken(JSObject *fun)
{
    return (CalleeToken *)fun;
}
static inline CalleeToken
CalleeToToken(JSScript *script)
{
    return (CalleeToken *)(uintptr_t(script) | 1);
}
static inline bool
IsCalleeTokenFunction(CalleeToken token)
{
    return (uintptr_t(token) & 1) == 0;
}
static inline JSObject *
CalleeTokenToFunction(CalleeToken token)
{
    JS_ASSERT(IsCalleeTokenFunction(token));
    return (JSObject *)token;
}
static inline JSScript *
CalleeTokenToScript(CalleeToken token)
{
    JS_ASSERT(!IsCalleeTokenFunction(token));
    return (JSScript*)(uintptr_t(token) & ~uintptr_t(1));
}

}
}

#endif // jsion_frames_h__

