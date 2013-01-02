/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExpStatics_h__
#define RegExpStatics_h__

#include "mozilla/GuardObjects.h"

#include "jscntxt.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/Vector.h"

#include "vm/MatchPairs.h"
#include "vm/RegExpObject.h"

namespace js {

class RegExpStatics
{
    /* The latest RegExp output, set after execution. */
    VectorMatchPairs        matches;
    HeapPtr<JSLinearString> matchesInput;

    /* The previous RegExp input, used to resolve lazy state. */
    RegExpHeapGuard         regexp;
    size_t                  lastIndex;

    /* The latest RegExp input, set before execution. */
    HeapPtr<JSString>       pendingInput;
    RegExpFlag              flags;

    /*
     * If true, |matchesInput|, |regexp|, and |lastIndex| may be used
     * to replay the last executed RegExp, and |matches| is invalid.
     */
    bool                    pendingLazyEvaluation;

    /* Linkage for preserving RegExpStatics during nested RegExp execution. */
    RegExpStatics           *bufferLink;
    bool                    copied;

  public:
    RegExpStatics() : bufferLink(NULL), copied(false) { clear(); }
    static JSObject *create(JSContext *cx, GlobalObject *parent);

  private:
    bool executeLazy(JSContext *cx);

    inline void aboutToWrite();
    inline void copyTo(RegExpStatics &dst);

    inline void restore();
    bool save(JSContext *cx, RegExpStatics *buffer) {
        JS_ASSERT(!buffer->copied && !buffer->bufferLink);
        buffer->bufferLink = bufferLink;
        bufferLink = buffer;
        if (!buffer->matches.allocOrExpandArray(matches.length())) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }

    inline void checkInvariants();

    /*
     * Check whether the index at |checkValidIndex| is valid (>= 0).
     * If so, construct a string for it and place it in |*out|.
     * If not, place undefined in |*out|.
     */
    bool makeMatch(JSContext *cx, size_t checkValidIndex, size_t pairNum, Value *out);
    bool createDependent(JSContext *cx, size_t start, size_t end, Value *out);

    void markFlagsSet(JSContext *cx);

    struct InitBuffer {};
    explicit RegExpStatics(InitBuffer) : bufferLink(NULL), copied(false) {}

    friend class PreserveRegExpStatics;

  public:
    /* Mutators. */
    inline void updateLazily(JSContext *cx, JSLinearString *input,
                             RegExpShared *shared, size_t lastIndex);
    inline bool updateFromMatchPairs(JSContext *cx, JSLinearString *input, MatchPairs &newPairs);
    inline void setMultiline(JSContext *cx, bool enabled);

    inline void clear();

    /* Corresponds to JSAPI functionality to set the pending RegExp input. */
    inline void reset(JSContext *cx, JSString *newInput, bool newMultiline);

    inline void setPendingInput(JSString *newInput);

  public:
    /* Default match accessor. */
    const MatchPairs &getMatches() const {
        /* Safe: only used by String methods, which do not set lazy mode. */
        JS_ASSERT(!pendingLazyEvaluation);
        return matches;
    }

    JSString *getPendingInput() const { return pendingInput; }

    RegExpFlag getFlags() const { return flags; }
    bool multiline() const { return flags & MultilineFlag; }

    /* Returns whether results for a non-empty match are present. */
    bool matched() const {
        /* Safe: only used by String methods, which do not set lazy mode. */
        JS_ASSERT(!pendingLazyEvaluation);
        JS_ASSERT(matches.pairCount() > 0);
        return matches[0].limit - matches[0].start > 0;
    }

    void mark(JSTracer *trc) {
        /*
         * Changes to this function must also be reflected in
         * RegExpStatics::AutoRooter::trace().
         */
        if (pendingInput)
            MarkString(trc, &pendingInput, "res->pendingInput");
        if (matchesInput)
            MarkString(trc, &matchesInput, "res->matchesInput");
        if (regexp.initialized())
            regexp->trace(trc);
    }

    /* Value creators. */

    bool createPendingInput(JSContext *cx, Value *out);
    bool createLastMatch(JSContext *cx, Value *out);
    bool createLastParen(JSContext *cx, Value *out);
    bool createParen(JSContext *cx, size_t pairNum, Value *out);
    bool createLeftContext(JSContext *cx, Value *out);
    bool createRightContext(JSContext *cx, Value *out);

    /* Infallible substring creators. */

    void getParen(size_t pairNum, JSSubString *out) const;
    void getLastMatch(JSSubString *out) const;
    void getLastParen(JSSubString *out) const;
    void getLeftContext(JSSubString *out) const;
    void getRightContext(JSSubString *out) const;

    /* PreserveRegExpStatics helpers. */

    class AutoRooter : private AutoGCRooter
    {
      public:
        explicit AutoRooter(JSContext *cx, RegExpStatics *statics_
                            MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
          : AutoGCRooter(cx, REGEXPSTATICS), statics(statics_), skip(cx, statics_)
        {
            MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        }

        friend void AutoGCRooter::trace(JSTracer *trc);
        void trace(JSTracer *trc);

      private:
        RegExpStatics *statics;
        SkipRoot skip;
        MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    };
};

class PreserveRegExpStatics
{
    RegExpStatics * const original;
    RegExpStatics buffer;
    RegExpStatics::AutoRooter bufferRoot;

  public:
    explicit PreserveRegExpStatics(JSContext *cx, RegExpStatics *original)
     : original(original),
       buffer(RegExpStatics::InitBuffer()),
       bufferRoot(cx, &buffer)
    {}

    bool init(JSContext *cx) {
        return original->save(cx, &buffer);
    }

    inline ~PreserveRegExpStatics();
};

size_t SizeOfRegExpStaticsData(const JSObject *obj, JSMallocSizeOfFun mallocSizeOf);

} /* namespace js */

#endif /* RegExpStatics_h__ */
