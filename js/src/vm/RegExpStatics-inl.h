/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExpStatics_inl_h__
#define RegExpStatics_inl_h__

#include "RegExpStatics.h"

#include "vm/String-inl.h"

namespace js {

inline js::RegExpStatics *
js::GlobalObject::getRegExpStatics() const
{
    JSObject &resObj = getSlot(REGEXP_STATICS).toObject();
    return static_cast<RegExpStatics *>(resObj.getPrivate());
}

inline size_t
SizeOfRegExpStaticsData(const JSObject *obj, JSMallocSizeOfFun mallocSizeOf)
{
    return mallocSizeOf(obj->getPrivate());
}

inline
RegExpStatics::RegExpStatics()
  : pendingLazyEvaluation(false),
    bufferLink(NULL),
    copied(false)
{
    clear();
}

inline bool
RegExpStatics::createDependent(JSContext *cx, size_t start, size_t end, Value *out)
{
    /* Private function: caller must perform lazy evaluation. */
    JS_ASSERT(!pendingLazyEvaluation);

    JS_ASSERT(start <= end);
    JS_ASSERT(end <= matchesInput->length());
    JSString *str = js_NewDependentString(cx, matchesInput, start, end - start);
    if (!str)
        return false;
    *out = StringValue(str);
    return true;
}

inline bool
RegExpStatics::createPendingInput(JSContext *cx, Value *out)
{
    /* Lazy evaluation need not be resolved to return the input. */
    out->setString(pendingInput ? pendingInput.get() : cx->runtime->emptyString);
    return true;
}

inline bool
RegExpStatics::makeMatch(JSContext *cx, size_t checkValidIndex, size_t pairNum, Value *out)
{
    /* Private function: caller must perform lazy evaluation. */
    JS_ASSERT(!pendingLazyEvaluation);

    bool checkWhich  = checkValidIndex % 2;
    size_t checkPair = checkValidIndex / 2;

    if (matches.empty() || checkPair >= matches.pairCount() ||
        (checkWhich ? matches[checkPair].limit : matches[checkPair].start) < 0)
    {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    const MatchPair &pair = matches[pairNum];
    return createDependent(cx, pair.start, pair.limit, out);
}

inline bool
RegExpStatics::createLastMatch(JSContext *cx, Value *out)
{
    if (!executeLazy(cx))
        return false;
    return makeMatch(cx, 0, 0, out);
}

inline bool
RegExpStatics::createLastParen(JSContext *cx, Value *out)
{
    if (!executeLazy(cx))
        return false;

    if (matches.empty() || matches.pairCount() == 1) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    const MatchPair &pair = matches[matches.pairCount() - 1];
    if (pair.start == -1) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    JS_ASSERT(pair.start >= 0 && pair.limit >= 0);
    JS_ASSERT(pair.limit >= pair.start);
    return createDependent(cx, pair.start, pair.limit, out);
}

inline bool
RegExpStatics::createParen(JSContext *cx, size_t pairNum, Value *out)
{
    JS_ASSERT(pairNum >= 1);
    if (!executeLazy(cx))
        return false;

    if (matches.empty() || pairNum >= matches.pairCount()) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    return makeMatch(cx, pairNum * 2, pairNum, out);
}

inline bool
RegExpStatics::createLeftContext(JSContext *cx, Value *out)
{
    if (!executeLazy(cx))
        return false;

    if (matches.empty()) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    if (matches[0].start < 0) {
        *out = UndefinedValue();
        return true;
    }
    return createDependent(cx, 0, matches[0].start, out);
}

inline bool
RegExpStatics::createRightContext(JSContext *cx, Value *out)
{
    if (!executeLazy(cx))
        return false;

    if (matches.empty()) {
        out->setString(cx->runtime->emptyString);
        return true;
    }
    if (matches[0].limit < 0) {
        *out = UndefinedValue();
        return true;
    }
    return createDependent(cx, matches[0].limit, matchesInput->length(), out);
}

inline void
RegExpStatics::getParen(size_t pairNum, JSSubString *out) const
{
    JS_ASSERT(!pendingLazyEvaluation);

    JS_ASSERT(pairNum >= 1 && pairNum < matches.pairCount());
    const MatchPair &pair = matches[pairNum];
    if (pair.isUndefined()) {
        *out = js_EmptySubString;
        return;
    }
    out->chars  = matchesInput->chars() + pair.start;
    out->length = pair.length();
}

inline void
RegExpStatics::getLastMatch(JSSubString *out) const
{
    JS_ASSERT(!pendingLazyEvaluation);

    if (matches.empty()) {
        *out = js_EmptySubString;
        return;
    }
    JS_ASSERT(matchesInput);
    out->chars = matchesInput->chars() + matches[0].start;
    JS_ASSERT(matches[0].limit >= matches[0].start);
    out->length = matches[0].length();
}

inline void
RegExpStatics::getLastParen(JSSubString *out) const
{
    JS_ASSERT(!pendingLazyEvaluation);

    /* Note: the first pair is the whole match. */
    if (matches.empty() || matches.pairCount() == 1) {
        *out = js_EmptySubString;
        return;
    }
    getParen(matches.parenCount(), out);
}

inline void
RegExpStatics::getLeftContext(JSSubString *out) const
{
    JS_ASSERT(!pendingLazyEvaluation);

    if (matches.empty()) {
        *out = js_EmptySubString;
        return;
    }
    out->chars = matchesInput->chars();
    out->length = matches[0].start;
}

inline void
RegExpStatics::getRightContext(JSSubString *out) const
{
    JS_ASSERT(!pendingLazyEvaluation);

    if (matches.empty()) {
        *out = js_EmptySubString;
        return;
    }
    out->chars = matchesInput->chars() + matches[0].limit;
    JS_ASSERT(matches[0].limit <= int(matchesInput->length()));
    out->length = matchesInput->length() - matches[0].limit;
}

inline void
RegExpStatics::copyTo(RegExpStatics &dst)
{
    /* Destination buffer has already been reserved by save(). */
    if (!pendingLazyEvaluation)
        dst.matches.initArrayFrom(matches);

    dst.matchesInput = matchesInput;
    dst.regexp = regexp;
    dst.lastIndex = lastIndex;
    dst.pendingInput = pendingInput;
    dst.flags = flags;
    dst.pendingLazyEvaluation = pendingLazyEvaluation;

    JS_ASSERT_IF(pendingLazyEvaluation, regexp);
    JS_ASSERT_IF(pendingLazyEvaluation, matchesInput);
}

inline void
RegExpStatics::aboutToWrite()
{
    if (bufferLink && !bufferLink->copied) {
        copyTo(*bufferLink);
        bufferLink->copied = true;
    }
}

inline void
RegExpStatics::restore()
{
    if (bufferLink->copied)
        bufferLink->copyTo(*this);
    bufferLink = bufferLink->bufferLink;
}

inline void
RegExpStatics::updateLazily(JSContext *cx, JSLinearString *input,
                            RegExpObject *regexp, size_t lastIndex)
{
    JS_ASSERT(input && regexp);
    aboutToWrite();

    BarrieredSetPair<JSString, JSLinearString>(cx->compartment,
                                               pendingInput, input,
                                               matchesInput, input);
    pendingLazyEvaluation = true;
    this->regexp = regexp;
    this->lastIndex = lastIndex;
}

inline bool
RegExpStatics::updateFromMatchPairs(JSContext *cx, JSLinearString *input, MatchPairs &newPairs)
{
    JS_ASSERT(input);
    aboutToWrite();

    /* Unset all lazy state. */
    pendingLazyEvaluation = false;
    this->regexp = NULL;
    this->lastIndex = size_t(-1);

    BarrieredSetPair<JSString, JSLinearString>(cx->compartment,
                                               pendingInput, input,
                                               matchesInput, input);

    if (!matches.initArrayFrom(newPairs)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

inline void
RegExpStatics::clear()
{
    aboutToWrite();
    flags = RegExpFlag(0);
    pendingInput = NULL;
    pendingLazyEvaluation = false;
    matchesInput = NULL;
    matches.forgetArray();
}

inline void
RegExpStatics::setPendingInput(JSString *newInput)
{
    aboutToWrite();
    pendingInput = newInput;
}

PreserveRegExpStatics::~PreserveRegExpStatics()
{
    original->restore();
}

inline void
RegExpStatics::setMultiline(JSContext *cx, bool enabled)
{
    aboutToWrite();
    if (enabled) {
        flags = RegExpFlag(flags | MultilineFlag);
        markFlagsSet(cx);
    } else {
        flags = RegExpFlag(flags & ~MultilineFlag);
    }
}

inline void
RegExpStatics::markFlagsSet(JSContext *cx)
{
    /*
     * Flags set on the RegExp function get propagated to constructed RegExp
     * objects, which interferes with optimizations that inline RegExp cloning
     * or avoid cloning entirely. Scripts making this assumption listen to
     * type changes on RegExp.prototype, so mark a state change to trigger
     * recompilation of all such code (when recompiling, a stub call will
     * always be performed).
     */
    JS_ASSERT(this == cx->global()->getRegExpStatics());

    types::MarkTypeObjectFlags(cx, cx->global(), types::OBJECT_FLAG_REGEXP_FLAGS_SET);
}

inline void
RegExpStatics::reset(JSContext *cx, JSString *newInput, bool newMultiline)
{
    aboutToWrite();
    clear();
    pendingInput = newInput;
    setMultiline(cx, newMultiline);
    checkInvariants();
}

inline void
RegExpStatics::checkInvariants()
{
#ifdef DEBUG
    if (pendingLazyEvaluation) {
        JS_ASSERT(regexp);
        JS_ASSERT(pendingInput);
        return;
    }

    if (matches.empty()) {
        JS_ASSERT(!matchesInput);
        return;
    }

    /* Pair count is non-zero, so there must be match pairs input. */
    JS_ASSERT(matchesInput);
    size_t mpiLen = matchesInput->length();

    /* Both members of the first pair must be non-negative. */
    JS_ASSERT(!matches[0].isUndefined());
    JS_ASSERT(matches[0].limit >= 0);

    /* Present pairs must be valid. */
    for (size_t i = 0; i < matches.pairCount(); i++) {
        if (matches[i].isUndefined())
            continue;
        const MatchPair &pair = matches[i];
        JS_ASSERT(mpiLen >= size_t(pair.limit) && pair.limit >= pair.start && pair.start >= 0);
    }
#endif /* DEBUG */
}

} /* namespace js */

inline js::RegExpStatics *
JSContext::regExpStatics()
{
    return global()->getRegExpStatics();
}

#endif
