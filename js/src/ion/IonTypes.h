/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_types_h_
#define jsion_types_h_

#include <jstypes.h>

namespace js {
namespace ion {

typedef uint32_t SnapshotOffset;
typedef uint32_t BailoutId;

static const SnapshotOffset INVALID_SNAPSHOT_OFFSET = uint32_t(-1);

// Different kinds of bailouts. When extending this enum, make sure to check
// the bits reserved for bailout kinds in Bailouts.h
enum BailoutKind
{
    // A normal bailout triggered from type, shape, and assorted overflow
    // guards in the compiler.
    Bailout_Normal,

    // A bailout at the very start of a function indicates that there may be
    // a type mismatch in the arguments that necessitates a reflow.
    Bailout_ArgumentCheck,

    // A bailout required to monitor a newly observed type in a type inference
    // barrier.
    Bailout_TypeBarrier,

    // A bailout required to monitor the result of a VM call.
    Bailout_Monitor,

    // A bailout triggered by a bounds-check failure.
    Bailout_BoundsCheck,

    // A shape guard based on TI information failed.
    Bailout_ShapeGuard,

    // A shape guard based on JM ICs failed.
    Bailout_CachedShapeGuard
};

#ifdef DEBUG
inline const char *
BailoutKindString(BailoutKind kind)
{
    switch (kind) {
      case Bailout_Normal:
        return "Bailout_Normal";
      case Bailout_ArgumentCheck:
        return "Bailout_ArgumentCheck";
      case Bailout_TypeBarrier:
        return "Bailout_TypeBarrier";
      case Bailout_Monitor:
        return "Bailout_Monitor";
      case Bailout_BoundsCheck:
        return "Bailout_BoundsCheck";
      case Bailout_ShapeGuard:
        return "Bailout_ShapeGuard";
      case Bailout_CachedShapeGuard:
        return "Bailout_CachedShapeGuard";
      default:
        JS_NOT_REACHED("Invalid BailoutKind");
    }
    return "INVALID_BAILOUT_KIND";
}
#endif

// The ordering of this enumeration is important: Anything < Value is a
// specialized type. Furthermore, anything < String has trivial conversion to
// a number.
enum MIRType
{
    MIRType_Undefined,
    MIRType_Null,
    MIRType_Boolean,
    MIRType_Int32,
    MIRType_Double,
    MIRType_String,
    MIRType_Object,
    MIRType_Magic,
    MIRType_Value,
    MIRType_None,         // Invalid, used as a placeholder.
    MIRType_Slots,        // A slots vector
    MIRType_Elements,     // An elements vector
    MIRType_Pointer,      // An opaque pointer that receives no special treatment
    MIRType_Shape,        // A Shape pointer.
    MIRType_ForkJoinSlice // js::ForkJoinSlice*
};

#ifdef DEBUG
// Track the pipeline of opcodes which has produced a snapshot.
#define TRACK_SNAPSHOTS 1
#endif

} // namespace ion
} // namespace js

#endif // jsion_types_h_

