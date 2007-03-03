/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Thebes gfx.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>

#include "nsMathUtils.h"

#include "gfxWindowsNativeDrawing.h"
#include "gfxWindowsSurface.h"
#include "gfxAlphaRecovery.h"

enum {
    RENDER_STATE_INIT,

    RENDER_STATE_NATIVE_DRAWING,
    RENDER_STATE_NATIVE_DRAWING_DONE,

    RENDER_STATE_ALPHA_RECOVERY_BLACK,
    RENDER_STATE_ALPHA_RECOVERY_BLACK_DONE,
    RENDER_STATE_ALPHA_RECOVERY_WHITE,
    RENDER_STATE_ALPHA_RECOVERY_WHITE_DONE,

    RENDER_STATE_DONE
};

gfxWindowsNativeDrawing::gfxWindowsNativeDrawing(gfxContext* ctx,
                                                 const gfxRect& nativeRect,
                                                 PRUint32 nativeDrawFlags)
    : mContext(ctx), mNativeRect(nativeRect), mNativeDrawFlags(nativeDrawFlags), mRenderState(RENDER_STATE_INIT)
{
}

HDC
gfxWindowsNativeDrawing::BeginNativeDrawing()
{
    if (mRenderState == RENDER_STATE_INIT) {
        nsRefPtr<gfxASurface> surf = mContext->CurrentSurface(&mDeviceOffset.x, &mDeviceOffset.y);

        gfxMatrix m = mContext->CurrentMatrix();
        if (!m.HasNonTranslation())
            mTransformType = TRANSLATION_ONLY;
        else if (m.HasNonAxisAlignedTransform())
            mTransformType = COMPLEX;
        else
            mTransformType = AXIS_ALIGNED_SCALE;

        // if this is a native win32 surface, we don't have to
        // redirect rendering to our own HDC; in some cases,
        // we may be able to use the HDC from the surface directly.
        if (surf->GetType() == gfxASurface::SurfaceTypeWin32 &&
            (surf->GetContentType() == gfxASurface::CONTENT_COLOR ||
             (surf->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA &&
              (mNativeDrawFlags & CAN_DRAW_TO_COLOR_ALPHA))))
        {
            if (mTransformType == TRANSLATION_ONLY) {
                mRenderState = RENDER_STATE_NATIVE_DRAWING;

                mTranslation = m.GetTranslation();

                mWinSurface = NS_STATIC_CAST(gfxWindowsSurface*, NS_STATIC_CAST(gfxASurface*, surf.get()));
            } else if (((mTransformType == AXIS_ALIGNED_SCALE)
                        && (mNativeDrawFlags & CAN_AXIS_ALIGNED_SCALE)) ||
                       (mNativeDrawFlags & CAN_COMPLEX_TRANSFORM))
            {
                mWorldTransform.eM11 = (FLOAT) m.xx;
                mWorldTransform.eM12 = (FLOAT) m.yx;
                mWorldTransform.eM21 = (FLOAT) m.xy;
                mWorldTransform.eM22 = (FLOAT) m.yy;
                mWorldTransform.eDx  = (FLOAT) m.x0;
                mWorldTransform.eDy  = (FLOAT) m.y0;

                mRenderState = RENDER_STATE_NATIVE_DRAWING;
                mWinSurface = NS_STATIC_CAST(gfxWindowsSurface*, NS_STATIC_CAST(gfxASurface*, surf.get()));
            }
        }

        // If we couldn't do native drawing, then we have to do two-buffer drawing
        // and do alpha recovery
        if (mRenderState == RENDER_STATE_INIT) {
            mRenderState = RENDER_STATE_ALPHA_RECOVERY_BLACK;

            // we only do the scale bit if we can do an axis aligned
            // scale; otherwise we scale (if necessary) after
            // rendering with cairo.  Note that if we're doing alpha recovery,
            // we cannot do a full complex transform with win32 (I mean, we could, but
            // it would require more code that's not here.)
            if (mTransformType == TRANSLATION_ONLY || !(mNativeDrawFlags & CAN_AXIS_ALIGNED_SCALE)) {
                mScale = gfxSize(1.0, 1.0);

                mTempSurfaceSize.width = (PRInt32) NS_ceil(mNativeRect.size.width);
                mTempSurfaceSize.height = (PRInt32) NS_ceil(mNativeRect.size.height);
            } else {
                // figure out the scale factors
                mScale = m.ScaleFactors(PR_TRUE);

                mWorldTransform.eM11 = (FLOAT) mScale.width;
                mWorldTransform.eM12 = 0.0f;
                mWorldTransform.eM21 = 0.0f;
                mWorldTransform.eM22 = (FLOAT) mScale.height;
                mWorldTransform.eDx  = 0.0f;
                mWorldTransform.eDy  = 0.0f;

                mTempSurfaceSize.width = (PRInt32) NS_ceil(mNativeRect.size.width * mScale.width);
                mTempSurfaceSize.height = (PRInt32) NS_ceil(mNativeRect.size.height * mScale.height);
            }
        }
    }

    if (mRenderState == RENDER_STATE_NATIVE_DRAWING) {
        // we can just do native drawing directly to the context's surface

        // Need to force the clip to be set
        mContext->UpdateSurfaceClip();

        // grab the DC
        mDC = mWinSurface->GetDC();

        // do we need to use SetWorldTransform?
        if (mTransformType != TRANSLATION_ONLY) {
            SetGraphicsMode(mDC, GM_ADVANCED);
            GetWorldTransform(mDC, &mOldWorldTransform);
            SetWorldTransform(mDC, &mWorldTransform);
        }

        GetViewportOrgEx(mDC, &mOrigViewportOrigin);
        SetViewportOrgEx(mDC,
                         mOrigViewportOrigin.x + (int)mDeviceOffset.x,
                         mOrigViewportOrigin.y + (int)mDeviceOffset.y,
                         NULL);

        return mDC;
    } else if (mRenderState == RENDER_STATE_ALPHA_RECOVERY_BLACK ||
               mRenderState == RENDER_STATE_ALPHA_RECOVERY_WHITE)
    {
        // we're going to use mWinSurface to create our temporary surface here

        // get us a RGB24 DIB; DIB is important, because
        // we can later call GetImageSurface on it.
        mWinSurface = new gfxWindowsSurface(mTempSurfaceSize);
        mDC = mWinSurface->GetDC();

        RECT r = { 0, 0, mTempSurfaceSize.width, mTempSurfaceSize.height };
        if (mRenderState == RENDER_STATE_ALPHA_RECOVERY_BLACK)
            FillRect(mDC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        else
            FillRect(mDC, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));

        if ((mTransformType != TRANSLATION_ONLY) &&
            (mNativeDrawFlags & CAN_AXIS_ALIGNED_SCALE))
        {
            SetGraphicsMode(mDC, GM_ADVANCED);
            SetWorldTransform(mDC, &mWorldTransform);
        }

        return mDC;
    } else {
        NS_ERROR("Bogus render state!");
        return nsnull;
    }
}

PRBool
gfxWindowsNativeDrawing::ShouldRenderAgain()
{
    switch (mRenderState) {
        case RENDER_STATE_NATIVE_DRAWING_DONE:
            return PR_FALSE;

        case RENDER_STATE_ALPHA_RECOVERY_BLACK_DONE:
            mRenderState = RENDER_STATE_ALPHA_RECOVERY_WHITE;
            return PR_TRUE;

        case RENDER_STATE_ALPHA_RECOVERY_WHITE_DONE:
            return PR_FALSE;

        default:
            NS_ERROR("Invalid RenderState in gfxWindowsNativeDrawing::ShouldRenderAgain");
            break;
    }

    return PR_FALSE;
}

void
gfxWindowsNativeDrawing::EndNativeDrawing()
{
    if (mRenderState == RENDER_STATE_NATIVE_DRAWING) {
        // we drew directly to the HDC in the context; undo our changes
        SetViewportOrgEx(mDC, mOrigViewportOrigin.x, mOrigViewportOrigin.y, NULL);

        if (mTransformType != TRANSLATION_ONLY)
            SetWorldTransform(mDC, &mOldWorldTransform);

        mWinSurface->MarkDirty();

        mRenderState = RENDER_STATE_NATIVE_DRAWING_DONE;
    } else if (mRenderState == RENDER_STATE_ALPHA_RECOVERY_BLACK) {
        mBlackSurface = mWinSurface;
        mWinSurface = nsnull;

        mRenderState = RENDER_STATE_ALPHA_RECOVERY_BLACK_DONE;
    } else if (mRenderState == RENDER_STATE_ALPHA_RECOVERY_WHITE) {
        mWhiteSurface = mWinSurface;
        mWinSurface = nsnull;

        mRenderState = RENDER_STATE_ALPHA_RECOVERY_WHITE_DONE;
    } else {
        NS_ERROR("Invalid RenderState in gfxWindowsNativeDrawing::EndNativeDrawing");
    }
}

void
gfxWindowsNativeDrawing::PaintToContext()
{
    if (mRenderState == RENDER_STATE_NATIVE_DRAWING_DONE) {
        // nothing to do, it already went to the context
        mRenderState = RENDER_STATE_DONE;
    } else if (mRenderState == RENDER_STATE_ALPHA_RECOVERY_WHITE_DONE) {
        nsRefPtr<gfxImageSurface> black = mBlackSurface->GetImageSurface();
        nsRefPtr<gfxImageSurface> white = mWhiteSurface->GetImageSurface();
        nsRefPtr<gfxImageSurface> alphaSurface =
            gfxAlphaRecovery::RecoverAlpha(black, white, mTempSurfaceSize);

        mContext->Save();
        mContext->MoveTo(mNativeRect.pos);
        mContext->NewPath();
        mContext->Rectangle(gfxRect(gfxPoint(0.0, 0.0), mNativeRect.size));

        nsRefPtr<gfxPattern> pat = new gfxPattern(alphaSurface);

        gfxMatrix m;
        m.Scale(mScale.width, mScale.height);
        pat->SetMatrix(m);

        if (mNativeDrawFlags & DO_NEAREST_NEIGHBOR_FILTERING)
            pat->SetFilter(0);

        mContext->SetPattern(pat);
        mContext->Fill();
        mContext->Restore();

        mRenderState = RENDER_STATE_DONE;
    } else {
        NS_ERROR("Invalid RenderState in gfxWindowsNativeDrawing::PaintToContext");
    }
}

void
gfxWindowsNativeDrawing::TransformToNativeRect(const gfxRect& r,
                                               RECT& rout)
{
    /* If we're doing native drawing, then we're still in the coordinate space
     * of the context; otherwise, we're in our own little world,
     * relative to the passed-in nativeRect.
     */
    if (mRenderState == RENDER_STATE_NATIVE_DRAWING) {
        if (mTransformType == TRANSLATION_ONLY) {
            rout.left = (LONG) (r.pos.x + NS_round(mTranslation.x));
            rout.right = (LONG) (rout.left + r.size.width);
            rout.top = (LONG) (r.pos.y + NS_round(mTranslation.y));
            rout.bottom = (LONG) (rout.top + r.size.height);
        } else {
            rout.left = (LONG) r.pos.x;
            rout.right = (LONG) (r.pos.x + r.size.width);
            rout.top = (LONG) r.pos.y;
            rout.bottom = (LONG) (r.pos.y + r.size.height);
        }
    } else {
        rout.left = (LONG) (r.pos.x - NS_round(mNativeRect.pos.x));
        rout.right = (LONG) (rout.left + r.size.width);
        rout.top = (LONG) (r.pos.y - NS_round(mNativeRect.pos.y));
        rout.bottom = (LONG) (rout.top + r.size.height);
    }
}


