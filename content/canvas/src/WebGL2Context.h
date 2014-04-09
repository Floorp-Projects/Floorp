/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL2CONTEXT_H_
#define WEBGL2CONTEXT_H_

#include "WebGLContext.h"

namespace mozilla {

class WebGL2Context
    : public WebGLContext
{
// -----------------------------------------------------------------------------
// PUBLIC
public:

    // -------------------------------------------------------------------------
    // DESTRUCTOR

    virtual ~WebGL2Context();


    // -------------------------------------------------------------------------
    // STATIC FUNCTIONS

    static bool IsSupported();

    static WebGL2Context* Create();


    // -------------------------------------------------------------------------
    // IMPLEMENT WebGLContext

    virtual bool IsWebGL2() const MOZ_OVERRIDE
    {
        return true;
    }


    // -------------------------------------------------------------------------
    // IMPLEMENT nsWrapperCache

    virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;


// -----------------------------------------------------------------------------
// PRIVATE
private:

    // -------------------------------------------------------------------------
    // CONSTRUCTOR

    WebGL2Context();


};

} // namespace mozilla

#endif
