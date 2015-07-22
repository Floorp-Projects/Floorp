/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Writing to the drawing buffer

void
WebGL2Context::DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLintptr offset)
{
    GenerateWarning("drawRangeElements: Not Implemented.");
}

} // namespace mozilla
