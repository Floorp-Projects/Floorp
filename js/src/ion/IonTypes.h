/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_types_h_
#define jsion_types_h_

#include <jstypes.h>

namespace js {
namespace ion {

typedef uint64_t uint64;
typedef int64_t int64;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint8_t uint8;
typedef int8_t int8;

typedef uint32 SnapshotOffset;
typedef uint32 BailoutId;

static const SnapshotOffset INVALID_SNAPSHOT_OFFSET = uint32(-1);

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

    // A bailout to trigger recompilation to inline calls when the script is hot.
    Bailout_RecompileCheck,

    // A bailout triggered by a bounds-check failure.
    Bailout_BoundsCheck,

    // Like Bailout_Normal, but invalidate the current IonScript.
    Bailout_Invalidate
};

#ifdef DEBUG
// Track the pipeline of opcodes which has produced a snapshot.
#define TRACK_SNAPSHOTS 1
#endif

} // namespace ion
} // namespace js

#endif // jsion_types_h_

