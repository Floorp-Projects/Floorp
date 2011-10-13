/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Leary <cdleary@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef RegExpStatics_h__
#define RegExpStatics_h__

#include "jscntxt.h"

#include "js/Vector.h"

namespace js {

class RegExpStatics
{
    typedef Vector<int, 20, SystemAllocPolicy> MatchPairs;
    MatchPairs      matchPairs;
    /* The input that was used to produce matchPairs. */
    JSLinearString  *matchPairsInput;
    /* The input last set on the statics. */
    JSString        *pendingInput;
    RegExpFlag      flags;
    RegExpStatics   *bufferLink;
    bool            copied;

    bool createDependent(JSContext *cx, size_t start, size_t end, Value *out) const;

    void copyTo(RegExpStatics &dst) {
        dst.matchPairs.clear();
        /* 'save' has already reserved space in matchPairs */
        dst.matchPairs.infallibleAppend(matchPairs);
        dst.matchPairsInput = matchPairsInput;
        dst.pendingInput = pendingInput;
        dst.flags = flags;
    }

    void aboutToWrite() {
        if (bufferLink && !bufferLink->copied) {
            copyTo(*bufferLink);
            bufferLink->copied = true;
        }
    }

    bool save(JSContext *cx, RegExpStatics *buffer) {
        JS_ASSERT(!buffer->copied && !buffer->bufferLink);
        buffer->bufferLink = bufferLink;
        bufferLink = buffer;
        if (!buffer->matchPairs.reserve(matchPairs.length())) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }

    void restore() {
        if (bufferLink->copied)
            bufferLink->copyTo(*this);
        bufferLink = bufferLink->bufferLink;
    }

    void checkInvariants() {
#if DEBUG
        if (pairCount() == 0) {
            JS_ASSERT(!matchPairsInput);
            return;
        }

        /* Pair count is non-zero, so there must be match pairs input. */
        JS_ASSERT(matchPairsInput);
        size_t mpiLen = matchPairsInput->length();

        /* Both members of the first pair must be non-negative. */
        JS_ASSERT(pairIsPresent(0));
        JS_ASSERT(get(0, 1) >= 0);

        /* Present pairs must be valid. */
        for (size_t i = 0; i < pairCount(); ++i) {
            if (!pairIsPresent(i))
                continue;
            int start = get(i, 0);
            int limit = get(i, 1);
            JS_ASSERT(mpiLen >= size_t(limit) && limit >= start && start >= 0);
        }
#endif
    }

    /* 
     * Since the first pair indicates the whole match, the paren pair
     * numbers have to be in the range [1, pairCount).
     */
    void checkParenNum(size_t pairNum) const {
        JS_ASSERT(1 <= pairNum);
        JS_ASSERT(pairNum < pairCount());
    }

    /* Precondition: paren is present. */
    size_t getParenLength(size_t pairNum) const {
        checkParenNum(pairNum);
        JS_ASSERT(pairIsPresent(pairNum));
        return get(pairNum, 1) - get(pairNum, 0);
    }

    int get(size_t pairNum, bool which) const {
        JS_ASSERT(pairNum < pairCount());
        return matchPairs[2 * pairNum + which];
    }

    /*
     * Check whether the index at |checkValidIndex| is valid (>= 0).
     * If so, construct a string for it and place it in |*out|.
     * If not, place undefined in |*out|.
     */
    bool makeMatch(JSContext *cx, size_t checkValidIndex, size_t pairNum, Value *out) const;

    void markFlagsSet(JSContext *cx);

    struct InitBuffer {};
    explicit RegExpStatics(InitBuffer) : bufferLink(NULL), copied(false) {}

    friend class PreserveRegExpStatics;

  public:
    RegExpStatics() : bufferLink(NULL), copied(false) { clear(); }

    static JSObject *create(JSContext *cx, GlobalObject *parent);

    /* Mutators. */

    bool updateFromMatch(JSContext *cx, JSLinearString *input, int *buf, size_t matchItemCount) {
        aboutToWrite();
        pendingInput = input;

        if (!matchPairs.resizeUninitialized(matchItemCount)) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        for (size_t i = 0; i < matchItemCount; ++i)
            matchPairs[i] = buf[i];

        matchPairsInput = input;
        return true;
    }

    inline void setMultiline(JSContext *cx, bool enabled);

    void clear() {
        aboutToWrite();
        flags = RegExpFlag(0);
        pendingInput = NULL;
        matchPairsInput = NULL;
        matchPairs.clear();
    }

    /* Corresponds to JSAPI functionality to set the pending RegExp input. */
    inline void reset(JSContext *cx, JSString *newInput, bool newMultiline);

    void setPendingInput(JSString *newInput) {
        aboutToWrite();
        pendingInput = newInput;
    }

    /* Accessors. */

    /*
     * When there is a match present, the pairCount is at least 1 for the whole
     * match. There is one additional pair per parenthesis.
     *
     * Getting a parenCount requires there to be a match result as a precondition.
     */

  private:
    size_t pairCount() const {
        JS_ASSERT(matchPairs.length() % 2 == 0);
        return matchPairs.length() / 2;
    }

  public:
    size_t parenCount() const {
        size_t pc = pairCount();
        JS_ASSERT(pc);
        return pc - 1;
    }

    JSString *getPendingInput() const { return pendingInput; }
    RegExpFlag getFlags() const { return flags; }
    bool multiline() const { return flags & MultilineFlag; }

    size_t matchStart() const {
        int start = get(0, 0);
        JS_ASSERT(start >= 0);
        return size_t(start);
    }

    size_t matchLimit() const {
        int limit = get(0, 1);
        JS_ASSERT(size_t(limit) >= matchStart() && limit >= 0);
        return size_t(limit);
    }

    /* Returns whether results for a non-empty match are present. */
    bool matched() const {
        JS_ASSERT(pairCount() > 0);
        JS_ASSERT_IF(get(0, 1) == -1, get(1, 1) == -1);
        return get(0, 1) - get(0, 0) > 0;
    }

    void mark(JSTracer *trc) const {
        if (pendingInput)
            JS_CALL_STRING_TRACER(trc, pendingInput, "res->pendingInput");
        if (matchPairsInput)
            JS_CALL_STRING_TRACER(trc, matchPairsInput, "res->matchPairsInput");
    }

    bool pairIsPresent(size_t pairNum) const {
        return get(pairNum, 0) >= 0;
    }

    /* Value creators. */

    bool createPendingInput(JSContext *cx, Value *out) const;
    bool createLastMatch(JSContext *cx, Value *out) const { return makeMatch(cx, 0, 0, out); }
    bool createLastParen(JSContext *cx, Value *out) const;
    bool createLeftContext(JSContext *cx, Value *out) const;
    bool createRightContext(JSContext *cx, Value *out) const;

    /* @param pairNum   Any number >= 1. */
    bool createParen(JSContext *cx, size_t pairNum, Value *out) const {
        JS_ASSERT(pairNum >= 1);
        if (pairNum >= pairCount()) {
            out->setString(cx->runtime->emptyString);
            return true;
        }
        return makeMatch(cx, pairNum * 2, pairNum, out);
    }

    /* Substring creators. */

    void getParen(size_t pairNum, JSSubString *out) const;
    void getLastMatch(JSSubString *out) const;
    void getLastParen(JSSubString *out) const;
    void getLeftContext(JSSubString *out) const;
    void getRightContext(JSSubString *out) const;
};

class PreserveRegExpStatics
{
    RegExpStatics * const original;
    RegExpStatics buffer;

  public:
    explicit PreserveRegExpStatics(RegExpStatics *original)
     : original(original),
       buffer(RegExpStatics::InitBuffer())
    {}

    bool init(JSContext *cx) {
        return original->save(cx, &buffer);
    }

    ~PreserveRegExpStatics() {
        original->restore();
    }
};

} /* namespace js */

#endif
