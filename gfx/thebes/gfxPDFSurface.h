/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PDFSURFACE_H
#define GFX_PDFSURFACE_H

#include "gfxASurface.h"
#include "gfxContext.h"

/* for the output stream */
#include "nsCOMPtr.h"
#include "nsIOutputStream.h"

class THEBES_API gfxPDFSurface : public gfxASurface {
public:
    gfxPDFSurface(nsIOutputStream *aStream, const gfxSize& aSizeInPoints);
    virtual ~gfxPDFSurface();

    virtual nsresult BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName);
    virtual nsresult EndPrinting();
    virtual nsresult AbortPrinting();
    virtual nsresult BeginPage();
    virtual nsresult EndPage();
    virtual void Finish();

    void SetDPI(double x, double y);
    void GetDPI(double *xDPI, double *yDPI);

    // this is in points!
    virtual const gfxIntSize GetSize() const
    {
        return gfxIntSize(mSize.width, mSize.height);
    }

    virtual PRInt32 GetDefaultContextFlags() const
    {
        return gfxContext::FLAG_SIMPLIFY_OPERATORS |
               gfxContext::FLAG_DISABLE_SNAPPING |
               gfxContext::FLAG_DISABLE_COPY_BACKGROUND;
    }

private:
    nsCOMPtr<nsIOutputStream> mStream;
    double mXDPI;
    double mYDPI;
    gfxSize mSize;
};

#endif /* GFX_PDFSURFACE_H */
