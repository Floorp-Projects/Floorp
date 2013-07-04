/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/RegExpStatics.h"

#include "vm/RegExpStaticsObject.h"

#include "jsobjinlines.h"

#include "vm/RegExpObject-inl.h"
#include "vm/RegExpStatics-inl.h"

using namespace js;

/*
 * RegExpStatics allocates memory -- in order to keep the statics stored
 * per-global and not leak, we create a js::Class to wrap the C++ instance and
 * provide an appropriate finalizer. We store an instance of that js::Class in
 * a global reserved slot.
 */

static void
resc_finalize(FreeOp *fop, JSObject *obj)
{
    RegExpStatics *res = static_cast<RegExpStatics *>(obj->getPrivate());
    fop->delete_(res);
}

static void
resc_trace(JSTracer *trc, JSObject *obj)
{
    void *pdata = obj->getPrivate();
    JS_ASSERT(pdata);
    RegExpStatics *res = static_cast<RegExpStatics *>(pdata);
    res->mark(trc);
}

Class RegExpStaticsObject::class_ = {
    "RegExpStatics",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    resc_finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    resc_trace
};

JSObject *
RegExpStatics::create(JSContext *cx, GlobalObject *parent)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &RegExpStaticsObject::class_, NULL, parent);
    if (!obj)
        return NULL;
    RegExpStatics *res = cx->new_<RegExpStatics>();
    if (!res)
        return NULL;
    obj->setPrivate(static_cast<void *>(res));
    return obj;
}

bool
RegExpStatics::executeLazy(JSContext *cx)
{
    if (!pendingLazyEvaluation)
        return true;

    JS_ASSERT(lazySource);
    JS_ASSERT(matchesInput);
    JS_ASSERT(lazyIndex != size_t(-1));

    /* Retrieve or create the RegExpShared in this compartment. */
    RegExpGuard g(cx);
    if (!cx->compartment()->regExps.get(cx, lazySource, lazyFlags, &g))
        return false;

    /*
     * It is not necessary to call aboutToWrite(): evaluation of
     * implicit copies is safe.
     */

    size_t length = matchesInput->length();
    const jschar *chars = matchesInput->chars();

    /* Execute the full regular expression. */
    RegExpRunStatus status = g->execute(cx, chars, length, &this->lazyIndex, this->matches);
    if (status == RegExpRunStatus_Error)
        return false;

    /*
     * RegExpStatics are only updated on successful (matching) execution.
     * Re-running the same expression must therefore produce a matching result.
     */
    JS_ASSERT(status == RegExpRunStatus_Success);

    /* Unset lazy state and remove rooted values that now have no use. */
    pendingLazyEvaluation = false;
    lazySource = NULL;
    lazyIndex = size_t(-1);

    return true;
}
