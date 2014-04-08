/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Recover.h"

#include "jit/IonSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;

void
RInstruction::readRecoverData(CompactBufferReader &reader, RInstructionStorage *raw)
{
    uint32_t op = reader.readUnsigned();
    switch (Opcode(op)) {
      case Recover_ResumePoint:
        new (raw->addr()) RResumePoint(reader);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Bad decoding of the previous instruction?");
        break;
    }
}

bool
MResumePoint::writeRecoverData(CompactBufferWriter &writer) const
{
    writer.writeUnsigned(uint32_t(RInstruction::Recover_ResumePoint));

    MBasicBlock *bb = block();
    JSFunction *fun = bb->info().funMaybeLazy();
    JSScript *script = bb->info().script();
    uint32_t exprStack = stackDepth() - bb->info().ninvoke();

#ifdef DEBUG
    // Ensure that all snapshot which are encoded can safely be used for
    // bailouts.
    if (GetIonContext()->cx) {
        uint32_t stackDepth;
        bool reachablePC;
        jsbytecode *bailPC = pc();

        if (mode() == MResumePoint::ResumeAfter)
            bailPC = GetNextPc(pc());

        if (!ReconstructStackDepth(GetIonContext()->cx, script,
                                   bailPC, &stackDepth, &reachablePC))
        {
            return false;
        }

        if (reachablePC) {
            if (JSOp(*bailPC) == JSOP_FUNCALL) {
                // For fun.call(this, ...); the reconstructStackDepth will
                // include the this. When inlining that is not included.  So the
                // exprStackSlots will be one less.
                MOZ_ASSERT(stackDepth - exprStack <= 1);
            } else if (JSOp(*bailPC) != JSOP_FUNAPPLY &&
                       !IsGetPropPC(bailPC) && !IsSetPropPC(bailPC))
            {
                // For fun.apply({}, arguments) the reconstructStackDepth will
                // have stackdepth 4, but it could be that we inlined the
                // funapply. In that case exprStackSlots, will have the real
                // arguments in the slots and not be 4.

                // With accessors, we have different stack depths depending on
                // whether or not we inlined the accessor, as the inlined stack
                // contains a callee function that should never have been there
                // and we might just be capturing an uneventful property site,
                // in which case there won't have been any violence.
                MOZ_ASSERT(exprStack == stackDepth);
            }
        }
    }
#endif

    // Test if we honor the maximum of arguments at all times.  This is a sanity
    // check and not an algorithm limit. So check might be a bit too loose.  +4
    // to account for scope chain, return value, this value and maybe
    // arguments_object.
    MOZ_ASSERT(CountArgSlots(script, fun) < SNAPSHOT_MAX_NARGS + 4);

    uint32_t implicit = StartArgSlot(script);
    uint32_t formalArgs = CountArgSlots(script, fun);
    uint32_t nallocs = formalArgs + script->nfixed() + exprStack;

    IonSpew(IonSpew_Snapshots, "Starting frame; implicit %u, formals %u, fixed %u, exprs %u",
            implicit, formalArgs - implicit, script->nfixed(), exprStack);

    uint32_t pcoff = script->pcToOffset(pc());
    IonSpew(IonSpew_Snapshots, "Writing pc offset %u, nslots %u", pcoff, nallocs);
    writer.writeUnsigned(pcoff);
    writer.writeUnsigned(nallocs);
    return true;
}

RResumePoint::RResumePoint(CompactBufferReader &reader)
{
    static_assert(sizeof(*this) <= sizeof(RInstructionStorage),
                  "Storage space is too small to decode this recover instruction.");
    pcOffset_ = reader.readUnsigned();
    numOperands_ = reader.readUnsigned();
    IonSpew(IonSpew_Snapshots, "Read RResumePoint (pc offset %u, nslots %u)",
            pcOffset_, numOperands_);
}
