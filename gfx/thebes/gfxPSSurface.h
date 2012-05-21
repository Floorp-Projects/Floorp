/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PSSURFACE_H
#define GFX_PSSURFACE_H

#include "gfxASurface.h"

/* for the output stream */
#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "gfxContext.h"

class THEBES_API gfxPSSurface : public gfxASurface {
public:
    enum PageOrientation {
        PORTRAIT,
        LANDSCAPE
    };

    gfxPSSurface(nsIOutputStream *aStream, const gfxSize& aSizeInPoints, PageOrientation aOrientation);
    virtual ~gfxPSSurface();

    virtual nsresult BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName);
    virtual nsresult EndPrinting();
    virtual nsresult AbortPrinting();
    virtual nsresult BeginPage();
    virtual nsresult EndPage();
    virtual void Finish();

    void SetDPI(double x, double y);
    void GetDPI(double *xDPI, double *yDPI);

    virtual bool GetRotateForLandscape() { return (mOrientation == LANDSCAPE); }

    // this is in points!
    virtual const gfxIntSize GetSize() const
    {
        return mSize;
    }

    virtual PRInt32 GetDefaultContextFlags() const
    {
        return gfxContext::FLAG_SIMPLIFY_OPERATORS |
               gfxContext::FLAG_DISABLE_SNAPPING;
    }

private:
    nsCOMPtr<nsIOutputStream> mStream;
    double mXDPI;
    double mYDPI;
    gfxIntSize mSize;
    PageOrientation mOrientation;
};

#endif /* GFX_PSSURFACE_H */
