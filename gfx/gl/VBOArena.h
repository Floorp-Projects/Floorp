/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VBOARENA_H_
#define VBOARENA_H_

#include "GLTypes.h"
#include <vector>

namespace mozilla {
namespace gl {

class GLContext;

class VBOArena {
public:
    // Allocate a new VBO.
    GLuint Allocate(GLContext *aGLContext);

    // Re-use previously allocated VBOs.
    void Reset();

    // Throw away all allocated VBOs.
    void Flush(GLContext *aGLContext);
private:
    std::vector<GLuint> mAllocatedVBOs;
    std::vector<GLuint> mAvailableVBOs;
};

}
}

#endif
