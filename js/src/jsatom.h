/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsatom_h
#define jsatom_h

#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"

#include "jsalloc.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "gc/Rooting.h"
#include "js/GCAPI.h"
#include "js/GCHashTable.h"
#include "vm/CommonPropertyNames.h"

class JSAtom;
class JSAutoByteString;

namespace js {

/*
 * Return a printable, lossless char[] representation of a string-type atom.
 * The lifetime of the result matches the lifetime of bytes.
 */
extern const char*
AtomToPrintableString(JSContext* cx, JSAtom* atom, JSAutoByteString* bytes);

class AtomStateEntry
{
    uintptr_t bits;

    static const uintptr_t NO_TAG_MASK = uintptr_t(-1) - 1;

  public:
    AtomStateEntry() : bits(0) {}
    AtomStateEntry(const AtomStateEntry& other) : bits(other.bits) {}
    AtomStateEntry(JSAtom* ptr, bool tagged)
      : bits(uintptr_t(ptr) | uintptr_t(tagged))
    {
        MOZ_ASSERT((uintptr_t(ptr) & 0x1) == 0);
    }

    bool isPinned() const {
        return bits & 0x1;
    }

    /*
     * Non-branching code sequence. Note that the const_cast is safe because
     * the hash function doesn't consider the tag to be a portion of the key.
     */
    void setPinned(bool pinned) const {
        const_cast<AtomStateEntry*>(this)->bits |= uintptr_t(pinned);
    }

    JSAtom* asPtr(JSContext* cx) const;
    JSAtom* asPtrUnbarriered() const;

    bool needsSweep() {
        JSAtom* atom = asPtrUnbarriered();
        return gc::IsAboutToBeFinalizedUnbarriered(&atom);
    }
};

struct AtomHasher
{
    struct Lookup
    {
        union {
            const JS::Latin1Char* latin1Chars;
            const char16_t* twoByteChars;
        };
        bool isLatin1;
        size_t length;
        const JSAtom* atom; /* Optional. */
        JS::AutoCheckCannotGC nogc;

        HashNumber hash;

        MOZ_ALWAYS_INLINE Lookup(const char16_t* chars, size_t length)
          : twoByteChars(chars), isLatin1(false), length(length), atom(nullptr)
        {
            hash = mozilla::HashString(chars, length);
        }
        MOZ_ALWAYS_INLINE Lookup(const JS::Latin1Char* chars, size_t length)
          : latin1Chars(chars), isLatin1(true), length(length), atom(nullptr)
        {
            hash = mozilla::HashString(chars, length);
        }
        inline explicit Lookup(const JSAtom* atom);
    };

    static HashNumber hash(const Lookup& l) { return l.hash; }
    static MOZ_ALWAYS_INLINE bool match(const AtomStateEntry& entry, const Lookup& lookup);
    static void rekey(AtomStateEntry& k, const AtomStateEntry& newKey) { k = newKey; }
};

using AtomSet = JS::GCHashSet<AtomStateEntry, AtomHasher, SystemAllocPolicy>;

// This class is a wrapper for AtomSet that is used to ensure the AtomSet is
// not modified. It should only expose read-only methods from AtomSet.
// Note however that the atoms within the table can be marked during GC.
class FrozenAtomSet
{
    AtomSet* mSet;

public:
    // This constructor takes ownership of the passed-in AtomSet.
    explicit FrozenAtomSet(AtomSet* set) { mSet = set; }

    ~FrozenAtomSet() { js_delete(mSet); }

    MOZ_ALWAYS_INLINE AtomSet::Ptr readonlyThreadsafeLookup(const AtomSet::Lookup& l) const;

    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return mSet->sizeOfIncludingThis(mallocSizeOf);
    }

    typedef AtomSet::Range Range;

    AtomSet::Range all() const { return mSet->all(); }
};

class PropertyName;

}  /* namespace js */

extern bool
AtomIsPinned(JSContext* cx, JSAtom* atom);

#ifdef DEBUG

// This may be called either with or without the atoms lock held.
extern bool
AtomIsPinnedInRuntime(JSRuntime* rt, JSAtom* atom);

#endif // DEBUG

/* Well-known predefined C strings. */
#define DECLARE_PROTO_STR(name,code,init,clasp) extern const char js_##name##_str[];
JS_FOR_EACH_PROTOTYPE(DECLARE_PROTO_STR)
#undef DECLARE_PROTO_STR

#define DECLARE_CONST_CHAR_STR(idpart, id, text)  extern const char js_##idpart##_str[];
FOR_EACH_COMMON_PROPERTYNAME(DECLARE_CONST_CHAR_STR)
#undef DECLARE_CONST_CHAR_STR

/* Constant strings that are not atomized. */
extern const char js_getter_str[];
extern const char js_send_str[];
extern const char js_setter_str[];

namespace js {

class AutoLockForExclusiveAccess;

/*
 * Atom tracing and garbage collection hooks.
 */
void
TraceAtoms(JSTracer* trc, AutoLockForExclusiveAccess& lock);

void
TracePermanentAtoms(JSTracer* trc);

void
TraceWellKnownSymbols(JSTracer* trc);

/* N.B. must correspond to boolean tagging behavior. */
enum PinningBehavior
{
    DoNotPinAtom = false,
    PinAtom = true
};

extern JSAtom*
Atomize(JSContext* cx, const char* bytes, size_t length,
        js::PinningBehavior pin = js::DoNotPinAtom,
        const mozilla::Maybe<uint32_t>& indexValue = mozilla::Nothing());

template <typename CharT>
extern JSAtom*
AtomizeChars(JSContext* cx, const CharT* chars, size_t length,
             js::PinningBehavior pin = js::DoNotPinAtom);

extern JSAtom*
AtomizeUTF8Chars(JSContext* cx, const char* utf8Chars, size_t utf8ByteLength);

extern JSAtom*
AtomizeString(JSContext* cx, JSString* str, js::PinningBehavior pin = js::DoNotPinAtom);

template <AllowGC allowGC>
extern JSAtom*
ToAtom(JSContext* cx, typename MaybeRooted<Value, allowGC>::HandleType v);

enum XDRMode {
    XDR_ENCODE,
    XDR_DECODE
};

template <XDRMode mode>
class XDRState;

template<XDRMode mode>
bool
XDRAtom(XDRState<mode>* xdr, js::MutableHandleAtom atomp);

#ifdef DEBUG

bool AtomIsMarked(Zone* zone, JSAtom* atom);
bool AtomIsMarked(Zone* zone, jsid id);
bool AtomIsMarked(Zone* zone, const Value& value);

#endif // DEBUG

} /* namespace js */

#endif /* jsatom_h */
