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

#include "Safepoints.h"
#include "IonSpewer.h"

using namespace js;
using namespace ion;

bool
SafepointWriter::init(uint32 localSlotCount)
{
    frameSlots_ = BitSet::New(localSlotCount);
    if (!frameSlots_)
        return false;

    return true;
}

uint32
SafepointWriter::startEntry()
{
    IonSpew(IonSpew_Safepoints, "Encoding safepoint (position %d):", stream_.length());
    return uint32(stream_.length());
}

void
SafepointWriter::writeOsiCallPointOffset(uint32 osiCallPointOffset)
{
    stream_.writeUnsigned(osiCallPointOffset);
}

static void
WriteRegisterMask(CompactBufferWriter &stream, uint32 bits)
{
    if (sizeof(PackedRegisterMask) == 8)
        stream.writeByte(bits);
    else
        stream.writeUnsigned(bits);
}

void
SafepointWriter::writeGcRegs(GeneralRegisterSet actual, GeneralRegisterSet spilled)
{
    WriteRegisterMask(stream_, actual.bits());
    if (!actual.empty())
        WriteRegisterMask(stream_, spilled.bits());

#ifdef DEBUG
    for (AnyRegisterIterator iter(actual, FloatRegisterSet()); iter.more(); iter++)
        IonSpew(IonSpew_Safepoints, "    gc reg: %s", (*iter).name());
#endif
}

static void
MapSlotsToBitset(BitSet *set, CompactBufferWriter &stream, uint32 nslots, uint32 *slots)
{
    set->clear();

    for (uint32 i = 0; i < nslots; i++) {
        // Slots are represented at a distance from |fp|. Since the stack grows
        // down, this means slots start at index 1, so we subtract 1 to pack
        // the bitset.
        set->insert(slots[i] - 1);
    }

    size_t count = set->rawLength();
    uint32 *words = set->raw();
    for (size_t i = 0; i < count; i++)
        stream.writeUnsigned(words[i]);
}

void
SafepointWriter::writeGcSlots(uint32 nslots, uint32 *slots)
{
#ifdef DEBUG
    for (uint32 i = 0; i < nslots; i++)
        IonSpew(IonSpew_Safepoints, "    gc slot: %d", slots[i]);
#endif

    MapSlotsToBitset(frameSlots_, stream_, nslots, slots);
}

void
SafepointWriter::writeValueSlots(uint32 nslots, uint32 *slots)
{
#ifdef DEBUG
    for (uint32 i = 0; i < nslots; i++)
        IonSpew(IonSpew_Safepoints, "    gc value: %d", slots[i]);
#endif

    MapSlotsToBitset(frameSlots_, stream_, nslots, slots);
}

#ifdef DEBUG
static void
DumpNunboxPart(const LAllocation &a)
{
    if (a.isStackSlot()) {
        fprintf(IonSpewFile, "stack %d", a.toStackSlot()->slot());
    } else if (a.isArgument()) {
        fprintf(IonSpewFile, "arg %d", a.toArgument()->index());
    } else {
        fprintf(IonSpewFile, "reg %s", a.toGeneralReg()->reg().name());
    }
}
#endif // DEBUG

void
SafepointWriter::writeNunboxParts(uint32 nentries, SafepointNunboxEntry *entries)
{
#ifdef DEBUG
    if (IonSpewEnabled(IonSpew_Safepoints)) {
        for (uint32 i = 0; i < nentries; i++) {
            IonSpewHeader(IonSpew_Safepoints);
            fprintf(IonSpewFile, "    nunbox (type in ");
            DumpNunboxPart(entries[i].type);
            fprintf(IonSpewFile, ", payload in ");
            DumpNunboxPart(entries[i].payload);
            fprintf(IonSpewFile, ")");
        }
    }
#endif

    if (nentries) {
        // JS_NOT_REACHED("NYI - encode these once we support out-of-line calls");
        return;
    }
}

void
SafepointWriter::endEntry()
{
    IonSpew(IonSpew_Safepoints, "    -- entry ended at %d", uint32(stream_.length()));
}

SafepointReader::SafepointReader(IonScript *script, const SafepointIndex *si)
  : stream_(script->safepoints() + si->safepointOffset(),
            script->safepoints() + script->safepointsSize()),
    localSlotCount_(script->frameLocals())
{
    osiCallPointOffset_ = stream_.readUnsigned();

    gcSpills_ = GeneralRegisterSet(stream_.readUnsigned());
    if (gcSpills_.empty())
        allSpills_ = gcSpills_;
    else
        allSpills_ = GeneralRegisterSet(stream_.readUnsigned());

    advanceFromGcRegs();
}

uint32
SafepointReader::osiReturnPointOffset() const
{
    return osiCallPointOffset_ + Assembler::patchWrite_NearCallSize();
}

CodeLocationLabel
SafepointReader::InvalidationPatchPoint(IonScript *script, const SafepointIndex *si)
{
    SafepointReader reader(script, si);

    return CodeLocationLabel(script->method(), reader.osiCallPointOffset());
}

void
SafepointReader::advanceFromGcRegs()
{
    currentSlotChunkNumber_ = 0;
    currentSlotChunk_ = stream_.readUnsigned();
}

bool
SafepointReader::getSlotFromBitmap(uint32 *slot)
{
    while (currentSlotChunk_ == 0) {
        currentSlotChunkNumber_++;

        // Are there any more chunks to read?
        if (currentSlotChunkNumber_ == BitSet::RawLengthForBits(localSlotCount_))
            return false;

        // Yes, read the next chunk.
        currentSlotChunk_ = stream_.readUnsigned();
    }

    // The current chunk still has bits in it, so get the next bit, then mask
    // it out of the slot chunk.
    uint32 bit;
    JS_FLOOR_LOG2(bit, currentSlotChunk_);
    currentSlotChunk_ &= ~(1 << bit);

    // Return the slot, taking care to add 1 back in since it was subtracted
    // when added in the original bitset.
    *slot = (currentSlotChunkNumber_ * sizeof(uint32) * 8) + bit + 1;
    return true;
}

bool
SafepointReader::getGcSlot(uint32 *slot)
{
    if (getSlotFromBitmap(slot))
        return true;
    advanceFromGcSlots();
    return false;
}

void
SafepointReader::advanceFromGcSlots()
{
    // No, reset the counter.
    currentSlotChunkNumber_ = 0;
#ifdef JS_NUNBOX32
    currentSlotChunk_ = stream_.readUnsigned();
#endif
}

bool
SafepointReader::getValueSlot(uint32 *slot)
{
#ifdef JS_NUNBOX32
    if (getSlotFromBitmap(slot))
        return true;
    advanceFromValueSlots();
    return false;
#else
    return false;
#endif
}

void
SafepointReader::advanceFromValueSlots()
{
}

