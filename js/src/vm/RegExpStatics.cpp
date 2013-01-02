/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/RegExpStatics.h"

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
resc_finalize(FreeOp *fop, RawObject obj)
{
    RegExpStatics *res = static_cast<RegExpStatics *>(obj->getPrivate());
    fop->delete_(res);
}

static void
resc_trace(JSTracer *trc, RawObject obj)
{
    void *pdata = obj->getPrivate();
    JS_ASSERT(pdata);
    RegExpStatics *res = static_cast<RegExpStatics *>(pdata);
    res->mark(trc);
}

Class js::RegExpStaticsClass = {
    "RegExpStatics",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
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
    JSObject *obj = NewObjectWithGivenProto(cx, &RegExpStaticsClass, NULL, parent);
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

    JS_ASSERT(regexp.initialized());
    JS_ASSERT(matchesInput);
    JS_ASSERT(lastIndex != size_t(-1));

    /*
     * It is not necessary to call aboutToWrite(): evaluation of
     * implicit copies is safe.
     */

    size_t length = matchesInput->length();
    StableCharPtr chars(matchesInput->chars(), length);

    /* Execute the full regular expression. */
    RegExpRunStatus status = regexp->execute(cx, chars, length, &this->lastIndex, this->matches);
    if (status == RegExpRunStatus_Error)
        return false;

    /* Unset lazy state and remove rooted values that now have no use. */
    pendingLazyEvaluation = false;
    regexp.release();
    lastIndex = size_t(-1);

    return true;
}
