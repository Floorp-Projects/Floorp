/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PATH_H
#define GFX_PATH_H

#include "gfxTypes.h"
#include "nsISupportsImpl.h"

class gfxContext;
struct gfxPoint;
typedef struct cairo_path cairo_path_t;

/**
 * Class representing a path. Can be created by copying the current path
 * of a gfxContext.
 */
class THEBES_API gfxPath {
    NS_INLINE_DECL_REFCOUNTING(gfxPath)

    friend class gfxContext;

protected:
    gfxPath(cairo_path_t* aPath);

public:
    virtual ~gfxPath();

protected:
    cairo_path_t* mPath;
};

/**
 * Specialization of a path that only contains linear pieces. Can be created
 * from the existing path of a gfxContext.
 */
class THEBES_API gfxFlattenedPath : public gfxPath {
    friend class gfxContext;

protected:
    gfxFlattenedPath(cairo_path_t* aPath);

public:
    virtual ~gfxFlattenedPath();

    /**
     * Returns calculated total length of path
     */
    gfxFloat GetLength();

    /**
     * Returns a point a certain distance along the path.  Return is
     * first or last point of the path if the requested length offset
     * is outside the range for the path.
     * @param aOffset offset inpath parameter space (x=length, y=normal offset)
     * @param aAngle optional - output tangent
     */
    gfxPoint FindPoint(gfxPoint aOffset,
                       gfxFloat* aAngle = nullptr);
};

#endif
