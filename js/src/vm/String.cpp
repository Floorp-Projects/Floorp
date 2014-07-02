/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/String-inl.h"

#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/TypeTraits.h"

#include "gc/Marking.h"

#include "jscntxtinlines.h"
#include "jscompartmentinlines.h"

using namespace js;

using mozilla::IsSame;
using mozilla::PodCopy;
using mozilla::RangedPtr;
using mozilla::RoundUpPow2;

using JS::AutoCheckCannotGC;

size_t
JSString::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    // JSRope: do nothing, we'll count all children chars when we hit the leaf strings.
    if (isRope())
        return 0;

    JS_ASSERT(isLinear());

    // JSDependentString: do nothing, we'll count the chars when we hit the base string.
    if (isDependent())
        return 0;

    JS_ASSERT(isFlat());

    // JSExtensibleString: count the full capacity, not just the used space.
    if (isExtensible()) {
        JSExtensibleString &extensible = asExtensible();
        return extensible.hasLatin1Chars()
               ? mallocSizeOf(extensible.rawLatin1Chars())
               : mallocSizeOf(extensible.rawTwoByteChars());
    }

    // JSExternalString: don't count, the chars could be stored anywhere.
    if (isExternal())
        return 0;

    // JSInlineString, JSFatInlineString [JSInlineAtom, JSFatInlineAtom]: the chars are inline.
    if (isInline())
        return 0;

    // JSAtom, JSUndependedString: measure the space for the chars.  For
    // JSUndependedString, there is no need to count the base string, for the
    // same reason as JSDependentString above.
    JSFlatString &flat = asFlat();
    return flat.hasLatin1Chars()
           ? mallocSizeOf(flat.rawLatin1Chars())
           : mallocSizeOf(flat.rawTwoByteChars());
}

#ifdef DEBUG

template <typename CharT>
/*static */ void
JSString::dumpChars(const CharT *s, size_t n, FILE *fp)
{
    if (n == SIZE_MAX) {
        n = 0;
        while (s[n])
            n++;
    }

    fputc('"', fp);
    for (size_t i = 0; i < n; i++) {
        jschar c = s[i];
        if (c == '\n')
            fprintf(fp, "\\n");
        else if (c == '\t')
            fprintf(fp, "\\t");
        else if (c >= 32 && c < 127)
            fputc(s[i], fp);
        else if (c <= 255)
            fprintf(fp, "\\x%02x", unsigned(c));
        else
            fprintf(fp, "\\u%04x", unsigned(c));
    }
    fputc('"', fp);
}

template void
JSString::dumpChars(const Latin1Char *s, size_t n, FILE *fp);

template void
JSString::dumpChars(const jschar *s, size_t n, FILE *fp);

void
JSString::dumpCharsNoNewline(FILE *fp)
{
    if (JSLinearString *linear = ensureLinear(nullptr)) {
        AutoCheckCannotGC nogc;
        if (hasLatin1Chars())
            dumpChars(linear->latin1Chars(nogc), length(), fp);
        else
            dumpChars(linear->twoByteChars(nogc), length(), fp);
    } else {
        fprintf(fp, "(oom in JSString::dumpCharsNoNewline)");
    }
}

void
JSString::dump()
{
    if (JSLinearString *linear = ensureLinear(nullptr)) {
        AutoCheckCannotGC nogc;
        if (hasLatin1Chars()) {
            const Latin1Char *chars = linear->latin1Chars(nogc);
            fprintf(stderr, "JSString* (%p) = Latin1Char * (%p) = ", (void *) this,
                    (void *) chars);
            dumpChars(chars, length(), stderr);
        } else {
            const jschar *chars = linear->twoByteChars(nogc);
            fprintf(stderr, "JSString* (%p) = jschar * (%p) = ", (void *) this,
                    (void *) chars);
            dumpChars(chars, length(), stderr);
        }
    } else {
        fprintf(stderr, "(oom in JSString::dump)");
    }
    fputc('\n', stderr);
}

bool
JSString::equals(const char *s)
{
    const jschar *c = getChars(nullptr);
    if (!c) {
        fprintf(stderr, "OOM in JSString::equals!\n");
        return false;
    }
    while (*c && *s) {
        if (*c != *s)
            return false;
        c++;
        s++;
    }
    return *c == *s;
}
#endif /* DEBUG */

void
JSLinearString::debugUnsafeConvertToLatin1()
{
    // Temporary helper function to test changes for bug 998392.

    MOZ_ASSERT(hasTwoByteChars());
    MOZ_ASSERT(!hasBase());

    size_t len = length();
    const jschar *twoByteChars = chars();
    char *latin1Chars = (char *)twoByteChars;

    for (size_t i = 0; i < len; i++) {
        MOZ_ASSERT((twoByteChars[i] & 0xff00) == 0);
        latin1Chars[i] = char(twoByteChars[i]);
    }

    latin1Chars[len] = '\0';
    d.u1.flags |= LATIN1_CHARS_BIT;
}

template <typename CharT>
static MOZ_ALWAYS_INLINE bool
AllocChars(ThreadSafeContext *maybecx, size_t length, CharT **chars, size_t *capacity)
{
    /*
     * String length doesn't include the null char, so include it here before
     * doubling. Adding the null char after doubling would interact poorly with
     * round-up malloc schemes.
     */
    size_t numChars = length + 1;

    /*
     * Grow by 12.5% if the buffer is very large. Otherwise, round up to the
     * next power of 2. This is similar to what we do with arrays; see
     * JSObject::ensureDenseArrayElements.
     */
    static const size_t DOUBLING_MAX = 1024 * 1024;
    numChars = numChars > DOUBLING_MAX ? numChars + (numChars / 8) : RoundUpPow2(numChars);

    /* Like length, capacity does not include the null char, so take it out. */
    *capacity = numChars - 1;

    JS_STATIC_ASSERT(JSString::MAX_LENGTH * sizeof(CharT) < UINT32_MAX);
    size_t bytes = numChars * sizeof(CharT);
    *chars = (CharT *)(maybecx ? maybecx->malloc_(bytes) : js_malloc(bytes));
    return *chars != nullptr;
}

bool
JSRope::copyLatin1CharsZ(ThreadSafeContext *cx, ScopedJSFreePtr<Latin1Char> &out) const
{
    return copyCharsInternal<Latin1Char>(cx, out, true);
}

bool
JSRope::copyTwoByteCharsZ(ThreadSafeContext *cx, ScopedJSFreePtr<jschar> &out) const
{
    return copyCharsInternal<jschar>(cx, out, true);
}

bool
JSRope::copyLatin1Chars(ThreadSafeContext *cx, ScopedJSFreePtr<Latin1Char> &out) const
{
    return copyCharsInternal<Latin1Char>(cx, out, false);
}

bool
JSRope::copyTwoByteChars(ThreadSafeContext *cx, ScopedJSFreePtr<jschar> &out) const
{
    return copyCharsInternal<jschar>(cx, out, false);
}

template <typename CharT>
bool
JSRope::copyCharsInternal(ThreadSafeContext *cx, ScopedJSFreePtr<CharT> &out,
                          bool nullTerminate) const
{
    /*
     * Perform non-destructive post-order traversal of the rope, splatting
     * each node's characters into a contiguous buffer.
     */

    size_t n = length();
    if (cx)
        out.reset(cx->pod_malloc<CharT>(n + 1));
    else
        out.reset(js_pod_malloc<CharT>(n + 1));

    if (!out)
        return false;

    Vector<const JSString *, 8, SystemAllocPolicy> nodeStack;
    const JSString *str = this;
    CharT *pos = out;
    while (true) {
        if (str->isRope()) {
            if (!nodeStack.append(str->asRope().rightChild()))
                return false;
            str = str->asRope().leftChild();
        } else {
            CopyChars(pos, str->asLinear());
            pos += str->length();
            if (nodeStack.empty())
                break;
            str = nodeStack.popCopy();
        }
    }

    JS_ASSERT(pos == out + n);

    if (nullTerminate)
        out[n] = 0;

    return true;
}

namespace js {

template <>
void
CopyChars(jschar *dest, const JSLinearString &str)
{
    AutoCheckCannotGC nogc;
    if (str.hasTwoByteChars())
        PodCopy(dest, str.twoByteChars(nogc), str.length());
    else
        CopyAndInflateChars(dest, str.latin1Chars(nogc), str.length());
}

template <>
void
CopyChars(Latin1Char *dest, const JSLinearString &str)
{
    AutoCheckCannotGC nogc;
    PodCopy(dest, str.latin1Chars(nogc), str.length());
}

} /* namespace js */

template<JSRope::UsingBarrier b, typename CharT>
JSFlatString *
JSRope::flattenInternal(ExclusiveContext *maybecx)
{
    /*
     * Perform a depth-first dag traversal, splatting each node's characters
     * into a contiguous buffer. Visit each rope node three times:
     *   1. record position in the buffer and recurse into left child;
     *   2. recurse into the right child;
     *   3. transform the node into a dependent string.
     * To avoid maintaining a stack, tree nodes are mutated to indicate how many
     * times they have been visited. Since ropes can be dags, a node may be
     * encountered multiple times during traversal. However, step 3 above leaves
     * a valid dependent string, so everything works out.
     *
     * While ropes avoid all sorts of quadratic cases with string
     * concatenation, they can't help when ropes are immediately flattened.
     * One idiomatic case that we'd like to keep linear (and has traditionally
     * been linear in SM and other JS engines) is:
     *
     *   while (...) {
     *     s += ...
     *     s.flatten
     *   }
     *
     * To do this, when the buffer for a to-be-flattened rope is allocated, the
     * allocation size is rounded up. Then, if the resulting flat string is the
     * left-hand side of a new rope that gets flattened and there is enough
     * capacity, the rope is flattened into the same buffer, thereby avoiding
     * copying the left-hand side. Clearing the 'extensible' bit turns off this
     * optimization. This is necessary, e.g., when the JSAPI hands out the raw
     * null-terminated char array of a flat string.
     *
     * N.B. This optimization can create chains of dependent strings.
     */
    const size_t wholeLength = length();
    size_t wholeCapacity;
    CharT *wholeChars;
    JSString *str = this;
    CharT *pos;

    /*
     * JSString::flattenData is a tagged pointer to the parent node.
     * The tag indicates what to do when we return to the parent.
     */
    static const uintptr_t Tag_Mask = 0x3;
    static const uintptr_t Tag_FinishNode = 0x0;
    static const uintptr_t Tag_VisitRightChild = 0x1;

    AutoCheckCannotGC nogc;

    /* Find the left most string, containing the first string. */
    JSRope *leftMostRope = this;
    while (leftMostRope->leftChild()->isRope())
        leftMostRope = &leftMostRope->leftChild()->asRope();

    if (leftMostRope->leftChild()->isExtensible()) {
        JSExtensibleString &left = leftMostRope->leftChild()->asExtensible();
        size_t capacity = left.capacity();
        if (capacity >= wholeLength && left.hasTwoByteChars() == IsSame<CharT, jschar>::value) {
            /*
             * Simulate a left-most traversal from the root to leftMost->leftChild()
             * via first_visit_node
             */
            JS_ASSERT(str->isRope());
            while (str != leftMostRope) {
                if (b == WithIncrementalBarrier) {
                    JSString::writeBarrierPre(str->d.s.u2.left);
                    JSString::writeBarrierPre(str->d.s.u3.right);
                }
                JSString *child = str->d.s.u2.left;
                JS_ASSERT(child->isRope());
                str->setNonInlineChars(left.nonInlineChars<CharT>(nogc));
                child->d.u1.flattenData = uintptr_t(str) | Tag_VisitRightChild;
                str = child;
            }
            if (b == WithIncrementalBarrier) {
                JSString::writeBarrierPre(str->d.s.u2.left);
                JSString::writeBarrierPre(str->d.s.u3.right);
            }
            str->setNonInlineChars(left.nonInlineChars<CharT>(nogc));
            wholeCapacity = capacity;
            wholeChars = const_cast<CharT *>(left.nonInlineChars<CharT>(nogc));
            pos = wholeChars + left.d.u1.length;
            JS_STATIC_ASSERT(!(EXTENSIBLE_FLAGS & DEPENDENT_FLAGS));
            left.d.u1.flags ^= (EXTENSIBLE_FLAGS | DEPENDENT_FLAGS);
            left.d.s.u3.base = (JSLinearString *)this;  /* will be true on exit */
            StringWriteBarrierPostRemove(maybecx, &left.d.s.u2.left);
            StringWriteBarrierPost(maybecx, (JSString **)&left.d.s.u3.base);
            goto visit_right_child;
        }
    }

    if (!AllocChars(maybecx, wholeLength, &wholeChars, &wholeCapacity))
        return nullptr;

    pos = wholeChars;
    first_visit_node: {
        if (b == WithIncrementalBarrier) {
            JSString::writeBarrierPre(str->d.s.u2.left);
            JSString::writeBarrierPre(str->d.s.u3.right);
        }

        JSString &left = *str->d.s.u2.left;
        str->setNonInlineChars(pos);
        StringWriteBarrierPostRemove(maybecx, &str->d.s.u2.left);
        if (left.isRope()) {
            /* Return to this node when 'left' done, then goto visit_right_child. */
            left.d.u1.flattenData = uintptr_t(str) | Tag_VisitRightChild;
            str = &left;
            goto first_visit_node;
        }
        CopyChars(pos, left.asLinear());
        pos += left.length();
    }
    visit_right_child: {
        JSString &right = *str->d.s.u3.right;
        if (right.isRope()) {
            /* Return to this node when 'right' done, then goto finish_node. */
            right.d.u1.flattenData = uintptr_t(str) | Tag_FinishNode;
            str = &right;
            goto first_visit_node;
        }
        CopyChars(pos, right.asLinear());
        pos += right.length();
    }
    finish_node: {
        if (str == this) {
            JS_ASSERT(pos == wholeChars + wholeLength);
            *pos = '\0';
            str->d.u1.length = wholeLength;
            if (IsSame<CharT, jschar>::value)
                str->d.u1.flags = EXTENSIBLE_FLAGS;
            else
                str->d.u1.flags = EXTENSIBLE_FLAGS | LATIN1_CHARS_BIT;
            str->setNonInlineChars(wholeChars);
            str->d.s.u3.capacity = wholeCapacity;
            StringWriteBarrierPostRemove(maybecx, &str->d.s.u2.left);
            StringWriteBarrierPostRemove(maybecx, &str->d.s.u3.right);
            return &this->asFlat();
        }
        uintptr_t flattenData = str->d.u1.flattenData;
        if (IsSame<CharT, jschar>::value)
            str->d.u1.flags = DEPENDENT_FLAGS;
        else
            str->d.u1.flags = DEPENDENT_FLAGS | LATIN1_CHARS_BIT;
        str->d.u1.length = pos - str->asLinear().nonInlineChars<CharT>(nogc);
        str->d.s.u3.base = (JSLinearString *)this;       /* will be true on exit */
        StringWriteBarrierPost(maybecx, (JSString **)&str->d.s.u3.base);
        str = (JSString *)(flattenData & ~Tag_Mask);
        if ((flattenData & Tag_Mask) == Tag_VisitRightChild)
            goto visit_right_child;
        JS_ASSERT((flattenData & Tag_Mask) == Tag_FinishNode);
        goto finish_node;
    }
}

template<JSRope::UsingBarrier b>
JSFlatString *
JSRope::flattenInternal(ExclusiveContext *maybecx)
{
    if (hasTwoByteChars())
        return flattenInternal<b, jschar>(maybecx);
    return flattenInternal<b, Latin1Char>(maybecx);
}

JSFlatString *
JSRope::flatten(ExclusiveContext *maybecx)
{
#ifdef JSGC_INCREMENTAL
    if (zone()->needsBarrier())
        return flattenInternal<WithIncrementalBarrier>(maybecx);
#endif
    return flattenInternal<NoBarrier>(maybecx);
}

template <AllowGC allowGC>
JSString *
js::ConcatStrings(ThreadSafeContext *cx,
                  typename MaybeRooted<JSString*, allowGC>::HandleType left,
                  typename MaybeRooted<JSString*, allowGC>::HandleType right)
{
    JS_ASSERT_IF(!left->isAtom(), cx->isInsideCurrentZone(left));
    JS_ASSERT_IF(!right->isAtom(), cx->isInsideCurrentZone(right));

    size_t leftLen = left->length();
    if (leftLen == 0)
        return right;

    size_t rightLen = right->length();
    if (rightLen == 0)
        return left;

    size_t wholeLength = leftLen + rightLen;
    if (!JSString::validateLength(cx, wholeLength))
        return nullptr;

    bool isLatin1 = left->hasLatin1Chars() && right->hasLatin1Chars();
    bool canUseFatInline = isLatin1
                           ? JSFatInlineString::latin1LengthFits(wholeLength)
                           : JSFatInlineString::twoByteLengthFits(wholeLength);
    if (canUseFatInline && cx->isJSContext()) {
        JSFatInlineString *str = NewGCFatInlineString<allowGC>(cx);
        if (!str)
            return nullptr;

        AutoCheckCannotGC nogc;
        ScopedThreadSafeStringInspector leftInspector(left);
        ScopedThreadSafeStringInspector rightInspector(right);
        if (!leftInspector.ensureChars(cx, nogc) || !rightInspector.ensureChars(cx, nogc))
            return nullptr;

        if (isLatin1) {
            Latin1Char *buf = str->initLatin1(wholeLength);
            PodCopy(buf, leftInspector.latin1Chars(), leftLen);
            PodCopy(buf + leftLen, rightInspector.latin1Chars(), rightLen);
            buf[wholeLength] = 0;
        } else {
            jschar *buf = str->initTwoByte(wholeLength);
            if (leftInspector.hasTwoByteChars())
                PodCopy(buf, leftInspector.twoByteChars(), leftLen);
            else
                CopyAndInflateChars(buf, leftInspector.latin1Chars(), leftLen);
            if (rightInspector.hasTwoByteChars())
                PodCopy(buf + leftLen, rightInspector.twoByteChars(), rightLen);
            else
                CopyAndInflateChars(buf + leftLen, rightInspector.latin1Chars(), rightLen);
            buf[wholeLength] = 0;
        }

        return str;
    }

    return JSRope::new_<allowGC>(cx, left, right, wholeLength);
}

template JSString *
js::ConcatStrings<CanGC>(ThreadSafeContext *cx, HandleString left, HandleString right);

template JSString *
js::ConcatStrings<NoGC>(ThreadSafeContext *cx, JSString *left, JSString *right);

template <typename CharT>
JSFlatString *
JSDependentString::undependInternal(ExclusiveContext *cx)
{
    /*
     * We destroy the base() pointer in undepend, so we need a pre-barrier. We
     * don't need a post-barrier because there aren't any outgoing pointers
     * afterwards.
     */
    JSString::writeBarrierPre(base());

    size_t n = length();
    CharT *s = cx->pod_malloc<CharT>(n + 1);
    if (!s)
        return nullptr;

    AutoCheckCannotGC nogc;
    PodCopy(s, nonInlineChars<CharT>(nogc), n);
    s[n] = '\0';
    setNonInlineChars<CharT>(s);

    /*
     * Transform *this into an undepended string so 'base' will remain rooted
     * for the benefit of any other dependent string that depends on *this.
     */
    if (IsSame<CharT, Latin1Char>::value)
        d.u1.flags = UNDEPENDED_FLAGS | LATIN1_CHARS_BIT;
    else
        d.u1.flags = UNDEPENDED_FLAGS;

    return &this->asFlat();
}

JSFlatString *
JSDependentString::undepend(ExclusiveContext *cx)
{
    JS_ASSERT(JSString::isDependent());
    return hasLatin1Chars()
           ? undependInternal<Latin1Char>(cx)
           : undependInternal<jschar>(cx);
}

template <typename CharT>
/* static */ bool
JSFlatString::isIndexSlow(const CharT *s, size_t length, uint32_t *indexp)
{
    CharT ch = *s;

    if (!JS7_ISDEC(ch))
        return false;

    if (length > UINT32_CHAR_BUFFER_LENGTH)
        return false;

    /*
     * Make sure to account for the '\0' at the end of characters, dereferenced
     * in the loop below.
     */
    RangedPtr<const CharT> cp(s, length + 1);
    const RangedPtr<const CharT> end(s + length, s, length + 1);

    uint32_t index = JS7_UNDEC(*cp++);
    uint32_t oldIndex = 0;
    uint32_t c = 0;

    if (index != 0) {
        while (JS7_ISDEC(*cp)) {
            oldIndex = index;
            c = JS7_UNDEC(*cp);
            index = 10 * index + c;
            cp++;
        }
    }

    /* It's not an element if there are characters after the number. */
    if (cp != end)
        return false;

    /*
     * Look out for "4294967296" and larger-number strings that fit in
     * UINT32_CHAR_BUFFER_LENGTH: only unsigned 32-bit integers shall pass.
     */
    if (oldIndex < UINT32_MAX / 10 || (oldIndex == UINT32_MAX / 10 && c <= (UINT32_MAX % 10))) {
        *indexp = index;
        return true;
    }

    return false;
}

template bool
JSFlatString::isIndexSlow(const Latin1Char *s, size_t length, uint32_t *indexp);

template bool
JSFlatString::isIndexSlow(const jschar *s, size_t length, uint32_t *indexp);

bool
ScopedThreadSafeStringInspector::ensureChars(ThreadSafeContext *cx, const AutoCheckCannotGC &nogc)
{
    if (state_ != Uninitialized)
        return true;

    if (cx->isExclusiveContext()) {
        JSLinearString *linear = str_->ensureLinear(cx->asExclusiveContext());
        if (!linear)
            return false;
        if (linear->hasTwoByteChars()) {
            state_ = TwoByte;
            twoByteChars_ = linear->twoByteChars(nogc);
        } else {
            state_ = Latin1;
            latin1Chars_ = linear->latin1Chars(nogc);
        }
    } else {
        if (str_->isLinear()) {
            if (str_->hasLatin1Chars()) {
                state_ = Latin1;
                latin1Chars_ = str_->asLinear().latin1Chars(nogc);
            } else {
                state_ = TwoByte;
                twoByteChars_ = str_->asLinear().twoByteChars(nogc);
            }
        } else {
            if (str_->hasLatin1Chars()) {
                ScopedJSFreePtr<Latin1Char> chars;
                if (!str_->asRope().copyLatin1Chars(cx, chars))
                    return false;
                state_ = Latin1;
                latin1Chars_ = chars;
                scopedChars_ = chars.forget();
            } else {
                ScopedJSFreePtr<jschar> chars;
                if (!str_->asRope().copyTwoByteChars(cx, chars))
                    return false;
                state_ = TwoByte;
                twoByteChars_ = chars;
                scopedChars_ = chars.forget();
            }
        }
    }

    MOZ_ASSERT(state_ != Uninitialized);
    return true;
}

/*
 * Set up some tools to make it easier to generate large tables. After constant
 * folding, for each n, Rn(0) is the comma-separated list R(0), R(1), ..., R(2^n-1).
 * Similary, Rn(k) (for any k and n) generates the list R(k), R(k+1), ..., R(k+2^n-1).
 * To use this, define R appropriately, then use Rn(0) (for some value of n), then
 * undefine R.
 */
#define R2(n) R(n),  R((n) + (1 << 0)),  R((n) + (2 << 0)),  R((n) + (3 << 0))
#define R4(n) R2(n), R2((n) + (1 << 2)), R2((n) + (2 << 2)), R2((n) + (3 << 2))
#define R6(n) R4(n), R4((n) + (1 << 4)), R4((n) + (2 << 4)), R4((n) + (3 << 4))
#define R7(n) R6(n), R6((n) + (1 << 6))

/*
 * This is used when we generate our table of short strings, so the compiler is
 * happier if we use |c| as few times as possible.
 */
#define FROM_SMALL_CHAR(c) jschar((c) + ((c) < 10 ? '0' :      \
                                         (c) < 36 ? 'a' - 10 : \
                                         'A' - 36))

/*
 * Declare length-2 strings. We only store strings where both characters are
 * alphanumeric. The lower 10 short chars are the numerals, the next 26 are
 * the lowercase letters, and the next 26 are the uppercase letters.
 */
#define TO_SMALL_CHAR(c) ((c) >= '0' && (c) <= '9' ? (c) - '0' :              \
                          (c) >= 'a' && (c) <= 'z' ? (c) - 'a' + 10 :         \
                          (c) >= 'A' && (c) <= 'Z' ? (c) - 'A' + 36 :         \
                          StaticStrings::INVALID_SMALL_CHAR)

#define R TO_SMALL_CHAR
const StaticStrings::SmallChar StaticStrings::toSmallChar[] = { R7(0) };
#undef R

bool
StaticStrings::init(JSContext *cx)
{
    AutoLockForExclusiveAccess lock(cx);
    AutoCompartment ac(cx, cx->runtime()->atomsCompartment());

    for (uint32_t i = 0; i < UNIT_STATIC_LIMIT; i++) {
        jschar buffer[] = { jschar(i), '\0' };
        JSFlatString *s = NewStringCopyN<NoGC>(cx, buffer, 1);
        if (!s)
            return false;
        unitStaticTable[i] = s->morphAtomizedStringIntoPermanentAtom();
    }

    for (uint32_t i = 0; i < NUM_SMALL_CHARS * NUM_SMALL_CHARS; i++) {
        jschar buffer[] = { FROM_SMALL_CHAR(i >> 6), FROM_SMALL_CHAR(i & 0x3F), '\0' };
        JSFlatString *s = NewStringCopyN<NoGC>(cx, buffer, 2);
        if (!s)
            return false;
        length2StaticTable[i] = s->morphAtomizedStringIntoPermanentAtom();
    }

    for (uint32_t i = 0; i < INT_STATIC_LIMIT; i++) {
        if (i < 10) {
            intStaticTable[i] = unitStaticTable[i + '0'];
        } else if (i < 100) {
            size_t index = ((size_t)TO_SMALL_CHAR((i / 10) + '0') << 6) +
                TO_SMALL_CHAR((i % 10) + '0');
            intStaticTable[i] = length2StaticTable[index];
        } else {
            jschar buffer[] = { jschar('0' + (i / 100)),
                                jschar('0' + ((i / 10) % 10)),
                                jschar('0' + (i % 10)),
                                '\0' };
            JSFlatString *s = NewStringCopyN<NoGC>(cx, buffer, 3);
            if (!s)
                return false;
            intStaticTable[i] = s->morphAtomizedStringIntoPermanentAtom();
        }
    }

    return true;
}

void
StaticStrings::trace(JSTracer *trc)
{
    /* These strings never change, so barriers are not needed. */

    for (uint32_t i = 0; i < UNIT_STATIC_LIMIT; i++)
        MarkPermanentAtom(trc, unitStaticTable[i], "unit-static-string");

    for (uint32_t i = 0; i < NUM_SMALL_CHARS * NUM_SMALL_CHARS; i++)
        MarkPermanentAtom(trc, length2StaticTable[i], "length2-static-string");

    /* This may mark some strings more than once, but so be it. */
    for (uint32_t i = 0; i < INT_STATIC_LIMIT; i++)
        MarkPermanentAtom(trc, intStaticTable[i], "int-static-string");
}

template <typename CharT>
/* static */ bool
StaticStrings::isStatic(const CharT *chars, size_t length)
{
    switch (length) {
      case 1:
        return chars[0] < UNIT_STATIC_LIMIT;
      case 2:
        return fitsInSmallChar(chars[0]) && fitsInSmallChar(chars[1]);
      case 3:
        if ('1' <= chars[0] && chars[0] <= '9' &&
            '0' <= chars[1] && chars[1] <= '9' &&
            '0' <= chars[2] && chars[2] <= '9') {
            int i = (chars[0] - '0') * 100 +
                      (chars[1] - '0') * 10 +
                      (chars[2] - '0');

            return unsigned(i) < INT_STATIC_LIMIT;
        }
        return false;
      default:
        return false;
    }
}

/* static */ bool
StaticStrings::isStatic(JSAtom *atom)
{
    AutoCheckCannotGC nogc;
    return atom->hasLatin1Chars()
           ? isStatic(atom->latin1Chars(nogc), atom->length())
           : isStatic(atom->twoByteChars(nogc), atom->length());
}

AutoStableStringChars::~AutoStableStringChars()
{
    if (ownsChars_) {
        MOZ_ASSERT(state_ == Latin1 || state_ == TwoByte);
        if (state_ == Latin1)
            js_free(const_cast<Latin1Char*>(latin1Chars_));
        else
            js_free(const_cast<jschar*>(twoByteChars_));
    }
}

bool
AutoStableStringChars::init(JSContext *cx, JSString *s)
{
    s_ = s->ensureLinear(cx);
    if (!s_)
        return false;

    MOZ_ASSERT(state_ == Uninitialized);

    if (s_->hasLatin1Chars()) {
        state_ = Latin1;
        latin1Chars_ = s_->rawLatin1Chars();
    } else {
        state_ = TwoByte;
        twoByteChars_ = s_->rawTwoByteChars();
    }

    return true;
}

bool
AutoStableStringChars::initTwoByte(JSContext *cx, JSString *s)
{
    s_ = s->ensureLinear(cx);
    if (!s_)
        return false;

    MOZ_ASSERT(state_ == Uninitialized);

    if (s_->hasTwoByteChars()) {
        state_ = TwoByte;
        twoByteChars_ = s_->rawTwoByteChars();
        return true;
    }

    jschar *chars = cx->pod_malloc<jschar>(s_->length() + 1);
    if (!chars)
        return false;

    CopyAndInflateChars(chars, s_->rawLatin1Chars(), s_->length());
    chars[s_->length()] = 0;

    state_ = TwoByte;
    ownsChars_ = true;
    twoByteChars_ = chars;
    return true;
}

bool js::EnableLatin1Strings = false;

#ifdef DEBUG
void
JSAtom::dump()
{
    fprintf(stderr, "JSAtom* (%p) = ", (void *) this);
    this->JSString::dump();
}
#endif /* DEBUG */
