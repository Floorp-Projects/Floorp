/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL1CONTEXT_H_
#define WEBGL1CONTEXT_H_

#include "WebGLContext.h"

namespace mozilla {

class WebGL1Context
    : public WebGLContext
{
// -----------------------------------------------------------------------------
// PUBLIC
public:

    // -------------------------------------------------------------------------
    // CONSTRUCTOR & DESTRUCTOR

    WebGL1Context();
    virtual ~WebGL1Context();


    // -------------------------------------------------------------------------
    // IMPLEMENT WebGLContext

    virtual bool IsWebGL2() const MOZ_OVERRIDE
    {
        return false;
    }


    // -------------------------------------------------------------------------
    // IMPLEMENT nsWrapperCache

    virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;


};

} // namespace mozilla

#endif
