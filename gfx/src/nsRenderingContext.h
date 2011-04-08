/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef NSRENDERINGCONTEXT__H__
#define NSRENDERINGCONTEXT__H__

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIWidget.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsColor.h"
#include "nsRect.h"
#include "nsIRegion.h"
#include "nsTransform2D.h"
#include "nsIThebesFontMetrics.h"
#include "gfxContext.h"

typedef enum {
    nsClipCombine_kIntersect = 0,
    nsClipCombine_kUnion = 1,
    nsClipCombine_kSubtract = 2,
    nsClipCombine_kReplace = 3
} nsClipCombine;

typedef enum {
    nsLineStyle_kNone   = 0,
    nsLineStyle_kSolid  = 1,
    nsLineStyle_kDashed = 2,
    nsLineStyle_kDotted = 3
} nsLineStyle;

/* Struct used to represent the overall extent of a string
 * whose rendering may involve switching between different
 * fonts that have different metrics.
 */
struct nsTextDimensions {
    // max ascent amongst all the fonts needed to represent the string
    nscoord ascent;

    // max descent amongst all the fonts needed to represent the string
    nscoord descent;

    // width of the string
    nscoord width;


    nsTextDimensions()
    {
        Clear();
    }

    /* Set all member data to zero */
    void
    Clear() {
        ascent = descent = width = 0;
    }

    /* Sum with another dimension */
    void
    Combine(const nsTextDimensions& aOther) {
        if (ascent < aOther.ascent) ascent = aOther.ascent;
        if (descent < aOther.descent) descent = aOther.descent;
        width += aOther.width;
    }
};

#ifdef MOZ_MATHML
/* Struct used for accurate measurements of a string in order
 * to allow precise positioning when processing MathML.
 */
struct nsBoundingMetrics {

    ///////////
    // Metrics that _exactly_ enclose the text:

    // The character coordinate system is the one used on X Windows:
    // 1. The origin is located at the intersection of the baseline
    //    with the left of the character's cell.
    // 2. All horizontal bearings are oriented from left to right.
    // 2. All horizontal bearings are oriented from left to right.
    // 3. The ascent is oriented from bottom to top (being 0 at the orgin).
    // 4. The descent is oriented from top to bottom (being 0 at the origin).

    // Note that Win32/Mac/PostScript use a different convention for
    // the descent (all vertical measurements are oriented from bottom
    // to top on these palatforms). Make sure to flip the sign of the
    // descent on these platforms for cross-platform compatibility.

    // Any of the following member variables listed here can have
    // positive or negative value.

    nscoord leftBearing;
    /* The horizontal distance from the origin of the drawing
       operation to the left-most part of the drawn string. */

    nscoord rightBearing;
    /* The horizontal distance from the origin of the drawing
       operation to the right-most part of the drawn string.
       The _exact_ width of the string is therefore:
       rightBearing - leftBearing */

    nscoord ascent;
    /* The vertical distance from the origin of the drawing
       operation to the top-most part of the drawn string. */

    nscoord descent;
    /* The vertical distance from the origin of the drawing
       operation to the bottom-most part of the drawn string.
       The _exact_ height of the string is therefore:
       ascent + descent */

    //////////
    // Metrics for placing other surrounding text:

    nscoord width;
    /* The horizontal distance from the origin of the drawing
       operation to the correct origin for drawing another string
       to follow the current one. Depending on the font, this
       could be greater than or less than the right bearing. */

    nsBoundingMetrics() {
        Clear();
    }

    //////////
    // Utility methods and operators:

    /* Set all member data to zero */
    void
    Clear() {
        leftBearing = rightBearing = 0;
        ascent = descent = width = 0;
    }

    /* Append another bounding metrics */
    void
    operator += (const nsBoundingMetrics& bm) {
        if (ascent + descent == 0 && rightBearing - leftBearing == 0) {
            ascent = bm.ascent;
            descent = bm.descent;
            leftBearing = width + bm.leftBearing;
            rightBearing = width + bm.rightBearing;
        }
        else {
            if (ascent < bm.ascent) ascent = bm.ascent;
            if (descent < bm.descent) descent = bm.descent;
            leftBearing = PR_MIN(leftBearing, width + bm.leftBearing);
            rightBearing = PR_MAX(rightBearing, width + bm.rightBearing);
        }
        width += bm.width;
    }
};
#endif // MOZ_MATHML


class nsRenderingContext
{
public:
    nsRenderingContext()
        : mP2A(0.), mLineStyle(nsLineStyle_kNone), mColor(NS_RGB(0,0,0))
    {}
    // ~nsRenderingContext() {}

    NS_INLINE_DECL_REFCOUNTING(nsRenderingContext)

    /**
     * Return the maximum length of a string that can be handled by
     * the platform using the current font metrics.
     */
    PRInt32 GetMaxStringLength();

    // Safe string method variants: by default, these defer to the more
    // elaborate methods below
    nsresult GetWidth(const nsString& aString, nscoord &aWidth,
                      PRInt32 *aFontID = nsnull);
    nsresult GetWidth(const char* aString, nscoord& aWidth);
    nsresult DrawString(const nsString& aString, nscoord aX, nscoord aY,
                        PRInt32 aFontID = -1,
                        const nscoord* aSpacing = nsnull);

    // Safe string methods
    nsresult GetWidth(const char* aString, PRUint32 aLength,
                      nscoord& aWidth);
    nsresult GetWidth(const PRUnichar *aString, PRUint32 aLength,
                      nscoord &aWidth, PRInt32 *aFontID = nsnull);
    nsresult GetWidth(char aC, nscoord &aWidth);
    nsresult GetWidth(PRUnichar aC, nscoord &aWidth,
                      PRInt32 *aFontID);

    nsresult GetTextDimensions(const char* aString, PRUint32 aLength,
                               nsTextDimensions& aDimensions);
    nsresult GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                               nsTextDimensions& aDimensions,
                               PRInt32* aFontID = nsnull);

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11)
    nsresult GetTextDimensions(const char*       aString,
                               PRInt32           aLength,
                               PRInt32           aAvailWidth,
                               PRInt32*          aBreaks,
                               PRInt32           aNumBreaks,
                               nsTextDimensions& aDimensions,
                               PRInt32&          aNumCharsFit,
                               nsTextDimensions& aLastWordDimensions,
                               PRInt32*          aFontID = nsnull);

    nsresult GetTextDimensions(const PRUnichar*  aString,
                               PRInt32           aLength,
                               PRInt32           aAvailWidth,
                               PRInt32*          aBreaks,
                               PRInt32           aNumBreaks,
                               nsTextDimensions& aDimensions,
                               PRInt32&          aNumCharsFit,
                               nsTextDimensions& aLastWordDimensions,
                               PRInt32*          aFontID = nsnull);
#endif
#ifdef MOZ_MATHML
    nsresult GetBoundingMetrics(const char*        aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics);
    nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics,
                                PRInt32*           aFontID = nsnull);
#endif
    nsresult DrawString(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        const nscoord* aSpacing = nsnull);
    nsresult DrawString(const PRUnichar *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        PRInt32 aFontID = -1,
                        const nscoord* aSpacing = nsnull);

    nsresult Init(nsIDeviceContext* aContext, gfxASurface* aThebesSurface);
    nsresult Init(nsIDeviceContext* aContext, gfxContext* aThebesContext);
    nsresult Init(nsIDeviceContext* aContext, nsIWidget *aWidget);
    nsresult CommonInit(void);
    already_AddRefed<nsIDeviceContext> GetDeviceContext();
    nsresult PushState(void);
    nsresult PopState(void);
    nsresult SetClipRect(const nsRect& aRect, nsClipCombine aCombine);
    nsresult SetLineStyle(nsLineStyle aLineStyle);
    nsresult SetClipRegion(const nsIntRegion& aRegion, nsClipCombine aCombine);
    nsresult SetColor(nscolor aColor);
    nsresult GetColor(nscolor &aColor) const;
    nsresult SetFont(const nsFont& aFont, nsIAtom* aLanguage,
                     gfxUserFontSet *aUserFontSet);
    nsresult SetFont(const nsFont& aFont,
                     gfxUserFontSet *aUserFontSet);
    nsresult SetFont(nsIFontMetrics *aFontMetrics);
    already_AddRefed<nsIFontMetrics> GetFontMetrics();
    nsresult Translate(const nsPoint& aPt);
    nsresult Scale(float aSx, float aSy);
    nsTransform2D* GetCurrentTransform();

    nsresult DrawLine(const nsPoint& aStartPt, const nsPoint& aEndPt);
    nsresult DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
    nsresult DrawRect(const nsRect& aRect);
    nsresult DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    nsresult FillRect(const nsRect& aRect);
    nsresult FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    nsresult InvertRect(const nsRect& aRect);
    nsresult InvertRect(nscoord aX, nscoord aY,
                        nscoord aWidth, nscoord aHeight);
    nsresult FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
    nsresult DrawEllipse(const nsRect& aRect);
    nsresult DrawEllipse(nscoord aX, nscoord aY,
                         nscoord aWidth, nscoord aHeight);
    nsresult FillEllipse(const nsRect& aRect);
    nsresult FillEllipse(nscoord aX, nscoord aY,
                         nscoord aWidth, nscoord aHeight);

    nsresult PushFilter(const nsRect& aRect,
                        PRBool aAreaIsOpaque, float aOpacity);
    nsresult PopFilter();

    enum GraphicDataType {
        NATIVE_CAIRO_CONTEXT = 1,
        NATIVE_GDK_DRAWABLE = 2,
        NATIVE_WINDOWS_DC = 3,
        NATIVE_MAC_THING = 4,
        NATIVE_THEBES_CONTEXT = 5,
        NATIVE_OS2_PS = 6
    };
    void* GetNativeGraphicData(GraphicDataType aType);

    struct PushedTranslation {
        float mSavedX, mSavedY;
    };
    class AutoPushTranslation {
        nsRenderingContext* mCtx;
        PushedTranslation mPushed;
    public:
        AutoPushTranslation(nsRenderingContext* aCtx, const nsPoint& aPt)
            : mCtx(aCtx) {
            mCtx->PushTranslation(&mPushed);
            mCtx->Translate(aPt);
        }
        ~AutoPushTranslation() {
            mCtx->PopTranslation(&mPushed);
        }
    };

    nsresult PushTranslation(PushedTranslation* aState);
    nsresult PopTranslation(PushedTranslation* aState);
    nsresult SetTranslation(const nsPoint& aPoint);

    /**
     * Let the device context know whether we want text reordered with
     * right-to-left base direction
     */
    nsresult SetRightToLeftText(PRBool aIsRTL);
    nsresult GetRightToLeftText(PRBool* aIsRTL);
    void SetTextRunRTL(PRBool aIsRTL);

    PRInt32 GetPosition(const PRUnichar *aText,
                        PRUint32 aLength,
                        nsPoint aPt);
    nsresult GetRangeWidth(const PRUnichar *aText,
                           PRUint32 aLength,
                           PRUint32 aStart,
                           PRUint32 aEnd,
                           PRUint32 &aWidth);
    nsresult GetRangeWidth(const char *aText,
                           PRUint32 aLength,
                           PRUint32 aStart,
                           PRUint32 aEnd,
                           PRUint32 &aWidth);

    nsresult RenderEPS(const nsRect& aRect, FILE *aDataFile);

    // Thebes specific stuff

    gfxContext *ThebesContext() { return mThebes; }

    nsTransform2D& CurrentTransform();

    void TransformCoord (nscoord *aX, nscoord *aY);

protected:
    nsresult GetWidthInternal(const char *aString, PRUint32 aLength,
                              nscoord &aWidth);
    nsresult GetWidthInternal(const PRUnichar *aString, PRUint32 aLength,
                              nscoord &aWidth, PRInt32 *aFontID = nsnull);

    nsresult DrawStringInternal(const char *aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                const nscoord* aSpacing = nsnull);
    nsresult DrawStringInternal(const PRUnichar *aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                PRInt32 aFontID = -1,
                                const nscoord* aSpacing = nsnull);

    nsresult GetTextDimensionsInternal(const char*       aString,
                                       PRUint32          aLength,
                                       nsTextDimensions& aDimensions);
    nsresult GetTextDimensionsInternal(const PRUnichar*  aString,
                                       PRUint32          aLength,
                                       nsTextDimensions& aDimensions,
                                       PRInt32*          aFontID = nsnull);
    nsresult GetTextDimensionsInternal(const char*       aString,
                                       PRInt32           aLength,
                                       PRInt32           aAvailWidth,
                                       PRInt32*          aBreaks,
                                       PRInt32           aNumBreaks,
                                       nsTextDimensions& aDimensions,
                                       PRInt32&          aNumCharsFit,
                                       nsTextDimensions& aLastWordDimensions,
                                       PRInt32*          aFontID = nsnull);
    nsresult GetTextDimensionsInternal(const PRUnichar*  aString,
                                       PRInt32           aLength,
                                       PRInt32           aAvailWidth,
                                       PRInt32*          aBreaks,
                                       PRInt32           aNumBreaks,
                                       nsTextDimensions& aDimensions,
                                       PRInt32&          aNumCharsFit,
                                       nsTextDimensions& aLastWordDimensions,
                                       PRInt32*          aFontID = nsnull);

    void UpdateTempTransformMatrix();

#ifdef MOZ_MATHML
    /**
     * Returns metrics (in app units) of an 8-bit character string
     */
    nsresult GetBoundingMetricsInternal(const char*        aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics);

    /**
     * Returns metrics (in app units) of a Unicode character string
     */
    nsresult GetBoundingMetricsInternal(const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics,
                                        PRInt32*           aFontID = nsnull);

#endif /* MOZ_MATHML */

    nsRefPtr<gfxContext> mThebes;
    nsCOMPtr<nsIDeviceContext> mDeviceContext;
    nsCOMPtr<nsIWidget> mWidget;
    nsCOMPtr<nsIThebesFontMetrics> mFontMetrics;

    double mP2A; // cached app units per device pixel value
    nsLineStyle mLineStyle;
    nscolor mColor;

    // for handing out to people
    nsTransform2D mTempTransform;

    // keeping track of pushgroup/popgroup opacities
    nsTArray<float> mOpacityArray;
};

#endif  // NSRENDERINGCONTEXT__H__
