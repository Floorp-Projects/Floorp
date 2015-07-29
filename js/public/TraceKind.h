/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TraceKind_h
#define js_TraceKind_h

namespace JS {

// When tracing a thing, the GC needs to know about the layout of the object it
// is looking at. There are a fixed number of different layouts that the GC
// knows about. The "trace kind" is a static map which tells which layout a GC
// thing has.
//
// Although this map is public, the details are completely hidden. Not all of
// the matching C++ types are exposed, and those that are, are opaque.
//
// See Value::gcKind() and JSTraceCallback in Tracer.h for more details.
enum class TraceKind
{
    // These trace kinds have a publicly exposed, although opaque, C++ type.
    // Note: The order here is determined by our Value packing. Other users
    //       should sort alphabetically, for consistency.
    Object = 0x00,
    String = 0x01,
    Symbol = 0x02,
    Script = 0x03,

    // Shape details are exposed through JS_TraceShapeCycleCollectorChildren.
    Shape = 0x04,

    // ObjectGroup details are exposed through JS_TraceObjectGroupCycleCollectorChildren.
    ObjectGroup = 0x05,

    // The kind associated with a nullptr.
    Null = 0x06,

    // The following kinds do not have an exposed C++ idiom.
    BaseShape = 0x0F,
    JitCode = 0x1F,
    LazyScript = 0x2F
};
const static uintptr_t OutOfLineTraceKindMask = 0x07;
static_assert(uintptr_t(JS::TraceKind::BaseShape) & OutOfLineTraceKindMask, "mask bits are set");
static_assert(uintptr_t(JS::TraceKind::JitCode) & OutOfLineTraceKindMask, "mask bits are set");
static_assert(uintptr_t(JS::TraceKind::LazyScript) & OutOfLineTraceKindMask, "mask bits are set");

} // namespace JS

#endif // js_TraceKind_h
