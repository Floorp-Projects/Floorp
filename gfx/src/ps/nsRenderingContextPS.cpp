/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsRenderingContextPS.h"
#include "nsFontMetricsPS.h"
#include <math.h>
#include "nsDeviceContextPS.h"
#include "nsPostScriptObj.h"  
#include "nsIRegion.h"      
#include "nsIImage.h"      

static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

// Macro to convert from TWIPS (1440 per inch) to POINTS (72 per inch)
#define NS_PIXELS_TO_POINTS(x) (x * 10)

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
  nsIFontMetrics  *mFontMetrics;
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
  mNext = nsnull;
  mMatrix.SetToIdentity();  
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mFontMetrics = nsnull;
  mCurrentColor = NS_RGB(0, 0, 0);
  mTextColor = NS_RGB(0, 0, 0);
  mLineStyle = nsLineStyle_kSolid;
}

/** ---------------------------------------------------
 *  Constructor of a state using the passed in state to initialize to
 *  Default Constructor for the state
 *	@update 12/21/98 dwc
 */
PS_State :: PS_State(PS_State &aState):mMatrix(&aState.mMatrix),mLocalClip(aState.mLocalClip)
{
  mNext = &aState;
  //mClipRegion = NULL;
  mCurrentColor = aState.mCurrentColor;
  mFontMetrics = nsnull;
  //mFont = NULL;
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
  //if (NULL != mClipRegion){
    //VERIFY(::DeleteObject(mClipRegion));
    //mClipRegion = NULL;
  //}

  //don't delete this because it lives in the font metrics
  //mFont = NULL;
}


static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
nsRenderingContextPS :: nsRenderingContextPS()
{
  NS_INIT_REFCNT();

  mPSObj = nsnull;     // local copy of printcontext, will be set on the init process
  mContext = nsnull;
  mFontMetrics = nsnull;

  mStateCache = new nsVoidArray();

  PushState();

  mP2T = 1.0f;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
nsRenderingContextPS :: ~nsRenderingContextPS()
{
  if (nsnull != mStateCache){
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0){
      PS_State *state = (PS_State *)mStateCache->ElementAt(cnt);
      mStateCache->RemoveElementAt(cnt);

      if (nsnull != state)
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
nsresult
nsRenderingContextPS :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{

  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIRenderingContextIID)){
    nsIRenderingContext* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID)){
    nsIRenderingContext* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsRenderingContextPS)
NS_IMPL_RELEASE(nsRenderingContextPS)

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsRenderingContextPS :: Init(nsIDeviceContext* aContext)
{
float app2dev;

  mContext = aContext;
  if (mContext) {
  	mPSObj = ((nsDeviceContextPS*)mContext)->GetPrintContext();
  }
  NS_IF_ADDREF(mContext);

  // initialize the matrix
  mContext->GetAppUnitsToDevUnits(app2dev);
	mTranMatrix->AddScale(app2dev, app2dev);
  mContext->GetDevUnitsToAppUnits(mP2T);
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
  NS_IF_ADDREF(mContext);
  aContext = mContext;
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
nsRenderingContextPS :: PopState(PRBool &aClipEmpty)
{
  PRBool  retval = PR_FALSE;

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

  aClipEmpty = retval;
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
NS_IMETHODIMP nsRenderingContextPS :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
nsRect  trect = aRect;
#ifdef XP_PC
PRInt32     cliptype;
#endif

  mStates->mLocalClip = aRect;

	mTranMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);
  mStates->mFlags |= FLAG_LOCAL_CLIP_VALID;

  if (aCombine == nsClipCombine_kIntersect){
    mPSObj->newpath();
    mPSObj->moveto( NS_PIXELS_TO_POINTS(trect.x), NS_PIXELS_TO_POINTS(trect.y));
    mPSObj->box(NS_PIXELS_TO_POINTS(trect.width), NS_PIXELS_TO_POINTS(trect.height));
    mPSObj->closepath();
    mPSObj->clip();
  } else if (aCombine == nsClipCombine_kUnion){
    mPSObj->newpath();
    mPSObj->moveto( NS_PIXELS_TO_POINTS(trect.x), NS_PIXELS_TO_POINTS(trect.y));
    mPSObj->box(NS_PIXELS_TO_POINTS(trect.width), NS_PIXELS_TO_POINTS(trect.height));
    mPSObj->closepath();
    mPSObj->clip();
  }else if (aCombine == nsClipCombine_kSubtract){
    mPSObj->newpath();
    mPSObj->clippath();   // get the current path
    mPSObj->moveto(NS_PIXELS_TO_POINTS(trect.x), NS_PIXELS_TO_POINTS(trect.y));
    mPSObj->box_subtract(NS_PIXELS_TO_POINTS(trect.width), NS_PIXELS_TO_POINTS(trect.height));
    mPSObj->closepath();
    mPSObj->clip();
  }else if (aCombine == nsClipCombine_kReplace){
    mPSObj->initclip();
    mPSObj->newpath();
    mPSObj->moveto(NS_PIXELS_TO_POINTS(trect.x), NS_PIXELS_TO_POINTS(trect.y));
    mPSObj->box(NS_PIXELS_TO_POINTS(trect.width), NS_PIXELS_TO_POINTS(trect.height));
    mPSObj->closepath();
    mPSObj->clip();
  }else{
    NS_ASSERTION(PR_FALSE, "illegal clip combination");
  }

#if defined(XP_PC) && !defined(XP_OS2)
  if (cliptype == NULLREGION)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;
#endif

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
nsRenderingContextPS :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect rect;
  nsIRegion* pRegion = (nsIRegion*)&aRegion;
  pRegion->GetBoundingBox(&rect.x, &rect.y, &rect.width, &rect.height);
  SetClipRect(rect, aCombine, aClipEmpty);

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
nsRenderingContextPS :: SetFont(const nsFont& aFont)
{
  NS_IF_RELEASE(mFontMetrics);
  if (mContext) {
  	mContext->GetMetricsFor(aFont, mFontMetrics);
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: SetFont(nsIFontMetrics *aFontMetrics)
{

  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{

  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: Translate(nscoord aX, nscoord aY)
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
nsRenderingContextPS :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
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

	mTranMatrix->TransformCoord(&aX0,&aY0);
	mTranMatrix->TransformCoord(&aX1,&aY1);

  // this has the moveto,lineto and the stroke
  mPSObj->line(NS_PIXELS_TO_POINTS(aX0),NS_PIXELS_TO_POINTS(aY0),
          NS_PIXELS_TO_POINTS(aX1),NS_PIXELS_TO_POINTS(aY1),1);

  return NS_OK;
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
  mTranMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
  mPSObj->moveto_loc(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
  np++;

  // we are ignoring the linestyle
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
		mTranMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
    mPSObj->lineto(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
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
nsRect	tr;

	tr = aRect;
	mTranMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

  mPSObj->newpath();
  mPSObj->moveto(NS_PIXELS_TO_POINTS(tr.x), NS_PIXELS_TO_POINTS(tr.y));
  mPSObj->box(NS_PIXELS_TO_POINTS(tr.width), NS_PIXELS_TO_POINTS(tr.height));
  mPSObj->closepath();
  mPSObj->stroke();

  return NS_OK;
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
  mPSObj->moveto(NS_PIXELS_TO_POINTS(aX), NS_PIXELS_TO_POINTS(aY));
  mPSObj->box(NS_PIXELS_TO_POINTS(aWidth), NS_PIXELS_TO_POINTS(aHeight));
  mPSObj->closepath();
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
nsRect	tr;

	tr = aRect;
	mTranMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

  mPSObj->newpath();
  mPSObj->moveto(NS_PIXELS_TO_POINTS(tr.x), NS_PIXELS_TO_POINTS(tr.y));
  mPSObj->box(NS_PIXELS_TO_POINTS(tr.width), NS_PIXELS_TO_POINTS(tr.height));
  mPSObj->closepath();
  mPSObj->fill();
  return NS_OK;
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
  mPSObj->moveto(NS_PIXELS_TO_POINTS(aX), NS_PIXELS_TO_POINTS(aY));
  mPSObj->box(NS_PIXELS_TO_POINTS(aWidth), NS_PIXELS_TO_POINTS(aHeight));
  mPSObj->closepath();
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
  mTranMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
  mPSObj->moveto_loc(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
  np++;

  // add all the points to the path
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
		mTranMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
    mPSObj->lineto(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
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
  mTranMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
  mPSObj->moveto_loc(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
  np++;

  // add all the points to the path
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
		mTranMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
    mPSObj->lineto(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
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

  mPSObj->comment("arc");
  mPSObj->newpath();
  mPSObj->moveto(NS_PIXELS_TO_POINTS(aX), NS_PIXELS_TO_POINTS(aY));
  mPSObj->ellipse(NS_PIXELS_TO_POINTS(aWidth), NS_PIXELS_TO_POINTS(aHeight));
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

  mPSObj->comment("arc");
  mPSObj->newpath();
  mPSObj->moveto(NS_PIXELS_TO_POINTS(aX), NS_PIXELS_TO_POINTS(aY));
  mPSObj->ellipse(NS_PIXELS_TO_POINTS(aWidth), NS_PIXELS_TO_POINTS(aHeight));
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
  mPSObj->moveto(NS_PIXELS_TO_POINTS(aX), NS_PIXELS_TO_POINTS(aY));
  mPSObj->arc(NS_PIXELS_TO_POINTS(aWidth), NS_PIXELS_TO_POINTS(aHeight),aStartAngle,aEndAngle);
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
  mPSObj->moveto(NS_PIXELS_TO_POINTS(aX), NS_PIXELS_TO_POINTS(aY));
  mPSObj->arc(NS_PIXELS_TO_POINTS(aWidth), NS_PIXELS_TO_POINTS(aHeight),aStartAngle,aEndAngle);
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
nsRenderingContextPS :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
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
nsRenderingContextPS :: GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString),aWidth);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetWidth(const char* aString,PRUint32 aLength,nscoord& aWidth)
{

  if (nsnull != mFontMetrics){
    ((nsFontMetricsPS*)mFontMetrics)->GetStringWidth(aString,aWidth,aLength);
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }

}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
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

  if (nsnull != mFontMetrics){
    ((nsFontMetricsPS*)mFontMetrics)->GetStringWidth(aString,aWidth,aLength);
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }

}

/** --------------------------------------------------- */

NS_IMETHODIMP
nsRenderingContextPS :: GetTextDimensions(const char* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions)
{
  if (nsnull != mFontMetrics){
    ((nsFontMetricsPS*)mFontMetrics)->GetStringWidth(aString,aDimensions.width,aLength);
     mFontMetrics->GetMaxAscent(aDimensions.ascent);
     mFontMetrics->GetMaxDescent(aDimensions.descent);
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsRenderingContextPS :: GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions, PRInt32* aFontID)
{
  if (nsnull != mFontMetrics){
    ((nsFontMetricsPS*)mFontMetrics)->GetStringWidth(aString,aDimensions.width,aLength);
     //XXX temporary - bug 96609
     mFontMetrics->GetMaxAscent(aDimensions.ascent);
     mFontMetrics->GetMaxDescent(aDimensions.descent);
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
nsRenderingContextPS :: DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  nscoord y;
  mFontMetrics->GetMaxAscent(y);
  return DrawString2(aString, aLength, aX, aY + y, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextPS :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  nscoord y;
  mFontMetrics->GetMaxAscent(y);
  return DrawString2(aString, aLength, aX, aY + y, aFontID, aSpacing);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawString2(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        const nscoord* aSpacing)
{
PRInt32	      x = aX;
PRInt32       y = aY;

  mPSObj->setlanggroup(nsnull);

  SetupFontAndColor();
  PRInt32 dxMem[500];
  PRInt32* dx0 = 0;
  if (nsnull != aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new PRInt32[aLength];
    }
    mTranMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }

	mTranMatrix->TransformCoord(&x, &y);
  PostscriptTextOut(aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aLength, (const nscoord*) (aSpacing ? dx0 : NULL), PR_FALSE);

  if ((nsnull != aSpacing) && (dx0 != dxMem)) {
    delete [] dx0;
  }

#if 0
  //this doesn't need to happen here anymore, but a
  //new api will come along that will need this stuff. MMP
  if (nsnull != mFontMetrics){
    nsFont *font;
    mFontMetrics->GetFont(font);
    PRUint8 decorations = font->decorations;

    if (decorations & NS_FONT_DECORATION_OVERLINE){
      nscoord offset;
      nscoord size;
      mFontMetrics->GetUnderline(offset, size);
      FillRect(aX, aY, aWidth, size);
    }
  }

#endif

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawString2(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing)
{
PRInt32         x = aX;
PRInt32         y = aY;
nsIFontMetrics  *fMetrics;

  nsCOMPtr<nsIAtom> langGroup = nsnull;
  ((nsFontMetricsPS*)mFontMetrics)->GetLangGroup(getter_AddRefs(langGroup));
  mPSObj->setlanggroup(langGroup.get());

  /* build up conversion table */
  mPSObj->preshow(aString, aLength);

  SetupFontAndColor();

  if (nsnull != aSpacing){
    // Slow, but accurate rendering
    const PRUnichar* end = aString + aLength;
    while (aString < end){
      x = aX;
      y = aY;
      mTranMatrix->TransformCoord(&x, &y);
	    PostscriptTextOut((PRUnichar *)aString, 1, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aFontID, aSpacing, PR_TRUE);
      aX += *aSpacing++;
      aString++;
    }
  } else {
    mTranMatrix->TransformCoord(&x, &y);
	  PostscriptTextOut(aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aFontID, aSpacing, PR_TRUE);
  }

  fMetrics = mFontMetrics;

  if (nsnull != fMetrics){
    const nsFont *font;
    fMetrics->GetFont(font);
    PRUint8 decorations = font->decorations;

    if (decorations & NS_FONT_DECORATION_OVERLINE){
      nscoord offset;
      nscoord size;
      fMetrics->GetUnderline(offset, size);
      //FillRect(aX, aY, aWidth, size);
    }
  }
  return NS_OK;
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

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
nscoord width, height;

  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage, aX, aY, width, height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
nsRect  tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage, tr);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
nsRect	sr,dr;

	sr = aSRect;
	mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
  sr.x = aSRect.x;
  sr.y = aSRect.y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  dr = aDRect;
	mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);


  mPSObj->colorimage(aImage,NS_PIXELS_TO_POINTS(sr.x),
              NS_PIXELS_TO_POINTS(sr.y),
              NS_PIXELS_TO_POINTS(dr.width),
              NS_PIXELS_TO_POINTS(dr.height));
  //return aImage->Draw(*this, mSurface, 0, 0, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
nsRect	tr;

	tr = aRect;
	mTranMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);

  //return aImage->Draw(*this, mSurface, tr.x, tr.y, tr.width, tr.height);
  mPSObj->colorimage(aImage,
                        NS_PIXELS_TO_POINTS(tr.x),
                        NS_PIXELS_TO_POINTS(tr.y), 
                        NS_PIXELS_TO_POINTS(tr.width),
                        NS_PIXELS_TO_POINTS(tr.height));
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS::DrawTile(nsIImage *aImage,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                                                    nscoord aWidth,nscoord aHeight)
{

  return NS_OK;
}


#ifdef USE_IMG2

#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

/* [noscript] void drawImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsPoint aDestPoint); */
NS_IMETHODIMP nsRenderingContextPS::DrawImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  nsPoint pt;
  nsRect sr;

  pt = *aDestPoint;
  mTranMatrix->TransformCoord(&pt.x, &pt.y);

  sr = *aSrcRect;
#if 0
  // need to do this if we fix the comments below
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  sr.x = aSrcRect->x;
  sr.y = aSrcRect->y;
#endif
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  // doesn't it seem like we should use more of the params here?  see comments in bug 76993
  // img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height,
  //           pt.x + sr.x, pt.y + sr.y, sr.width, sr.height);
  mPSObj->colorimage(img,
                     NS_PIXELS_TO_POINTS(pt.x),
                     NS_PIXELS_TO_POINTS(pt.y), 
                     NS_PIXELS_TO_POINTS(sr.width),
                     NS_PIXELS_TO_POINTS(sr.height));

  return NS_OK;
}

/* [noscript] void drawScaledImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsRect aDestRect); */
NS_IMETHODIMP nsRenderingContextPS::DrawScaledImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  nsRect dr;
  nsRect sr;

  dr = *aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

#if 0
  // need to do this if we fix the comments below
  sr = *aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  sr.x = aSrcRect->x;
  sr.y = aSrcRect->y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);
#endif

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  // doesn't it seem like we should use more of the params here?  see comments in bug 76993
  // img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
  mPSObj->colorimage(img,
                     NS_PIXELS_TO_POINTS(dr.x),
                     NS_PIXELS_TO_POINTS(dr.y), 
                     NS_PIXELS_TO_POINTS(dr.width),
                     NS_PIXELS_TO_POINTS(dr.height));

  return NS_OK;
}

#endif

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
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
void 
nsRenderingContextPS :: SetupFontAndColor(void)
{
nscoord         fontHeight = 0;
PRInt16         fontIndex;
const nsFont    *font;
nsFontHandle    fontHandle; 
nsAutoString    fontFamily;

  mFontMetrics->GetHeight(fontHeight);
  mFontMetrics->GetFont(font);
  mFontMetrics->GetFontHandle(fontHandle);

  //mStates->mFont = mCurrFont = tfont;
  mStates->mFontMetrics = mFontMetrics;

  // get the fontfamily we are using, not what we want, but what we are using
  fontFamily.AssignWithConversion(((nsFontMetricsPS*)mFontMetrics)->mAFMInfo->mPSFontInfo->mFamilyName);
  fontIndex = ((nsFontMetricsPS*)mFontMetrics)->GetFontIndex();

  mPSObj->setscriptfont(fontIndex,fontFamily,fontHeight,font->style,font->variant,font->weight,font->decorations);
}

/** ---------------------------------------------------
 *  See documentation in nsRenderingContextPS.h
 *	@update 3/24/2000 yueheng.xu@intel.com
 */
void 
nsRenderingContextPS :: PostscriptTextOut(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing, PRBool aIsUnicode)
{
nscoord         fontHeight = 0;
const nsFont    *font;

  mFontMetrics->GetHeight(fontHeight);
  mFontMetrics->GetFont(font);

  mPSObj->moveto(aX, aY);
  if (PR_TRUE != aIsUnicode) {
	  mPSObj->show(aString, aLength, "");	
  }
}


/** ---------------------------------------------------
 *  See documentation in nsRenderingContextPS.h
 *	@update 3/24/2000 yueheng.xu@intel.com
 */
void 
nsRenderingContextPS :: PostscriptTextOut(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing, PRBool aIsUnicode)
{
nscoord         fontHeight = 0;
const nsFont    *font;

  mFontMetrics->GetHeight(fontHeight);
  mFontMetrics->GetFont(font);

  mPSObj->moveto(aX, aY);
  if (PR_TRUE == aIsUnicode) {  
    mPSObj->show(aString, aLength, "");
  }
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

#endif


#ifdef DC
NS_IMETHODIMP
nsRenderingContextPS::GetColor(nsString& aColor)
{
  char cbuf[40];
  PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
              NS_GET_R(mCurrentColor),
              NS_GET_G(mCurrentColor),
              NS_GET_B(mCurrentColor));
  aColor = cbuf;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPS::SetColor(const nsString& aColor)
{
  nscolor rgb;
  if (NS_ColorNameToRGB(aColor, &rgb)) {
    SetColor(rgb);
  }
  else if (NS_HexToRGB(aColor, &rgb)) {
    SetColor(rgb);
  }
  return NS_OK;
}
#endif
