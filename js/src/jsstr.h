/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsstr_h___
#define jsstr_h___
/*
 * JS string type implementation.
 *
 * A JS string is a counted array of unicode characters.  To support handoff
 * of API client memory, the chars are allocated separately from the length,
 * necessitating a pointer after the count, to form a separately allocated
 * string descriptor.  String descriptors are GC'ed, while their chars are
 * allocated from the malloc heap.
 */
#include <ctype.h>
#include "jsapi.h"
#include "jsprvtd.h"
#include "jshashtable.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsvalue.h"
#include "jscell.h"

#define JSSTRING_BIT(n)             ((size_t)1 << (n))
#define JSSTRING_BITMASK(n)         (JSSTRING_BIT(n) - 1)

enum {
    UNIT_STRING_LIMIT        = 256U,
    SMALL_CHAR_LIMIT         = 128U, /* Bigger chars cannot be in a length-2 string. */
    NUM_SMALL_CHARS          = 64U,
    INT_STRING_LIMIT         = 256U,
    NUM_HUNDRED_STRINGS      = 156U
};

extern JSStringFinalizeOp str_finalizers[8];

extern jschar *
js_GetDependentStringChars(JSString *str);

extern JSString * JS_FASTCALL
js_ConcatStrings(JSContext *cx, JSString *left, JSString *right);

JS_STATIC_ASSERT(JS_BITS_PER_WORD >= 32);

struct JSRopeBufferInfo {
    /* Number of jschars we can hold, not including null terminator. */
    size_t capacity;
};

/* Forward declaration for friending. */
namespace js { namespace mjit {
    class Compiler;
}}

/*
 * The GC-thing "string" type.
 *
 * In FLAT strings, the mChars field  points to a flat character array owned by
 * its GC-thing descriptor. The array is terminated at index length by a zero
 * character and the size of the array in bytes is
 * (length + 1) * sizeof(jschar). The terminator is purely a backstop, in case
 * the chars pointer flows out to native code that requires \u0000 termination.
 *
 * A flat string with the ATOMIZED flag means that the string is hashed as
 * an atom. This flag is used to avoid re-hashing the already-atomized string.
 *
 * A flat string with the EXTENSIBLE flag means that the string may change into
 * a dependent string as part of an optimization with js_ConcatStrings:
 * extending |str1 = "abc"| with the character |str2 = str1 + "d"| will place
 * "d" in the extra capacity from |str1|, make that the buffer for |str2|, and
 * turn |str1| into a dependent string of |str2|.
 *
 * Flat strings without the EXTENSIBLE flag can be safely accessed by multiple
 * threads.
 *
 * When the string is DEPENDENT, the string depends on characters of another
 * string strongly referenced by the mBase field. The base member may point to
 * another dependent string if chars() has not been called yet.
 *
 * To optimize js_ConcatStrings and some other cases, we lazily concatenate
 * strings when possible, creating concatenation trees, a.k.a. ropes. A string
 * is an INTERIOR_NODE if it is a non-root, non-leaf node in a rope, and a
 * string is a TOP_NODE if it is the root of a rope. In order to meet API
 * requirements, chars() is not allowed to fail, so we build ropes so that they
 * form a well-defined tree structure, and the top node of every rope contains
 * an (almost) empty buffer that is large enough to contain the entire string.
 * Whenever chars() is called on a rope, it traverses its tree and fills that
 * buffer in, and when concatenating strings, we reuse these empty buffers
 * whenever possible, so that we can build a string through concatenation in
 * linear time, and have relatively few malloc calls when doing so.
 *
 * NB: Always use the length() and chars() accessor methods.
 */
struct JSString {
    friend class js::TraceRecorder;
    friend class js::mjit::Compiler;

    friend JSAtom *
    js_AtomizeString(JSContext *cx, JSString *str, uintN flags);
 public:
    /*
     * Not private because we want to be able to use static
     * initializers for them. Don't use these directly!
     */
    size_t                          mLengthAndFlags;  /* in all strings */
    union {
        jschar                      *mChars; /* in flat and dependent strings */
        JSString                    *mLeft;  /* in rope interior and top nodes */
    };
    union {
        /*
         * We may keep more than 4 inline chars, but 4 is necessary for all of
         * our static initialization.
         */
        jschar                      mInlineStorage[4]; /* In short strings. */
        struct {
            union {
                size_t              mCapacity; /* in extensible flat strings (optional) */
                JSString            *mParent; /* in rope interior nodes */
                JSRopeBufferInfo    *mBufferWithInfo; /* in rope top nodes */
            };
            union {
                JSString            *mBase;  /* in dependent strings */
                JSString            *mRight; /* in rope interior and top nodes */
            };
        } e;
    };

    /*
     * The mLengthAndFlags field in string headers has data arranged in the
     * following way:
     *
     * [ length (bits 4-31) ][ flags (bits 2-3) ][ type (bits 0-1) ]
     *
     * The length is packed in mLengthAndFlags, even in string types that don't
     * need 3 other fields, to make the length check simpler.
     *
     * When the string type is FLAT, the flags can contain ATOMIZED or
     * EXTENSIBLE.
     *
     * When the string type is INTERIOR_NODE or TOP_NODE, the flags area is
     * used to store the rope traversal count.
     */
    static const size_t FLAT =          0;
    static const size_t DEPENDENT =     1;
    static const size_t INTERIOR_NODE = 2;
    static const size_t TOP_NODE =      3;

    /* Rope/non-rope can be checked by checking one bit. */
    static const size_t ROPE_BIT = JSSTRING_BIT(1);

    static const size_t ATOMIZED = JSSTRING_BIT(2);
    static const size_t EXTENSIBLE = JSSTRING_BIT(3);

    static const size_t FLAGS_LENGTH_SHIFT = 4;

    static const size_t ROPE_TRAVERSAL_COUNT_SHIFT = 2;
    static const size_t ROPE_TRAVERSAL_COUNT_MASK = JSSTRING_BITMASK(4) -
                                                    JSSTRING_BITMASK(2);
    static const size_t ROPE_TRAVERSAL_COUNT_UNIT =
                                (1 << ROPE_TRAVERSAL_COUNT_SHIFT);

    static const size_t TYPE_MASK = JSSTRING_BITMASK(2);
    static const size_t TYPE_FLAGS_MASK = JSSTRING_BITMASK(4);

    inline bool hasFlag(size_t flag) const {
        return (mLengthAndFlags & flag) != 0;
    }

    inline js::gc::Cell *asCell() {
        return reinterpret_cast<js::gc::Cell *>(this);
    }
    
    inline js::gc::FreeCell *asFreeCell() {
        return reinterpret_cast<js::gc::FreeCell *>(this);
    }

    /*
     * Generous but sane length bound; the "-1" is there for comptibility with
     * OOM tests.
     */
    static const size_t MAX_LENGTH = (1 << 28) - 1;

    inline size_t type() const {
        return mLengthAndFlags & TYPE_MASK;
    }

    JS_ALWAYS_INLINE bool isDependent() const {
        return type() == DEPENDENT;
    }

    JS_ALWAYS_INLINE bool isFlat() const {
        return type() == FLAT;
    }

    inline bool isExtensible() const {
        return isFlat() && hasFlag(EXTENSIBLE);
    }

    inline bool isRope() const {
        return hasFlag(ROPE_BIT);
    }

    JS_ALWAYS_INLINE bool isAtomized() const {
        return isFlat() && hasFlag(ATOMIZED);
    }

    inline bool isInteriorNode() const {
        return type() == INTERIOR_NODE;
    }

    inline bool isTopNode() const {
        return type() == TOP_NODE;
    }

    JS_ALWAYS_INLINE jschar *chars() {
        if (JS_UNLIKELY(isRope()))
            flatten();
        return mChars;
    }

    JS_ALWAYS_INLINE size_t length() const {
        return mLengthAndFlags >> FLAGS_LENGTH_SHIFT;
    }

    JS_ALWAYS_INLINE bool empty() const {
        return length() == 0;
    }

    JS_ALWAYS_INLINE void getCharsAndLength(const jschar *&chars, size_t &length) {
        chars = this->chars();
        length = this->length();
    }

    JS_ALWAYS_INLINE void getCharsAndEnd(const jschar *&chars, const jschar *&end) {
        end = length() + (chars = this->chars());
    }

    JS_ALWAYS_INLINE jschar *inlineStorage() {
        JS_ASSERT(isFlat());
        return mInlineStorage;
    }

    /* Specific flat string initializer and accessor methods. */
    JS_ALWAYS_INLINE void initFlat(jschar *chars, size_t length) {
        JS_ASSERT(length <= MAX_LENGTH);
        JS_ASSERT(!isStatic(this));
        e.mBase = NULL;
        e.mCapacity = 0;
        mLengthAndFlags = (length << FLAGS_LENGTH_SHIFT) | FLAT;
        mChars = chars;
    }

    JS_ALWAYS_INLINE void initFlatExtensible(jschar *chars, size_t length, size_t cap) {
        JS_ASSERT(length <= MAX_LENGTH);
        JS_ASSERT(!isStatic(this));
        e.mBase = NULL;
        e.mCapacity = cap;
        mLengthAndFlags = (length << FLAGS_LENGTH_SHIFT) | FLAT | EXTENSIBLE;
        mChars = chars;
    }

    JS_ALWAYS_INLINE jschar *flatChars() const {
        JS_ASSERT(isFlat());
        return mChars;
    }

    JS_ALWAYS_INLINE size_t flatLength() const {
        JS_ASSERT(isFlat());
        return length();
    }

    JS_ALWAYS_INLINE size_t flatCapacity() const {
        JS_ASSERT(isFlat());
        return e.mCapacity;
    }

    /*
     * Methods to manipulate ATOMIZED and EXTENSIBLE flags of flat strings. It is
     * safe to use these without extra locking due to the following properties:
     *
     *   * We do not have a flatClearAtomized method, as a string remains
     *     atomized until the GC collects it.
     *
     *   * A thread may call flatSetExtensible only when it is the only
     *     thread accessing the string until a later call to
     *     flatClearExtensible.
     *
     *   * Multiple threads can call flatClearExtensible but the function actually
     *     clears the EXTENSIBLE flag only when the flag is set -- in which case
     *     only one thread can access the string (see previous property).
     *
     * Thus, when multiple threads access the string, JSString::flatSetAtomized
     * is the only function that can update the mLengthAndFlags field of the
     * string by changing the EXTENSIBLE bit from 0 to 1. We call the method only
     * after the string has been hashed. When some threads in js_ValueToStringId
     * see that the flag is set, it knows that the string was atomized.
     *
     * On the other hand, if the thread sees that the flag is unset, it could
     * be seeing a stale value when another thread has just atomized the string
     * and set the flag. But this can lead only to an extra call to
     * js_AtomizeString. This function would find that the string was already
     * hashed and return it with the atomized bit set.
     */
    inline void flatSetAtomized() {
        JS_ASSERT(isFlat());
        JS_ASSERT(!isStatic(this));
        JS_ATOMIC_SET_MASK((jsword *)&mLengthAndFlags, ATOMIZED);
    }

    inline void flatSetExtensible() {
        JS_ASSERT(isFlat());
        JS_ASSERT(!isAtomized());
        mLengthAndFlags |= EXTENSIBLE;
    }

    inline void flatClearExtensible() {
        JS_ASSERT(isFlat());

        /*
         * We cannot eliminate the flag check before writing to mLengthAndFlags as
         * static strings may reside in write-protected memory. See bug 599481.
         */
        if (mLengthAndFlags & EXTENSIBLE)
            mLengthAndFlags &= ~EXTENSIBLE;
    }

    /*
     * The chars pointer should point somewhere inside the buffer owned by bstr.
     * The caller still needs to pass bstr for GC purposes.
     */
    inline void initDependent(JSString *bstr, jschar *chars, size_t len) {
        JS_ASSERT(len <= MAX_LENGTH);
        JS_ASSERT(!isStatic(this));
        e.mParent = NULL;
        mChars = chars;
        mLengthAndFlags = DEPENDENT | (len << FLAGS_LENGTH_SHIFT);
        e.mBase = bstr;
    }

    inline JSString *dependentBase() const {
        JS_ASSERT(isDependent());
        return e.mBase;
    }

    JS_ALWAYS_INLINE jschar *dependentChars() {
        return mChars;
    }

    inline size_t dependentLength() const {
        JS_ASSERT(isDependent());
        return length();
    }

    /* Rope-related initializers and accessors. */
    inline void initTopNode(JSString *left, JSString *right, size_t len,
                            JSRopeBufferInfo *buf) {
        JS_ASSERT(left->length() + right->length() <= MAX_LENGTH);
        JS_ASSERT(!isStatic(this));
        mLengthAndFlags = TOP_NODE | (len << FLAGS_LENGTH_SHIFT);
        mLeft = left;
        e.mRight = right;
        e.mBufferWithInfo = buf;
    }

    inline void convertToInteriorNode(JSString *parent) {
        JS_ASSERT(isTopNode());
        e.mParent = parent;
        mLengthAndFlags = INTERIOR_NODE | (length() << FLAGS_LENGTH_SHIFT);
    }

    inline JSString *interiorNodeParent() const {
        JS_ASSERT(isInteriorNode());
        return e.mParent;
    }

    inline JSString *ropeLeft() const {
        JS_ASSERT(isRope());
        return mLeft;
    }

    inline JSString *ropeRight() const {
        JS_ASSERT(isRope());
        return e.mRight;
    }

    inline size_t topNodeCapacity() const {
        JS_ASSERT(isTopNode());
        return e.mBufferWithInfo->capacity;
    }

    inline JSRopeBufferInfo *topNodeBuffer() const {
        JS_ASSERT(isTopNode());
        return e.mBufferWithInfo;
    }

    inline void nullifyTopNodeBuffer() {
        JS_ASSERT(isTopNode());
        e.mBufferWithInfo = NULL;
    }

    /*
     * When flattening a rope, we need to convert a rope node to a dependent
     * string in two separate parts instead of calling initDependent.
     */
    inline void startTraversalConversion(jschar *chars, size_t offset) {
        JS_ASSERT(isInteriorNode());
        mChars = chars + offset;
    }    

    inline void finishTraversalConversion(JSString *base, jschar *chars,
                                          size_t end) {
        JS_ASSERT(isInteriorNode());
        /* Note that setting flags also clears the traversal count. */
        mLengthAndFlags = JSString::DEPENDENT |
            ((chars + end - mChars) << JSString::FLAGS_LENGTH_SHIFT);
        e.mBase = base;
    }

    inline void ropeClearTraversalCount() {
        JS_ASSERT(isRope());
        mLengthAndFlags &= ~ROPE_TRAVERSAL_COUNT_MASK;
    }

    inline size_t ropeTraversalCount() const {
        JS_ASSERT(isRope());
        return (mLengthAndFlags & ROPE_TRAVERSAL_COUNT_MASK) >>
                ROPE_TRAVERSAL_COUNT_SHIFT;
    }

    inline void ropeIncrementTraversalCount() {
        JS_ASSERT(isRope());
        mLengthAndFlags += ROPE_TRAVERSAL_COUNT_UNIT;
    }

    inline bool ensureNotDependent(JSContext *cx) {
        return !isDependent() || undepend(cx);
    }

    inline void ensureNotRope() {
        if (isRope())
            flatten();
    }

    const jschar *undepend(JSContext *cx);

    /* By design, this is not allowed to fail. */
    void flatten();

    typedef uint8 SmallChar;

    static inline bool fitsInSmallChar(jschar c) {
        return c < SMALL_CHAR_LIMIT && toSmallChar[c] != INVALID_SMALL_CHAR;
    }

    static inline bool isUnitString(void *ptr) {
        jsuword delta = reinterpret_cast<jsuword>(ptr) -
                        reinterpret_cast<jsuword>(unitStringTable);
        if (delta >= UNIT_STRING_LIMIT * sizeof(JSString))
            return false;

        /* If ptr points inside the static array, it must be well-aligned. */
        JS_ASSERT(delta % sizeof(JSString) == 0);
        return true;
    }

    static inline bool isLength2String(void *ptr) {
        jsuword delta = reinterpret_cast<jsuword>(ptr) -
                        reinterpret_cast<jsuword>(length2StringTable);
        if (delta >= NUM_SMALL_CHARS * NUM_SMALL_CHARS * sizeof(JSString))
            return false;

        /* If ptr points inside the static array, it must be well-aligned. */
        JS_ASSERT(delta % sizeof(JSString) == 0);
        return true;
    }

    static inline bool isHundredString(void *ptr) {
        jsuword delta = reinterpret_cast<jsuword>(ptr) -
                        reinterpret_cast<jsuword>(hundredStringTable);
        if (delta >= NUM_HUNDRED_STRINGS * sizeof(JSString))
            return false;

        /* If ptr points inside the static array, it must be well-aligned. */
        JS_ASSERT(delta % sizeof(JSString) == 0);
        return true;
    }

    static inline bool isStatic(void *ptr) {
        return isUnitString(ptr) || isLength2String(ptr) || isHundredString(ptr);
    }

#ifdef __SUNPRO_CC
#pragma align 8 (__1cIJSStringPunitStringTable_, __1cIJSStringSlength2StringTable_, __1cIJSStringShundredStringTable_)
#endif

    static const SmallChar INVALID_SMALL_CHAR = -1;

    static const jschar fromSmallChar[];
    static const SmallChar toSmallChar[];
    static const JSString unitStringTable[];
    static const JSString length2StringTable[];
    static const JSString hundredStringTable[];
    /*
     * Since int strings can be unit strings, length-2 strings, or hundred
     * strings, we keep a table to map from integer to the correct string.
     */
    static const JSString *const intStringTable[];
    static const char deflatedIntStringTable[];
    static const char deflatedUnitStringTable[];
    static const char deflatedLength2StringTable[];

    static JSString *unitString(jschar c);
    static JSString *getUnitString(JSContext *cx, JSString *str, size_t index);
    static JSString *length2String(jschar c1, jschar c2);
    static JSString *intString(jsint i);

    static JSString *lookupStaticString(const jschar *chars, size_t length);
    
    JS_ALWAYS_INLINE void finalize(JSContext *cx, unsigned thingKind);
};

/*
 * Short strings should be created in cases where it's worthwhile to avoid
 * mallocing the string buffer for a small string. We keep 2 string headers'
 * worth of space in short strings so that more strings can be stored this way.
 */
struct JSShortString : js::gc::Cell {
    JSString mHeader;
    JSString mDummy;

    /*
     * Set the length of the string, and return a buffer for the caller to write
     * to. This buffer must be written immediately, and should not be modified
     * afterward.
     */
    inline jschar *init(size_t length) {
        JS_ASSERT(length <= MAX_SHORT_STRING_LENGTH);
        mHeader.initFlat(mHeader.inlineStorage(), length);
        return mHeader.inlineStorage();
    }

    inline void resetLength(size_t length) {
        mHeader.initFlat(mHeader.flatChars(), length);
    }

    inline JSString *header() {
        return &mHeader;
    }

    static const size_t MAX_SHORT_STRING_LENGTH =
            ((sizeof(JSString) + 2 * sizeof(size_t)) / sizeof(jschar)) - 1;

    static inline bool fitsIntoShortString(size_t length) {
        return length <= MAX_SHORT_STRING_LENGTH;
    }
    
    JS_ALWAYS_INLINE void finalize(JSContext *cx, unsigned thingKind);
};

/*
 * We're doing some tricks to give us more space for short strings, so make
 * sure that space is ordered in the way we expect.
 */
JS_STATIC_ASSERT(offsetof(JSString, mInlineStorage) == 2 * sizeof(void *));
JS_STATIC_ASSERT(offsetof(JSShortString, mDummy) == sizeof(JSString));
JS_STATIC_ASSERT(offsetof(JSString, mInlineStorage) +
                 sizeof(jschar) * (JSShortString::MAX_SHORT_STRING_LENGTH + 1) ==
                 sizeof(JSShortString));

/*
 * An iterator that iterates through all nodes in a rope (the top node, the
 * interior nodes, and the leaves) without writing to any of the nodes.
 *
 * It is safe to iterate through a rope in this way, even when something else is
 * already iterating through it.
 *
 * To use, pass any node of the rope into the constructor. The first call should
 * be to init, which returns the first node, and each subsequent call should
 * be to next. NULL is returned when there are no more nodes to return.
 */
class JSRopeNodeIterator {
  private:
    JSString *mStr;
    size_t mUsedFlags;

    static const size_t DONE_LEFT = 0x1;
    static const size_t DONE_RIGHT = 0x2;

  public:
    JSRopeNodeIterator(JSString *str)
      : mUsedFlags(0)
    {
        mStr = str;
    }
    
    JSString *init() {
        /* If we were constructed with a non-rope string, just return that. */
        if (!mStr->isRope()) {
            JSString *oldStr = mStr;
            mStr = NULL;
            return oldStr;
        }
        /* Move to the farthest-left leaf in the rope. */
        while (mStr->isInteriorNode())
            mStr = mStr->interiorNodeParent();
        while (mStr->ropeLeft()->isInteriorNode())
            mStr = mStr->ropeLeft();
        JS_ASSERT(mUsedFlags == 0);
        return mStr;
    }

    JSString *next() {
        if (!mStr)
            return NULL;
        if (!mStr->ropeLeft()->isInteriorNode() && !(mUsedFlags & DONE_LEFT)) {
            mUsedFlags |= DONE_LEFT;
            return mStr->ropeLeft();
        }
        if (!mStr->ropeRight()->isInteriorNode() && !(mUsedFlags & DONE_RIGHT)) {
            mUsedFlags |= DONE_RIGHT;
            return mStr->ropeRight();
        }
        if (mStr->ropeRight()->isInteriorNode()) {
            /*
             * If we have a right child, go right once, then left as far as
             * possible.
             */
            mStr = mStr->ropeRight();
            while (mStr->ropeLeft()->isInteriorNode())
                mStr = mStr->ropeLeft();
        } else {
            /*
             * If we have no right child, follow our parent until we move
             * up-right.
             */
            JSString *prev;
            do {
                prev = mStr;
                /* Set the string to NULL if we reach the end of the tree. */
                mStr = mStr->isInteriorNode() ? mStr->interiorNodeParent() : NULL;
            } while (mStr && mStr->ropeRight() == prev);
        }
        mUsedFlags = 0;
        return mStr;
    }
};

/*
 * An iterator that returns the leaves of a rope (which hold the actual string
 * data) in order. The usage is the same as JSRopeNodeIterator.
 */
class JSRopeLeafIterator {
  private:
    JSRopeNodeIterator mNodeIterator;

  public:

    JSRopeLeafIterator(JSString *topNode) :
        mNodeIterator(topNode) {
        JS_ASSERT(topNode->isTopNode());
    }

    inline JSString *init() {
        JSString *str = mNodeIterator.init();
        while (str->isRope()) {
            str = mNodeIterator.next();
            JS_ASSERT(str);
        }
        return str;
    }

    inline JSString *next() {
        JSString *str;
        do {
            str = mNodeIterator.next();
        } while (str && str->isRope());
        return str;
    }
};

class JSRopeBuilder {
    JSContext   * const cx;
    JSString    *mStr;

  public:
    JSRopeBuilder(JSContext *cx);

    inline bool append(JSString *str) {
        mStr = js_ConcatStrings(cx, mStr, str);
        return !!mStr;
    }

    inline JSString *getStr() {
        return mStr;
    }
};
     
JS_STATIC_ASSERT(JSString::INTERIOR_NODE & JSString::ROPE_BIT);
JS_STATIC_ASSERT(JSString::TOP_NODE & JSString::ROPE_BIT);

JS_STATIC_ASSERT(((JSString::MAX_LENGTH << JSString::FLAGS_LENGTH_SHIFT) >>
                   JSString::FLAGS_LENGTH_SHIFT) == JSString::MAX_LENGTH);

extern const jschar *
js_GetStringChars(JSContext *cx, JSString *str);

extern const jschar *
js_UndependString(JSContext *cx, JSString *str);

extern JSBool
js_MakeStringImmutable(JSContext *cx, JSString *str);

extern JSString * JS_FASTCALL
js_toLowerCase(JSContext *cx, JSString *str);

extern JSString * JS_FASTCALL
js_toUpperCase(JSContext *cx, JSString *str);

struct JSSubString {
    size_t          length;
    const jschar    *chars;
};

extern jschar      js_empty_ucstr[];
extern JSSubString js_EmptySubString;

/* Unicode character attribute lookup tables. */
extern const uint8 js_X[];
extern const uint8 js_Y[];
extern const uint32 js_A[];

/* Enumerated Unicode general category types. */
typedef enum JSCharType {
    JSCT_UNASSIGNED             = 0,
    JSCT_UPPERCASE_LETTER       = 1,
    JSCT_LOWERCASE_LETTER       = 2,
    JSCT_TITLECASE_LETTER       = 3,
    JSCT_MODIFIER_LETTER        = 4,
    JSCT_OTHER_LETTER           = 5,
    JSCT_NON_SPACING_MARK       = 6,
    JSCT_ENCLOSING_MARK         = 7,
    JSCT_COMBINING_SPACING_MARK = 8,
    JSCT_DECIMAL_DIGIT_NUMBER   = 9,
    JSCT_LETTER_NUMBER          = 10,
    JSCT_OTHER_NUMBER           = 11,
    JSCT_SPACE_SEPARATOR        = 12,
    JSCT_LINE_SEPARATOR         = 13,
    JSCT_PARAGRAPH_SEPARATOR    = 14,
    JSCT_CONTROL                = 15,
    JSCT_FORMAT                 = 16,
    JSCT_PRIVATE_USE            = 18,
    JSCT_SURROGATE              = 19,
    JSCT_DASH_PUNCTUATION       = 20,
    JSCT_START_PUNCTUATION      = 21,
    JSCT_END_PUNCTUATION        = 22,
    JSCT_CONNECTOR_PUNCTUATION  = 23,
    JSCT_OTHER_PUNCTUATION      = 24,
    JSCT_MATH_SYMBOL            = 25,
    JSCT_CURRENCY_SYMBOL        = 26,
    JSCT_MODIFIER_SYMBOL        = 27,
    JSCT_OTHER_SYMBOL           = 28
} JSCharType;

/* Character classifying and mapping macros, based on java.lang.Character. */
#define JS_CCODE(c)     (js_A[js_Y[(js_X[(uint16)(c)>>6]<<6)|((c)&0x3F)]])
#define JS_CTYPE(c)     (JS_CCODE(c) & 0x1F)

#define JS_ISALPHA(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                     \
                           (1 << JSCT_LOWERCASE_LETTER) |                     \
                           (1 << JSCT_TITLECASE_LETTER) |                     \
                           (1 << JSCT_MODIFIER_LETTER) |                      \
                           (1 << JSCT_OTHER_LETTER))                          \
                          >> JS_CTYPE(c)) & 1)

#define JS_ISALNUM(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                     \
                           (1 << JSCT_LOWERCASE_LETTER) |                     \
                           (1 << JSCT_TITLECASE_LETTER) |                     \
                           (1 << JSCT_MODIFIER_LETTER) |                      \
                           (1 << JSCT_OTHER_LETTER) |                         \
                           (1 << JSCT_DECIMAL_DIGIT_NUMBER))                  \
                          >> JS_CTYPE(c)) & 1)

/* A unicode letter, suitable for use in an identifier. */
#define JS_ISLETTER(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                    \
                            (1 << JSCT_LOWERCASE_LETTER) |                    \
                            (1 << JSCT_TITLECASE_LETTER) |                    \
                            (1 << JSCT_MODIFIER_LETTER) |                     \
                            (1 << JSCT_OTHER_LETTER) |                        \
                            (1 << JSCT_LETTER_NUMBER))                        \
                           >> JS_CTYPE(c)) & 1)

/*
 * 'IdentifierPart' from ECMA grammar, is Unicode letter or combining mark or
 * digit or connector punctuation.
 */
#define JS_ISIDPART(c)  ((((1 << JSCT_UPPERCASE_LETTER) |                     \
                           (1 << JSCT_LOWERCASE_LETTER) |                     \
                           (1 << JSCT_TITLECASE_LETTER) |                     \
                           (1 << JSCT_MODIFIER_LETTER) |                      \
                           (1 << JSCT_OTHER_LETTER) |                         \
                           (1 << JSCT_LETTER_NUMBER) |                        \
                           (1 << JSCT_NON_SPACING_MARK) |                     \
                           (1 << JSCT_COMBINING_SPACING_MARK) |               \
                           (1 << JSCT_DECIMAL_DIGIT_NUMBER) |                 \
                           (1 << JSCT_CONNECTOR_PUNCTUATION))                 \
                          >> JS_CTYPE(c)) & 1)

/* Unicode control-format characters, ignored in input */
#define JS_ISFORMAT(c) (((1 << JSCT_FORMAT) >> JS_CTYPE(c)) & 1)

/*
 * This table is used in JS_ISWORD.  The definition has external linkage to
 * allow the raw table data to be used in the regular expression compiler.
 */
extern const bool js_alnum[];

/*
 * This macro performs testing for the regular expression word class \w, which
 * is defined by ECMA-262 15.10.2.6 to be [0-9A-Z_a-z].  If we want a
 * Unicode-friendlier definition of "word", we should rename this macro to
 * something regexp-y.
 */
#define JS_ISWORD(c)    ((c) < 128 && js_alnum[(c)])

#define JS_ISIDSTART(c) (JS_ISLETTER(c) || (c) == '_' || (c) == '$')
#define JS_ISIDENT(c)   (JS_ISIDPART(c) || (c) == '_' || (c) == '$')

#define JS_ISXMLSPACE(c)        ((c) == ' ' || (c) == '\t' || (c) == '\r' ||  \
                                 (c) == '\n')
#define JS_ISXMLNSSTART(c)      ((JS_CCODE(c) & 0x00000100) || (c) == '_')
#define JS_ISXMLNS(c)           ((JS_CCODE(c) & 0x00000080) || (c) == '.' ||  \
                                 (c) == '-' || (c) == '_')
#define JS_ISXMLNAMESTART(c)    (JS_ISXMLNSSTART(c) || (c) == ':')
#define JS_ISXMLNAME(c)         (JS_ISXMLNS(c) || (c) == ':')

#define JS_ISDIGIT(c)   (JS_CTYPE(c) == JSCT_DECIMAL_DIGIT_NUMBER)

const jschar BYTE_ORDER_MARK = 0xFEFF;
const jschar NO_BREAK_SPACE  = 0x00A0;

static inline bool
JS_ISSPACE(jschar c)
{
    unsigned w = c;

    if (w < 256)
        return (w <= ' ' && (w == ' ' || (9 <= w && w <= 0xD))) || w == NO_BREAK_SPACE;

    return w == BYTE_ORDER_MARK || (JS_CCODE(w) & 0x00070000) == 0x00040000;
}

#define JS_ISPRINT(c)   ((c) < 128 && isprint(c))

#define JS_ISUPPER(c)   (JS_CTYPE(c) == JSCT_UPPERCASE_LETTER)
#define JS_ISLOWER(c)   (JS_CTYPE(c) == JSCT_LOWERCASE_LETTER)

#define JS_TOUPPER(c)   ((jschar) ((JS_CCODE(c) & 0x00100000)                 \
                                   ? (c) - ((int32)JS_CCODE(c) >> 22)         \
                                   : (c)))
#define JS_TOLOWER(c)   ((jschar) ((JS_CCODE(c) & 0x00200000)                 \
                                   ? (c) + ((int32)JS_CCODE(c) >> 22)         \
                                   : (c)))

/*
 * Shorthands for ASCII (7-bit) decimal and hex conversion.
 * Manually inline isdigit for performance; MSVC doesn't do this for us.
 */
#define JS7_ISDEC(c)    ((((unsigned)(c)) - '0') <= 9)
#define JS7_UNDEC(c)    ((c) - '0')
#define JS7_ISHEX(c)    ((c) < 128 && isxdigit(c))
#define JS7_UNHEX(c)    (uintN)(JS7_ISDEC(c) ? (c) - '0' : 10 + tolower(c) - 'a')
#define JS7_ISLET(c)    ((c) < 128 && isalpha(c))

/* Initialize the String class, returning its prototype object. */
extern js::Class js_StringClass;

inline bool
JSObject::isString() const
{
    return getClass() == &js_StringClass;
}

extern JSObject *
js_InitStringClass(JSContext *cx, JSObject *obj);

extern const char js_escape_str[];
extern const char js_unescape_str[];
extern const char js_uneval_str[];
extern const char js_decodeURI_str[];
extern const char js_encodeURI_str[];
extern const char js_decodeURIComponent_str[];
extern const char js_encodeURIComponent_str[];

/* GC-allocate a string descriptor for the given malloc-allocated chars. */
extern JSString *
js_NewString(JSContext *cx, jschar *chars, size_t length);

/*
 * GC-allocate a string descriptor and steal the char buffer held by |cb|.
 * This function takes responsibility for adding the terminating '\0' required
 * by js_NewString.
 */
extern JSString *
js_NewStringFromCharBuffer(JSContext *cx, JSCharBuffer &cb);

extern JSString *
js_NewDependentString(JSContext *cx, JSString *base, size_t start,
                      size_t length);

/* Copy a counted string and GC-allocate a descriptor for it. */
extern JSString *
js_NewStringCopyN(JSContext *cx, const jschar *s, size_t n);

extern JSString *
js_NewStringCopyN(JSContext *cx, const char *s, size_t n);

/* Copy a C string and GC-allocate a descriptor for it. */
extern JSString *
js_NewStringCopyZ(JSContext *cx, const jschar *s);

extern JSString *
js_NewStringCopyZ(JSContext *cx, const char *s);

/*
 * Convert a value to a printable C string.
 */
typedef JSString *(*JSValueToStringFun)(JSContext *cx, const js::Value &v);

extern JS_FRIEND_API(const char *)
js_ValueToPrintable(JSContext *cx, const js::Value &, JSValueToStringFun v2sfun);

#define js_ValueToPrintableString(cx,v) \
    js_ValueToPrintable(cx, v, js_ValueToString)

#define js_ValueToPrintableSource(cx,v) \
    js_ValueToPrintable(cx, v, js_ValueToSource)

/*
 * Convert a value to a string, returning null after reporting an error,
 * otherwise returning a new string reference.
 */
extern JSString *
js_ValueToString(JSContext *cx, const js::Value &v);

namespace js {

/*
 * Most code that calls js_ValueToString knows the value is (probably) not a
 * string, so it does not make sense to put this inline fast path into
 * js_ValueToString.
 */
static JS_ALWAYS_INLINE JSString *
ValueToString_TestForStringInline(JSContext *cx, const Value &v)
{
    if (v.isString())
        return v.toString();
    return js_ValueToString(cx, v);
}

}

/*
 * This function implements E-262-3 section 9.8, toString. Convert the given
 * value to a string of jschars appended to the given buffer. On error, the
 * passed buffer may have partial results appended.
 */
extern JSBool
js_ValueToCharBuffer(JSContext *cx, const js::Value &v, JSCharBuffer &cb);

/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JS_FRIEND_API(JSString *)
js_ValueToSource(JSContext *cx, const js::Value &v);

/*
 * Compute a hash function from str. The caller can call this function even if
 * str is not a GC-allocated thing.
 */
extern uint32
js_HashString(JSString *str);

/*
 * Test if strings are equal. The caller can call the function even if str1
 * or str2 are not GC-allocated things.
 */
extern JSBool JS_FASTCALL
js_EqualStrings(JSString *str1, JSString *str2);

/*
 * Return less than, equal to, or greater than zero depending on whether
 * str1 is less than, equal to, or greater than str2.
 */
extern int32 JS_FASTCALL
js_CompareStrings(JSString *str1, JSString *str2);

/*
 * Boyer-Moore-Horspool superlinear search for pat:patlen in text:textlen.
 * The patlen argument must be positive and no greater than sBMHPatLenMax.
 *
 * Return the index of pat in text, or -1 if not found.
 */
static const jsuint sBMHCharSetSize = 256; /* ISO-Latin-1 */
static const jsuint sBMHPatLenMax   = 255; /* skip table element is uint8 */
static const jsint  sBMHBadPattern  = -2;  /* return value if pat is not ISO-Latin-1 */

extern jsint
js_BoyerMooreHorspool(const jschar *text, jsuint textlen,
                      const jschar *pat, jsuint patlen);

extern size_t
js_strlen(const jschar *s);

extern jschar *
js_strchr(const jschar *s, jschar c);

extern jschar *
js_strchr_limit(const jschar *s, jschar c, const jschar *limit);

#define js_strncpy(t, s, n)     memcpy((t), (s), (n) * sizeof(jschar))

inline void
js_short_strncpy(jschar *dest, const jschar *src, size_t num)
{
    /*
     * It isn't strictly necessary here for |num| to be small, but this function
     * is currently only called on buffers for short strings.
     */
    JS_ASSERT(JSShortString::fitsIntoShortString(num));
    for (size_t i = 0; i < num; i++)
        dest[i] = src[i];
}

/*
 * Return s advanced past any Unicode white space characters.
 */
static inline const jschar *
js_SkipWhiteSpace(const jschar *s, const jschar *end)
{
    JS_ASSERT(s <= end);
    while (s != end && JS_ISSPACE(*s))
        s++;
    return s;
}

/*
 * Inflate bytes to JS chars and vice versa.  Report out of memory via cx and
 * return null on error, otherwise return the jschar or byte vector that was
 * JS_malloc'ed. length is updated to the length of the new string in jschars.
 */
extern jschar *
js_InflateString(JSContext *cx, const char *bytes, size_t *length);

extern char *
js_DeflateString(JSContext *cx, const jschar *chars, size_t length);

/*
 * Inflate bytes to JS chars into a buffer. 'chars' must be large enough for
 * 'length' jschars. The buffer is NOT null-terminated. The destination length
 * must be be initialized with the buffer size and will contain on return the
 * number of copied chars. Conversion behavior depends on js_CStringsAreUTF8.
 */
extern JSBool
js_InflateStringToBuffer(JSContext *cx, const char *bytes, size_t length,
                         jschar *chars, size_t *charsLength);

/*
 * Same as js_InflateStringToBuffer, but always treats 'bytes' as UTF-8.
 */
extern JSBool
js_InflateUTF8StringToBuffer(JSContext *cx, const char *bytes, size_t length,
                             jschar *chars, size_t *charsLength);

/*
 * Get number of bytes in the deflated sequence of characters. Behavior depends
 * on js_CStringsAreUTF8.
 */
extern size_t
js_GetDeflatedStringLength(JSContext *cx, const jschar *chars,
                           size_t charsLength);

/*
 * Same as js_GetDeflatedStringLength, but always treats the result as UTF-8.
 */
extern size_t
js_GetDeflatedUTF8StringLength(JSContext *cx, const jschar *chars,
                               size_t charsLength);

/*
 * Deflate JS chars to bytes into a buffer. 'bytes' must be large enough for
 * 'length chars. The buffer is NOT null-terminated. The destination length
 * must to be initialized with the buffer size and will contain on return the
 * number of copied bytes. Conversion behavior depends on js_CStringsAreUTF8.
 */
extern JSBool
js_DeflateStringToBuffer(JSContext *cx, const jschar *chars,
                         size_t charsLength, char *bytes, size_t *length);

/*
 * Same as js_DeflateStringToBuffer, but always treats 'bytes' as UTF-8.
 */
extern JSBool
js_DeflateStringToUTF8Buffer(JSContext *cx, const jschar *chars,
                             size_t charsLength, char *bytes, size_t *length);

/*
 * Find or create a deflated string cache entry for str that contains its
 * characters chopped from Unicode code points into bytes.
 */
extern const char *
js_GetStringBytes(JSContext *cx, JSString *str);

/* Export a few natives and a helper to other files in SpiderMonkey. */
extern JSBool
js_str_escape(JSContext *cx, JSObject *obj, uintN argc, js::Value *argv,
              js::Value *rval);

/*
 * The String.prototype.replace fast-native entry point is exported for joined
 * function optimization in js{interp,tracer}.cpp.
 */
namespace js {
extern JSBool
str_replace(JSContext *cx, uintN argc, js::Value *vp);
}

extern JSBool
js_str_toString(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_str_charAt(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_str_charCodeAt(JSContext *cx, uintN argc, js::Value *vp);

/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 6 bytes long.  Return the number of UTF-8 bytes of data written.
 */
extern int
js_OneUcs4ToUtf8Char(uint8 *utf8Buffer, uint32 ucs4Char);

/*
 * Write str into buffer escaping any non-printable or non-ASCII character.
 * Guarantees that a NUL is at the end of the buffer. Returns the length of
 * the written output, NOT including the NUL. If buffer is null, just returns
 * the length of the output. If quote is not 0, it must be a single or double
 * quote character that will quote the output.
 *
 * The function is only defined for debug builds.
*/
#define js_PutEscapedString(buffer, bufferSize, str, quote)                   \
    js_PutEscapedStringImpl(buffer, bufferSize, NULL, str, quote)

/*
 * Write str into file escaping any non-printable or non-ASCII character.
 * Returns the number of bytes written to file. If quote is not 0, it must
 * be a single or double quote character that will quote the output.
 *
 * The function is only defined for debug builds.
*/
#define js_FileEscapedString(file, str, quote)                                \
    (JS_ASSERT(file), js_PutEscapedStringImpl(NULL, 0, file, str, quote))

extern JS_FRIEND_API(size_t)
js_PutEscapedStringImpl(char *buffer, size_t bufferSize, FILE *fp,
                        JSString *str, uint32 quote);

extern JSBool
js_String(JSContext *cx, uintN argc, js::Value *vp);

namespace js {

class DeflatedStringCache {
  public:
    DeflatedStringCache();
    bool init();
    ~DeflatedStringCache();

    void sweep(JSContext *cx);
    void remove(JSString *str);
    bool setBytes(JSContext *cx, JSString *str, char *bytes);

  private:
    struct StringPtrHasher
    {
        typedef JSString *Lookup;

        static HashNumber hash(JSString *str) {
            /*
             * We hash only GC-allocated Strings. They are aligned on
             * sizeof(JSString) boundary so we can improve hashing by stripping
             * initial zeros.
             */
            const jsuword ALIGN_LOG = tl::FloorLog2<sizeof(JSString)>::result;
            JS_STATIC_ASSERT(sizeof(JSString) == (size_t(1) << ALIGN_LOG));

            jsuword ptr = reinterpret_cast<jsuword>(str);
            jsuword key = ptr >> ALIGN_LOG;
            JS_ASSERT((key << ALIGN_LOG) == ptr);
            return HashNumber(key);
        }

        static bool match(JSString *s1, JSString *s2) {
            return s1 == s2;
        }
    };

    typedef HashMap<JSString *, char *, StringPtrHasher, SystemAllocPolicy> Map;

    /* cx is NULL when the caller is JS_GetStringBytes(JSString *). */
    char *getBytes(JSContext *cx, JSString *str);

    friend const char *
    ::js_GetStringBytes(JSContext *cx, JSString *str);

    Map                 map;
#ifdef JS_THREADSAFE
    JSLock              *lock;
#endif
};

} /* namespace js */

#endif /* jsstr_h___ */
