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
 *   David Anderson <danderson@mozilla.com>
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

#include "MIR.h"
#include "MIRGraph.h"
#include "IonLIR.h"
#include "IonLIR-inl.h"

using namespace js;
using namespace js::ion;

LSnapshot::LSnapshot(MSnapshot *mir)
  : numSlots_(mir->numOperands() * BOX_PIECES),
    slots_(NULL),
    mir_(mir)
{ }

bool
LSnapshot::init(MIRGenerator *gen)
{
    slots_ = gen->allocate<LAllocation>(numSlots_);
    return !!slots_;
}

LSnapshot *
LSnapshot::New(MIRGenerator *gen, MSnapshot *mir)
{
    LSnapshot *snapshot = new LSnapshot(mir);
    if (!snapshot->init(gen))
        return NULL;
    return snapshot;
}

void
LInstruction::printName(FILE *fp)
{
    static const char *names[] =
    {
#define LIROP(x) #x,
        LIR_OPCODE_LIST(LIROP)
#undef LIROP
    };
    const char *name = names[op()];
    size_t len = strlen(name);
    for (size_t i = 0; i < len; i++)
        fprintf(fp, "%c", tolower(name[i]));
}

static const char *TypeChars[] =
{
    "i",            // INTEGER
    "p",            // POINTER
    "o",            // OBJECT
    "f",            // DOUBLE
    "t",            // TYPE
    "d",            // PAYLOAD
    "x"             // BOX
};

static void
PrintDefinition(FILE *fp, const LDefinition &def)
{
    fprintf(fp, "[%s:%d", TypeChars[def.type()], def.virtualRegister());
    if (def.policy() == LDefinition::PRESET) {
        switch (def.output()->kind()) {
          case LAllocation::CONSTANT_VALUE:
          case LAllocation::CONSTANT_INDEX:
            fprintf(fp, " (c)");
            break;
          case LAllocation::GPR:
            fprintf(fp, " (%s)", def.output()->toGeneralReg()->reg().name());
            break;
          case LAllocation::FPU:
            fprintf(fp, " (%s)", def.output()->toFloatReg()->reg().name());
            break;
          case LAllocation::ARGUMENT:
            fprintf(fp, " (arg %d)", def.output()->toArgument()->index());
            break;
          default:
            JS_NOT_REACHED("unexpected preset allocation type");
            break;
        }
    } else if (def.policy() == LDefinition::CAN_REUSE_INPUT) {
        fprintf(fp, " (r?)");
    } else if (def.policy() == LDefinition::MUST_REUSE_INPUT) {
        fprintf(fp, " (rr)");
    }
    fprintf(fp, "]");
}

static void
PrintUse(FILE *fp, LUse *use)
{
    fprintf(fp, "(v%d:", use->virtualRegister());
    if (use->policy() == LUse::ANY) {
        fprintf(fp, "*");
    } else if (use->policy() == LUse::REGISTER) {
        fprintf(fp, "r");
    } else {
        // Unfortunately, we don't know here whether the virtual register is a
        // float or a double. Should we steal a bit in LUse for help? For now,
        // nothing defines any fixed xmm registers.
        fprintf(fp, "%s", RegisterCodes::GetName(RegisterCodes::Code(use->registerCode())));
    }
    if (use->killedAtStart())
        fprintf(fp, "!");
    fprintf(fp, ")");
}

void
LInstruction::printOperands(FILE *fp)
{
    for (size_t i = 0; i < numOperands(); i++) {
        fprintf(fp, " ");
        LAllocation *a = getOperand(i);
        switch (a->kind()) {
          case LAllocation::CONSTANT_VALUE:
          case LAllocation::CONSTANT_INDEX:
            fprintf(fp, "(c)");
            break;
          case LAllocation::GPR:
            fprintf(fp, "(=%s)", a->toGeneralReg()->reg().name());
            break;
          case LAllocation::FPU:
            fprintf(fp, "(=%s)", a->toFloatReg()->reg().name());
            break;
          case LAllocation::STACK_SLOT:
            fprintf(fp, "(stack:i%d)", a->toStackSlot()->slot());
            break;
          case LAllocation::DOUBLE_SLOT:
            fprintf(fp, "(stack:d%d)", a->toStackSlot()->slot());
            break;
          case LAllocation::ARGUMENT:
            fprintf(fp, "(arg:%d)", a->toArgument()->index());
            break;
          case LAllocation::USE:
            PrintUse(fp, a->toUse());
            break;
          default:
            JS_NOT_REACHED("what?");
            break;
        }
        if (i != numOperands() - 1)
            fprintf(fp, ",");
    }
}

void
LInstruction::print(FILE *fp)
{
    printName(fp);

    fprintf(fp, " (");
    for (size_t i = 0; i < numDefs(); i++) {
        PrintDefinition(fp, *getDef(i));
        if (i != numDefs() - 1)
            fprintf(fp, ", ");
    }
    fprintf(fp, ")");

    printInfo(fp);
}

