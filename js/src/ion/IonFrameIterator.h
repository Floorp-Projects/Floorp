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

#ifndef jsion_frame_iterator_h__
#define jsion_frame_iterator_h__

#include "jstypes.h"
#include "IonCode.h"
#include "SnapshotReader.h"

struct JSFunction;
struct JSScript;

namespace js {
namespace ion {

enum FrameType
{
    IonFrame_JS,
    IonFrame_Entry,
    IonFrame_Rectifier,
    IonFrame_Bailed_Rectifier,
    IonFrame_Exit
};

class IonCommonFrameLayout;
class IonActivation;
class IonJSFrameLayout;

class IonFrameIterator
{
    uint8 *current_;
    FrameType type_;
    uint8 *returnAddressToFp_;
    size_t frameSize_;
    mutable const SafepointIndex *cachedSafepointIndex_;

  public:
    IonFrameIterator(uint8 *top)
      : current_(top),
        type_(IonFrame_Exit),
        returnAddressToFp_(NULL),
        frameSize_(0),
        cachedSafepointIndex_(NULL)
    { }

    IonFrameIterator(IonJSFrameLayout *fp);

    // Current frame information.
    FrameType type() const {
        return type_;
    }
    uint8 *fp() const {
        return current_;
    }

    inline IonCommonFrameLayout *current() const;
    inline uint8 *returnAddress() const;

    IonJSFrameLayout *jsFrame() const {
        JS_ASSERT(type() == IonFrame_JS);
        return (IonJSFrameLayout *) fp();
    }

    // Returns whether the JS frame has been invalidated and, if so,
    // places the invalidated Ion script in |ionScript|.
    bool checkInvalidation(IonScript **ionScript) const;
    bool checkInvalidation() const;

    bool isScripted() const {
        return type_ == IonFrame_JS;
    }
    bool isEntry() const {
        return type_ == IonFrame_Entry;
    }
    bool isFunctionFrame() const;

    void *calleeToken() const;
    JSFunction *callee() const;
    JSFunction *maybeCallee() const;
    JSScript *script() const;

    // Returns the return address of the frame above this one (that is, the
    // return address that returns back to the current frame).
    uint8 *returnAddressToFp() const {
        return returnAddressToFp_;
    }

    // Previous frame information extracted from the current frame.
    inline size_t prevFrameLocalSize() const;
    inline FrameType prevType() const;
    uint8 *prevFp() const;

    // Returns the stack space used by the current frame, in bytes. This does
    // not include the size of its fixed header.
    inline size_t frameSize() const;

    // Functions used to iterate on frames. When prevType is IonFrame_Entry,
    // the current frame is the last frame.
    inline bool more() const {
        return type_ != IonFrame_Entry;
    }
    IonFrameIterator &operator++();

    // Returns the IonScript associated with this JS frame.
    IonScript *ionScript() const;

    // Returns the Safepoint associated with this JS frame. Incurs a lookup
    // overhead.
    const SafepointIndex *safepoint() const;

    // Returns the OSI index associated with this JS frame. Incurs a lookup
    // overhead.
    const OsiIndex *osiIndex() const;

    MachineState machineState() const;
};

class IonActivationIterator
{
    uint8 *top_;
    IonActivation *activation_;

  public:
    IonActivationIterator(JSContext *cx);
    IonActivationIterator(JSRuntime *rt);

    IonActivationIterator &operator++();

    IonActivation *activation() {
        return activation_;
    }
    uint8 *top() const {
        return top_;
    }
    bool more() const;
};

class FrameRecovery;
class IonJSFrameLayout;

// Reads frame information in snapshot-encoding order (that is, outermost frame
// to innermost frame).
class SnapshotIterator : public SnapshotReader
{
    IonJSFrameLayout *fp_;
    MachineState machine_;
    IonScript *ionScript_;

  private:
    uintptr_t fromLocation(const SnapshotReader::Location &loc);
    static Value FromTypedPayload(JSValueType type, uintptr_t payload);

    Value slotValue(const Slot &slot);

  public:
    SnapshotIterator(IonScript *ionScript, SnapshotOffset snapshotOffset,
                     IonJSFrameLayout *fp, const MachineState &machine);
    SnapshotIterator(const IonFrameIterator &iter);
    SnapshotIterator();

    Value read() {
        return slotValue(readSlot());
    }
};

// Reads frame information in callstack order (that is, innermost frame to
// outermost frame).
class InlineFrameIterator
{
    const IonFrameIterator *frame_;
    SnapshotIterator start_;
    SnapshotIterator si_;
    unsigned framesRead_;
    HeapPtr<JSFunction> callee_;
    HeapPtr<JSScript> script_;
    jsbytecode *pc_;

  private:
    void findNextFrame();

  public:
    InlineFrameIterator(const IonFrameIterator *iter);

    bool more() const {
        return frame_ && framesRead_ < start_.frameCount();
    }
    JSFunction *callee() const {
        JS_ASSERT(callee_);
        return callee_;
    }
    JSFunction *maybeCallee() const {
        return callee_;
    }
    JSScript *script() const {
        return script_;
    }
    jsbytecode *pc() const {
        return pc_;
    }
    SnapshotIterator snapshotIterator() const {
        return si_;
    }
    bool isFunctionFrame() const;
    InlineFrameIterator operator++();
};

} // namespace ion
} // namespace js

#endif // jsion_frames_iterator_h__

