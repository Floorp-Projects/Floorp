/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSRENDERINGCONTEXT__H__
#define NSRENDERINGCONTEXT__H__

#include <stdint.h>                     // for uint32_t
#include <sys/types.h>                  // for int32_t
#include "gfxContext.h"                 // for gfxContext
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/UserData.h"       // for UserData, UserDataKey
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsBoundingMetrics.h"          // for nsBoundingMetrics
#include "nsColor.h"                    // for nscolor
#include "nsCoord.h"                    // for nscoord, NSToIntRound
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsFontMetrics.h"              // for nsFontMetrics
#include "nsISupports.h"                // for NS_INLINE_DECL_REFCOUNTING, etc
#include "nsString.h"               // for nsString
#include "nscore.h"                     // for char16_t

class nsIntRegion;
struct nsPoint;
struct nsRect;

typedef enum {
    nsLineStyle_kNone   = 0,
    nsLineStyle_kSolid  = 1,
    nsLineStyle_kDashed = 2,
    nsLineStyle_kDotted = 3
} nsLineStyle;

class nsRenderingContext MOZ_FINAL
{
    typedef mozilla::gfx::UserData UserData;
    typedef mozilla::gfx::UserDataKey UserDataKey;
    typedef mozilla::gfx::DrawTarget DrawTarget;

public:
    nsRenderingContext() : mP2A(0.) {}

    NS_INLINE_DECL_REFCOUNTING(nsRenderingContext)

    void Init(nsDeviceContext* aContext, gfxContext* aThebesContext);
    void Init(nsDeviceContext* aContext, DrawTarget* aDrawTarget);

    // These accessors will never return null.
    gfxContext *ThebesContext() { return mThebes; }
    DrawTarget *GetDrawTarget() { return mThebes->GetDrawTarget(); }
    nsDeviceContext *DeviceContext() { return mDeviceContext; }

    // Graphics state

    void PushState(void);
    void PopState(void);
    void IntersectClip(const nsRect& aRect);
    void SetClip(const nsIntRegion& aRegion);
    void SetLineStyle(nsLineStyle aLineStyle);
    void SetColor(nscolor aColor);
    void Translate(const nsPoint& aPt);
    void Scale(float aSx, float aSy);

    class AutoPushTranslation {
        nsRenderingContext* mCtx;
    public:
        AutoPushTranslation(nsRenderingContext* aCtx, const nsPoint& aPt)
            : mCtx(aCtx) {
            mCtx->PushState();
            mCtx->Translate(aPt);
        }
        ~AutoPushTranslation() {
            mCtx->PopState();
        }
    };

    // Shapes

    void DrawLine(const nsPoint& aStartPt, const nsPoint& aEndPt);
    void DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
    void DrawRect(const nsRect& aRect);
    void DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    void DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

    void FillRect(const nsRect& aRect);
    void FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    void FillPolygon(const nsPoint aPoints[], int32_t aNumPoints);

    void FillEllipse(const nsRect& aRect);
    void FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

    void InvertRect(const nsRect& aRect);

    // Text

    void SetFont(nsFontMetrics *aFontMetrics);
    nsFontMetrics *FontMetrics() { return mFontMetrics; } // may be null

    void SetTextRunRTL(bool aIsRTL);

    nscoord GetWidth(char aC);
    nscoord GetWidth(char16_t aC);
    nscoord GetWidth(const nsString& aString);
    nscoord GetWidth(const char* aString);
    nscoord GetWidth(const char* aString, uint32_t aLength);
    nscoord GetWidth(const char16_t *aString, uint32_t aLength);

    nsBoundingMetrics GetBoundingMetrics(const char16_t *aString,
                                         uint32_t aLength);

    void DrawString(const nsString& aString, nscoord aX, nscoord aY);
    void DrawString(const char *aString, uint32_t aLength,
                    nscoord aX, nscoord aY);
    void DrawString(const char16_t *aString, uint32_t aLength,
                    nscoord aX, nscoord aY);

    void AddUserData(UserDataKey *key, void *userData, void (*destroy)(void*)) {
      mUserData.Add(key, userData, destroy);
    }
    void *GetUserData(UserDataKey *key) {
      return mUserData.Get(key);
    }
    void *RemoveUserData(UserDataKey *key) {
      return mUserData.Remove(key);
    }

private:
    // Private destructor, to discourage deletion outside of Release():
    ~nsRenderingContext()
    {
    }

    int32_t GetMaxChunkLength();

    nsRefPtr<gfxContext> mThebes;
    nsRefPtr<nsDeviceContext> mDeviceContext;
    nsRefPtr<nsFontMetrics> mFontMetrics;

    double mP2A; // cached app units per device pixel value

    UserData mUserData;
};

#endif  // NSRENDERINGCONTEXT__H__
