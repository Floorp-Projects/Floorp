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

#ifndef jsion_include_safepoints_h_
#define jsion_include_safepoints_h_

#include "Registers.h"
#include "CompactBuffer.h"
#include "BitSet.h"

#include "shared/Assembler-shared.h"

namespace js {
namespace ion {

struct SafepointNunboxEntry;

static const uint32 INVALID_SAFEPOINT_OFFSET = uint32(-1);

class SafepointWriter
{
    CompactBufferWriter stream_;
    BitSet *frameSlots_;

  public:
    bool init(uint32 localSlotCount);

    // A safepoint entry is written in the order these functions appear.
    uint32 startEntry();
    void writeOsiCallPointOffset(uint32 osiPointOffset);
    void writeGcRegs(GeneralRegisterSet actual, GeneralRegisterSet spilled);
    void writeGcSlots(uint32 nslots, uint32 *slots);
    void writeValueSlots(uint32 nslots, uint32 *slots);
    void writeNunboxParts(uint32 nentries, SafepointNunboxEntry *entries);
    void endEntry();

    size_t size() const {
        return stream_.length();
    }
    const uint8 *buffer() const {
        return stream_.buffer();
    }
};

class SafepointReader
{
    CompactBufferReader stream_;
    uint32 localSlotCount_;
    uint32 currentSlotChunk_;
    uint32 currentSlotChunkNumber_;
    uint32 osiCallPointOffset_;
    GeneralRegisterSet gcSpills_;
    GeneralRegisterSet allSpills_;

  private:
    void advanceFromGcRegs();
    void advanceFromGcSlots();
    void advanceFromValueSlots();
    bool getSlotFromBitmap(uint32 *slot);

  public:
    SafepointReader(IonScript *script, const SafepointIndex *si);

    static CodeLocationLabel InvalidationPatchPoint(IonScript *script, const SafepointIndex *si);

    uint32 osiCallPointOffset() const {
        return osiCallPointOffset_;
    }
    GeneralRegisterSet gcSpills() const {
        return gcSpills_;
    }
    GeneralRegisterSet allSpills() const {
        return allSpills_;
    }
    uint32 osiReturnPointOffset() const;

    // Returns true if a slot was read, false if there are no more slots.
    bool getGcSlot(uint32 *slot);

    // Returns true if a slot was read, false if there are no more slots.
    bool getValueSlot(uint32 *slot);
};

} // namespace ion
} // namespace js

#endif // jsion_include_safepoints_h_

