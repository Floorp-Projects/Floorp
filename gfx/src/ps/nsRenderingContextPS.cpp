/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "nsRenderingContextPS.h"
#include <math.h>
#include "libimg.h"
#include "nsDeviceContextPS.h"
#include "nsIScriptGlobalObject.h"
//#include "prprf.h"
//#include "nsPrintManager.h"
#include "nsPostScriptObj.h"        

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
  mTextColor = RGB(0, 0, 0);
  mLineStyle = nsLineStyle_kSolid;
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

  mTMatrix = nsnull;
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
  mPSObj = ((nsDeviceContextPS*)mContext)->GetPrintContext();
  NS_IF_ADDREF(mContext);

  // initialize the matrix
  mContext->GetAppUnitsToDevUnits(app2dev);
	mTMatrix->AddScale(app2dev, app2dev);
  mContext->GetDevUnitsToAppUnits(mP2T);
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

  mTMatrix = &mStates->mMatrix;

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
      mTMatrix = &mStates->mMatrix;
      SetLineStyle(mStates->mLineStyle);
    }
    else
      mTMatrix = nsnull;
  }

  aClipEmpty = retval;

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
int     cliptype;

  mStates->mLocalClip = aRect;

	mTMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);
  mStates->mFlags |= FLAG_LOCAL_CLIP_VALID;

  // how we combine the new rect with the previous?
  if (aCombine == nsClipCombine_kIntersect){
    // push the clipstate onto the postscript stack
    mPSObj->graphics_save();
  } else if (aCombine == nsClipCombine_kUnion){
    mPSObj->graphics_save();
    mPSObj->newpath();
    mPSObj->moveto( NS_PIXELS_TO_POINTS(trect.x), NS_PIXELS_TO_POINTS(trect.y));
    mPSObj->box(NS_PIXELS_TO_POINTS(trect.width), NS_PIXELS_TO_POINTS(trect.height));
    mPSObj->closepath();
    mPSObj->clip();
  }else if (aCombine == nsClipCombine_kSubtract){
    mPSObj->graphics_save();
    mPSObj->newpath();
    mPSObj->moveto(NS_PIXELS_TO_POINTS(trect.x), NS_PIXELS_TO_POINTS(trect.y));
    mPSObj->box_subtract(NS_PIXELS_TO_POINTS(trect.width), NS_PIXELS_TO_POINTS(trect.height));
    mPSObj->closepath();
    mPSObj->clip();
  }else if (aCombine == nsClipCombine_kReplace){
    mPSObj->graphics_save();
    // set the cliping rectangle to this
    mPSObj->initclip();
    mPSObj->newpath();
    mPSObj->moveto(NS_PIXELS_TO_POINTS(trect.x), NS_PIXELS_TO_POINTS(trect.y));
    mPSObj->box(NS_PIXELS_TO_POINTS(trect.width), NS_PIXELS_TO_POINTS(trect.height));
    mPSObj->closepath();
    mPSObj->clip();
  }else{
    NS_ASSERTION(FALSE, "illegal clip combination");
  }

  if (cliptype == NULLREGION)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  if (mStates->mFlags & FLAG_LOCAL_CLIP_VALID){
    aRect = mStates->mLocalClip;
    aClipValid = PR_TRUE;
  }else{
    aClipValid = PR_FALSE;
  }

  return NS_OK;}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{


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
  mContext->GetMetricsFor(aFont, mFontMetrics);
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
	mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: Scale(float aSx, float aSy)
{
	mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTMatrix;
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

	mTMatrix->TransformCoord(&aX0,&aY0);
	mTMatrix->TransformCoord(&aX1,&aY1);

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
POINT           pp;

  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  np = &aPoints[0];

	pp.x = np->x;
	pp.y = np->y;
  mTMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
  mPSObj->moveto_loc(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
  np++;

  // we are ignoring the linestyle
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
		mTMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
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
	mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

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

	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
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
	mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

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

	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  mPSObj->newpath();
  mPSObj->moveto(NS_PIXELS_TO_POINTS(aX), NS_PIXELS_TO_POINTS(aY));
  mPSObj->box(NS_PIXELS_TO_POINTS(aWidth), NS_PIXELS_TO_POINTS(aHeight));
  mPSObj->closepath();
  mPSObj->fill();
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
POINT           pp;

  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  np = &aPoints[0];

	pp.x = np->x;
	pp.y = np->y;
  mTMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
  mPSObj->moveto_loc(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
  np++;

  // we are ignoring the linestyle
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
		mTMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
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
POINT           pp;

  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  np = &aPoints[0];

	pp.x = np->x;
	pp.y = np->y;
  mTMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
  mPSObj->moveto_loc(NS_PIXELS_TO_POINTS(pp.x),NS_PIXELS_TO_POINTS(pp.y));
  np++;

  // we are ignoring the linestyle
	for (PRInt32 i = 1; i < aNumPoints; i++, np++){
		pp.x = np->x;
		pp.y = np->y;
		mTMatrix->TransformCoord((int*)&pp.x,(int*)&pp.y);
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

  mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupPen();

  //HBRUSH oldBrush = (HBRUSH)::SelectObject(mDC, ::GetStockObject(NULL_BRUSH));
  
  //::Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);
  //::SelectObject(mDC, oldBrush);

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
  mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupSolidPen();
  //SetupSolidBrush();
  
  //:Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);

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
  //if (nsLineStyle_kNone == mCurrLineStyle)
    //return NS_OK;

  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  //mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupPen();
  //SetupSolidBrush();

  // figure out the the coordinates of the arc from the angle
  distance = (float)sqrt((float)(aWidth * aWidth + aHeight * aHeight));
  cx = aX + aWidth / 2;
  cy = aY + aHeight / 2;

  anglerad = (float)(aStartAngle / (180.0 / 3.14159265358979323846));
  quad1 = (PRInt32)(aStartAngle / 90.0);
  sx = (PRInt32)(distance * cos(anglerad) + cx);
  sy = (PRInt32)(cy - distance * sin(anglerad));

  anglerad = (float)(aEndAngle / (180.0 / 3.14159265358979323846));
  quad2 = (PRInt32)(aEndAngle / 90.0);
  ex = (PRInt32)(distance * cos(anglerad) + cx);
  ey = (PRInt32)(cy - distance * sin(anglerad));

  // this just makes it consitent, on windows 95 arc will always draw CC, nt this sets direction
  //::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  //::Arc(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

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
  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  //mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupSolidPen();
  //SetupSolidBrush();

  // figure out the the coordinates of the arc from the angle
  distance = (float)sqrt((float)(aWidth * aWidth + aHeight * aHeight));
  cx = aX + aWidth / 2;
  cy = aY + aHeight / 2;

  anglerad = (float)(aStartAngle / (180.0 / 3.14159265358979323846));
  quad1 = (PRInt32)(aStartAngle / 90.0);
  sx = (PRInt32)(distance * cos(anglerad) + cx);
  sy = (PRInt32)(cy - distance * sin(anglerad));

  anglerad = (float)(aEndAngle / (180.0 / 3.14159265358979323846));
  quad2 = (PRInt32)(aEndAngle / 90.0);
  ex = (PRInt32)(distance * cos(anglerad) + cx);
  ey = (PRInt32)(cy - distance * sin(anglerad));

  // this just makes it consistent, on windows 95 arc will always draw CC,
  // on NT this sets direction
  //::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  //::Pie(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

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
    SIZE  size;

    SetupFontAndColor();
    aWidth = 12;
   
    // XXX WINDOWS ONLY
    aWidth = ::GetTextExtentPoint32(((nsDeviceContextPS*)mContext)->mDC , aString, aLength, &size);
    aWidth = NSToCoordRound(float(size.cx) * mP2T);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPS :: GetWidth(const PRUnichar *aString,PRUint32 aLength,nscoord &aWidth, PRInt32 *aFontID)
{
  if (nsnull != mFontMetrics){
    SIZE  size;

    SetupFontAndColor();
    aWidth = 12;

    // XXX WINDOWS ONLY
    aWidth = ::GetTextExtentPoint32W( ((nsDeviceContextPS*)mContext)->mDC, aString, aLength, &size);
    aWidth = NSToCoordRound(float(size.cx) * mP2T);

    if (nsnull != aFontID)
      *aFontID = 0;

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
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
PRInt32	      x = aX;
PRInt32       y = aY;

  SetupFontAndColor();

  INT dxMem[500];
  INT* dx0;
  if (nsnull != aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new INT[aLength];
    }
    mTMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }

	mTMatrix->TransformCoord(&x, &y);
  PostscriptTextOut(aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aLength, aSpacing ? dx0 : NULL, FALSE);

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
nsRenderingContextPS :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing)
{
PRInt32         x = aX;
PRInt32         y = aY;
nsIFontMetrics  *fMetrics;

  SetupFontAndColor();

  if (nsnull != aSpacing){
    // Slow, but accurate rendering
    const PRUnichar* end = aString + aLength;
    while (aString < end){
      x = aX;
      y = aY;
      mTMatrix->TransformCoord(&x, &y);
	    PostscriptTextOut((const char *)aString, 1, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aFontID, aSpacing, PR_TRUE);
      aX += *aSpacing++;
      aString++;
    }
  } else {
    mTMatrix->TransformCoord(&x, &y);
	  PostscriptTextOut((const char *)aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aFontID, aSpacing, PR_TRUE);
  }

  fMetrics = mFontMetrics;

  if (nsnull != fMetrics){
    nsFont *font;
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
  return DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aFontID, aSpacing);
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
	mTMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  dr = aDRect;
	mTMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  //return aImage->Draw(*this, mSurface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);

  // we have to output an image using the postscript image operator



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
	mTMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);

  //return aImage->Draw(*this, mSurface, tr.x, tr.y, tr.width, tr.height);
  return NS_OK;
}

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


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 12/21/98 dwc
 */
void 
nsRenderingContextPS :: SetupFontAndColor(void)
{
nscoord         fontHeight = 0;
nsFont          *font;
nsFontHandle    fontHandle;       // WINDOWS ONLY


  mFontMetrics->GetHeight(fontHeight);
  mFontMetrics->GetFont(font);


  mFontMetrics->GetFontHandle(fontHandle);

  HFONT         tfont = (HFONT)fontHandle;                      // WINDOWS ONLY
  ::SelectObject(((nsDeviceContextPS*)mContext)->mDC, tfont);    // WINDOWS ONLY

  //mStates->mFont = mCurrFont = tfont;
  mStates->mFontMetrics = mFontMetrics;

  mPSObj->setscriptfont(fontHeight,font->style,font->variant,font->weight,font->decorations);
}


/** ---------------------------------------------------
 *  See documentation in nsRenderingContextPS.h
 *	@update 12/21/98 dwc
 */
void 
nsRenderingContextPS :: PostscriptTextOut(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing, PRBool aIsUnicode)
{
int             ptr = 0;
unsigned int    i;
char            *buf = 0;
nscoord         fontHeight = 0,yCoord;
nsFont          *font;

  mFontMetrics->GetHeight(fontHeight);
  mFontMetrics->GetFont(font);

  yCoord = aY + (fontHeight / 2);
  mPSObj->moveto(aX, yCoord);
  if (PR_TRUE == aIsUnicode) {
    //XXX: Investigate how to really do unicode with Postscript
	  // Just remove the extra byte per character and draw that instead
    buf = new char[aLength];

    for (i = 0; i < aLength; i++) {
      buf[i] = aString[ptr];
	    ptr+=2;
	  }
	mPSObj->show(buf, aLength, "");
	delete buf;
  } else {
    mPSObj->show((char *)aString, aLength, "");
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
  char cbuf[40];
  aColor.ToCString(cbuf, sizeof(cbuf));
  if (NS_ColorNameToRGB(cbuf, &rgb)) {
    SetColor(rgb);
  }
  else if (NS_HexToRGB(cbuf, &rgb)) {
    SetColor(rgb);
  }
  return NS_OK;
}
#endif