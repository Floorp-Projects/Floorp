/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PATH_H
#define GFX_PATH_H

#include "gfxTypes.h"
#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"

class gfxContext;
typedef struct cairo_path cairo_path_t;

namespace mozilla {
namespace gfx {
class Path;
}
}

/**
 * Class representing a path. Can be created by copying the current path
 * of a gfxContext.
 */
class gfxPath {
    NS_INLINE_DECL_REFCOUNTING(gfxPath)

    friend class gfxContext;

    gfxPath(cairo_path_t* aPath);

public:
    gfxPath(mozilla::gfx::Path* aPath);
    virtual ~gfxPath();

private:
    cairo_path_t* mPath;
    mozilla::RefPtr<mozilla::gfx::Path> mMoz2DPath;
};

#endif
