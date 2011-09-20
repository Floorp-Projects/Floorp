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

#ifndef jsion_type_policy_h__
#define jsion_type_policy_h__

#include "TypeOracle.h"

namespace js {
namespace ion {

class MInstruction;
class MDefinition;

class TypeAnalysis
{
  public:
    virtual void addPreferredType(MDefinition *def, MIRType type) = 0;
    inline void preferType(MDefinition *def, MIRType type);
};

// A type policy directs the type analysis phases, which insert conversion,
// boxing, unboxing, and type changes as necessary.
class TypePolicy
{
  public:
    // Returns whether an instruction needs to change its output type,
    // necessitating a reflow of its dependencies. If false, no
    // respecialization was needed.
    //
    // A respecialization should never narrow, otherwise, the analysis could
    // not reach a fixpoint. For example, a Value should not narrow to an
    // Int32, lest it accidentally need to become a Value and back again.
    virtual bool respecialize(MInstruction *def) = 0;

    // Asks the instruction whether it would like to prefer an untyped value as
    // a particular type.
    virtual void specializeInputs(MInstruction *ins, TypeAnalysis *analysis) = 0;

    // Analyze the inputs of the instruction and perform one of the following
    // actions for each input:
    //  * Nothing; the input already type-checks.
    //  * If untyped, optionally ask the input to try and specialize its value.
    //  * Replace the operand with a conversion instruction.
    //  * Insert an unconditional deoptimization (no conversion possible).
    virtual bool adjustInputs(MInstruction *def) = 0;
};

class BoxInputsPolicy : public TypePolicy
{
  protected:
    MDefinition *boxAt(MInstruction *at, MDefinition *operand);

  public:
    virtual bool respecialize(MInstruction *def);
    virtual void specializeInputs(MInstruction *ins, TypeAnalysis *analyzer);
    virtual bool adjustInputs(MInstruction *def);
};

class BinaryArithPolicy : public BoxInputsPolicy
{
  protected:
    // Specifies three levels of specialization:
    //  - < Value. This input is expected and required.
    //  - == Any. Inputs are probably primitive.
    //  - == None. This op should not be specialized.
    MIRType specialization_;

  public:
    bool respecialize(MInstruction *def);
    void specializeInputs(MInstruction *ins, TypeAnalysis *analyzer);
    bool adjustInputs(MInstruction *def);
};

class BitwisePolicy : public BoxInputsPolicy
{
  protected:
    // Specifies three levels of specialization:
    //  - < Value. This input is expected and required.
    //  - == Any. Inputs are probably primitive.
    //  - == None. This op should not be specialized.
    MIRType specialization_;

  public:
    bool respecialize(MInstruction *def);
    void specializeInputs(MInstruction *ins, TypeAnalysis *analyzer);
    bool adjustInputs(MInstruction *def);
};

class TableSwitchPolicy : public BoxInputsPolicy
{
  public:
    bool respecialize(MInstruction *def);
    void specializeInputs(MInstruction *ins, TypeAnalysis *analyzer);
    bool adjustInputs(MInstruction *def);
};

class ComparePolicy : public BoxInputsPolicy
{
  protected:
    MIRType specialization_;

  public:
    bool respecialize(MInstruction *def);
    void specializeInputs(MInstruction *ins, TypeAnalysis *analyzer);
    bool adjustInputs(MInstruction *def);
};

class CallPolicy : public BoxInputsPolicy
{
  public:
    void specializeInputs(MInstruction *ins, TypeAnalysis *analyzer);
    bool adjustInputs(MInstruction *def);
};

static inline bool
CoercesToDouble(MIRType type)
{
    if (type == MIRType_Undefined || type == MIRType_Double)
        return true;
    return false;
}


} // namespace ion
} // namespace js

#endif // jsion_type_policy_h__

