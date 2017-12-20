/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

void
WebGLContext::BindVertexArray(WebGLVertexArray* array)
{
    if (IsContextLost())
        return;

    if (array && !ValidateObject("bindVertexArrayObject", *array))
        return;

    if (mBoundVertexArray) {
        mBoundVertexArray->AddBufferBindCounts(-1);
    }

    if (array == nullptr) {
        array = mDefaultVertexArray;
    }

    array->BindVertexArray();

    MOZ_ASSERT(mBoundVertexArray == array);
    if (mBoundVertexArray) {
        mBoundVertexArray->AddBufferBindCounts(+1);
    }
}

already_AddRefed<WebGLVertexArray>
WebGLContext::CreateVertexArray()
{
    if (IsContextLost())
        return nullptr;

    RefPtr<WebGLVertexArray> globj = CreateVertexArrayImpl();

    globj->GenVertexArray();

    return globj.forget();
}

WebGLVertexArray*
WebGLContext::CreateVertexArrayImpl()
{
    return WebGLVertexArray::Create(this);
}

void
WebGLContext::DeleteVertexArray(WebGLVertexArray* array)
{
    if (!ValidateDeleteObject("deleteVertexArray", array))
        return;

    if (mBoundVertexArray == array)
        BindVertexArray(static_cast<WebGLVertexArray*>(nullptr));

    array->RequestDelete();
}

bool
WebGLContext::IsVertexArray(const WebGLVertexArray* array)
{
    if (!ValidateIsObject("isVertexArray", array))
        return false;

    return array->IsVertexArray();
}

} // namespace mozilla
