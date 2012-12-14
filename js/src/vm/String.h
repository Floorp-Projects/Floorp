/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef String_h_
#define String_h_

#include "mozilla/Attributes.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jsfriendapi.h"
#include "jsstr.h"

#include "gc/Barrier.h"
#include "gc/Heap.h"

class JSString;
class JSDependentString;
class JSUndependedString;
class JSExtensibleString;
class JSExternalString;
class JSLinearString;
class JSStableString;
class JSInlineString;
class JSRope;
class JSAtom;

namespace js {

class StaticStrings;
class PropertyName;

/* The buffer length required to contain any unsigned 32-bit integer. */
static const size_t UINT32_CHAR_BUFFER_LENGTH = sizeof("4294967295") - 1;

} /* namespace js */

/*
 * JavaScript strings
 *
 * Conceptually, a JS string is just an array of chars and a length. This array
 * of chars may or may not be null-terminated and, if it is, the null character
 * is not included in the length.
 *
 * To improve performance of common operations, the following optimizations are
 * made which affect the engine's representation of strings:
 *
 *  - The plain vanilla representation is a "flat" string which consists of a
 *    string header in the GC heap and a malloc'd null terminated char array.
 *
 *  - To avoid copying a substring of an existing "base" string , a "dependent"
 *    string (JSDependentString) can be created which points into the base
 *    string's char array.
 *
 *  - To avoid O(n^2) char buffer copying, a "rope" node (JSRope) can be created
 *    to represent a delayed string concatenation. Concatenation (called
 *    flattening) is performed if and when a linear char array is requested. In
 *    general, ropes form a binary dag whose internal nodes are JSRope string
 *    headers with no associated char array and whose leaf nodes are either flat
 *    or dependent strings.
 *
 *  - To avoid copying the left-hand side when flattening, the left-hand side's
 *    buffer may be grown to make space for a copy of the right-hand side (see
 *    comment in JSString::flatten). This optimization requires that there are
 *    no external pointers into the char array. We conservatively maintain this
 *    property via a flat string's "extensible" property.
 *
 *  - To avoid allocating small char arrays, short strings can be stored inline
 *    in the string header (JSInlineString). To increase the max size of such
 *    inline strings, extra-large string headers can be used (JSShortString).
 *
 *  - To avoid comparing O(n) string equality comparison, strings can be
 *    canonicalized to "atoms" (JSAtom) such that there is a single atom with a
 *    given (length,chars).
 *
 *  - To avoid copying all strings created through the JSAPI, an "external"
 *    string (JSExternalString) can be created whose chars are managed by the
 *    JSAPI client.
 *
 * Although all strings share the same basic memory layout, we can conceptually
 * arrange them into a hierarchy of operations/invariants and represent this
 * hierarchy in C++ with classes:
 *
 * C++ type                     operations+fields / invariants+properties
 * ==========================   =========================================
 * JSString (abstract)          getCharsZ, getChars, length / -
 *  | \
 *  | JSRope                    leftChild, rightChild / -
 *  |
 * JSLinearString (abstract)    chars / might be null-terminated
 *  | \
 *  | JSDependentString         base / -
 *  |
 * JSFlatString                 - / null terminated
 *  |  |
 *  |  +-- JSStableString       - / may have external pointers into char array
 *  |  |
 *  |  +-- JSExternalString     - / char array memory managed by embedding
 *  |  |
 *  |  +-- JSExtensibleString   capacity / no external pointers into char array
 *  |  |
 *  |  +-- JSUndependedString   original dependent base / -
 *  |  |
 *  |  +-- JSInlineString       - / chars stored in header
 *  |         \
 *  |         JSShortString     - / header is fat
 *  |
 * JSAtom                       - / string equality === pointer equality
 *  |
 * js::PropertyName             - / chars don't contain an index (uint32_t)
 *
 * Classes marked with (abstract) above are not literally C++ Abstract Base
 * Classes (since there are no virtual functions, pure or not, in this
 * hierarchy), but have the same meaning: there are no strings with this type as
 * its most-derived type.
 *
 * Technically, there are three additional most-derived types that satisfy the
 * invariants of more than one of the abovementioned most-derived types:
 *  - InlineAtom = JSInlineString + JSAtom (atom with inline chars)
 *  - ShortAtom  = JSShortString  + JSAtom (atom with (more) inline chars)
 *  - StableAtom = JSStableString + JSAtom (atom with out-of-line chars)
 *
 * Derived string types can be queried from ancestor types via isX() and
 * retrieved with asX() debug-only-checked casts.
 *
 * The ensureX() operations mutate 'this' in place to effectively the type to be
 * at least X (e.g., ensureLinear will change a JSRope to be a JSFlatString).
 */

class JSString : public js::gc::Cell
{
  protected:
    static const size_t NUM_INLINE_CHARS = 2 * sizeof(void *) / sizeof(jschar);

    /* Fields only apply to string types commented on the right. */
    struct Data
    {
        size_t                     lengthAndFlags;      /* JSString */
        union {
            const jschar           *chars;              /* JSLinearString */
            JSString               *left;               /* JSRope */
        } u1;
        union {
            jschar                 inlineStorage[NUM_INLINE_CHARS]; /* JS(Inline|Short)String */
            struct {
                union {
                    JSLinearString *base;               /* JS(Dependent|Undepended)String */
                    JSString       *right;              /* JSRope */
                    size_t         capacity;            /* JSFlatString (extensible) */
                    const JSStringFinalizer *externalFinalizer;/* JSExternalString */
                } u2;
                union {
                    JSString       *parent;             /* JSRope (temporary) */
                    size_t         reserved;            /* may use for bug 615290 */
                } u3;
            } s;
        };
    } d;

  public:
    /* Flags exposed only for jits */

    /*
     * The low LENGTH_SHIFT bits of lengthAndFlags are used to encode the type
     * of the string. The remaining bits store the string length (which must be
     * less or equal than MAX_LENGTH).
     *
     * Instead of using a dense index to represent the most-derived type, string
     * types are encoded to allow single-op tests for hot queries (isRope,
     * isDependent, isFlat, isAtom) which, in view of subtyping, would require
     * slower (isX() || isY() || isZ()).
     *
     * The string type encoding can be summarized as follows. The "instance
     * encoding" entry for a type specifies the flag bits used to create a
     * string instance of that type. Abstract types have no instances and thus
     * have no such entry. The "subtype predicate" entry for a type specifies
     * the predicate used to query whether a JSString instance is subtype
     * (reflexively) of that type.
     *
     *   Rope         0000       0000
     *   Linear       -         !0000
     *   HasBase      -          xxx1
     *   Dependent    0001       0001
     *   Flat         -          isLinear && !isDependent
     *   Undepended   0011       0011
     *   Extensible   0010       0010
     *   Inline       0100       isFlat && !isExtensible && (u1.chars == inlineStorage) || isInt32)
     *   Stable       0100       isFlat && !isExtensible && (u1.chars != inlineStorage)
     *   Short        0100       header in FINALIZE_SHORT_STRING arena
     *   External     0100       header in FINALIZE_EXTERNAL_STRING arena
     *   Int32        0110       x110 (NYI, Bug 654190)
     *   Atom         1000       1xxx
     *   InlineAtom   1000       1000 && is Inline
     *   ShortAtom    1000       1000 && is Short
     *   Int32Atom    1110       1110 (NYI, Bug 654190)
     *
     *  "HasBase" here refers to the two string types that have a 'base' field:
     *  JSDependentString and JSUndependedString.
     *  A JSUndependedString is a JSDependentString which has been 'fixed' (by ensureFixed)
     *  to be null-terminated.  In such cases, the string must keep marking its base since
     *  there may be any number of *other* JSDependentStrings transitively depending on it.
     *
     */

    static const size_t LENGTH_SHIFT          = 4;
    static const size_t FLAGS_MASK            = JS_BITMASK(LENGTH_SHIFT);

    static const size_t ROPE_FLAGS            = 0;
    static const size_t DEPENDENT_FLAGS       = JS_BIT(0);
    static const size_t UNDEPENDED_FLAGS      = JS_BIT(0) | JS_BIT(1);
    static const size_t EXTENSIBLE_FLAGS      = JS_BIT(1);
    static const size_t FIXED_FLAGS           = JS_BIT(2);

    static const size_t INT32_MASK            = JS_BITMASK(3);
    static const size_t INT32_FLAGS           = JS_BIT(1) | JS_BIT(2);

    static const size_t HAS_BASE_BIT          = JS_BIT(0);
    static const size_t ATOM_BIT              = JS_BIT(3);

    static const size_t MAX_LENGTH            = JS_BIT(32 - LENGTH_SHIFT) - 1;

    size_t buildLengthAndFlags(size_t length, size_t flags) {
        JS_ASSERT(length <= MAX_LENGTH);
        JS_ASSERT(flags <= FLAGS_MASK);
        return (length << LENGTH_SHIFT) | flags;
    }

    /*
     * Helper function to validate that a string of a given length is
     * representable by a JSString. An allocation overflow is reported if false
     * is returned.
     */
    static inline bool validateLength(JSContext *cx, size_t length);

    static void staticAsserts() {
        JS_STATIC_ASSERT(JS_BITS_PER_WORD >= 32);
        JS_STATIC_ASSERT(((JSString::MAX_LENGTH << JSString::LENGTH_SHIFT) >>
                           JSString::LENGTH_SHIFT) == JSString::MAX_LENGTH);
        JS_STATIC_ASSERT(sizeof(JSString) ==
                         offsetof(JSString, d.inlineStorage) + NUM_INLINE_CHARS * sizeof(jschar));
        JS_STATIC_ASSERT(offsetof(JSString, d.u1.chars) ==
                         offsetof(js::shadow::Atom, chars));
    }

    /* Avoid lame compile errors in JSRope::flatten */
    friend class JSRope;

  public:
    /* All strings have length. */

    JS_ALWAYS_INLINE
    size_t length() const {
        return d.lengthAndFlags >> LENGTH_SHIFT;
    }

    JS_ALWAYS_INLINE
    bool empty() const {
        return d.lengthAndFlags <= FLAGS_MASK;
    }

    /*
     * All strings have a fallible operation to get an array of chars.
     * getCharsZ additionally ensures the array is null terminated.
     */

    inline const jschar *getChars(JSContext *cx);
    inline const jschar *getCharsZ(JSContext *cx);

    /* Fallible conversions to more-derived string types. */

    inline JSLinearString *ensureLinear(JSContext *cx);
    inline JSFlatString *ensureFlat(JSContext *cx);
    inline JSStableString *ensureStable(JSContext *cx);

    static bool ensureLinear(JSContext *cx, JSString *str) {
        return str->ensureLinear(cx) != NULL;
    }

    /* Type query and debug-checked casts */

    JS_ALWAYS_INLINE
    bool isRope() const {
        return (d.lengthAndFlags & FLAGS_MASK) == ROPE_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSRope &asRope() const {
        JS_ASSERT(isRope());
        return *(JSRope *)this;
    }

    JS_ALWAYS_INLINE
    bool isLinear() const {
        return !isRope();
    }

    JS_ALWAYS_INLINE
    JSLinearString &asLinear() const {
        JS_ASSERT(JSString::isLinear());
        return *(JSLinearString *)this;
    }

    JS_ALWAYS_INLINE
    bool isDependent() const {
        return (d.lengthAndFlags & FLAGS_MASK) == DEPENDENT_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSDependentString &asDependent() const {
        JS_ASSERT(isDependent());
        return *(JSDependentString *)this;
    }

    JS_ALWAYS_INLINE
    bool isFlat() const {
        return isLinear() && !isDependent();
    }

    JS_ALWAYS_INLINE
    JSFlatString &asFlat() const {
        JS_ASSERT(isFlat());
        return *(JSFlatString *)this;
    }

    JS_ALWAYS_INLINE
    bool isExtensible() const {
        return (d.lengthAndFlags & FLAGS_MASK) == EXTENSIBLE_FLAGS;
    }

    JS_ALWAYS_INLINE
    JSExtensibleString &asExtensible() const {
        JS_ASSERT(isExtensible());
        return *(JSExtensibleString *)this;
    }

    JS_ALWAYS_INLINE
    bool isInline() const {
        return isFlat() && !isExtensible() && (d.u1.chars == d.inlineStorage);
    }

    JS_ALWAYS_INLINE
    JSInlineString &asInline() const {
        JS_ASSERT(isInline());
        return *(JSInlineString *)this;
    }

    bool isShort() const;

    JS_ALWAYS_INLINE
    JSStableString &asStable() const {
        JS_ASSERT(!isInline());
        return *(JSStableString *)this;
    }

    /* For hot code, prefer other type queries. */
    bool isExternal() const;

    JS_ALWAYS_INLINE
    JSExternalString &asExternal() const {
        JS_ASSERT(isExternal());
        return *(JSExternalString *)this;
    }

    JS_ALWAYS_INLINE
    bool isUndepended() const {
        return (d.lengthAndFlags & FLAGS_MASK) == UNDEPENDED_FLAGS;
    }

    JS_ALWAYS_INLINE
    bool isAtom() const {
        return (d.lengthAndFlags & ATOM_BIT);
    }

    JS_ALWAYS_INLINE
    JSAtom &asAtom() const {
        JS_ASSERT(isAtom());
        return *(JSAtom *)this;
    }

    /* Only called by the GC for dependent or undepended strings. */

    inline bool hasBase() const {
        JS_STATIC_ASSERT((DEPENDENT_FLAGS | JS_BIT(1)) == UNDEPENDED_FLAGS);
        return (d.lengthAndFlags & HAS_BASE_BIT);
    }

    inline JSLinearString *base() const;

    inline void markBase(JSTracer *trc);

    /* Only called by the GC for strings with the FINALIZE_STRING kind. */

    inline void finalize(js::FreeOp *fop);

    /* Gets the number of bytes that the chars take on the heap. */

    size_t sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf);

    /* Offsets for direct field from jit code. */

    static size_t offsetOfLengthAndFlags() {
        return offsetof(JSString, d.lengthAndFlags);
    }

    static size_t offsetOfChars() {
        return offsetof(JSString, d.u1.chars);
    }

    static inline void writeBarrierPre(JSString *str);
    static inline void writeBarrierPost(JSString *str, void *addr);
    static inline bool needWriteBarrierPre(JSCompartment *comp);
    static inline void readBarrier(JSString *str);

    static inline js::ThingRootKind rootKind() { return js::THING_ROOT_STRING; }

#ifdef DEBUG
    void dump();
    static void dumpChars(const jschar *s, size_t len);
    bool equals(const char *s);
#endif

  private:
    JSString() MOZ_DELETE;
    JSString(const JSString &other) MOZ_DELETE;
    void operator=(const JSString &other) MOZ_DELETE;
};

class JSRope : public JSString
{
    enum UsingBarrier { WithIncrementalBarrier, NoBarrier };
    template<UsingBarrier b>
    JSFlatString *flattenInternal(JSContext *cx);

    friend class JSString;
    JSFlatString *flatten(JSContext *cx);

    void init(JSString *left, JSString *right, size_t length);

  public:
    static inline JSRope *new_(JSContext *cx, js::HandleString left,
                               js::HandleString right, size_t length);

    inline JSString *leftChild() const {
        JS_ASSERT(isRope());
        return d.u1.left;
    }

    inline JSString *rightChild() const {
        JS_ASSERT(isRope());
        return d.s.u2.right;
    }

    inline void markChildren(JSTracer *trc);
};

JS_STATIC_ASSERT(sizeof(JSRope) == sizeof(JSString));

class JSLinearString : public JSString
{
    friend class JSString;

    /* Vacuous and therefore unimplemented. */
    JSLinearString *ensureLinear(JSContext *cx) MOZ_DELETE;
    bool isLinear() const MOZ_DELETE;
    JSLinearString &asLinear() const MOZ_DELETE;

  public:
    JS_ALWAYS_INLINE
    const jschar *chars() const {
        JS_ASSERT(JSString::isLinear());
        return d.u1.chars;
    }
};

JS_STATIC_ASSERT(sizeof(JSLinearString) == sizeof(JSString));

class JSDependentString : public JSLinearString
{
    friend class JSString;
    JSFlatString *undepend(JSContext *cx);

    void init(JSLinearString *base, const jschar *chars, size_t length);

    /* Vacuous and therefore unimplemented. */
    bool isDependent() const MOZ_DELETE;
    JSDependentString &asDependent() const MOZ_DELETE;

  public:
    static inline JSLinearString *new_(JSContext *cx, JSLinearString *base,
                                       const jschar *chars, size_t length);
};

JS_STATIC_ASSERT(sizeof(JSDependentString) == sizeof(JSString));

class JSFlatString : public JSLinearString
{
    /* Vacuous and therefore unimplemented. */
    JSFlatString *ensureFlat(JSContext *cx) MOZ_DELETE;
    bool isFlat() const MOZ_DELETE;
    JSFlatString &asFlat() const MOZ_DELETE;

    bool isIndexSlow(uint32_t *indexp) const;

  public:
    JS_ALWAYS_INLINE
    const jschar *charsZ() const {
        JS_ASSERT(JSString::isFlat());
        return chars();
    }

    /*
     * Returns true if this string's characters store an unsigned 32-bit
     * integer value, initializing *indexp to that value if so.  (Thus if
     * calling isIndex returns true, js::IndexToString(cx, *indexp) will be a
     * string equal to this string.)
     */
    inline bool isIndex(uint32_t *indexp) const {
        const jschar *s = chars();
        return JS7_ISDEC(*s) && isIndexSlow(indexp);
    }

    /*
     * Returns a property name represented by this string, or null on failure.
     * You must verify that this is not an index per isIndex before calling
     * this method.
     */
    inline js::PropertyName *toPropertyName(JSContext *cx);

    /*
     * Once a JSFlatString sub-class has been added to the atom state, this
     * operation changes the string to the JSAtom type, in place.
     */
    inline JSAtom *morphAtomizedStringIntoAtom();

    inline void finalize(js::FreeOp *fop);
};

JS_STATIC_ASSERT(sizeof(JSFlatString) == sizeof(JSString));

class JSStableString : public JSFlatString
{
    void init(const jschar *chars, size_t length);

  public:
    static inline JSStableString *new_(JSContext *cx, const jschar *chars, size_t length);

    JS_ALWAYS_INLINE
    JS::StableCharPtr chars() const {
        JS_ASSERT(!JSString::isInline());
        return JS::StableCharPtr(d.u1.chars, length());
    }
};

JS_STATIC_ASSERT(sizeof(JSStableString) == sizeof(JSString));

#if !(defined(JSGC_ROOT_ANALYSIS) || defined(JSGC_USE_EXACT_ROOTING))
namespace js {
/*
 * Specialization of Rooted<T> to explicitly root the string rather than
 * relying on conservative stack scanning.
 *
 * In exact-gc builds, Rooted<T> already keeps the T reachable, so this hack is
 * ifdef'd out. In non-exact-gc builds, conservative scanning would ordinarily
 * pick up the slack. However in the case where the Rooted pointer is no longer
 * used, but some subobject or malloc'd memory with the same lifetime may be
 * used, conservative scanning can fail. JSStableString's chars() method makes
 * it particularly attractive to use that way, so we explicitly keep the
 * JSString gc-reachable for the full lifetime of the Rooted<JSStableString *>.
 *
 * It would suffice simply to force the pointer to remain on the stack, a la
 * JS::Anchor<T>, but for some reason using that voodoo here seems to cause
 * some compilers (clang, VC++ with PGO) to generate incorrect code.
 */
template <>
class Rooted<JSStableString *>
{
  public:
    Rooted(JSContext *cx, JSStableString *initial = NULL
           MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : rooter(cx, initial)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    operator JSStableString *() const { return get(); }
    JSStableString * operator ->() const { return get(); }
    JSStableString ** address() { return reinterpret_cast<JSStableString **>(rooter.addr()); }
    JSStableString * const * address() const {
        return reinterpret_cast<JSStableString * const *>(rooter.addr());
    }
    JSStableString * get() const { return static_cast<JSStableString *>(rooter.string()); }

    Rooted & operator =(JSStableString *value)
    {
        JS_ASSERT(!RootMethods<JSStableString *>::poisoned(value));
        rooter.setString(value);
        return *this;
    }

    Rooted & operator =(const Rooted &value)
    {
        rooter.setString(value.get());
        return *this;
    }

  private:
    JS::AutoStringRooter rooter;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    Rooted(const Rooted &) MOZ_DELETE;
};
}
#endif

class JSExtensibleString : public JSFlatString
{
    /* Vacuous and therefore unimplemented. */
    bool isExtensible() const MOZ_DELETE;
    JSExtensibleString &asExtensible() const MOZ_DELETE;

  public:
    JS_ALWAYS_INLINE
    size_t capacity() const {
        JS_ASSERT(JSString::isExtensible());
        return d.s.u2.capacity;
    }
};

JS_STATIC_ASSERT(sizeof(JSExtensibleString) == sizeof(JSString));

class JSInlineString : public JSFlatString
{
    static const size_t MAX_INLINE_LENGTH = NUM_INLINE_CHARS - 1;

  public:
    static inline JSInlineString *new_(JSContext *cx);

    inline jschar *init(size_t length);

    JSStableString *uninline(JSContext *cx);

    inline void resetLength(size_t length);

    static bool lengthFits(size_t length) {
        return length <= MAX_INLINE_LENGTH;
    }
};

JS_STATIC_ASSERT(sizeof(JSInlineString) == sizeof(JSString));

class JSShortString : public JSInlineString
{
    /* This can be any value that is a multiple of CellSize. */
    static const size_t INLINE_EXTENSION_CHARS = sizeof(JSString::Data) / sizeof(jschar);

    static void staticAsserts() {
        JS_STATIC_ASSERT(INLINE_EXTENSION_CHARS % js::gc::CellSize == 0);
        JS_STATIC_ASSERT(MAX_SHORT_LENGTH + 1 ==
                         (sizeof(JSShortString) -
                          offsetof(JSShortString, d.inlineStorage)) / sizeof(jschar));
    }

  protected: /* to fool clang into not warning this is unused */
    jschar inlineStorageExtension[INLINE_EXTENSION_CHARS];

  public:
    static inline JSShortString *new_(JSContext *cx);

    static const size_t MAX_SHORT_LENGTH = JSString::NUM_INLINE_CHARS +
                                           INLINE_EXTENSION_CHARS
                                           -1 /* null terminator */;

    static bool lengthFits(size_t length) {
        return length <= MAX_SHORT_LENGTH;
    }

    /* Only called by the GC for strings with the FINALIZE_SHORT_STRING kind. */

    JS_ALWAYS_INLINE void finalize(js::FreeOp *fop);
};

JS_STATIC_ASSERT(sizeof(JSShortString) == 2 * sizeof(JSString));

class JSExternalString : public JSFlatString
{
    void init(const jschar *chars, size_t length, const JSStringFinalizer *fin);

    /* Vacuous and therefore unimplemented. */
    bool isExternal() const MOZ_DELETE;
    JSExternalString &asExternal() const MOZ_DELETE;

  public:
    static inline JSExternalString *new_(JSContext *cx, const jschar *chars, size_t length,
                                         const JSStringFinalizer *fin);

    const JSStringFinalizer *externalFinalizer() const {
        JS_ASSERT(JSString::isExternal());
        return d.s.u2.externalFinalizer;
    }

    /* Only called by the GC for strings with the FINALIZE_EXTERNAL_STRING kind. */

    inline void finalize(js::FreeOp *fop);
};

JS_STATIC_ASSERT(sizeof(JSExternalString) == sizeof(JSString));

class JSUndependedString : public JSFlatString
{
    /*
     * JSUndependedString is not explicitly used and is only present for
     * consistency. See JSDependentString::undepend for how a JSDependentString
     * gets morphed into a JSUndependedString.
     */
};

JS_STATIC_ASSERT(sizeof(JSUndependedString) == sizeof(JSString));

class JSAtom : public JSFlatString
{
    /* Vacuous and therefore unimplemented. */
    bool isAtom() const MOZ_DELETE;
    JSAtom &asAtom() const MOZ_DELETE;

  public:
    /* Returns the PropertyName for this.  isIndex() must be false. */
    inline js::PropertyName *asPropertyName();

    inline void finalize(js::FreeOp *fop);

#ifdef DEBUG
    void dump();
#endif
};

JS_STATIC_ASSERT(sizeof(JSAtom) == sizeof(JSString));

namespace js {

class StaticStrings
{
  private:
    /* Bigger chars cannot be in a length-2 string. */
    static const size_t SMALL_CHAR_LIMIT    = 128U;
    static const size_t NUM_SMALL_CHARS     = 64U;

    JSAtom *length2StaticTable[NUM_SMALL_CHARS * NUM_SMALL_CHARS];

    void clear() {
        PodArrayZero(unitStaticTable);
        PodArrayZero(length2StaticTable);
        PodArrayZero(intStaticTable);
    }

  public:
    /* We keep these public for the JITs. */
    static const size_t UNIT_STATIC_LIMIT   = 256U;
    JSAtom *unitStaticTable[UNIT_STATIC_LIMIT];

    static const size_t INT_STATIC_LIMIT    = 256U;
    JSAtom *intStaticTable[INT_STATIC_LIMIT];

    StaticStrings() {
        clear();
    }

    bool init(JSContext *cx);
    void trace(JSTracer *trc);
    void finish() {
        clear();
    }

    static inline bool hasUint(uint32_t u);
    inline JSAtom *getUint(uint32_t u);

    static inline bool hasInt(int32_t i);
    inline JSAtom *getInt(int32_t i);

    static inline bool hasUnit(jschar c);
    JSAtom *getUnit(jschar c);

    /* May not return atom, returns null on (reported) failure. */
    inline JSLinearString *getUnitStringForElement(JSContext *cx, JSString *str, size_t index);

    static bool isStatic(JSAtom *atom);

    /* Return null if no static atom exists for the given (chars, length). */
    inline JSAtom *lookup(const jschar *chars, size_t length);

  private:
    typedef uint8_t SmallChar;
    static const SmallChar INVALID_SMALL_CHAR = -1;

    static inline bool fitsInSmallChar(jschar c);

    static const SmallChar toSmallChar[];

    JSAtom *getLength2(jschar c1, jschar c2);
    JSAtom *getLength2(uint32_t u);
};

/*
 * Represents an atomized string which does not contain an index (that is, an
 * unsigned 32-bit value).  Thus for any PropertyName propname,
 * ToString(ToUint32(propname)) never equals propname.
 *
 * To more concretely illustrate the utility of PropertyName, consider that it
 * is used to partition, in a type-safe manner, the ways to refer to a
 * property, as follows:
 *
 *   - uint32_t indexes,
 *   - PropertyName strings which don't encode uint32_t indexes, and
 *   - jsspecial special properties (non-ES5 properties like object-valued
 *     jsids, JSID_EMPTY, JSID_VOID, E4X's default XML namespace, and maybe in
 *     the future Harmony-proposed private names).
 */
class PropertyName : public JSAtom
{};

JS_STATIC_ASSERT(sizeof(PropertyName) == sizeof(JSString));

static JS_ALWAYS_INLINE jsid
NameToId(PropertyName *name)
{
    return NON_INTEGER_ATOM_TO_JSID(name);
}

typedef HeapPtr<JSAtom> HeapPtrAtom;

class AutoNameVector : public AutoVectorRooter<PropertyName *>
{
    typedef AutoVectorRooter<PropertyName *> BaseType;
  public:
    explicit AutoNameVector(JSContext *cx
                            JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<PropertyName *>(cx, NAMEVECTOR)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    HandlePropertyName operator[](size_t i) const {
        return HandlePropertyName::fromMarkedLocation(&BaseType::operator[](i));
    }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

/* Avoid requiring vm/String-inl.h just to call getChars. */

JS_ALWAYS_INLINE const jschar *
JSString::getChars(JSContext *cx)
{
    JS::AutoAssertNoGC nogc;
    if (JSLinearString *str = ensureLinear(cx))
        return str->chars();
    return NULL;
}

JS_ALWAYS_INLINE const jschar *
JSString::getCharsZ(JSContext *cx)
{
    JS::AutoAssertNoGC nogc;
    if (JSFlatString *str = ensureFlat(cx))
        return str->chars();
    return NULL;
}

JS_ALWAYS_INLINE JSLinearString *
JSString::ensureLinear(JSContext *cx)
{
    JS::AutoAssertNoGC nogc;
    return isLinear()
           ? &asLinear()
           : asRope().flatten(cx);
}

JS_ALWAYS_INLINE JSFlatString *
JSString::ensureFlat(JSContext *cx)
{
    JS::AutoAssertNoGC nogc;
    return isFlat()
           ? &asFlat()
           : isDependent()
             ? asDependent().undepend(cx)
             : asRope().flatten(cx);
}

JS_ALWAYS_INLINE JSStableString *
JSString::ensureStable(JSContext *maybecx)
{
    JS::AutoAssertNoGC nogc;
    if (isRope()) {
        JSFlatString *flat = asRope().flatten(maybecx);
        if (!flat)
            return NULL;
        JS_ASSERT(!flat->isInline());
        return &flat->asStable();
    }

    if (isDependent()) {
        JSFlatString *flat = asDependent().undepend(maybecx);
        if (!flat)
            return NULL;
        return &flat->asStable();
    }

    if (!isInline())
        return &asStable();

    JS_ASSERT(isInline());
    return asInline().uninline(maybecx);
}

inline JSLinearString *
JSString::base() const
{
    JS_ASSERT(hasBase());
    JS_ASSERT(!d.s.u2.base->isInline());
    return d.s.u2.base;
}

inline js::PropertyName *
JSAtom::asPropertyName()
{
#ifdef DEBUG
    uint32_t dummy;
    JS_ASSERT(!isIndex(&dummy));
#endif
    return static_cast<js::PropertyName *>(this);
}

#endif
