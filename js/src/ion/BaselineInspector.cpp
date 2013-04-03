/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineIC.h"
#include "BaselineInspector.h"

using namespace js;
using namespace js::ion;

bool
SetElemICInspector::sawOOBTypedArrayWrite() const
{
    if (!icEntry_)
        return false;

    // Check for SetElem_TypedArray stubs with expectOutOfBounds set.
    for (ICStub *stub = icEntry_->firstStub(); stub; stub = stub->next()) {
        if (!stub->isSetElem_TypedArray())
            continue;
        if (stub->toSetElem_TypedArray()->expectOutOfBounds())
            return true;
    }
    return false;
}

RawShape
BaselineInspector::maybeMonomorphicShapeForPropertyOp(jsbytecode *pc)
{
    if (!hasBaselineScript())
        return NULL;

    JS_ASSERT(isValidPC(pc));
    const ICEntry &entry = icEntryFromPC(pc);

    ICStub *stub = entry.firstStub();
    ICStub *next = stub->next();

    if (!next || !next->isFallback())
        return NULL;

    if (stub->isGetProp_Native()) {
        JS_ASSERT(next->isGetProp_Fallback());
        if (next->toGetProp_Fallback()->hadUnoptimizableAccess())
            return NULL;
        return stub->toGetProp_Native()->shape();
    }

    if (stub->isSetProp_Native()) {
        JS_ASSERT(next->isSetProp_Fallback());
        if (next->toSetProp_Fallback()->hadUnoptimizableAccess())
            return NULL;
        return stub->toSetProp_Native()->shape();
    }

    return NULL;
}
