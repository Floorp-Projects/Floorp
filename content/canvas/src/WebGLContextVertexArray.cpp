/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLVertexArray.h"

using namespace mozilla;

void
WebGLContext::BindVertexArray(WebGLVertexArray *array)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindVertexArrayObject", array))
        return;

    if (array && array->IsDeleted()) {
        /* http://www.khronos.org/registry/gles/extensions/OES/OES_vertex_array_object.txt
         * BindVertexArrayOES fails and an INVALID_OPERATION error is
         * generated if array is not a name returned from a previous call to
         * GenVertexArraysOES, or if such a name has since been deleted with
         * DeleteVertexArraysOES
         */
        ErrorInvalidOperation("bindVertexArray: can't bind a deleted array!");
        return;
    }

    InvalidateBufferFetching();

    MakeContextCurrent();

    if (array) {
        gl->fBindVertexArray(array->GLName());
        array->SetHasEverBeenBound(true);
        mBoundVertexArray = array;
    }
    else {
        gl->fBindVertexArray(0);
        mBoundVertexArray = mDefaultVertexArray;
    }
}

already_AddRefed<WebGLVertexArray>
WebGLContext::CreateVertexArray()
{
    if (IsContextLost())
        return nullptr;

    nsRefPtr<WebGLVertexArray> globj = new WebGLVertexArray(this);

    MakeContextCurrent();
    gl->fGenVertexArrays(1, &globj->mGLName);

    mVertexArrays.insertBack(globj);

    return globj.forget();
}

void
WebGLContext::DeleteVertexArray(WebGLVertexArray *array)
{
    if (IsContextLost())
        return;

    if (array == nullptr)
        return;

    if (array->IsDeleted())
        return;

    if (mBoundVertexArray == array)
        BindVertexArray(static_cast<WebGLVertexArray*>(nullptr));

    array->RequestDelete();
}

bool
WebGLContext::IsVertexArray(WebGLVertexArray *array)
{
    if (IsContextLost())
        return false;

    if (!array)
        return false;

    return ValidateObjectAllowDeleted("isVertexArray", array) &&
           !array->IsDeleted() &&
           array->HasEverBeenBound();
}


