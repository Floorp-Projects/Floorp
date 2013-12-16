/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_Id_h
#define js_Id_h

// A jsid is an identifier for a property or method of an object which is
// either a 31-bit signed integer, interned string or object.
//
// Also, there is an additional jsid value, jsid::voidId(), which does not occur in
// JS scripts but may be used to indicate the absence of a valid jsid.  A void
// jsid is not a valid id and only arises as an exceptional API return value,
// such as in JS_NextProperty. Embeddings must not pass jsid::voidId() into JSAPI
// entry points expecting a jsid and do not need to handle jsid::voidId() in hooks
// receiving a jsid except when explicitly noted in the API contract.
//
// A jsid is not implicitly convertible to or from a jsval; JS_ValueToId or
// JS_IdToValue must be used instead.

#include "mozilla/Attributes.h"
#include "mozilla/NullPtr.h"
 
#include "jstypes.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"

#define JSID_TYPE_STRING                 0x0
#define JSID_TYPE_INT                    0x1
#define JSID_TYPE_VOID                   0x2
#define JSID_TYPE_OBJECT                 0x4
#define JSID_TYPE_MASK                   0x7

struct jsid
{
    size_t asBits;

    jsid() {}

#if !defined(_MSC_VER) && !defined(__sparc)
    // jsid must be POD so that MSVC and SPARC will pass it and return it by
    // value.  (See also bug 689101 and bug 737344, for this problem with
    // respect to JS::Value, which also has a comment like this next to it.)
  private:
#endif
    MOZ_CONSTEXPR jsid(size_t bits) : asBits(bits) {}

  public:
    static MOZ_CONSTEXPR jsid voidId() { return jsid(JSID_TYPE_VOID); }
    static MOZ_CONSTEXPR jsid emptyId() { return jsid(JSID_TYPE_OBJECT); }

    bool operator==(jsid rhs) const { return asBits == rhs.asBits; }
    bool operator!=(jsid rhs) const { return asBits != rhs.asBits; }
};
#define JSID_BITS(id) (id.asBits)

// Avoid using canonical 'id' for jsid parameters since this is a magic word in
// Objective-C++ which, apparently, wants to be able to #include jsapi.h.
#define id iden

static JS_ALWAYS_INLINE bool
JSID_IS_STRING(jsid id)
{
    return (JSID_BITS(id) & JSID_TYPE_MASK) == 0;
}

static JS_ALWAYS_INLINE JSString *
JSID_TO_STRING(jsid id)
{
    JS_ASSERT(JSID_IS_STRING(id));
    return (JSString *)JSID_BITS(id);
}

static JS_ALWAYS_INLINE bool
JSID_IS_ZERO(jsid id)
{
    return JSID_BITS(id) == 0;
}

static JS_ALWAYS_INLINE bool
JSID_IS_INT(jsid id)
{
    return !!(JSID_BITS(id) & JSID_TYPE_INT);
}

static JS_ALWAYS_INLINE int32_t
JSID_TO_INT(jsid id)
{
    JS_ASSERT(JSID_IS_INT(id));
    return ((uint32_t)JSID_BITS(id)) >> 1;
}

#define JSID_INT_MIN  0
#define JSID_INT_MAX  INT32_MAX

static JS_ALWAYS_INLINE bool
INT_FITS_IN_JSID(int32_t i)
{
    return i >= 0;
}

static JS_ALWAYS_INLINE jsid
INT_TO_JSID(int32_t i)
{
    jsid id;
    JS_ASSERT(INT_FITS_IN_JSID(i));
    JSID_BITS(id) = ((i << 1) | JSID_TYPE_INT);
    return id;
}

static JS_ALWAYS_INLINE bool
JSID_IS_OBJECT(jsid id)
{
    return (JSID_BITS(id) & JSID_TYPE_MASK) == JSID_TYPE_OBJECT &&
           (size_t)JSID_BITS(id) != JSID_TYPE_OBJECT;
}

static JS_ALWAYS_INLINE JSObject *
JSID_TO_OBJECT(jsid id)
{
    JS_ASSERT(JSID_IS_OBJECT(id));
    return (JSObject *)(JSID_BITS(id) & ~(size_t)JSID_TYPE_MASK);
}

static JS_ALWAYS_INLINE jsid
OBJECT_TO_JSID(JSObject *obj)
{
    jsid id;
    JS_ASSERT(obj != nullptr);
    JS_ASSERT(((size_t)obj & JSID_TYPE_MASK) == 0);
    JSID_BITS(id) = ((size_t)obj | JSID_TYPE_OBJECT);
    return id;
}

static JS_ALWAYS_INLINE bool
JSID_IS_GCTHING(jsid id)
{
    return JSID_IS_STRING(id) || JSID_IS_OBJECT(id);
}

static JS_ALWAYS_INLINE void *
JSID_TO_GCTHING(jsid id)
{
    return (void *)(JSID_BITS(id) & ~(size_t)JSID_TYPE_MASK);
}

static JS_ALWAYS_INLINE bool
JSID_IS_VOID(const jsid id)
{
    JS_ASSERT_IF(((size_t)JSID_BITS(id) & JSID_TYPE_MASK) == JSID_TYPE_VOID,
                 JSID_BITS(id) == JSID_TYPE_VOID);
    return ((size_t)JSID_BITS(id) == JSID_TYPE_VOID);
}

static JS_ALWAYS_INLINE bool
JSID_IS_EMPTY(const jsid id)
{
    return ((size_t)JSID_BITS(id) == JSID_TYPE_OBJECT);
}

#undef id

extern JS_PUBLIC_DATA(const JS::Handle<jsid>) JSID_VOIDHANDLE;
extern JS_PUBLIC_DATA(const JS::Handle<jsid>) JSID_EMPTYHANDLE;

namespace js {

inline bool
IsPoisonedId(jsid iden)
{
    if (JSID_IS_STRING(iden))
        return JS::IsPoisonedPtr(JSID_TO_STRING(iden));
    if (JSID_IS_OBJECT(iden))
        return JS::IsPoisonedPtr(JSID_TO_OBJECT(iden));
    return false;
}

template <> struct GCMethods<jsid>
{
    static jsid initial() { return jsid::voidId(); }
    static ThingRootKind kind() { return THING_ROOT_ID; }
    static bool poisoned(jsid id) { return IsPoisonedId(id); }
    static bool needsPostBarrier(jsid id) { return false; }
#ifdef JSGC_GENERATIONAL
    static void postBarrier(jsid *idp) {}
    static void relocate(jsid *idp) {}
#endif
};

}

#endif /* js_Id_h */
