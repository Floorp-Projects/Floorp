/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/RematerializedFrame.h"
#include "jit/IonFrames.h"

#include "vm/ArgumentsObject.h"

#include "jsscriptinlines.h"
#include "jit/IonFrames-inl.h"

using namespace js;
using namespace jit;

struct CopyValueToRematerializedFrame
{
    Value *slots;

    explicit CopyValueToRematerializedFrame(Value *slots)
      : slots(slots)
    { }

    void operator()(const Value &v) {
        *slots++ = v;
    }
};

RematerializedFrame::RematerializedFrame(ThreadSafeContext *cx, uint8_t *top,
                                         InlineFrameIterator &iter)
  : prevUpToDate_(false),
    top_(top),
    frameNo_(iter.frameNo()),
    numActualArgs_(iter.numActualArgs()),
    script_(iter.script())
{
    CopyValueToRematerializedFrame op(slots_);
    iter.readFrameArgsAndLocals(cx, op, op, &scopeChain_, &returnValue_,
                                &argsObj_, &thisValue_, ReadFrame_Actuals);
}

/* static */ RematerializedFrame *
RematerializedFrame::New(ThreadSafeContext *cx, uint8_t *top, InlineFrameIterator &iter)
{
    unsigned numFormals = iter.isFunctionFrame() ? iter.callee()->nargs() : 0;
    size_t numBytes = sizeof(RematerializedFrame) +
        (Max(numFormals, iter.numActualArgs()) +
         iter.script()->nfixed()) * sizeof(Value) -
        sizeof(Value); // 1 Value included in sizeof(RematerializedFrame)

    void *buf = cx->calloc_(numBytes);
    if (!buf)
        return nullptr;

    return new (buf) RematerializedFrame(cx, top, iter);
}

CallObject &
RematerializedFrame::callObj() const
{
    JS_ASSERT(hasCallObj());

    JSObject *scope = scopeChain();
    while (!scope->is<CallObject>())
        scope = scope->enclosingScope();
    return scope->as<CallObject>();
}

void
RematerializedFrame::mark(JSTracer *trc)
{
    gc::MarkScriptRoot(trc, &script_, "remat ion frame script");
    gc::MarkObjectRoot(trc, &scopeChain_, "remat ion frame scope chain");
    gc::MarkValueRoot(trc, &returnValue_, "remat ion frame return value");
    gc::MarkValueRoot(trc, &thisValue_, "remat ion frame this");
    gc::MarkValueRootRange(trc, slots_, slots_ + numActualArgs_ + script_->nfixed(),
                           "remat ion frame stack");
}

void
RematerializedFrame::dump()
{
    fprintf(stderr, " Rematerialized Optimized Frame%s\n", inlined() ? " (inlined)" : "");
    if (isFunctionFrame()) {
        fprintf(stderr, "  callee fun: ");
#ifdef DEBUG
        js_DumpObject(callee());
#else
        fprintf(stderr, "?\n");
#endif
    } else {
        fprintf(stderr, "  global frame, no callee\n");
    }

    fprintf(stderr, "  file %s line %u\n",
            script()->filename(), (unsigned) script()->lineno());

    fprintf(stderr, "  script = %p\n", (void*) script());

    if (isFunctionFrame()) {
        fprintf(stderr, "  scope chain: ");
#ifdef DEBUG
        js_DumpObject(scopeChain());
#else
        fprintf(stderr, "?\n");
#endif

        if (hasArgsObj()) {
            fprintf(stderr, "  args obj: ");
#ifdef DEBUG
            js_DumpObject(&argsObj());
#else
            fprintf(stderr, "?\n");
#endif
        }

        fprintf(stderr, "  this: ");
#ifdef DEBUG
        js_DumpValue(thisValue());
#else
        fprintf(stderr, "?\n");
#endif

        for (unsigned i = 0; i < numActualArgs(); i++) {
            if (i < numFormalArgs())
                fprintf(stderr, "  formal (arg %d): ", i);
            else
                fprintf(stderr, "  overflown (arg %d): ", i);
#ifdef DEBUG
            js_DumpValue(argv()[i]);
#else
            fprintf(stderr, "?\n");
#endif
        }

        for (unsigned i = 0; i < script()->nfixed(); i++) {
            fprintf(stderr, "  local %d: ", i);
#ifdef DEBUG
            js_DumpValue(locals()[i]);
#else
            fprintf(stderr, "?\n");
#endif
        }
    }

    fputc('\n', stderr);
}
