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

#include "Assembler-x64.h"
#include "jsgcmark.h"

using namespace js;
using namespace js::ion;

void
Assembler::writeRelocation(JmpSrc src)
{
    if (!relocations_.length()) {
        // The relocation table starts with a fixed-width integer pointing
        // to the start of the extended jump table. But, we don't know the
        // actual extended jump table offset yet, so write a 0 which we'll
        // patch later.
        relocations_.writeFixedUint32(0);
    }
    relocations_.writeUnsigned(src.offset());
    relocations_.writeUnsigned(jumps_.length());
}

void
Assembler::addPendingJump(JmpSrc src, void *target, Relocation::Kind reloc)
{
    // Emit reloc before modifying the jump table, since it computes a 0-based
    // index.
    if (reloc == Relocation::CODE)
        writeRelocation(src);
    enoughMemory_ &= jumps_.append(RelativePatch(src.offset(), target, reloc));
}

void
Assembler::flush()
{
    if (!jumps_.length() || oom())
        return;

    // Emit the jump table.
    masm.align(16);
    extendedJumpTable_ = masm.size();

    // Now that we know the offset to the jump table, squirrel it into the
    // relocation buffer.
    JS_ASSERT(relocations_.length() >= sizeof(uint32));
    *(uint32 *)relocations_.buffer() = extendedJumpTable_;

    for (size_t i = 0; i < jumps_.length(); i++) {
#ifdef DEBUG
        size_t oldSize = masm.size();
#endif
        masm.jmp_rip(0);
        masm.immediate64(0);
        masm.align(SizeOfJumpTableEntry);
        JS_ASSERT(masm.size() - oldSize == SizeOfJumpTableEntry);
    }
}

void
Assembler::executableCopy(uint8 *buffer)
{
    AssemblerX86Shared::executableCopy(buffer);

    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        uint8 *src = buffer + rp.offset;
        if (JSC::X86Assembler::canRelinkJump(src, rp.target)) {
            JSC::X86Assembler::setRel32(src, rp.target);
        } else {
            // An extended jump table must exist, and its offset must be in
            // range.
            JS_ASSERT(extendedJumpTable_);
            JS_ASSERT((extendedJumpTable_ + i * SizeOfJumpTableEntry) < size() - SizeOfJumpTableEntry);

            // Patch the jump to go to the extended jump entry.
            uint8 *entry = buffer + extendedJumpTable_ + i * SizeOfJumpTableEntry;
            JSC::X86Assembler::setRel32(src, entry);

            // Now patch the pointer, note that we need to align it to
            // *after* the extended jump, i.e. after the 64-bit immedate.
            JSC::X86Assembler::repatchPointer(entry + SizeOfExtendedJump, rp.target);
        }
    }
}

class RelocationIterator
{
    CompactBufferReader reader_;
    uint32 tableStart_;
    uint32 offset_;
    uint32 extOffset_;

  public:
    RelocationIterator(CompactBufferReader &reader)
      : reader_(reader)
    {
        tableStart_ = reader_.readFixedUint32();
    }

    bool read() {
        if (!reader_.more())
            return false;
        offset_ = reader_.readUnsigned();
        extOffset_ = reader_.readUnsigned();
        return true;
    }

    uint32 offset() const {
        return offset_;
    }
    uint32 extendedOffset() const {
        return extOffset_;
    }
};

IonCode *
Assembler::CodeFromJump(IonCode *code, uint8 *jump)
{
    uint8 *target = (uint8 *)JSC::X86Assembler::getRel32Target(jump);
    if (target >= code->raw() && target < code->raw() + code->instructionsSize()) {
        // This jump is within the code buffer, so it has been redirected to
        // the extended jump table.
        JS_ASSERT(target + SizeOfJumpTableEntry < code->raw() + code->instructionsSize());

        target = (uint8 *)JSC::X86Assembler::getPointer(target + SizeOfExtendedJump);
    }

    return IonCode::FromExecutable(target);
}

void
Assembler::TraceRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader)
{
    RelocationIterator iter(reader);
    while (iter.read()) {
        IonCode *child = CodeFromJump(code, code->raw() + iter.offset());
        MarkIonCode(trc, child, "rel32");
    }
}

