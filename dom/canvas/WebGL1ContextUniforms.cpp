/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL1Context.h"

namespace mozilla {

bool
WebGL1Context::ValidateUniformMatrixTranspose(bool transpose, const char* info)
{
    if (transpose) {
        ErrorInvalidValue("%s: transpose must be FALSE as per the "
                          "OpenGL ES 2.0 spec", info);
        return false;
    }

    return true;
}

} // namespace mozilla
