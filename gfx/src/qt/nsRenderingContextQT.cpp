/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsRenderingContextQT.h"
#include "nsRegionQT.h"
#include "nsGfxCIID.h"
#include <math.h>

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

PRLogModuleInfo * QtGfxLM = PR_NewLogModule("QtGfx");

class GraphicsState
{
public:
    GraphicsState();
    ~GraphicsState();

    nsTransform2D  * mMatrix;
    nsRect           mLocalClip;
    nsRegionQT     * mClipRegion;
    nscolor          mColor;
    nsLineStyle      mLineStyle;
    nsIFontMetrics * mFontMetrics;
    QFont          * mFont;
};

GraphicsState::GraphicsState()
{
    mMatrix      = nsnull;
    mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
    mClipRegion  = nsnull;
    mColor       = NS_RGB(0, 0, 0);
    mLineStyle   = nsLineStyle_kSolid;
    mFontMetrics = nsnull;
    mFont        = nsnull;
}

GraphicsState::~GraphicsState()
{
}


nsRenderingContextQT::nsRenderingContextQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::nsRenderingContextQT\n"));
    NS_INIT_REFCNT();

    mFontMetrics        = nsnull;
    mContext            = nsnull;
    mSurface            = nsnull;
    mOffscreenSurface   = nsnull;
    mCurrentColor       = 0;
    mCurrentLineStyle   = nsLineStyle_kSolid;
    mCurrentFont        = nsnull;
    mCurrentFontMetrics = nsnull;
    mTMatrix            = nsnull;
    mP2T                = 1.0f;
    mStateCache         = new nsVoidArray();
    mRegion             = new nsRegionQT();
    mRegion->Init();

    PushState();
}

nsRenderingContextQT::~nsRenderingContextQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::~nsRenderingContextQT\n"));
    delete mRegion;

    mTMatrix = nsnull;

    // Destroy the State Machine
    if (nsnull != mStateCache)
    {
        PRInt32 cnt = mStateCache->Count();

        while (--cnt >= 0)
        {
            GraphicsState *state = (GraphicsState *)mStateCache->ElementAt(cnt);
            mStateCache->RemoveElementAt(cnt);

            if (nsnull != state)
            {
                delete state;
            }
        }

        delete mStateCache;
        mStateCache = nsnull;
    }

    // Destroy the front buffer and it's GC if one was allocated for it
    if (nsnull != mOffscreenSurface) 
    {
        delete mOffscreenSurface;
    }

    NS_IF_RELEASE(mFontMetrics);
    NS_IF_RELEASE(mContext);

    if (nsnull != mCurrentFontMetrics)
    {
        delete mCurrentFontMetrics;
    }
}

NS_IMPL_QUERY_INTERFACE(nsRenderingContextQT, kRenderingContextIID)
NS_IMPL_ADDREF(nsRenderingContextQT)
NS_IMPL_RELEASE(nsRenderingContextQT)

NS_IMETHODIMP nsRenderingContextQT::Init(nsIDeviceContext* aContext,
                                         nsIWidget *aWindow)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::Init\n"));
    mContext = aContext;
    NS_IF_ADDREF(mContext);

    mSurface = new nsDrawingSurfaceQT();

    QPixmap * pixmap = (QPixmap *) aWindow->GetNativeData(NS_NATIVE_WINDOW);
    QPainter * gc = (QPainter *) aWindow->GetNativeData(NS_NATIVE_GRAPHIC);

    mSurface->Init(pixmap, gc);

    mOffscreenSurface = mSurface;

    return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextQT::Init(nsIDeviceContext* aContext,
                                         nsDrawingSurface aSurface)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::Init\n"));
    mContext = aContext;
    NS_IF_ADDREF(mContext);

    mSurface = (nsDrawingSurfaceQT *) aSurface;

    return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextQT::CommonInit()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::CommonInit\n"));
    mContext->GetDevUnitsToAppUnits(mP2T);
    float app2dev;
    mContext->GetAppUnitsToDevUnits(app2dev);
    mTMatrix->AddScale(app2dev, app2dev);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetHints(PRUint32& aResult)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetHints\n"));
    PRUint32 result = 0;

    // Most X servers implement 8 bit text rendering alot faster than
    // XChar2b rendering. In addition, we can avoid the PRUnichar to
    // XChar2b conversion. So we set this bit...
    result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

    // XXX see if we are rendering to the local display or to a remote
    // dispaly and set the NS_RENDERING_HINT_REMOTE_RENDERING accordingly

    aResult = result;
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::LockDrawingSurface(PRInt32 aX, 
                                                       PRInt32 aY,
                                                       PRUint32 aWidth, 
                                                       PRUint32 aHeight,
                                                       void **aBits, 
                                                       PRInt32 *aStride,
                                                       PRInt32 *aWidthBytes, 
                                                       PRUint32 aFlags)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::LockDrawingSurface\n"));
#if 0
    PushState();
    return mSurface->Lock(aX, 
                          aY, 
                          aWidth, 
                          aHeight,
                          aBits, 
                          aStride, 
                          aWidthBytes, 
                          aFlags);
#else
    return NS_OK;
#endif
}

NS_IMETHODIMP nsRenderingContextQT::UnlockDrawingSurface(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::UnlockDrawingSurface\n"));
#if 0
    PRBool clipstate;
    PopState(clipstate);
    mSurface->Unlock();
#endif
    return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::SelectOffScreenDrawingSurface\n"));
    if (nsnull == aSurface)
    {
        mSurface = mOffscreenSurface;
    }
    else
    {
        mSurface = (nsDrawingSurfaceQT *) aSurface;
    }

    return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::GetDrawingSurface(nsDrawingSurface *aSurface)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetDrawingSurface\n"));
    *aSurface = mSurface;
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::Reset()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::Reset\n"));
    return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::GetDeviceContext(nsIDeviceContext *&aContext)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetDeviceContext\n"));
    NS_IF_ADDREF(mContext);
    aContext = mContext;
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::PushState(void)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::PushState\n"));
    GraphicsState * state = new GraphicsState();

    // Push into this state object, add to vector
    state->mMatrix = mTMatrix;

    mStateCache->AppendElement(state);

    if (nsnull == mTMatrix)
    {
        mTMatrix = new nsTransform2D();
    }
    else
    {
        mTMatrix = new nsTransform2D(mTMatrix);
    }

    PRBool clipState;
    GetClipRect(state->mLocalClip, clipState);

    state->mClipRegion = mRegion;

    if (nsnull != state->mClipRegion) 
    {
        mRegion = new nsRegionQT();
        mRegion->Init();
        mRegion->SetTo(state->mClipRegion);
    }

    state->mColor = mCurrentColor;
    state->mLineStyle = mCurrentLineStyle;

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::PopState(PRBool &aClipEmpty)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::PopState\n"));
    PRBool bEmpty = PR_FALSE;

    PRUint32 cnt = mStateCache->Count();
    GraphicsState * state;

    if (cnt > 0) 
    {
        state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
        mStateCache->RemoveElementAt(cnt - 1);

        // Assign all local attributes from the state object just popped
        if (mTMatrix)
        {
            delete mTMatrix;
        }
        mTMatrix = state->mMatrix;

        if (nsnull != mRegion)
        {
            delete mRegion;
        }

        mRegion = state->mClipRegion;

        if (nsnull != mRegion && mRegion->IsEmpty() == PR_TRUE) 
        {
            bEmpty = PR_TRUE;
        }
        else
        {
            // Select in the old region.  We probably want to set a dirty flag
            // and only do this IFF we need to draw before the next Pop.  We'd
            // need to check the state flag on every draw operation.
            if (nsnull != mRegion)
            {
                QRegion *rgn;
                mRegion->GetNativeRegion((void*&)rgn);

                mSurface->GetGC()->setClipRegion(*rgn);
                // can we destroy rgn now?
            }
        }

        if (state->mColor != mCurrentColor)
        {
            SetColor(state->mColor);
        }

        if (state->mLineStyle != mCurrentLineStyle)
        {
            SetLineStyle(state->mLineStyle);
        }


        // Delete this graphics state object
        delete state;
    }

    aClipEmpty = bEmpty;

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::IsVisibleRect(const nsRect& aRect,
                                                  PRBool &aVisible)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::IsVisibleRect\n"));
    aVisible = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetClipRect(nsRect &aRect, 
                                                PRBool &aClipValid)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetClipRect\n"));
    PRInt32 x, y, w, h;
    if (!mRegion->IsEmpty())
    {
        mRegion->GetBoundingBox(&x,&y,&w,&h);
        aRect.SetRect(x,y,w,h);
        aClipValid = PR_TRUE;
    } 
    else 
    {
        aRect.SetRect(0,0,0,0);
        aClipValid = PR_FALSE;
    }

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::SetClipRect(const nsRect& aRect,
                                                nsClipCombine aCombine,
                                                PRBool &aClipEmpty)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::SetClipRect\n"));
    nsRect trect = aRect;
    QRegion *rgn = nsnull;

    mTMatrix->TransformCoord(&trect.x, &trect.y,
                             &trect.width, &trect.height);

    switch(aCombine)
    {
    case nsClipCombine_kIntersect:
        mRegion->Intersect(trect.x,trect.y,trect.width,trect.height);
        break;
    case nsClipCombine_kUnion:
        mRegion->Union(trect.x,trect.y,trect.width,trect.height);
        break;
    case nsClipCombine_kSubtract:
        mRegion->Subtract(trect.x,trect.y,trect.width,trect.height);
        break;
    case nsClipCombine_kReplace:
        mRegion->SetTo(trect.x,trect.y,trect.width,trect.height);
        break;
    }

    aClipEmpty = mRegion->IsEmpty();

    mRegion->GetNativeRegion((void*&)rgn);
    mSurface->GetGC()->setClipRegion(*rgn);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::SetClipRegion(const nsIRegion& aRegion,
                                                  nsClipCombine aCombine,
                                                  PRBool &aClipEmpty)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::SetClipRegion\n"));
    QRegion *rgn;
 
    switch(aCombine)
    {
    case nsClipCombine_kIntersect:
        mRegion->Intersect(aRegion);
        break;
    case nsClipCombine_kUnion:
        mRegion->Union(aRegion);
        break;
    case nsClipCombine_kSubtract:
        mRegion->Subtract(aRegion);
        break;
    case nsClipCombine_kReplace:
        mRegion->SetTo(aRegion);
        break;
    }

    aClipEmpty = mRegion->IsEmpty();
    mRegion->GetNativeRegion((void*&)rgn);
    mSurface->GetGC()->setClipRegion(*rgn);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetClipRegion(nsIRegion **aRegion)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetClipRegion\n"));
    nsresult  rv = NS_OK;

    NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

    if (nsnull == *aRegion)
    {
        nsRegionQT *rgn = new nsRegionQT();

        if (nsnull != rgn)
        {
            NS_ADDREF(rgn);

            rv = rgn->Init();

            if (NS_OK == rv)
                *aRegion = rgn;
            else
                NS_RELEASE(rgn);
        }
        else
        {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    }

    if (rv == NS_OK)
    {
        (*aRegion)->SetTo(*mRegion);
    }

    return rv;
}

NS_IMETHODIMP nsRenderingContextQT::SetColor(nscolor aColor)
{
    if (nsnull == mContext)
    {
        return NS_ERROR_FAILURE;
    }
      
    mCurrentColor = aColor;

    QColor color(NS_GET_R(mCurrentColor),
                 NS_GET_G(mCurrentColor),
                 NS_GET_B(mCurrentColor));

    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsRenderingContextQT::SetColor: r=%d, g=%d, b=%d", 
            color.red(), 
            color.green(), 
            color.blue()));

    mSurface->GetGC()->setPen(color);
  
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetColor(nscolor &aColor) const
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetColor\n"));
    aColor = mCurrentColor;
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::SetFont(const nsFont& aFont)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::SetFont\n"));
    NS_IF_RELEASE(mFontMetrics);
    mContext->GetMetricsFor(aFont, mFontMetrics);
    return SetFont(mFontMetrics);
}

NS_IMETHODIMP nsRenderingContextQT::SetFont(nsIFontMetrics *aFontMetrics)
{
    NS_IF_RELEASE(mFontMetrics);
    mFontMetrics = aFontMetrics;
    NS_IF_ADDREF(mFontMetrics);

    if (mFontMetrics)
    {
        nsFontHandle  fontHandle;
        mFontMetrics->GetFontHandle(fontHandle);
        mCurrentFont = (QFont *)fontHandle;
        mCurrentFontMetrics = new QFontMetrics(*mCurrentFont);

    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsRenderingContextQT::SetFont to %s, %d pt\n",
            (const char *)mCurrentFont->family(),
            mCurrentFont->pointSize()));

        mSurface->GetGC()->setFont(*mCurrentFont);
    }

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::SetLineStyle(nsLineStyle aLineStyle)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::SetLineStyle\n"));
    if (aLineStyle != mCurrentLineStyle)
    {
        switch(aLineStyle)
        { 
        case nsLineStyle_kSolid:
            mSurface->GetGC()->setPen(QPen::SolidLine);
            break;
            
        case nsLineStyle_kDashed: 
            mSurface->GetGC()->setPen(QPen::DashLine);
            break;
        
        case nsLineStyle_kDotted: 
            mSurface->GetGC()->setPen(QPen::DotLine);
            break;

        default:
            break;
        }
    
        mCurrentLineStyle = aLineStyle ;
    }

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetLineStyle(nsLineStyle &aLineStyle)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetLineStyle\n"));
    aLineStyle = mCurrentLineStyle;
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQT::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetFontMetrics\n"));
    NS_IF_ADDREF(mFontMetrics);
    aFontMetrics = mFontMetrics;
    return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextQT::Translate(nscoord aX, nscoord aY)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::Translate\n"));
    mTMatrix->AddTranslation((float)aX,(float)aY);
    return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextQT::Scale(float aSx, float aSy)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::Scale\n"));
    mTMatrix->AddScale(aSx, aSy);
    return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::GetCurrentTransform(nsTransform2D *&aTransform)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetCurrentTransform\n"));
    aTransform = mTMatrix;
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQT::CreateDrawingSurface(nsRect *aBounds,
                                           PRUint32 aSurfFlags,
                                           nsDrawingSurface &aSurface)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::CreateDrawingSurface\n"));
    if (nsnull == mSurface) 
    {
        aSurface = nsnull;
        return NS_ERROR_FAILURE;
    }

    if ((aBounds == NULL) || (aBounds->width <= 0) || (aBounds->height) <= 0)
    {
        return NS_ERROR_FAILURE;
    }

    QPixmap * pixmap = new QPixmap(aBounds->width, aBounds->height/*, depth*/);
    QPainter * painter = new QPainter();

    nsDrawingSurfaceQT * surface = new nsDrawingSurfaceQT();

    surface->Init(pixmap, painter);

    aSurface = (nsDrawingSurface)surface;

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DestroyDrawingSurface(nsDrawingSurface aDS)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DestroyDrawingSurface\n"));
    nsDrawingSurfaceQT * surface = (nsDrawingSurfaceQT *) aDS;

    if ((surface == NULL) || (surface->GetPixmap() == NULL))
    {
        return NS_ERROR_FAILURE;
    }

    QPainter * painter = surface->GetGC();
    delete painter;
    QPixmap * pixmap = surface->GetPixmap();
    delete pixmap;

    NS_IF_RELEASE(surface);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawLine(nscoord aX0, 
                                             nscoord aY0, 
                                             nscoord aX1, 
                                             nscoord aY1)
{
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsRenderingContextQT::DrawLine: (%d,%d) to (%d,%d)\n",
            aX0, 
            aY0, 
            aX1, 
            aY1));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    mTMatrix->TransformCoord(&aX0,&aY0);
    mTMatrix->TransformCoord(&aX1,&aY1);

#if 0
    if (aY0 != aY1) 
    {
        aY1--;
    }
    if (aX0 != aX1) 
    {
        aX1--;
    }
#endif

    mSurface->GetGC()->drawLine(aX0, aY0, aX1, aY1);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawPolyline(const nsPoint aPoints[], 
                                                 PRInt32 aNumPoints)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawPolyline\n"));
    PRInt32 i;

    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    QPointArray * pts = new QPointArray(aNumPoints);
    for (i = 0; i < aNumPoints; i++)
    {
        nsPoint p = aPoints[i];
        mTMatrix->TransformCoord(&p.x,&p.y);
        pts->setPoint(i, p.x, p.y);
    }

    mSurface->GetGC()->drawPolyline(*pts);

    delete pts;

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawRect(const nsRect& aRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawRect\n"));
    return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawRect(nscoord aX, 
                                             nscoord aY, 
                                             nscoord aWidth, 
                                             nscoord aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawRect\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    nscoord x,y,w,h;

    x = aX;
    y = aY;
    w = aWidth;
    h = aHeight;

    mTMatrix->TransformCoord(&x, &y, &w, &h);

    if (w && h)
    {
        mSurface->GetGC()->drawRect(x, y, w, h);
    }

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillRect(const nsRect& aRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::FillRect\n"));
    return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::FillRect(nscoord aX, 
                                             nscoord aY, 
                                             nscoord aWidth, 
                                             nscoord aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::FillRect\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    nscoord x,y,w,h;

    x = aX;
    y = aY;
    w = aWidth;
    h = aHeight;

    mTMatrix->TransformCoord(&x,&y,&w,&h);

    QColor color (NS_GET_R(mCurrentColor),
                  NS_GET_G(mCurrentColor),
                  NS_GET_B(mCurrentColor));

    mSurface->GetGC()->fillRect(x, y, w, h, color);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::InvertRect(const nsRect& aRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::InvertRect\n"));
    return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::InvertRect(nscoord aX, 
                                               nscoord aY, 
                                               nscoord aWidth, 
                                               nscoord aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::InvertRect\n"));
    if (nsnull == mTMatrix || nsnull == mSurface) 
    {
        return NS_ERROR_FAILURE;
    }

    nscoord x,y,w,h;

    x = aX;
    y = aY;
    w = aWidth;
    h = aHeight;

    mTMatrix->TransformCoord(&x,&y,&w,&h);

    // Set XOR drawing mode
    mSurface->GetGC()->setRasterOp(Qt::XorROP);

    // Fill the rect
    QColor color (NS_GET_R(mCurrentColor),
                  NS_GET_G(mCurrentColor),
                  NS_GET_B(mCurrentColor));

    mSurface->GetGC()->fillRect(x, y, w, h, color);
    
    // Back to normal copy drawing mode
    mSurface->GetGC()->setRasterOp(Qt::CopyROP);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawPolygon(const nsPoint aPoints[], 
                                                PRInt32 aNumPoints)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawPolygon\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    QPointArray *pts = new QPointArray(aNumPoints);
    for (PRInt32 i = 0; i < aNumPoints; i++)
    {
        nsPoint p = aPoints[i];
        mTMatrix->TransformCoord(&p.x,&p.y);
        pts->setPoint(i, p.x, p.y);
    }

    // drawPolygon() fills in the polygon with the current foreground color,
    // so we have to set the foreground color temporarily to be the same as
    // the background color, before restoring it.

    //mRenderingSurface->gc->setBrush(mRenderingSurface->gc->backgroundColor());

    mSurface->GetGC()->drawPolygon(*pts);

    //mRenderingSurface->gc->setBrush(NS_GET_R(mCurrentColor),
    //                                NS_GET_G(mCurrentColor),
    //                                NS_GET_B(mCurrentColor));

    delete pts;

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillPolygon(const nsPoint aPoints[], 
                                                PRInt32 aNumPoints)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::FillPolygon\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    QPointArray *pts = new QPointArray(aNumPoints);
    for (PRInt32 i = 0; i < aNumPoints; i++)
    {
        nsPoint p = aPoints[i];
        mTMatrix->TransformCoord(&p.x,&p.y);
        pts->setPoint(i, p.x, p.y);
    }

    mSurface->GetGC()->drawPolygon(*pts);
        
    delete pts;

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawEllipse(const nsRect& aRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawEllipse\n"));
    return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawEllipse(nscoord aX, 
                                                nscoord aY, 
                                                nscoord aWidth, 
                                                nscoord aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawEllipse\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    nscoord x,y,w,h;

    x = aX;
    y = aY;
    w = aWidth;
    h = aHeight;

    mTMatrix->TransformCoord(&x,&y,&w,&h);

    mSurface->GetGC()->drawEllipse(x, y, w, h);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillEllipse(const nsRect& aRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::FillEllipse\n"));
    return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::FillEllipse(nscoord aX, 
                                                nscoord aY, 
                                                nscoord aWidth, 
                                                nscoord aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::FillEllipse\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    nscoord x,y,w,h;

    x = aX;
    y = aY;
    w = aWidth;
    h = aHeight;

    mTMatrix->TransformCoord(&x,&y,&w,&h);

    //mRenderingSurface->gc->drawEllipse(x, y, w, h);
    mSurface->GetGC()->drawChord(x, y, w, h, 0, 16 * 360);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawArc(const nsRect& aRect,
                                            float aStartAngle, 
                                            float aEndAngle)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawArc\n"));
    return DrawArc(aRect.x,
                   aRect.y,
                   aRect.width,
                   aRect.height,
                   aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextQT::DrawArc(nscoord aX, 
                                            nscoord aY,
                                            nscoord aWidth, 
                                            nscoord aHeight,
                                            float aStartAngle, 
                                            float aEndAngle)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawArc\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    nscoord x,y,w,h;

    x = aX;
    y = aY;
    w = aWidth;
    h = aHeight;

    mTMatrix->TransformCoord(&x,&y,&w,&h);

    mSurface->GetGC()->drawArc(x, 
                               y, 
                               w, 
                               h, 
                               aStartAngle, 
                               aEndAngle - aStartAngle);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillArc(const nsRect& aRect,
                                            float aStartAngle, 
                                            float aEndAngle)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::FillArc\n"));
    return FillArc(aRect.x,
                   aRect.y,
                   aRect.width,
                   aRect.height,
                   aStartAngle,aEndAngle);
}


NS_IMETHODIMP nsRenderingContextQT::FillArc(nscoord aX, 
                                            nscoord aY,
                                            nscoord aWidth, 
                                            nscoord aHeight,
                                            float aStartAngle, 
                                            float aEndAngle)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::FillArc\n"));
    if (nsnull == mTMatrix || 
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC()) 
    {
        return NS_ERROR_FAILURE;
    }

    nscoord x,y,w,h;

    x = aX;
    y = aY;
    w = aWidth;
    h = aHeight;

    mTMatrix->TransformCoord(&x,&y,&w,&h);

    mSurface->GetGC()->drawPie(x, 
                               y, 
                               w, 
                               h, 
                               aStartAngle, 
                               aEndAngle - aStartAngle);    

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(char aC, nscoord &aWidth)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetWidth\n"));
    int rawWidth = mCurrentFontMetrics->width(aC);
    aWidth = NSToCoordRound(rawWidth * mP2T);
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(PRUnichar aC, 
                                             nscoord& aWidth,
                                             PRInt32* aFontID)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetWidth\n"));
    int rawWidth = mCurrentFontMetrics->width((QChar) aC); 
    aWidth = NSToCoordRound(rawWidth * mP2T);
    if (nsnull != aFontID)
    {
        *aFontID = 0;
    }
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const nsString& aString,
                                             nscoord& aWidth, 
                                             PRInt32* aFontID)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetWidth\n"));
    return GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const char* aString, 
                                             nscoord& aWidth)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetWidth of \"%s\"\n",
                                   aString));
    return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const char* aString, 
                                             PRUint32 aLength,
                                             nscoord& aWidth)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetWidth of \"%s\"\n",
                                   aString));
    if (0 == aLength) 
    {
        aWidth = 0;
    }
    else if (nsnull == aString)
    {
        return NS_ERROR_FAILURE;
    }
    else
    {
        int rawWidth = mCurrentFontMetrics->width(aString, aLength);
        aWidth = NSToCoordRound(rawWidth * mP2T);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const PRUnichar* aString, 
                                             PRUint32 aLength,
                                             nscoord& aWidth, 
                                             PRInt32* aFontID)
{
    if (0 == aLength) 
    {
        aWidth = 0;
    }
    else if (nsnull == aString)
    {
        return NS_ERROR_FAILURE;
    }
    else 
    {
        QChar uChars[aLength];

        for (PRUint32 i = 0; i < aLength; i++)
        {
            uChars[i] = (QChar) aString[i];
        }
        QString string(uChars, aLength);
        PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::GetWidth of \"%s\"\n",
                                       string.ascii()));
        int rawWidth = mCurrentFontMetrics->width(string);
        aWidth = NSToCoordRound(rawWidth * mP2T);
    }
    if (nsnull != aFontID)
    {
        *aFontID = 0;
    }

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawString(const char *aString, 
                                               PRUint32 aLength,
                                               nscoord aX, 
                                               nscoord aY,
                                               const nscoord* aSpacing)
{
    PR_LOG(QtGfxLM, 
           PR_LOG_DEBUG, 
           ("nsRenderingContextQT::DrawString: drawing \"%s\" with length %d\n",
            aString,
            aLength));
    if (0 != aLength) 
    {
        if (nsnull == mTMatrix || 
            nsnull == mSurface ||
            nsnull == mSurface->GetPixmap() ||
            nsnull == mSurface->GetGC() ||
            nsnull == aString)
        {
            return NS_ERROR_FAILURE;
        }

        nscoord x = aX;
        nscoord y = aY;

        // Substract xFontStruct ascent since drawing specifies baseline
        if (mFontMetrics) 
        {
            mFontMetrics->GetMaxAscent(y);
            y += aY;
        }

        if (nsnull != aSpacing) 
        {
            // Render the string, one character at a time...
            const char* end = aString + aLength;
            while (aString < end) 
            {
                char ch = *aString++;
                nscoord xx = x;
                nscoord yy = y;
                mTMatrix->TransformCoord(&xx, &yy);
                QString str = (QChar) ch;
                mSurface->GetGC()->drawText(xx, yy, str, 1);
                //mSurface->GetGC()->drawText(xx, yy, ch, 1);
                x += *aSpacing++;
            }
        }
        else 
        {
            mTMatrix->TransformCoord(&x, &y);
            mSurface->GetGC()->drawText(x, y, aString, aLength);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawString(const PRUnichar* aString, 
                                               PRUint32 aLength,
                                               nscoord aX, 
                                               nscoord aY,
                                               PRInt32 aFontID,
                                               const nscoord* aSpacing)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawString\n"));
    if (0 != aLength) 
    {
        if (nsnull == mTMatrix || 
            nsnull == mSurface ||
            nsnull == mSurface->GetPixmap() ||
            nsnull == mSurface->GetGC() ||
            nsnull == aString)
        {
            return NS_ERROR_FAILURE;
        }

        nscoord x = aX;
        nscoord y = aY;

        if (mFontMetrics) 
        {
            mFontMetrics->GetMaxAscent(y);
            y += aY;
        }

        if (nsnull != aSpacing) 
        {
            // Render the string, one character at a time...
            const PRUnichar* end = aString + aLength;
            while (aString < end) 
            {
                QChar ch = (QChar) *aString++;
                nscoord xx = x;
                nscoord yy = y;
                mTMatrix->TransformCoord(&xx, &yy);
                mSurface->GetGC()->drawText(xx, yy, ch, 1);
                x += *aSpacing++;
            }
        }
        else 
        {
            mTMatrix->TransformCoord(&x, &y);
            QString string((QChar *) aString, aLength);
            mSurface->GetGC()->drawText(x, y, string);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawString(const nsString& aString,
                                               nscoord aX, 
                                               nscoord aY,
                                               PRInt32 aFontID,
                                               const nscoord* aSpacing)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawString\n"));
    return DrawString(aString.GetUnicode(), 
                      aString.Length(),
                      aX, 
                      aY, 
                      aFontID, 
                      aSpacing);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage, 
                                              nscoord aX, 
                                              nscoord aY)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawImage\n"));
    nscoord width  = NSToCoordRound(mP2T * aImage->GetWidth());
    nscoord height = NSToCoordRound(mP2T * aImage->GetHeight());

    return DrawImage(aImage,aX,aY,width,height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage, 
                                              nscoord aX, 
                                              nscoord aY,
                                              nscoord aWidth, 
                                              nscoord aHeight)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawImage\n"));
    nsRect	tr;

    tr.x      = aX;
    tr.y      = aY;
    tr.width  = aWidth;
    tr.height = aHeight;

    return DrawImage(aImage,tr);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage, 
                                              const nsRect& aRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawImage\n"));
    nsRect	tr;

    tr = aRect;
    mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

    return aImage->Draw(*this,
                        mSurface,
                        tr.x,
                        tr.y,
                        tr.width,
                        tr.height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage, 
                                              const nsRect& aSRect, 
                                              const nsRect& aDRect)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::DrawImage\n"));
    nsRect	sr,dr;

    sr = aSRect;
    mTMatrix ->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);

    dr = aDRect;
    mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);

    return aImage->Draw(*this,
                        mSurface,
                        sr.x,
                        sr.y,
                        sr.width,
                        sr.height,
                        dr.x,
                        dr.y,
                        dr.width,
                        dr.height);
}

NS_IMETHODIMP 
nsRenderingContextQT::CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                        PRInt32 aSrcX, 
                                        PRInt32 aSrcY,
                                        const nsRect &aDestBounds,
                                        PRUint32 aCopyFlags)
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsRenderingContextQT::CopyOffScreenBits\n"));
    PRInt32               x = aSrcX;
    PRInt32               y = aSrcY;
    nsRect                drect = aDestBounds;
    nsDrawingSurfaceQT  *destsurf;

    if (nsnull == aSrcSurf ||
        nsnull == mTMatrix ||
        nsnull == mSurface ||
        nsnull == mSurface->GetPixmap() ||
        nsnull == mSurface->GetGC())
    {
        return NS_ERROR_FAILURE;
    }

    if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
    {
        NS_ASSERTION(!(nsnull == mSurface), "no back buffer");
        destsurf = mSurface;
    }
    else
    {
        destsurf = mOffscreenSurface;
    }

    if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    {
        mTMatrix->TransformCoord(&x, &y);
    }

    if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    {
        mTMatrix->TransformCoord(&drect.x, 
                                 &drect.y, 
                                 &drect.width, 
                                 &drect.height);
    }

    //XXX flags are unused. that would seem to mean that there is
    //inefficiency somewhere... MMP

    destsurf->GetGC()->drawPixmap(x, 
                                  y, 
                                  *((nsDrawingSurfaceQT *)aSrcSurf)->GetPixmap(), 
                                  drect.x, 
                                  drect.y, 
                                  drect.width, 
                                  drect.height);

    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
    return NS_OK;
}
