/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Leon Sha <leon.sha@sun.com>
 *   Ken Herron <kherron@fastmail.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsRenderingContextPS.h"
#include "nsFontMetricsPS.h"
#include "nsDeviceContextPS.h"
#include "nsPostScriptObj.h"  
#include "nsIRegion.h"      
#include "nsIImage.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include <math.h>

#define NS_PIXELS_TO_POINTS(x) ((x) * 10)

#define FLAG_CLIP_VALID       0x0001
#define FLAG_CLIP_CHANGED     0x0002
#define FLAG_LOCAL_CLIP_VALID 0x0004

#define FLAGS_ALL             (FLAG_CLIP_VALID | FLAG_CLIP_CHANGED | FLAG_LOCAL_CLIP_VALID)

/** ---------------------------------------------------
 *  Class definition of a postscript graphics state to me maintained
 *  by the layout engine
 *	@update 12/21/98 dwc
 */
class PS_State
{
public:
  PS_State();
  PS_State(PS_State &aState);
  ~PS_State();

  PS_State        *mNext;
  nsTransform2D   mMatrix;
  nsRect          mLocalClip;
  nsCOMPtr<nsIFontMetrics>  mFontMetrics;
  nscolor         mCurrentColor;
  nscolor         mTextColor;
  nsLineStyle     mLineStyle;
  PRInt32         mFlags;
};

/** ---------------------------------------------------
 *  Default Constructor for the state
 *	@update 12/21/98 dwc
 */
PS_State :: PS_State()
{
  mNext         = nsnull;
  mMatrix.SetToIdentity();  
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mFontMetrics  = nsnull;
  mCurrentColor = NS_RGB(0, 0, 0);
  mTextColor    = NS_RGB(0, 0, 0);
  mLineStyle    = nsLineStyle_kSolid;
}

/** ---------------------------------------------------
 *  Constructor of a state using the passed in state to initialize to
 *  Default Constructor for the state
 *	@update 12/21/98 dwc
 */
PS_State :: PS_State(PS_State &aState) : 
  mMatrix(&aState.mMatrix),
  mLocalClip(aState.mLocalClip)
{
  mNext = &aState;
  //mClipRegion = nsnull;
  mCurrentColor = aState.mCurrentColor;
  mFontMetrics = nsnull;
  //mFont = nsnull;
  mFlags = ~FLAGS_ALL;
  mTextColor = aState.mTextColor;
  mLineStyle = aState.mLineStyle;
}

/** ---------------------------------------------------
 *  Destructor for a state
 *	@update 12/21/98 dwc
 */
PS_State :: ~PS_State()
{
  //if (nsnull != mClipRegion){
    //VERIFY(::DeleteObject(mClipRegion));
    //mClipRegion = nsnull;
  //}

  //don't delete this because it lives in the font metrics
  //mFont = nsnull;
}


NS_IMPL_ISUPPORTS1(nsRenderingContextPS, nsIRenderingContext)

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
nsRenderingContextPS :: nsRenderingContextPS()
{
  mPSObj = nsnull;     // local copy of printcontext, will be set on the init process
  mContext = nsnull;
  mFontMetrics = nsnull;

  mStateCache = new nsVoidArray();

  mP2T = 1.0f;

  PushState();
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
nsRenderingContextPS::~nsRenderingContextPS()
{
  if (mStateCache){
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0){
      PS_State *state = (PS_State *)mStateCache->ElementAt(cnt);
      mStateCache->RemoveElementAt(cnt);

      if (state)
        delete state;
    }

    delete mStateCache;
    mStateCache = nsnull;
  }

  mTranMatrix = nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsRenderingContextPS::Init(nsIDeviceContext* aContext)
{
  NS_ENSURE_TRUE(nsnull != aContext, NS_ERROR_NULL_POINTER);

  mContext = aContext;
  mP2T = mContext->DevUnitsToAppUnits();

  mPSObj = NS_REINTERPRET_CAST(nsDeviceContextPS *, mContext.get())->GetPrintContext();

  NS_ENSURE_TRUE(nsnull != mPSObj, NS_ERROR_NULL_POINTER);

  // Layout's coordinate system places the origin at top left with Y
  // increasing down; PS places the origin at bottom left with Y increasing
  // upward. Both systems use twips for units, so no resizing is needed.
  mTranMatrix->SetToScale(1.0, -1.0);
  mTranMatrix->AddTranslation(0, -mPSObj->mPrintSetup->height);

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: UnlockDrawingSurface(void)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsRenderingContextPS :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsRenderingContextPS :: GetDrawingSurface(nsDrawingSurface *aSurface)
{
  *aSurface = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsRenderingContextPS :: GetHints(PRUint32& aResult)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: Reset()
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  aContext = mContext;
  NS_IF_ADDREF(aContext);
  return NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: PushState(void)
{
  PRInt32 cnt = mStateCache->Count();

  if (cnt == 0){
    if (nsnull == mStates)
      mStates = new PS_State();
    else
      mStates = new PS_State(*mStates);
  } else {
    PS_State *state = (PS_State *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    state->mNext = mStates;

    //clone state info
    state->mMatrix = mStates->mMatrix;
    state->mLocalClip = mStates->mLocalClip;
    state->mCurrentColor = mStates->mCurrentColor;
    state->mFontMetrics = mStates->mFontMetrics;
    state->mTextColor = mStates->mTextColor;
    state->mLineStyle = mStates->mLineStyle;

    mStates = state;
  }

  mTranMatrix = &mStates->mMatrix;

  // at startup, the graphics state is not saved yet
  if(mPSObj)
    mPSObj->graphics_save();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: PopState(void)
{
  if (nsnull == mStates){
    NS_ASSERTION(!(nsnull == mStates), "state underflow");
  } else {
    PS_State *oldstate = mStates;

    mStates = mStates->mNext;

    mStateCache->AppendElement(oldstate);

    if (nsnull != mStates){
      mTranMatrix = &mStates->mMatrix;
      SetLineStyle(mStates->mLineStyle);
    }
    else
      mTranMatrix = nsnull;
  }

  mPSObj->graphics_restore();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine)
{
  nsRect  trect = aRect;

  mStates->mLocalClip = aRect;

  mTranMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);
  mStates->mFlags |= FLAG_LOCAL_CLIP_VALID;

  if (aCombine == nsClipCombine_kIntersect){
    mPSObj->newpath();
    mPSObj->box(trect.x, trect.y, trect.width, trect.height);
  } else if (aCombine == nsClipCombine_kUnion){
    mPSObj->newpath();
    mPSObj->box(trect.x, trect.y, trect.width, trect.height);
  }else if (aCombine == nsClipCombine_kSubtract){
    mPSObj->newpath();
    mPSObj->clippath();   // get the current path
    mPSObj->box_subtract(trect.x, trect.y, trect.width, trect.height);
  }else if (aCombine == nsClipCombine_kReplace){
    mPSObj->initclip();
    mPSObj->newpath();
    mPSObj->box(trect.x, trect.y, trect.width, trect.height);
  }else{
    NS_ASSERTION(PR_FALSE, "illegal clip combination");
    return NS_ERROR_INVALID_ARG;
  }
  mPSObj->clip();
  mPSObj->newpath();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  if (mStates->mLocalClip.width !=0){
    aRect = mStates->mLocalClip;
    aClipValid = PR_TRUE;
  }else{
    aClipValid = PR_FALSE;
  }

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine)
{
  nsRect rect;
  nsIRegion* pRegion = (nsIRegion*)&aRegion;
  pRegion->GetBoundingBox(&rect.x, &rect.y, &rect.width, &rect.height);
  SetClipRect(rect, aCombine);

  return NS_OK; 
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: CopyClipRegion(nsIRegion &aRegion)
{
  //XXX wow, needs to do something.
  return NS_OK; 
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetClipRegion(nsIRegion **aRegion)
{
  //XXX wow, needs to do something.
  return NS_OK; 
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: SetColor(nscolor aColor)
{
  mPSObj->setcolor(aColor);
  mCurrentColor = aColor;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: GetColor(nscolor &aColor) const
{
  aColor = mCurrentColor;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: SetLineStyle(nsLineStyle aLineStyle)
{
  mCurrLineStyle = aLineStyle;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetLineStyle(nsLineStyle &aLineStyle)
{
  aLineStyle = mCurrLineStyle;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::SetFont(const nsFont& aFont, nsIAtom* aLangGroup)
{
  nsCOMPtr<nsIFontMetrics> newMetrics;
  nsresult rv = mContext->GetMetricsFor( aFont, aLangGroup, *getter_AddRefs(newMetrics) );

  if (NS_SUCCEEDED(rv)) {
    rv = SetFont(newMetrics);
  }
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::SetFont(nsIFontMetrics *aFontMetrics)
{
  mFontMetrics = (nsFontMetricsPS *)aFontMetrics;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  aFontMetrics = (nsIFontMetrics *)mFontMetrics;
  NS_IF_ADDREF(aFontMetrics);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::Translate(nscoord aX, nscoord aY)
{
  mTranMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: Scale(float aSx, float aSy)
{
	mTranMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTranMatrix;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: CreateDrawingSurface(const nsRect& aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  return NS_OK;   // offscreen test
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  return NS_OK;   // offscreen test
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  // Layout expects lines to be one scaled pixel wide.
  float scale;
  NS_REINTERPRET_CAST(DeviceContextImpl *, mContext.get())->GetCanonicalPixelScale(scale);
  int width = NSToCoordRound(TWIPS_PER_POINT_FLOAT * scale);

  // If this line is vertical (horizontal), the geometric line defined
  // by our start and end points is actually the left edge (top edge)
  // of the line that should appear on the page.
  if (aX0 == aX1) {
    // Vertical. For better control we draw this as a filled
    // rectangle instead of a stroked line.
    return FillRect(aX0, aY0, width, aY1 - aY0);
  }
  else if (aY0 == aY1) {
    // Horizontal.
    return FillRect(aX0, aY0, aX1 - aX0, width);
  }
  else {
    // Angled line. Just stroke it.
    mTranMatrix->TransformCoord(&aX0,&aY0);
    mTranMatrix->TransformCoord(&aX1,&aY1);
    mPSObj->line(aX0, aY0, aX1, aY1, width);
    return NS_OK;
  }
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{

const nsPoint*  np;
nsPoint         pp;

  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  np = &aPoints[0];

  pp.x = np->x;
  pp.y = np->y;
  mTranMatrix->TransformCoord(&pp.x, &pp.y);
  mPSObj->moveto(pp.x, pp.y);
  np++;

  // we are ignoring the linestyle
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
                mTranMatrix->TransformCoord(&pp.x, &pp.y);
                mPSObj->lineto(pp.x, pp.y);
	}

  // we dont close the path, this will give us a polyline
  mPSObj->stroke();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawRect(const nsRect& aRect)
{
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{

	mTranMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  mPSObj->newpath();
  mPSObj->box(aX, aY, aWidth, aHeight);
  mPSObj->stroke();
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: FillRect(const nsRect& aRect)
{
  return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{

	mTranMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  mPSObj->newpath();
  mPSObj->box(aX, aY, aWidth, aHeight);
  mPSObj->fill();
  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextPS :: InvertRect(const nsRect& aRect)
{
	NS_NOTYETIMPLEMENTED("nsRenderingContextPS::InvertRect");

  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextPS :: InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	NS_NOTYETIMPLEMENTED("nsRenderingContextPS::InvertRect");

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
const nsPoint*  np;
nsPoint         pp;

  mPSObj->newpath();

  // point np to the polypoints
  np = &aPoints[0];

  // do the initial moveto
	pp.x = np->x;
	pp.y = np->y;
  mTranMatrix->TransformCoord(&pp.x, &pp.y);
  mPSObj->moveto(pp.x, pp.y);
  np++;

  // add all the points to the path
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
                mTranMatrix->TransformCoord(&pp.x, &pp.y);
                mPSObj->lineto(pp.x, pp.y);
	}

  mPSObj->closepath();
  mPSObj->stroke();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
const nsPoint*  np;
nsPoint         pp;

  mPSObj->newpath();

  // point np to the polypoints
  np = &aPoints[0];

  // do the initial moveto
	pp.x = np->x;
	pp.y = np->y;
  mTranMatrix->TransformCoord(&pp.x, &pp.y);
  mPSObj->moveto(pp.x, pp.y);
  np++;

  // add all the points to the path
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
                mTranMatrix->TransformCoord(&pp.x, &pp.y);
                mPSObj->lineto(pp.x, pp.y);
	}

  mPSObj->closepath();
  mPSObj->fill();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  //SetupPen();
  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  mPSObj->comment("ellipse");
  mPSObj->newpath();
  mPSObj->moveto(aX, aY);
  mPSObj->arc(aWidth, aHeight, 0.0, 360.0);
  mPSObj->closepath();
  mPSObj->stroke();

  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextPS :: FillEllipse(const nsRect& aRect)
{
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  mPSObj->comment("ellipse");
  mPSObj->newpath();
  mPSObj->moveto(aX, aY);
  mPSObj->arc(aWidth, aHeight, 0.0, 360.0);
  mPSObj->closepath();
  mPSObj->fill();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  //SetupPen();
  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  mPSObj->comment("arc");
  mPSObj->newpath();
  mPSObj->moveto(aX, aY);
  mPSObj->arc(aWidth, aHeight, aStartAngle, aEndAngle);
  mPSObj->closepath();
  mPSObj->stroke();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  //SetupPen();
  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  mPSObj->comment("arc");
  mPSObj->newpath();
  mPSObj->moveto(aX, aY);
  mPSObj->arc(aWidth, aHeight, aStartAngle, aEndAngle);
  mPSObj->closepath();
  mPSObj->fill();

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetWidth(char ch, nscoord& aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth, aFontID);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString),aWidth);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::GetWidth(const char* aString,PRUint32 aLength,nscoord& aWidth)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (mFontMetrics) {
    rv = NS_REINTERPRET_CAST(nsFontMetricsPS *, mFontMetrics.get())->GetStringWidth(aString,aWidth,aLength);
  }
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetWidth(const PRUnichar *aString,PRUint32 aLength,nscoord &aWidth, PRInt32 *aFontID)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mFontMetrics) {
    rv = NS_REINTERPRET_CAST(nsFontMetricsPS *, mFontMetrics.get())->GetStringWidth(aString, aWidth, aLength);
  }

  return rv;
}

/** --------------------------------------------------- */
NS_IMETHODIMP
nsRenderingContextPS::GetTextDimensions(const char*       aString,
                                        PRInt32           aLength,
                                        PRInt32           aAvailWidth,
                                        PRInt32*          aBreaks,
                                        PRInt32           aNumBreaks,
                                        nsTextDimensions& aDimensions,
                                        PRInt32&          aNumCharsFit,
                                        nsTextDimensions& aLastWordDimensions,
                                        PRInt32*          aFontID)
{
  NS_NOTYETIMPLEMENTED("nsRenderingContextPS::GetTextDimensions");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRenderingContextPS::GetTextDimensions(const PRUnichar*  aString,
                                        PRInt32           aLength,
                                        PRInt32           aAvailWidth,
                                        PRInt32*          aBreaks,
                                        PRInt32           aNumBreaks,
                                        nsTextDimensions& aDimensions,
                                        PRInt32&          aNumCharsFit,
                                        nsTextDimensions& aLastWordDimensions,
                                        PRInt32*          aFontID)
{
  NS_NOTYETIMPLEMENTED("nsRenderingContextPS::GetTextDimensions");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRenderingContextPS :: GetTextDimensions(const char* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mFontMetrics) {
    nsFontMetricsPS *metrics = NS_REINTERPRET_CAST(nsFontMetricsPS *, mFontMetrics.get());
    metrics->GetStringWidth(aString, aDimensions.width, aLength);
    metrics->GetMaxAscent(aDimensions.ascent);
    metrics->GetMaxDescent(aDimensions.descent);
    rv = NS_OK;
  }
  
  return rv;
}

NS_IMETHODIMP
nsRenderingContextPS :: GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions, PRInt32* aFontID)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mFontMetrics) {
    nsFontMetricsPS *metrics = NS_REINTERPRET_CAST(nsFontMetricsPS *, mFontMetrics.get());
    metrics->GetStringWidth(aString, aDimensions.width, aLength);
     //XXX temporary - bug 96609
    metrics->GetMaxAscent(aDimensions.ascent);
    metrics->GetMaxDescent(aDimensions.descent);
    rv = NS_OK;
  }

  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsRenderingContextPS :: DrawString(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        const nscoord* aSpacing)
{
  NS_ENSURE_TRUE(mTranMatrix && mPSObj && mFontMetrics, NS_ERROR_NULL_POINTER);

  nsFontMetricsPS *metrics = NS_REINTERPRET_CAST(nsFontMetricsPS *, mFontMetrics.get());
  NS_ENSURE_TRUE(metrics, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAtom> langGroup;
  mFontMetrics->GetLangGroup(getter_AddRefs(langGroup));
  mPSObj->setlanggroup(langGroup);

  if (aLength == 0)
    return NS_OK;
  nsFontPS* fontPS = nsFontPS::FindFont(aString[0], *metrics->GetFont(), metrics);
  NS_ENSURE_TRUE(fontPS, NS_ERROR_FAILURE);
  fontPS->SetupFont(this);

  PRUint32 i, start = 0;
  for (i=0; i<aLength; i++) {
    nsFontPS* fontThisChar;
    fontThisChar = nsFontPS::FindFont(aString[i], *metrics->GetFont(), metrics);
    NS_ENSURE_TRUE(fontThisChar, NS_ERROR_FAILURE);
    if (fontThisChar != fontPS) {
      // draw text up to this point
      aX += DrawString(aString+start, i-start, aX, aY, fontPS, 
                       aSpacing?aSpacing+start:nsnull);
      start = i;

      // setup for following text
      fontPS = fontThisChar;
      fontPS->SetupFont(this);
    }
  }

  // draw the last part
  if (aLength-start)
    DrawString(aString+start, aLength-start, aX, aY, fontPS, 
               aSpacing?aSpacing+start:nsnull);

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing)
{
  NS_ENSURE_TRUE(mTranMatrix && mPSObj && mFontMetrics, NS_ERROR_NULL_POINTER);
  
  nsFontMetricsPS *metrics = NS_REINTERPRET_CAST(nsFontMetricsPS *, mFontMetrics.get());
  NS_ENSURE_TRUE(metrics, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAtom> langGroup = nsnull;
  mFontMetrics->GetLangGroup(getter_AddRefs(langGroup));
  mPSObj->setlanggroup(langGroup.get());

  /* build up conversion table */
  mPSObj->preshow(aString, aLength);

  if (aLength == 0)
    return NS_OK;
  nsFontPS* fontPS = nsFontPS::FindFont(aString[0], *metrics->GetFont(), metrics);
  NS_ENSURE_TRUE(fontPS, NS_ERROR_FAILURE);
  fontPS->SetupFont(this);

  PRUint32 i, start = 0;
  for (i=0; i<aLength; i++) {
    nsFontPS* fontThisChar;
    fontThisChar = nsFontPS::FindFont(aString[i], *metrics->GetFont(), metrics);
    NS_ENSURE_TRUE(fontThisChar, NS_ERROR_FAILURE);
    if (fontThisChar != fontPS) {
      // draw text up to this point
      aX += DrawString(aString+start, i-start, aX, aY, fontPS, 
                       aSpacing?aSpacing+start:nsnull);
      start = i;

      // setup for following text
      fontPS = fontThisChar;
      fontPS->SetupFont(this);
    }
  }

  // draw the last part
  if (aLength-start)
    DrawString(aString+start, aLength-start, aX, aY, fontPS, 
               aSpacing?aSpacing+start:nsnull);

  return NS_OK;
}

PRInt32 
nsRenderingContextPS::DrawString(const char *aString, PRUint32 aLength,
                                 nscoord &aX, nscoord &aY, nsFontPS* aFontPS,
                                 const nscoord* aSpacing)
{
  nscoord width = 0;
  PRInt32 x = aX;
  PRInt32 y = aY;

  PRInt32 dxMem[500];
  PRInt32* dx0 = 0;
  if (aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new PRInt32[aLength];
      NS_ENSURE_TRUE(dx0, NS_ERROR_OUT_OF_MEMORY);
    }
    mTranMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }

  mTranMatrix->TransformCoord(&x, &y);
  width = aFontPS->DrawString(this, x, y, aString, aLength);

  if ((aSpacing) && (dx0 != dxMem)) {
    delete [] dx0;
  }

  return width;
}


PRInt32 
nsRenderingContextPS::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                 nscoord aX, nscoord aY, nsFontPS* aFontPS,
                                 const nscoord* aSpacing)
{
  nscoord width = 0;
  PRInt32 x = aX;
  PRInt32 y = aY;

  if (aSpacing) {
    // Slow, but accurate rendering
    const PRUnichar* end = aString + aLength;
    while (aString < end){
      x = aX;
      y = aY;
      mTranMatrix->TransformCoord(&x, &y);
      aFontPS->DrawString(this, x, y, aString, 1);
      aX += *aSpacing++;
      aString++;
    }
    width = aX;
  } else {
    mTranMatrix->TransformCoord(&x, &y);
    width = aFontPS->DrawString(this, x, y, aString, aLength);
  }

  return width;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawString(const nsString& aString,nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextPS::DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect)
{
  // Transform the destination rectangle.
  nsRect dr = aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  // Transform the source rectangle. We don't use the matrix for this;
  // just convert the twips back into points.
  nsRect sr = aSrcRect;
  sr.x /= TWIPS_PER_POINT_INT;
  sr.y /= TWIPS_PER_POINT_INT;
  sr.width /= TWIPS_PER_POINT_INT;
  sr.height /= TWIPS_PER_POINT_INT;

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  nsRect ir;
  iframe->GetRect(ir);
  mPSObj->draw_image(img, sr, ir, dr);
  return NS_OK;
}

#ifdef MOZ_MATHML
  /**
   * Returns metrics (in app units) of an 8-bit character string
   */
NS_IMETHODIMP 
nsRenderingContextPS::GetBoundingMetrics(const char*        aString,
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics)
{
  // Fill me up 
  return NS_ERROR_NOT_IMPLEMENTED;
}

  /**
   * Returns metrics (in app units) of a Unicode character string
   */
NS_IMETHODIMP 
nsRenderingContextPS::GetBoundingMetrics(const PRUnichar*   aString,
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics,
                                         PRInt32*           aFontID)
{
  // Fill me up 
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif /* MOZ_MATHML */

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsRenderingContextPS :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  Output postscript supplied by the caller to the print job. The
 *  caller should have already called PushState() (and preferably
 *  SetClipRect()).
 *    @update  9/31/2003 kherron
 *    @param   aData    Buffer containing postscript to be output
 *             aDataLen Number of characters in aData
 *    @return  NS_OK
 */
NS_IMETHODIMP nsRenderingContextPS::RenderPostScriptDataFragment(const unsigned char *aData, unsigned long aDatalen)
{
  NS_ASSERTION(mPSObj != NULL, "No nsPostScriptObj");

  // Reset the coordinate system to point-sized. The origin and Y axis
  // orientation are already correct.
  mPSObj->scale(TWIPS_PER_POINT_FLOAT, TWIPS_PER_POINT_FLOAT);
  fwrite(aData, aDatalen, 1, mPSObj->GetScriptHandle());

  return NS_OK;
}

#ifdef NOTNOW
HPEN nsRenderingContextPS :: SetupSolidPen(void)
{
  return mCurrPen;
}

HPEN nsRenderingContextPS :: SetupDashedPen(void)
{
  return mCurrPen;
}

HPEN nsRenderingContextPS :: SetupDottedPen(void)
{
  return mCurrPen;
}

#endif /* NOTNOW */

