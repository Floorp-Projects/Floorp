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

#ifndef jsion_lir_x64_h__
#define jsion_lir_x64_h__

#include "ion/TypeOracle.h"

namespace js {
namespace ion {

// Given a typed input, returns an untyped box.
class LBox : public LInstructionHelper<1, 1, 0>
{
    MIRType type_;

  public:
    LIR_HEADER(Box);

    LBox(MIRType type, const LAllocation &payload)
      : type_(type)
    {
        setOperand(0, payload);
    }

    MIRType type() const {
        return type_;
    }
};

// Given an untyped input, guards on whether it's an integer and returns an
// integer payload.
class LUnboxBoolean : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(UnboxBoolean);

    LUnboxBoolean(const LAllocation &input, const LDefinition &temp) {
        setOperand(0, input);
        setTemp(0, temp);
    }
};

// Given an untyped input, guards on whether it's an integer and returns an
// integer payload.
class LUnboxInteger : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(UnboxInteger);

    LUnboxInteger(const LAllocation &input) {
        setOperand(0, input);
    }

    static const size_t Input = 0;

    const LDefinition *output() {
        return getDef(0);
    }
};

// Given an untyped input, guards on whether it's a double and returns a double
// payload.
class LUnboxDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(UnboxDouble);

    LUnboxDouble(const LAllocation &input) {
        setOperand(0, input);
    }

    static const size_t Input = 0;

    const LDefinition *output() {
        return getDef(0);
    }
};

// Given an untyped input, guards on whether it's an object and returns the
// pointer.
class LUnboxObject : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(UnboxObject);

    LUnboxObject(const LAllocation &input) {
        setOperand(0, input);
    }
};

// Given an untyped input, guards on whether it's a string and returns the
// pointer.
class LUnboxString : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(UnboxString);

    LUnboxString(const LAllocation &input, const LDefinition &temp) {
        setOperand(0, input);
        setTemp(0, temp);
    }
};

// Constant double.
class LDouble : public LInstructionHelper<1, 0, 0>
{
    double d_;

  public:
    LIR_HEADER(Double);

    LDouble(double d) : d_(d)
    { }

    double getDouble() const {
        return d_;
    }
    const LDefinition *output() {
        return getDef(0);
    }
};

} // namespace ion
} // namespace js

#endif // jsion_lir_x64_h__

