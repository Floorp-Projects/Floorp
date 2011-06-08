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

#ifndef jsion_cpu_x64_regs_h__
#define jsion_cpu_x64_regs_h__

namespace js {
namespace ion {

static const ptrdiff_t STACK_SLOT_SIZE       = 8;

class RegisterCodes {
  public:
    enum Code {
        RAX,
        RCX,
        RDX,
        RBX,
        RSP,
        RBP,
        RSI,
        RDI,
        R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        R14,
        R15
    };

    static const uint32 Total = 16;
    static const uint32 Allocatable = 13;

    static const uint32 AllMask = (1 << Total) - 1;

    static const uint32 VolatileMask =
        (1 << RAX) |
        (1 << RCX) |
        (1 << RDX) |
# if !defined(_WIN64)
        (1 << RSI) |
        (1 << RDI) |
# endif
        (1 << R8) |
        (1 << R9) |
        (1 << R10) |
        (1 << R11);

    static const uint32 NonVolatileMask =
        (1 << RBX) |
#if defined(_WIN64)
        (1 << RSI) |
        (1 << RDI) |
#endif
        (1 << RBP) |
        (1 << R12) |
        (1 << R13) |
        (1 << R14) |
        (1 << R15);

    static const uint32 SingleByteRegs = VolatileMask | NonVolatileMask;

    static const uint32 NonAllocatableMask =
        (1 << RSP) |
        (1 << RBP) |
        (1 << R11);         // JSC uses this as a scratch register.

    static const uint32 AllocatableMask = AllMask & ~NonAllocatableMask;
};

class FloatRegisterCodes {
  public:
    enum Code {
        XMM0,
        XMM1,
        XMM2,
        XMM3,
        XMM4,
        XMM5,
        XMM6,
        XMM7,
        XMM8,
        XMM9,
        XMM10,
        XMM11,
        XMM12,
        XMM13,
        XMM14,
        XMM15
    };

    static const uint32 Total = 16;
    static const uint32 Allocatable = 16;

    static const uint32 AllMask = (1 << Total) - 1;

    static const uint32 VolatileMask = 
#if defined(_WIN64)
        (1 << XMM0) |
        (1 << XMM1) |
        (1 << XMM2) |
        (1 << XMM3) |
        (1 << XMM4) |
        (1 << XMM5);
#else
        AllMask;
#endif


    static const uint32 NonVolatileMask = AllMask & ~VolatileMask;
    static const uint32 AllocatableMask = AllMask;
};

} // namespace js
} // namespace ion

#endif // jsion_cpu_x64_regs_h__

