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

#ifndef jsion_ionframes_arm_h__
#define jsion_ionframes_arm_h__

namespace js {
namespace ion {

class IonFramePrefix;
// Layout of the frame prefix. This assumes the stack architecture grows down.
// If this is ever not the case, we'll have to refactor.
class IonCommonFrameLayout
{
    void *returnAddress_;
    void *padding;
    uintptr_t descriptor_;
  public:
    static size_t offsetOfDescriptor() {
        return offsetof(IonCommonFrameLayout, descriptor_);
    }
    static size_t offsetOfReturnAddress() {
        return offsetof(IonCommonFrameLayout, returnAddress_);
    }
    FrameType prevType() const {
        return FrameType(descriptor_ & ((1 << FRAMETYPE_BITS) - 1));
    }
    size_t prevFrameLocalSize() const {
        return descriptor_ >> FRAMETYPE_BITS;
    }
    void setFrameDescriptor(size_t size, FrameType type) {
        descriptor_ = (size << FRAMETYPE_BITS) | type;
    }
    uint8 *returnAddress() const {
        JS_NOT_REACHED("ReturnAddress NYI");
        return NULL;
    }
    uint8 **returnAddressPtr() {
        JS_NOT_REACHED("NYI");
        return NULL;
    }
};
// this is the layout of the frame that is used when we enter Ion code from EABI code
class IonEntryFrameLayout : public IonCommonFrameLayout
{
};

class IonJSFrameLayout : public IonEntryFrameLayout
{
  protected:
    void *calleeToken_;
  public:
    void *calleeToken() const {
        return calleeToken_;
    }
    void replaceCalleeToken(void *value) {
        calleeToken_ = value;
    }
    void setInvalidationRecord(InvalidationRecord *record) {
        replaceCalleeToken(InvalidationRecordToToken(record));
    }

    static size_t offsetOfCalleeToken() {
        return offsetof(IonJSFrameLayout, calleeToken_);
    }

};
typedef IonJSFrameLayout IonFrameData;

class IonRectifierFrameLayout : public IonJSFrameLayout
{
};

// this is the frame layout when we are exiting ion code, and about to enter EABI code
class IonExitFrameLayout : public IonCommonFrameLayout
{
};

} // ion
} // js
#endif // jsion_ionframes_arm_h
