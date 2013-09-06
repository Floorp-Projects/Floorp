/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsatom_h
#define jsatom_h

#include "mozilla/HashFunctions.h"

#include "jsalloc.h"

#include "gc/Barrier.h"
#include "gc/Rooting.h"
#include "vm/CommonPropertyNames.h"

class JSAtom;
class JSAutoByteString;

struct JSIdArray {
    int length;
    js::HeapId vector[1];    /* actually, length jsid words */
};

namespace js {

JS_STATIC_ASSERT(sizeof(HashNumber) == 4);

static JS_ALWAYS_INLINE js::HashNumber
HashId(jsid id)
{
    return mozilla::HashGeneric(JSID_BITS(id));
}

struct JsidHasher
{
    typedef jsid Lookup;
    static HashNumber hash(const Lookup &l) {
        return HashNumber(JSID_BITS(l));
    }
    static bool match(const jsid &id, const Lookup &l) {
        return id == l;
    }
};

/*
 * Return a printable, lossless char[] representation of a string-type atom.
 * The lifetime of the result matches the lifetime of bytes.
 */
extern const char *
AtomToPrintableString(ExclusiveContext *cx, JSAtom *atom, JSAutoByteString *bytes);

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

    static HashNumber hash(const Lookup &l) { return mozilla::HashString(l.chars, l.length); }
    static inline bool match(const AtomStateEntry &entry, const Lookup &lookup);
    static void rekey(AtomStateEntry &k, const AtomStateEntry& newKey) { k = newKey; }
};

typedef HashSet<AtomStateEntry, AtomHasher, SystemAllocPolicy> AtomSet;

class PropertyName;

}  /* namespace js */

extern bool
AtomIsInterned(JSContext *cx, JSAtom *atom);

/* Well-known predefined C strings. */
#define DECLARE_PROTO_STR(name,code,init) extern const char js_##name##_str[];
JS_FOR_EACH_PROTOTYPE(DECLARE_PROTO_STR)
#undef DECLARE_PROTO_STR

#define DECLARE_CONST_CHAR_STR(idpart, id, text)  extern const char js_##idpart##_str[];
FOR_EACH_COMMON_PROPERTYNAME(DECLARE_CONST_CHAR_STR)
#undef DECLARE_CONST_CHAR_STR

/* Constant strings that are not atomized. */
extern const char js_break_str[];
extern const char js_case_str[];
extern const char js_catch_str[];
extern const char js_class_str[];
extern const char js_close_str[];
extern const char js_const_str[];
extern const char js_continue_str[];
extern const char js_debugger_str[];
extern const char js_default_str[];
extern const char js_do_str[];
extern const char js_else_str[];
extern const char js_enum_str[];
extern const char js_export_str[];
extern const char js_extends_str[];
extern const char js_finally_str[];
extern const char js_for_str[];
extern const char js_getter_str[];
extern const char js_if_str[];
extern const char js_implements_str[];
extern const char js_import_str[];
extern const char js_in_str[];
extern const char js_instanceof_str[];
extern const char js_interface_str[];
extern const char js_let_str[];
extern const char js_new_str[];
extern const char js_package_str[];
extern const char js_private_str[];
extern const char js_protected_str[];
extern const char js_public_str[];
extern const char js_send_str[];
extern const char js_setter_str[];
extern const char js_static_str[];
extern const char js_super_str[];
extern const char js_switch_str[];
extern const char js_this_str[];
extern const char js_try_str[];
extern const char js_typeof_str[];
extern const char js_void_str[];
extern const char js_while_str[];
extern const char js_with_str[];

namespace js {

extern const char * const TypeStrings[];

/*
 * Initialize atom state. Return true on success, false on failure to allocate
 * memory. The caller must zero rt->atomState before calling this function and
 * only call it after js_InitGC successfully returns.
 */
extern bool
InitAtoms(JSRuntime *rt);

/*
 * Free and clear atom state including any interned string atoms. This
 * function must be called before js_FinishGC.
 */
extern void
FinishAtoms(JSRuntime *rt);

/*
 * Atom tracing and garbage collection hooks.
 */
extern void
MarkAtoms(JSTracer *trc);

extern void
SweepAtoms(JSRuntime *rt);

extern bool
InitCommonNames(JSContext *cx);

extern void
FinishCommonNames(JSRuntime *rt);

/* N.B. must correspond to boolean tagging behavior. */
enum InternBehavior
{
    DoNotInternAtom = false,
    InternAtom = true
};

extern JSAtom *
Atomize(ExclusiveContext *cx, const char *bytes, size_t length,
        js::InternBehavior ib = js::DoNotInternAtom);

template <AllowGC allowGC>
extern JSAtom *
AtomizeChars(ExclusiveContext *cx, const jschar *chars, size_t length,
             js::InternBehavior ib = js::DoNotInternAtom);

template <AllowGC allowGC>
extern JSAtom *
AtomizeString(ExclusiveContext *cx, JSString *str, js::InternBehavior ib = js::DoNotInternAtom);

template <AllowGC allowGC>
extern JSAtom *
ToAtom(ExclusiveContext *cx, typename MaybeRooted<Value, allowGC>::HandleType v);

enum XDRMode {
    XDR_ENCODE,
    XDR_DECODE
};

template <XDRMode mode>
class XDRState;

template<XDRMode mode>
bool
XDRAtom(XDRState<mode> *xdr, js::MutableHandleAtom atomp);

} /* namespace js */

#endif /* jsatom_h */
