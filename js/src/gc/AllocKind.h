/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * GC-internal definition of GC cell kinds.
 */

#ifndef gc_AllocKind_h
#define gc_AllocKind_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/EnumeratedRange.h"

#include <stdint.h>

#include "js/TraceKind.h"

namespace js {
namespace gc {

/* The GC allocation kinds. */
// FIXME: uint8_t would make more sense for the underlying type, but causes
// miscompilations in GCC (fixed in 4.8.5 and 4.9.3). See also bug 1143966.
enum class AllocKind {
    FIRST,
    OBJECT_FIRST = FIRST,
    FUNCTION = FIRST,
    FUNCTION_EXTENDED,
    OBJECT0,
    OBJECT0_BACKGROUND,
    OBJECT2,
    OBJECT2_BACKGROUND,
    OBJECT4,
    OBJECT4_BACKGROUND,
    OBJECT8,
    OBJECT8_BACKGROUND,
    OBJECT12,
    OBJECT12_BACKGROUND,
    OBJECT16,
    OBJECT16_BACKGROUND,
    OBJECT_LIMIT,
    OBJECT_LAST = OBJECT_LIMIT - 1,
    SCRIPT,
    LAZY_SCRIPT,
    SHAPE,
    ACCESSOR_SHAPE,
    BASE_SHAPE,
    OBJECT_GROUP,
    FAT_INLINE_STRING,
    STRING,
    EXTERNAL_STRING,
    FAT_INLINE_ATOM,
    ATOM,
    SYMBOL,
    JITCODE,
    SCOPE,
    REGEXP_SHARED,
    LIMIT,
    LAST = LIMIT - 1
};

// Macro to enumerate the different allocation kinds supplying information about
// the trace kind, C++ type and allocation size.
#define FOR_EACH_OBJECT_ALLOCKIND(D) \
 /* AllocKind              TraceKind      TypeName           SizedType */ \
    D(FUNCTION,            Object,        JSObject,          JSFunction) \
    D(FUNCTION_EXTENDED,   Object,        JSObject,          FunctionExtended) \
    D(OBJECT0,             Object,        JSObject,          JSObject_Slots0) \
    D(OBJECT0_BACKGROUND,  Object,        JSObject,          JSObject_Slots0) \
    D(OBJECT2,             Object,        JSObject,          JSObject_Slots2) \
    D(OBJECT2_BACKGROUND,  Object,        JSObject,          JSObject_Slots2) \
    D(OBJECT4,             Object,        JSObject,          JSObject_Slots4) \
    D(OBJECT4_BACKGROUND,  Object,        JSObject,          JSObject_Slots4) \
    D(OBJECT8,             Object,        JSObject,          JSObject_Slots8) \
    D(OBJECT8_BACKGROUND,  Object,        JSObject,          JSObject_Slots8) \
    D(OBJECT12,            Object,        JSObject,          JSObject_Slots12) \
    D(OBJECT12_BACKGROUND, Object,        JSObject,          JSObject_Slots12) \
    D(OBJECT16,            Object,        JSObject,          JSObject_Slots16) \
    D(OBJECT16_BACKGROUND, Object,        JSObject,          JSObject_Slots16)

#define FOR_EACH_NONOBJECT_ALLOCKIND(D) \
 /* AllocKind              TraceKind      TypeName           SizedType */ \
    D(SCRIPT,              Script,        JSScript,          JSScript) \
    D(LAZY_SCRIPT,         LazyScript,    js::LazyScript,    js::LazyScript) \
    D(SHAPE,               Shape,         js::Shape,         js::Shape) \
    D(ACCESSOR_SHAPE,      Shape,         js::AccessorShape, js::AccessorShape) \
    D(BASE_SHAPE,          BaseShape,     js::BaseShape,     js::BaseShape) \
    D(OBJECT_GROUP,        ObjectGroup,   js::ObjectGroup,   js::ObjectGroup) \
    D(FAT_INLINE_STRING,   String,        JSFatInlineString, JSFatInlineString) \
    D(STRING,              String,        JSString,          JSString) \
    D(EXTERNAL_STRING,     String,        JSExternalString,  JSExternalString) \
    D(FAT_INLINE_ATOM,     String,        js::FatInlineAtom, js::FatInlineAtom) \
    D(ATOM,                String,        js::NormalAtom,    js::NormalAtom) \
    D(SYMBOL,              Symbol,        JS::Symbol,        JS::Symbol) \
    D(JITCODE,             JitCode,       js::jit::JitCode,  js::jit::JitCode) \
    D(SCOPE,               Scope,         js::Scope,         js::Scope) \
    D(REGEXP_SHARED,       RegExpShared,  js::RegExpShared,  js::RegExpShared)

#define FOR_EACH_ALLOCKIND(D) \
    FOR_EACH_OBJECT_ALLOCKIND(D) \
    FOR_EACH_NONOBJECT_ALLOCKIND(D)

static_assert(int(AllocKind::FIRST) == 0, "Various places depend on AllocKind starting at 0, "
                                          "please audit them carefully!");
static_assert(int(AllocKind::OBJECT_FIRST) == 0, "Various places depend on AllocKind::OBJECT_FIRST "
                                                 "being 0, please audit them carefully!");

inline bool
IsAllocKind(AllocKind kind)
{
    return kind >= AllocKind::FIRST && kind <= AllocKind::LIMIT;
}

inline bool
IsValidAllocKind(AllocKind kind)
{
    return kind >= AllocKind::FIRST && kind <= AllocKind::LAST;
}

inline bool
IsObjectAllocKind(AllocKind kind)
{
    return kind >= AllocKind::OBJECT_FIRST && kind <= AllocKind::OBJECT_LAST;
}

inline bool
IsShapeAllocKind(AllocKind kind)
{
    return kind == AllocKind::SHAPE || kind == AllocKind::ACCESSOR_SHAPE;
}

// Returns a sequence for use in a range-based for loop,
// to iterate over all alloc kinds.
inline decltype(mozilla::MakeEnumeratedRange(AllocKind::FIRST, AllocKind::LIMIT))
AllAllocKinds()
{
    return mozilla::MakeEnumeratedRange(AllocKind::FIRST, AllocKind::LIMIT);
}

// Returns a sequence for use in a range-based for loop,
// to iterate over all object alloc kinds.
inline decltype(mozilla::MakeEnumeratedRange(AllocKind::OBJECT_FIRST, AllocKind::OBJECT_LIMIT))
ObjectAllocKinds()
{
    return mozilla::MakeEnumeratedRange(AllocKind::OBJECT_FIRST, AllocKind::OBJECT_LIMIT);
}

// Returns a sequence for use in a range-based for loop,
// to iterate over alloc kinds from |first| to |limit|, exclusive.
inline decltype(mozilla::MakeEnumeratedRange(AllocKind::FIRST, AllocKind::LIMIT))
SomeAllocKinds(AllocKind first = AllocKind::FIRST, AllocKind limit = AllocKind::LIMIT)
{
    MOZ_ASSERT(IsAllocKind(first), "|first| is not a valid AllocKind!");
    MOZ_ASSERT(IsAllocKind(limit), "|limit| is not a valid AllocKind!");
    return mozilla::MakeEnumeratedRange(first, limit);
}

// AllAllocKindArray<ValueType> gives an enumerated array of ValueTypes,
// with each index corresponding to a particular alloc kind.
template<typename ValueType> using AllAllocKindArray =
    mozilla::EnumeratedArray<AllocKind, AllocKind::LIMIT, ValueType>;

// ObjectAllocKindArray<ValueType> gives an enumerated array of ValueTypes,
// with each index corresponding to a particular object alloc kind.
template<typename ValueType> using ObjectAllocKindArray =
    mozilla::EnumeratedArray<AllocKind, AllocKind::OBJECT_LIMIT, ValueType>;

static inline JS::TraceKind
MapAllocToTraceKind(AllocKind kind)
{
    static const JS::TraceKind map[] = {
#define EXPAND_ELEMENT(allocKind, traceKind, type, sizedType) \
        JS::TraceKind::traceKind,
FOR_EACH_ALLOCKIND(EXPAND_ELEMENT)
#undef EXPAND_ELEMENT
    };

    static_assert(MOZ_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT),
                  "AllocKind-to-TraceKind mapping must be in sync");
    return map[size_t(kind)];
}

/*
 * This must be an upper bound, but we do not need the least upper bound, so
 * we just exclude non-background objects.
 */
static const size_t MAX_BACKGROUND_FINALIZE_KINDS =
    size_t(AllocKind::LIMIT) - size_t(AllocKind::OBJECT_LIMIT) / 2;

static inline bool
IsNurseryAllocable(AllocKind kind)
{
    MOZ_ASSERT(IsValidAllocKind(kind));
    static const bool map[] = {
        true,      /* AllocKind::FUNCTION */
        true,      /* AllocKind::FUNCTION_EXTENDED */
        false,     /* AllocKind::OBJECT0 */
        true,      /* AllocKind::OBJECT0_BACKGROUND */
        false,     /* AllocKind::OBJECT2 */
        true,      /* AllocKind::OBJECT2_BACKGROUND */
        false,     /* AllocKind::OBJECT4 */
        true,      /* AllocKind::OBJECT4_BACKGROUND */
        false,     /* AllocKind::OBJECT8 */
        true,      /* AllocKind::OBJECT8_BACKGROUND */
        false,     /* AllocKind::OBJECT12 */
        true,      /* AllocKind::OBJECT12_BACKGROUND */
        false,     /* AllocKind::OBJECT16 */
        true,      /* AllocKind::OBJECT16_BACKGROUND */
        false,     /* AllocKind::SCRIPT */
        false,     /* AllocKind::LAZY_SCRIPT */
        false,     /* AllocKind::SHAPE */
        false,     /* AllocKind::ACCESSOR_SHAPE */
        false,     /* AllocKind::BASE_SHAPE */
        false,     /* AllocKind::OBJECT_GROUP */
        false,     /* AllocKind::FAT_INLINE_STRING */
        false,     /* AllocKind::STRING */
        false,     /* AllocKind::EXTERNAL_STRING */
        false,     /* AllocKind::FAT_INLINE_ATOM */
        false,     /* AllocKind::ATOM */
        false,     /* AllocKind::SYMBOL */
        false,     /* AllocKind::JITCODE */
        false,     /* AllocKind::SCOPE */
        false,     /* AllocKind::REGEXP_SHARED */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT));
    return map[size_t(kind)];
}

static inline bool
IsBackgroundFinalized(AllocKind kind)
{
    MOZ_ASSERT(IsValidAllocKind(kind));
    static const bool map[] = {
        true,      /* AllocKind::FUNCTION */
        true,      /* AllocKind::FUNCTION_EXTENDED */
        false,     /* AllocKind::OBJECT0 */
        true,      /* AllocKind::OBJECT0_BACKGROUND */
        false,     /* AllocKind::OBJECT2 */
        true,      /* AllocKind::OBJECT2_BACKGROUND */
        false,     /* AllocKind::OBJECT4 */
        true,      /* AllocKind::OBJECT4_BACKGROUND */
        false,     /* AllocKind::OBJECT8 */
        true,      /* AllocKind::OBJECT8_BACKGROUND */
        false,     /* AllocKind::OBJECT12 */
        true,      /* AllocKind::OBJECT12_BACKGROUND */
        false,     /* AllocKind::OBJECT16 */
        true,      /* AllocKind::OBJECT16_BACKGROUND */
        false,     /* AllocKind::SCRIPT */
        true,      /* AllocKind::LAZY_SCRIPT */
        true,      /* AllocKind::SHAPE */
        true,      /* AllocKind::ACCESSOR_SHAPE */
        true,      /* AllocKind::BASE_SHAPE */
        true,      /* AllocKind::OBJECT_GROUP */
        true,      /* AllocKind::FAT_INLINE_STRING */
        true,      /* AllocKind::STRING */
        true,      /* AllocKind::EXTERNAL_STRING */
        true,      /* AllocKind::FAT_INLINE_ATOM */
        true,      /* AllocKind::ATOM */
        true,      /* AllocKind::SYMBOL */
        false,     /* AllocKind::JITCODE */
        true,      /* AllocKind::SCOPE */
        true,      /* AllocKind::REGEXP_SHARED */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT));
    return map[size_t(kind)];
}

} /* namespace gc */
} /* namespace js */

#endif /* gc_AllocKind_h */
