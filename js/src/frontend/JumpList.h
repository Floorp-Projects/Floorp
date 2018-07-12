/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_JumpList_h
#define frontend_JumpList_h

#include <stddef.h>

#include "js/TypeDecls.h"

namespace js {
namespace frontend {

// Linked list of jump instructions that need to be patched. The linked list is
// stored in the bytes of the incomplete bytecode that will be patched, so no
// extra memory is needed, and patching the instructions destroys the list.
//
// Example:
//
//     JumpList brList;
//     if (!emitJump(JSOP_IFEQ, &brList))
//         return false;
//     ...
//     JumpTarget label;
//     if (!emitJumpTarget(&label))
//         return false;
//     ...
//     if (!emitJump(JSOP_GOTO, &brList))
//         return false;
//     ...
//     patchJumpsToTarget(brList, label);
//
//                 +-> -1
//                 |
//                 |
//    ifeq ..   <+ +                +-+   ifeq ..
//    ..         |                  |     ..
//  label:       |                  +-> label:
//    jumptarget |                  |     jumptarget
//    ..         |                  |     ..
//    goto .. <+ +                  +-+   goto .. <+
//             |                                   |
//             |                                   |
//             +                                   +
//           brList                              brList
//
//       |                                  ^
//       +------- patchJumpsToTarget -------+
//

// Offset of a jump target instruction, used for patching jump instructions.
struct JumpTarget {
    ptrdiff_t offset;
};

struct JumpList {
    JumpList() {}
    // -1 is used to mark the end of jump lists.
    ptrdiff_t offset = -1;

    // Add a jump instruction to the list.
    void push(jsbytecode* code, ptrdiff_t jumpOffset);

    // Patch all jump instructions in this list to jump to `target`.  This
    // clobbers the list.
    void patchAll(jsbytecode* code, JumpTarget target);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_JumpList_h */
