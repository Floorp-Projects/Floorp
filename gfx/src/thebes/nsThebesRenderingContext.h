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

#ifndef NSTHEBESRENDERINGCONTEXT__H__
#define NSTHEBESRENDERINGCONTEXT__H__

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIRenderingContext.h"
#include "nsRenderingContextImpl.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIWidget.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsColor.h"
#include "nsRect.h"
#include "nsIRegion.h"
#include "nsTransform2D.h"
#include "nsVoidArray.h"
#include "nsIThebesFontMetrics.h"
#include "nsIThebesRenderingContext.h"
#include "gfxContext.h"

class nsIImage;

class nsThebesDrawingSurface;

class nsThebesRenderingContext : public nsIThebesRenderingContext,
                                 public nsRenderingContextImpl
{
public:
    nsThebesRenderingContext();
    virtual ~nsThebesRenderingContext();

    NS_DECL_ISUPPORTS

    NS_IMETHOD Init(nsIDeviceContext* aContext, gfxASurface* aThebesSurface);
    NS_IMETHOD Init(nsIDeviceContext* aContext, gfxContext* aThebesContext);

    NS_IMETHOD Init(nsIDeviceContext* aContext, nsIWidget *aWidget);
    NS_IMETHOD Init(nsIDeviceContext* aContext, nsIDrawingSurface *aSurface);
    NS_IMETHOD CommonInit(void);
    NS_IMETHOD Reset(void);
    NS_IMETHOD GetDeviceContext(nsIDeviceContext *& aDeviceContext);
    NS_IMETHOD LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                  PRUint32 aWidth, PRUint32 aHeight,
                                  void **aBits, PRInt32 *aStride,
                                  PRInt32 *aWidthBytes,
                                  PRUint32 aFlags);
    NS_IMETHOD UnlockDrawingSurface(void);
    NS_IMETHOD SelectOffScreenDrawingSurface(nsIDrawingSurface *aSurface);
    NS_IMETHOD GetDrawingSurface(nsIDrawingSurface **aSurface);
    NS_IMETHOD GetHints(PRUint32& aResult);
    NS_IMETHOD DrawNativeWidgetPixmap(void* aSrcSurfaceBlack,
                                      void* aSrcSurfaceWhite,
                                      const nsIntSize& aSrcSize, 
                                      const nsPoint& aDestPos);
    NS_IMETHOD PushState(void);
    NS_IMETHOD PopState(void);
    NS_IMETHOD IsVisibleRect(const nsRect& aRect, PRBool &aIsVisible);
    NS_IMETHOD SetClipRect(const nsRect& aRect, nsClipCombine aCombine);
    NS_IMETHOD GetClipRect(nsRect &aRect, PRBool &aHasLocalClip);
    NS_IMETHOD SetLineStyle(nsLineStyle aLineStyle);
    NS_IMETHOD GetLineStyle(nsLineStyle &aLineStyle);
    NS_IMETHOD GetPenMode(nsPenMode &aPenMode);
    NS_IMETHOD SetPenMode(nsPenMode aPenMode);
    NS_IMETHOD SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine);
    NS_IMETHOD CopyClipRegion(nsIRegion &aRegion);
    NS_IMETHOD GetClipRegion(nsIRegion **aRegion);
    NS_IMETHOD SetColor(nscolor aColor);
    NS_IMETHOD GetColor(nscolor &aColor) const;
    NS_IMETHOD SetFont(const nsFont& aFont, nsIAtom* aLangGroup);
    NS_IMETHOD SetFont(nsIFontMetrics *aFontMetrics);
    NS_IMETHOD GetFontMetrics(nsIFontMetrics *&aFontMetrics);
    NS_IMETHOD Translate(nscoord aX, nscoord aY);
    NS_IMETHOD Scale(float aSx, float aSy);
    NS_IMETHOD GetCurrentTransform(nsTransform2D *&aTransform);
    NS_IMETHOD CreateDrawingSurface(const nsRect &aBounds, PRUint32 aSurfFlags, nsIDrawingSurface* &aSurface);
    NS_IMETHOD CreateDrawingSurface(nsNativeWidget aWidget, nsIDrawingSurface* &aSurface);
    NS_IMETHOD DestroyDrawingSurface(nsIDrawingSurface *aDS);

    NS_IMETHOD DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
    NS_IMETHOD DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints);
    NS_IMETHOD DrawRect(const nsRect& aRect);
    NS_IMETHOD DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    NS_IMETHOD FillRect(const nsRect& aRect);
    NS_IMETHOD FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    NS_IMETHOD InvertRect(const nsRect& aRect);
    NS_IMETHOD InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    NS_IMETHOD FlushRect(const nsRect& aRect);
    NS_IMETHOD FlushRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    NS_IMETHOD DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
    NS_IMETHOD FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
    NS_IMETHOD DrawEllipse(const nsRect& aRect);
    NS_IMETHOD DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    NS_IMETHOD FillEllipse(const nsRect& aRect);
    NS_IMETHOD FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    NS_IMETHOD DrawArc(const nsRect& aRect,
                       float aStartAngle, float aEndAngle);
    NS_IMETHOD DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                       float aStartAngle, float aEndAngle);
    NS_IMETHOD FillArc(const nsRect& aRect,
                       float aStartAngle, float aEndAngle);
    NS_IMETHOD FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                       float aStartAngle, float aEndAngle);

    NS_IMETHOD GetWidth(const nsString& aString, nscoord &aWidth,
                        PRInt32 *aFontID = nsnull)
    { return nsRenderingContextImpl::GetWidth(aString, aWidth, aFontID); }
    NS_IMETHOD GetWidth(const char* aString, nscoord& aWidth)
    { return nsRenderingContextImpl::GetWidth(aString, aWidth); }
    NS_IMETHOD GetWidth(const char* aString, PRUint32 aLength,
                        nscoord& aWidth)
    { return nsRenderingContextImpl::GetWidth(aString, aLength, aWidth); }
    NS_IMETHOD GetWidth(const PRUnichar *aString, PRUint32 aLength,
                        nscoord &aWidth, PRInt32 *aFontID = nsnull)
    { return nsRenderingContextImpl::GetWidth(aString, aLength, aWidth, aFontID); }
    NS_IMETHOD DrawString(const nsString& aString, nscoord aX, nscoord aY,
                          PRInt32 aFontID = -1,
                          const nscoord* aSpacing = nsnull)
    { return nsRenderingContextImpl::DrawString(aString, aX, aY, aFontID, aSpacing); }
  
    NS_IMETHOD GetWidth(char aC, nscoord &aWidth);
    NS_IMETHOD GetWidth(PRUnichar aC, nscoord &aWidth,
                        PRInt32 *aFontID);
    
    NS_IMETHOD GetWidthInternal(const char *aString, PRUint32 aLength, nscoord &aWidth);
    NS_IMETHOD GetWidthInternal(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth,
                                PRInt32 *aFontID);
  
    NS_IMETHOD DrawStringInternal(const char *aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  const nscoord* aSpacing);
    NS_IMETHOD DrawStringInternal(const PRUnichar *aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing);
  
    NS_IMETHOD GetTextDimensionsInternal(const char* aString, PRUint32 aLength,
                                         nsTextDimensions& aDimensions);
    NS_IMETHOD GetTextDimensionsInternal(const PRUnichar *aString, PRUint32 aLength,
                                         nsTextDimensions& aDimensions,PRInt32 *aFontID);
    NS_IMETHOD GetTextDimensionsInternal(const char*       aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID);
    NS_IMETHOD GetTextDimensionsInternal(const PRUnichar*  aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID);

#ifdef MOZ_MATHML
    /**
     * Returns metrics (in app units) of an 8-bit character string
     */
    NS_IMETHOD GetBoundingMetricsInternal(const char*        aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics);
    
    /**
     * Returns metrics (in app units) of a Unicode character string
     */
    NS_IMETHOD GetBoundingMetricsInternal(const PRUnichar*   aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics,
                                          PRInt32*           aFontID = nsnull);

#endif /* MOZ_MATHML */

    virtual PRInt32 GetMaxStringLength();

    NS_IMETHOD PushFilter(const nsRect& aRect, PRBool aAreaIsOpaque, float aOpacity);
    NS_IMETHOD PopFilter();

    NS_IMETHOD CopyOffScreenBits(nsIDrawingSurface *aSrcSurf,
                                 PRInt32 aSrcX, PRInt32 aSrcY,
                                 const nsRect &aDestBounds,
                                 PRUint32 aCopyFlags);
    virtual void* GetNativeGraphicData(GraphicDataType aType);
    NS_IMETHOD GetBackbuffer(const nsRect &aRequestedSize,
                             const nsRect &aMaxSize,
                             PRBool aForBlending,
                             nsIDrawingSurface* &aBackbuffer);
    NS_IMETHOD ReleaseBackbuffer(void);
    NS_IMETHOD DestroyCachedBackbuffer(void);
    NS_IMETHOD UseBackbuffer(PRBool* aUseBackbuffer);

    NS_IMETHOD PushTranslation(PushedTranslation* aState);
    NS_IMETHOD PopTranslation(PushedTranslation* aState);
    NS_IMETHOD SetTranslation(nscoord aX, nscoord aY);

    NS_IMETHOD DrawImage(imgIContainer *aImage,
                         const nsRect &aSrcRect,
                         const nsRect &aDestRect);
    NS_IMETHOD DrawTile(imgIContainer *aImage, nscoord aXOffset, nscoord aYOffset,
                        const nsRect * aTargetRect);
    NS_IMETHOD SetRightToLeftText(PRBool aIsRTL);
    NS_IMETHOD GetRightToLeftText(PRBool* aIsRTL);
    virtual void SetTextRunRTL(PRBool aIsRTL);

    NS_IMETHOD GetClusterInfo(const PRUnichar *aText,
                              PRUint32 aLength,
                              PRUint8 *aClusterStarts);
    virtual PRInt32 GetPosition(const PRUnichar *aText,
                                PRUint32 aLength,
                                nsPoint aPt);
    NS_IMETHOD GetRangeWidth(const PRUnichar *aText,
                             PRUint32 aLength,
                             PRUint32 aStart,
                             PRUint32 aEnd,
                             PRUint32 &aWidth);
    NS_IMETHOD GetRangeWidth(const char *aText,
                             PRUint32 aLength,
                             PRUint32 aStart,
                             PRUint32 aEnd,
                             PRUint32 &aWidth);

    NS_IMETHOD RenderEPS(const nsRect& aRect, FILE *aDataFile);

    // Thebes specific stuff

    gfxContext *Thebes() { return mThebes; }

    nsTransform2D& CurrentTransform();

    void TransformCoord (nscoord *aX, nscoord *aY);

protected:
    nsCOMPtr<nsIDeviceContext> mDeviceContext;
    // cached pixels2twips, twips2pixels values
    double mP2A;

    nsCOMPtr<nsIWidget> mWidget;

    // we need to manage our own clip region (since we can't get at
    // the old one from cairo)
    nsCOMPtr<nsIThebesFontMetrics> mFontMetrics;

    nsLineStyle mLineStyle;
    nscolor mColor;

    // this is always the local surface
    nsCOMPtr<nsThebesDrawingSurface> mLocalDrawingSurface;

    // this is the current drawing surface; might be same
    // as local, or might be offscreen
    nsCOMPtr<nsThebesDrawingSurface> mDrawingSurface;

    // the rendering context
    nsRefPtr<gfxContext> mThebes;

    // for handing out to people
    void UpdateTempTransformMatrix();
    nsTransform2D mTempTransform;

    // keeping track of pushgroup/popgroup opacities
    nsTArray<float> mOpacityArray;
};

#endif  // NSCAIRORENDERINGCONTEXT__H__
