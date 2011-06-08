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

#ifndef jsion_cpu_x86_regs_h__
#define jsion_cpu_x86_regs_h__

namespace js {
namespace ion {

static const ptrdiff_t STACK_SLOT_SIZE       = 4;

class RegisterCodes {
  public:
    enum Code {
        EAX,
        ECX,
        EDX,
        EBX,
        ESP,
        EBP,
        ESI,
        EDI
    };

    static const uint32 Total = 8;
    static const uint32 Allocatable = 6;

    static const uint32 AllMask = (1 << Total) - 1;

    static const uint32 VolatileMask =
        (1 << EAX) |
        (1 << ECX) |
        (1 << EDX);

    static const uint32 NonVolatileMask =
        (1 << EBX) |
        (1 << ESI) |
        (1 << EDI) |
        (1 << EBP);

    static const uint32 SingleByteRegs =
        (1 << EAX) |
        (1 << ECX) |
        (1 << EDX) |
        (1 << EBX);

    static const uint32 NonAllocatableMask =
        (1 << ESP) |
        (1 << EBP);

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
        XMM7
    };

    static const uint32 Total = 8;
    static const uint32 Allocatable = 8;

    static const uint32 AllMask = (1 << Total) - 1;

    static const uint32 VolatileMask = AllMask;
    static const uint32 NonVolatileMask = 0;
    static const uint32 AllocatableMask = AllMask;
};

} // namespace js
} // namespace ion

#endif // jsion_cpu_x86_regs_h__

