/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsatom_h___
#define jsatom_h___

#include <stddef.h>
#include "jsalloc.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jslock.h"
#include "jsversion.h"

#include "gc/Barrier.h"
#include "js/HashTable.h"
#include "mozilla/HashFunctions.h"

struct JSIdArray {
    int length;
    js::HeapId vector[1];    /* actually, length jsid words */
};

namespace js {

JS_STATIC_ASSERT(sizeof(HashNumber) == 4);

static JS_ALWAYS_INLINE js::HashNumber
HashId(jsid id)
{
    return HashGeneric(JSID_BITS(id));
}

template<>
struct DefaultHasher<jsid>
{
    typedef jsid Lookup;
    static HashNumber hash(const Lookup &l) {
        return HashNumber(JSID_BITS(l));
    }
    static bool match(const jsid &id, const Lookup &l) {
        return id == l;
    }
};

} /* namespace js */

/*
 * Return a printable, lossless char[] representation of a string-type atom.
 * The lifetime of the result matches the lifetime of bytes.
 */
extern const char *
js_AtomToPrintableString(JSContext *cx, JSAtom *atom, JSAutoByteString *bytes);

namespace js {

/* Compute a hash function from chars/length. */
inline uint32_t
HashChars(const jschar *chars, size_t length)
{
    uint32_t h = 0;
    for (; length; chars++, length--)
        h = JS_ROTATE_LEFT32(h, 4) ^ *chars;
    return h;
}

class AtomStateEntry
{
    uintptr_t bits;

    static const uintptr_t NO_TAG_MASK = uintptr_t(-1) - 1;

  public:
    AtomStateEntry() : bits(0) {}
    AtomStateEntry(const AtomStateEntry &other) : bits(other.bits) {}
    AtomStateEntry(JSAtom *ptr, bool tagged)
      : bits(uintptr_t(ptr) | uintptr_t(tagged))
    {
        JS_ASSERT((uintptr_t(ptr) & 0x1) == 0);
    }

    bool isTagged() const {
        return bits & 0x1;
    }

    /*
     * Non-branching code sequence. Note that the const_cast is safe because
     * the hash function doesn't consider the tag to be a portion of the key.
     */
    void setTagged(bool enabled) const {
        const_cast<AtomStateEntry *>(this)->bits |= uintptr_t(enabled);
    }

    JSAtom *asPtr() const;
};

struct AtomHasher
{
    struct Lookup
    {
        const jschar    *chars;
        size_t          length;
        const JSAtom    *atom; /* Optional. */

        Lookup(const jschar *chars, size_t length) : chars(chars), length(length), atom(NULL) {}
        inline Lookup(const JSAtom *atom);
    };

    static HashNumber hash(const Lookup &l) { return HashChars(l.chars, l.length); }
    static inline bool match(const AtomStateEntry &entry, const Lookup &lookup);
};

typedef HashSet<AtomStateEntry, AtomHasher, SystemAllocPolicy> AtomSet;

/*
 * On encodings:
 *
 * - Some string functions have an optional FlationCoding argument that allow
 *   the caller to force CESU-8 encoding handling.
 * - Functions that don't take a FlationCoding base their NormalEncoding
 *   behavior on the js_CStringsAreUTF8 value. NormalEncoding is either raw
 *   (simple zero-extension) or UTF-8 depending on js_CStringsAreUTF8.
 * - Functions that explicitly state their encoding do not use the
 *   js_CStringsAreUTF8 value.
 *
 * CESU-8 (Compatibility Encoding Scheme for UTF-16: 8-bit) is a variant of
 * UTF-8 that allows us to store any wide character string as a narrow
 * character string. For strings containing mostly ascii, it saves space.
 * http://www.unicode.org/reports/tr26/
 */

enum FlationCoding
{
    NormalEncoding,
    CESU8Encoding
};

class PropertyName;

}  /* namespace js */

struct JSAtomState
{
    js::AtomSet         atoms;

    /*
     * From this point until the end of struct definition the struct must
     * contain only js::PropertyName fields. We use this to access the storage
     * occupied by the common atoms in js_FinishCommonAtoms.
     *
     * js_common_atom_names defined in jsatom.cpp contains C strings for atoms
     * in the order of atom fields here. Therefore you must update that array
     * if you change member order here.
     */

    /* The rt->emptyString atom, see jsstr.c's js_InitRuntimeStringState. */
    js::PropertyName    *emptyAtom;

    /*
     * Literal value and type names.
     * NB: booleanAtoms must come right before typeAtoms!
     */
    js::PropertyName    *booleanAtoms[2];
    js::PropertyName    *typeAtoms[JSTYPE_LIMIT];
    js::PropertyName    *nullAtom;

    /* Standard class constructor or prototype names. */
    js::PropertyName    *classAtoms[JSProto_LIMIT];

    /* Various built-in or commonly-used atoms, pinned on first context. */
#define DEFINE_ATOM(id, text)          js::PropertyName *id##Atom;
#define DEFINE_PROTOTYPE_ATOM(id)      js::PropertyName *id##Atom;
#define DEFINE_KEYWORD_ATOM(id)        js::PropertyName *id##Atom;
#include "jsatom.tbl"
#undef DEFINE_ATOM
#undef DEFINE_PROTOTYPE_ATOM
#undef DEFINE_KEYWORD_ATOM

    static const size_t commonAtomsOffset;

    void junkAtoms() {
#ifdef DEBUG
        memset(commonAtomsStart(), JS_FREE_PATTERN, sizeof(*this) - commonAtomsOffset);
#endif
    }

    JSAtom **commonAtomsStart() {
        return reinterpret_cast<JSAtom **>(&emptyAtom);
    }

    void checkStaticInvariants();
};

extern bool
AtomIsInterned(JSContext *cx, JSAtom *atom);

#define ATOM(name) js::HandlePropertyName::fromMarkedLocation(&cx->runtime->atomState.name##Atom)

#define COMMON_ATOM_INDEX(name)                                               \
    ((offsetof(JSAtomState, name##Atom) - JSAtomState::commonAtomsOffset)     \
     / sizeof(JSAtom*))
#define COMMON_TYPE_ATOM_INDEX(type)                                          \
    ((offsetof(JSAtomState, typeAtoms[type]) - JSAtomState::commonAtomsOffset)\
     / sizeof(JSAtom*))

#define NAME_OFFSET(name)       offsetof(JSAtomState, name##Atom)
#define OFFSET_TO_NAME(rt,off)  (*(js::PropertyName **)((char*)&(rt)->atomState + (off)))
#define CLASS_NAME_OFFSET(name) offsetof(JSAtomState, classAtoms[JSProto_##name])
#define CLASS_NAME(cx,name)     ((cx)->runtime->atomState.classAtoms[JSProto_##name])

extern const char *const js_common_atom_names[];
extern const size_t      js_common_atom_count;

/*
 * Macros to access C strings for JSType and boolean literals.
 */
#define JS_BOOLEAN_STR(type) (js_common_atom_names[1 + (type)])
#define JS_TYPE_STR(type)    (js_common_atom_names[1 + 2 + (type)])

/* Type names. */
extern const char   js_object_str[];
extern const char   js_undefined_str[];

/* Well-known predefined C strings. */
#define JS_PROTO(name,code,init) extern const char js_##name##_str[];
#include "jsproto.tbl"
#undef JS_PROTO

#define DEFINE_ATOM(id, text)  extern const char js_##id##_str[];
#define DEFINE_PROTOTYPE_ATOM(id)
#define DEFINE_KEYWORD_ATOM(id)
#include "jsatom.tbl"
#undef DEFINE_ATOM
#undef DEFINE_PROTOTYPE_ATOM
#undef DEFINE_KEYWORD_ATOM

#if JS_HAS_GENERATORS
extern const char   js_close_str[];
extern const char   js_send_str[];
#endif

/* Constant strings that are not atomized. */
extern const char   js_getter_str[];
extern const char   js_setter_str[];

namespace js {

/*
 * Initialize atom state. Return true on success, false on failure to allocate
 * memory. The caller must zero rt->atomState before calling this function and
 * only call it after js_InitGC successfully returns.
 */
extern JSBool
InitAtomState(JSRuntime *rt);

/*
 * Free and clear atom state including any interned string atoms. This
 * function must be called before js_FinishGC.
 */
extern void
FinishAtomState(JSRuntime *rt);

/*
 * Atom tracing and garbage collection hooks.
 */
extern void
MarkAtomState(JSTracer *trc);

extern void
SweepAtomState(JSRuntime *rt);

extern bool
InitCommonAtoms(JSContext *cx);

extern void
FinishCommonAtoms(JSRuntime *rt);

/* N.B. must correspond to boolean tagging behavior. */
enum InternBehavior
{
    DoNotInternAtom = false,
    InternAtom = true
};

extern JSAtom *
Atomize(JSContext *cx, const char *bytes, size_t length,
        js::InternBehavior ib = js::DoNotInternAtom,
        js::FlationCoding fc = js::NormalEncoding);

extern JSAtom *
AtomizeChars(JSContext *cx, const jschar *chars, size_t length,
             js::InternBehavior ib = js::DoNotInternAtom);

extern JSAtom *
AtomizeString(JSContext *cx, JSString *str, js::InternBehavior ib = js::DoNotInternAtom);

inline JSAtom *
ToAtom(JSContext *cx, const js::Value &v);

bool
InternNonIntElementId(JSContext *cx, JSObject *obj, const Value &idval,
                      jsid *idp, MutableHandleValue vp);

inline bool
InternNonIntElementId(JSContext *cx, JSObject *obj, const Value &idval, jsid *idp)
{
    RootedValue dummy(cx);
    return InternNonIntElementId(cx, obj, idval, idp, &dummy);
}

template<XDRMode mode>
bool
XDRAtom(XDRState<mode> *xdr, JSAtom **atomp);

} /* namespace js */

#endif /* jsatom_h___ */
