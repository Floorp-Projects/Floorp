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

#include "Ion.h"
#include "IonFrames.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jsfun.h"

using namespace js;
using namespace js::ion;

FrameRecovery::FrameRecovery(uint8 *fp, uint8 *sp, const MachineState &machine)
  : fp_((IonJSFrameLayout *)fp),
    sp_(sp),
    machine_(machine)
{
    if (IsCalleeTokenFunction(fp_->calleeToken())) {
        callee_ = CalleeTokenToFunction(fp_->calleeToken());
        fun_ = callee_->getFunctionPrivate();
        script_ = fun_->script();
    } else {
        script_ = CalleeTokenToScript(fp_->calleeToken());
    }
}

void
FrameRecovery::setBailoutId(BailoutId bailoutId)
{
    snapshotOffset_ = ionScript()->bailoutToSnapshot(bailoutId);
}

FrameRecovery
FrameRecovery::FromBailoutId(uint8 *fp, uint8 *sp, const MachineState &machine,
                             BailoutId bailoutId)
{
    FrameRecovery frame(fp, sp, machine);
    frame.setBailoutId(bailoutId);
    return frame;
}

FrameRecovery
FrameRecovery::FromSnapshot(uint8 *fp, uint8 *sp, const MachineState &machine,
                            SnapshotOffset snapshotOffset)
{
    FrameRecovery frame(fp, sp, machine);
    frame.setSnapshotOffset(snapshotOffset);
    return frame;
}

IonScript *
FrameRecovery::ionScript() const
{
    return script_->ion;
}

void
IonFrameIterator::prev()
{
    JS_ASSERT(type_ != IonFrame_Entry);

    IonCommonFrameLayout *current = (IonCommonFrameLayout *)current_;

    // If the next frame is the entry frame, just exit. Don't update current_,
    // since the entry and first frames overlap.
    if (current->prevType() == IonFrame_Entry) {
        type_ = IonFrame_Entry;
        return;
    }

    size_t currentSize;
    switch (type_) {
      case IonFrame_JS:
        currentSize = sizeof(IonJSFrameLayout);
        break;
      case IonFrame_Rectifier:
        currentSize = sizeof(IonRectifierFrameLayout);
        break;
      case IonFrame_Exit:
        currentSize = sizeof(IonExitFrameLayout);
        break;
      default:
        JS_NOT_REACHED("unexpected frame type");
        return;
    }
    currentSize += current->prevFrameLocalSize();

    type_ = current->prevType();
    current_ += currentSize;
}

void
ion::HandleException(ResumeFromException *rfe)
{
    JSContext *cx = GetIonContext()->cx;

    IonFrameIterator iter(JS_THREAD_DATA(cx)->ionTop);
    while (iter.type() != IonFrame_Entry)
        iter.prev();

    rfe->stackPointer = iter.fp();
}

