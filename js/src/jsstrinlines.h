/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsstrinlines_h
#define jsstrinlines_h

#include "jsstr.h"

#include "mozilla/Attributes.h"

#include "jsatom.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"

#include "vm/String-inl.h"

namespace js {

class RopeBuilder {
    JSContext *cx;
    RootedString res;

    RopeBuilder(const RopeBuilder &other) MOZ_DELETE;
    void operator=(const RopeBuilder &other) MOZ_DELETE;

  public:
    RopeBuilder(JSContext *cx)
      : cx(cx), res(cx, cx->runtime()->emptyString)
    {}

    inline bool append(HandleString str) {
        res = ConcatStrings<CanGC>(cx, res, str);
        return !!res;
    }

    inline JSString *result() {
        return res;
    }
};

class StringSegmentRange
{
    /*
     * If malloc() shows up in any profiles from this vector, we can add a new
     * StackAllocPolicy which stashes a reusable freed-at-gc buffer in the cx.
     */
    AutoStringVector stack;
    Rooted<JSLinearString*> cur;

    bool settle(JSString *str) {
        while (str->isRope()) {
            JSRope &rope = str->asRope();
            if (!stack.append(rope.rightChild()))
                return false;
            str = rope.leftChild();
        }
        cur = &str->asLinear();
        return true;
    }

  public:
    StringSegmentRange(JSContext *cx)
      : stack(cx), cur(cx)
    {}

    JS_WARN_UNUSED_RESULT bool init(JSString *str) {
        JS_ASSERT(stack.empty());
        return settle(str);
    }

    bool empty() const {
        return cur == NULL;
    }

    JSLinearString *front() const {
        JS_ASSERT(!cur->isRope());
        return cur;
    }

    JS_WARN_UNUSED_RESULT bool popFront() {
        JS_ASSERT(!empty());
        if (stack.empty()) {
            cur = NULL;
            return true;
        }
        return settle(stack.popCopy());
    }
};

}  /* namespace js */

#endif /* jsstrinlines_h */
