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
 
#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsFontMetricsMac.h"
#include "nsIRegion.h"
#include "nsIEnumerator.h"
#include "nsRegionMac.h"

#include "nsTransform2D.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"
#include "nsCOMPtr.h"
#include "plhash.h"

#include <FixMath.h>
#include <Gestalt.h>


typedef enum {
  kFontChanged = 1,
  kColorChanged = 2
} styleChanges;

typedef struct  {
	short f;
	short s;
	nscolor c;
	short bi;
} atsuiLayoutCacheKey;

class ATSUILayoutCache {
public:
	ATSUILayoutCache();
	~ATSUILayoutCache();
	PRBool Get(short font, short size, PRBool b, PRBool i, nscolor color, ATSUTextLayout *txlayout);
	void Set(short font, short size, PRBool b, PRBool i, nscolor color, ATSUTextLayout txlayout);
	PRBool Get(atsuiLayoutCacheKey *key, ATSUTextLayout *txlayout);
	void Set(atsuiLayoutCacheKey *key, ATSUTextLayout txlayout);
	
private:			
	struct PLHashTable* mTable;
	PRUint32 mCount;
};
static PR_CALLBACK PLHashNumber hashKey( const void *key)
{
	atsuiLayoutCacheKey* real = (atsuiLayoutCacheKey*)key;	
	return 	real->f + (real-> s << 7) + (real->bi << 12) + real->c;
}
static PR_CALLBACK PRIntn compareKeys(const void *v1, const void *v2)
{
	atsuiLayoutCacheKey *k1 = (atsuiLayoutCacheKey *)v1;
	atsuiLayoutCacheKey *k2 = (atsuiLayoutCacheKey *)v2;
	return (k1->f == k2->f) && (k1->c == k2->c ) && (k1->s == k2->s) && (k1->bi == k2->bi);
}
static PR_CALLBACK PRIntn compareValues(const void *v1, const void *v2)
{
	ATSUTextLayout t1 = (ATSUTextLayout)v1;
	ATSUTextLayout t2 = (ATSUTextLayout)v2;
	return t1 == t2;
}
static PR_CALLBACK PRIntn freeHashEntries(PLHashEntry *he, PRIntn i, void *arg)
{
	delete (atsuiLayoutCacheKey*) he->key;
	ATSUDisposeTextLayout( (ATSUTextLayout) he->value);
	return HT_ENUMERATE_REMOVE;
}
ATSUILayoutCache::ATSUILayoutCache()
{
	mTable = PL_NewHashTable(8 , (PLHashFunction) hashKey, 
								(PLHashComparator) compareKeys, 
								(PLHashComparator) compareValues,
							nsnull, nsnull);
	mCount = 0;
}
ATSUILayoutCache::~ATSUILayoutCache()
{
	if(mTable)
	{
		PL_HashTableEnumerateEntries(mTable, freeHashEntries, 0);
		PL_HashTableDestroy(mTable);
		mTable = nsnull;
	}
}
PRBool ATSUILayoutCache::Get(short font, short size, PRBool b, PRBool i, nscolor color, ATSUTextLayout *txlayout)
{
	atsuiLayoutCacheKey k = {font, size,color,  ( b ? 1 : 0 ) + ( i ? 2 : 0 )};
	return Get(&k, txlayout);
}

void ATSUILayoutCache::Set(short font, short size, PRBool b, PRBool i, nscolor color, ATSUTextLayout txlayout)
{
	atsuiLayoutCacheKey k = {font, size,color, ( b ? 1 : 0 ) + ( i ? 2 : 0 )};
	return Set(&k, txlayout);
}

PRBool ATSUILayoutCache::Get(atsuiLayoutCacheKey *key, ATSUTextLayout *txlayout)
{
	PLHashEntry **hep = PL_HashTableRawLookup(mTable, hashKey(key), key);
	PLHashEntry *he = *hep;
	if( he )
	{
		*txlayout = (ATSUTextLayout)he-> value;
		return PR_TRUE;
	}
	return PR_FALSE;
}

void ATSUILayoutCache::Set(atsuiLayoutCacheKey *key, ATSUTextLayout txlayout)
{
	atsuiLayoutCacheKey *newKey = new atsuiLayoutCacheKey;
	newKey->f = key->f; newKey->s = key->s; newKey->bi = key->bi; newKey->c = key->c;
	
	PL_HashTableAdd(mTable, newKey, txlayout);
	mCount ++;
}

static ATSUILayoutCache* gTxLayoutCache= nsnull;
#pragma mark -

#ifdef OLDDRAWINGSURFACE
//------------------------------------------------------------------------
//	GraphicState and DrawingSurface
//
//		Internal classes
//------------------------------------------------------------------------

class GraphicState
{
public:
  GraphicState();
  GraphicState(GraphicState* aGS);
  ~GraphicState();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

	void				Clear();
	void				Init(nsDrawingSurface aSurface);
	void				Init(GrafPtr aPort);
	void				Init(nsIWidget* aWindow);
	void				Duplicate(GraphicState* aGS);	// would you prefer an '=' operator?

protected:
	RgnHandle		DuplicateRgn(RgnHandle aRgn);

public:
  nsTransform2D * 			mTMatrix; 					// transform that all the graphics drawn here will obey

	PRInt32               mOffx;
  PRInt32               mOffy;

  RgnHandle							mMainRegion;
  RgnHandle			    		mClipRegion;

  nscolor               mColor;
  PRInt32               mFont;
  nsIFontMetrics * 			mFontMetrics;
	PRInt32               mCurrFontHandle;
};



class DrawingSurface
{
public:
	DrawingSurface();
	~DrawingSurface();

	void						Init(nsDrawingSurface aSurface);
	void						Init(GrafPtr aPort);
	void						Init(nsIWidget* aWindow);
	void						Clear();

	GrafPtr					GetPort()   		{return mPort;}
	GraphicState*		GetGS()     		{return mGS;}

protected:
	GrafPtr					mPort;
	GraphicState*		mGS;
};
#else

#define DrawingSurface	nsDrawingSurfaceMac


#endif


//------------------------------------------------------------------------

GraphicState::GraphicState()
{
	// everything is initialized to 0 through the 'new' operator
}


//------------------------------------------------------------------------

GraphicState::GraphicState(GraphicState* aGS)
{
	this->Duplicate(aGS);
}

//------------------------------------------------------------------------

GraphicState::~GraphicState()
{
	Clear();
}

//------------------------------------------------------------------------

void GraphicState::Clear()
{
	if (mTMatrix)
	{
		delete mTMatrix;
		mTMatrix = nsnull;
	}

	if (mMainRegion)
	{
		::DisposeRgn(mMainRegion);
		mMainRegion = nsnull;
	}

	if (mClipRegion)
	{
		::DisposeRgn(mClipRegion);
		mClipRegion = nsnull;
	}

  NS_IF_RELEASE(mFontMetrics);

  mOffx						= 0;
  mOffy						= 0;
  mColor 					= NS_RGB(255,255,255);
	mFont						= 0;
  mFontMetrics		= nsnull;
  mCurrFontHandle	= 0;
}

//------------------------------------------------------------------------

void GraphicState::Init(nsDrawingSurface aSurface)
{
	// retrieve the grafPort
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);
#ifdef OLDDRAWINGSURFACE
	GrafPtr port = surface->GetPort();
#else
	GrafPtr port;
	surface->GetGrafPtr(&port);
#endif

	// init from grafPort
	Init(port);
}

//------------------------------------------------------------------------

void GraphicState::Init(GrafPtr aPort)
{
	// delete old values
	Clear();

	// init from grafPort (usually an offscreen port)
	RgnHandle	rgn = ::NewRgn();
  ::RectRgn(rgn, &aPort->portRect);

  mMainRegion			= rgn;
  mClipRegion			= DuplicateRgn(rgn);

}

//------------------------------------------------------------------------

void GraphicState::Init(nsIWidget* aWindow)
{
	// delete old values
	Clear();

	// init from widget
  mOffx						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETX);
  mOffy						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETY);

	RgnHandle widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
	mMainRegion			= DuplicateRgn(widgetRgn);
  mClipRegion			= DuplicateRgn(widgetRgn);
}

//------------------------------------------------------------------------

void GraphicState::Duplicate(GraphicState* aGS)
{
	// delete old values
	Clear();

	// copy new ones
	if (aGS->mTMatrix)
		mTMatrix = new nsTransform2D(aGS->mTMatrix);
	else
		mTMatrix = nsnull;

	mOffx						= aGS->mOffx;
	mOffy						= aGS->mOffy;

	mMainRegion			= DuplicateRgn(aGS->mMainRegion);
	mClipRegion			= DuplicateRgn(aGS->mClipRegion);

	mColor					= aGS->mColor;
	mFont						= aGS->mFont;
	mFontMetrics		= aGS->mFontMetrics;
	NS_IF_ADDREF(mFontMetrics);

	mCurrFontHandle	= aGS->mCurrFontHandle;
}


//------------------------------------------------------------------------

RgnHandle GraphicState::DuplicateRgn(RgnHandle aRgn)
{
	RgnHandle dupRgn = nsnull;
	if (aRgn)
	{
		dupRgn = ::NewRgn();
		if (dupRgn)
			::CopyRgn(aRgn, dupRgn);
	}
	return dupRgn;
}


//------------------------------------------------------------------------

#pragma mark -


#ifdef OLDDRAWINGSURFACE
DrawingSurface::DrawingSurface()
{
	mPort = nsnull;
	mGS = new GraphicState();
}

DrawingSurface::~DrawingSurface()
{
	Clear();
}

void DrawingSurface::Clear()
{
	if (mGS)
	{
		delete mGS;
		mGS = nsnull;
	}
}

void DrawingSurface::Init(nsDrawingSurface aSurface)
{
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);
	mPort = surface->GetPort();
	mGS->Init(surface);
}

void DrawingSurface::Init(GrafPtr aPort)
{
  mPort = aPort;
  mGS->Init(aPort);
}

void DrawingSurface::Init(nsIWidget* aWindow)
{
  mPort = static_cast<GrafPtr>(aWindow->GetNativeData(NS_NATIVE_DISPLAY));
  mGS->Init(aWindow);
}
#endif


//------------------------------------------------------------------------

#pragma mark -

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

PRBool nsRenderingContextMac::gATSUI = PR_FALSE;
PRBool nsRenderingContextMac::gATSUI_Init = PR_FALSE;



nsRenderingContextMac::nsRenderingContextMac()
{
  NS_INIT_REFCNT();

  mP2T							= 1.0f;
  mContext					= nsnull ;

#ifdef OLDDRAWINGSURFACE
  mOriginalSurface	= new DrawingSurface();
  mFrontSurface			= new DrawingSurface();
#else
  mOriginalSurface	= new nsDrawingSurfaceMac();
  mFrontSurface			= new nsDrawingSurfaceMac();
#endif

	mCurrentSurface		= nsnull;
  mPort							= nsnull;
	mGS								= nsnull;

  mGSStack					= new nsVoidArray();
  long				version;
  if(! gATSUI_Init)
  {
  	gATSUI =  (::Gestalt(gestaltATSUVersion, &version) == noErr); // turn on ATSUI if it is available
  	gTxLayoutCache = new ATSUILayoutCache();
  	gATSUI_Init = PR_TRUE;
	// gATSUI = PR_FALSE; // force not using ATSUI
  }
  mChanges = kFontChanged | kColorChanged;
  mLastTextLayout = nsnull;
}


//------------------------------------------------------------------------

nsRenderingContextMac::~nsRenderingContextMac()
{
	// restore stuff
  NS_IF_RELEASE(mContext);
  if (mOriginalSurface)
  {
#ifdef OLDDRAWINGSURFACE
		::SetPort(mOriginalSurface->GetPort());
#else
		GrafPtr port;
		mOriginalSurface->GetGrafPtr(&port);
		::SetPort(port);
#endif
		::SetOrigin(0,0); 		//¥TODO? Setting to 0,0 may not really reset the state properly.
													// Maybe we should also restore the GS from mOriginalSurface.
	}

	// delete surfaces
	if (mOriginalSurface)
	{
		delete mOriginalSurface;
		mOriginalSurface = nsnull;
	}

	if (mFrontSurface)
	{
		delete mFrontSurface;
		mFrontSurface = nsnull;
	}

	mCurrentSurface = nsnull;
	mPort = nsnull;
	mGS = nsnull;

	// delete the stack and its contents
	if (mGSStack)
	{
	  PRInt32 cnt = mGSStack->Count();
	  for (PRInt32 i = 0; i < cnt; i ++)
	  {
	    GraphicState* gs = (GraphicState*)mGSStack->ElementAt(i);
    	if (gs)
    		delete gs;
	  }
	  delete mGSStack;
	  mGSStack = nsnull;
	}
}


NS_IMPL_QUERY_INTERFACE(nsRenderingContextMac, kRenderingContextIID);
NS_IMPL_ADDREF(nsRenderingContextMac);
NS_IMPL_RELEASE(nsRenderingContextMac);


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsIWidget* aWindow)
{
  if (nsnull == aWindow->GetNativeData(NS_NATIVE_WINDOW))
    return NS_ERROR_NOT_INITIALIZED;

  mContext = aContext;
  NS_IF_ADDREF(mContext);
 
#ifdef OLDDRAWINGSURFACE
	if (mOriginalSurface->GetPort() == nsnull)
		mOriginalSurface->Init(aWindow);
#else
		GrafPtr port;
		mOriginalSurface->GetGrafPtr(&port);
		if(port == nsnull)
			mOriginalSurface->Init(aWindow);
#endif

 	// select the surface
	mFrontSurface->Init(aWindow);
	SelectDrawingSurface(mFrontSurface);

	// clip out the children from the GS main region
	nsCOMPtr<nsIEnumerator> children ( dont_AddRef(aWindow->GetChildren()) );
  nsresult result = NS_OK;
	if (children)
	{
		RgnHandle myRgn = ::NewRgn();
		RgnHandle childRgn = ::NewRgn();
		::CopyRgn(mGS->mMainRegion, myRgn);

		children->First();
		do
		{
			nsCOMPtr<nsISupports> child;
			if ( NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(child))) )
			{	
				// thanks to sdr@camsoft.com for pointing out a memory leak here,
				// leading to the use of nsCOMPtr
				nsCOMPtr<nsIWidget> childWidget;
        childWidget = do_QueryInterface( child, &result);
				if ( childWidget ) {
					nsRect childRect;
					Rect macRect;
					childWidget->GetBounds(childRect);
					::SetRect(&macRect, childRect.x, childRect.y, childRect.x + childRect.width, childRect.y + childRect.height);
					::RectRgn(childRgn, &macRect);
					::DiffRgn(myRgn, childRgn, myRgn);
				}	
			}
		} while (NS_SUCCEEDED(children->Next()));			

		::CopyRgn(myRgn, mGS->mMainRegion);
		::CopyRgn(myRgn, mGS->mClipRegion);
		::DisposeRgn(myRgn);
		::DisposeRgn(childRgn);
	}

  return result;
}

//------------------------------------------------------------------------

// should only be called for an offscreen drawing surface, without an offset or clip region
NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);
   
#ifdef OLDDRAWINGSURFACE
	if (mOriginalSurface->GetPort() == nsnull)
		mOriginalSurface->Init(aSurface);
#else
		GrafPtr port;
		mOriginalSurface->GetGrafPtr(&port);
		if(port==nsnull)
			mOriginalSurface->Init(aSurface);
#endif

	// select the surface
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);
	SelectDrawingSurface(surface);

  return NS_OK;
}

//------------------------------------------------------------------------

// used by nsDeviceContextMac::CreateRenderingContext() for printing
nsresult nsRenderingContextMac::Init(nsIDeviceContext* aContext, GrafPtr aPort)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);
 
#ifdef OLDDRAWINGSURFACE
	if (mOriginalSurface->GetPort() == nsnull)
		mOriginalSurface->Init(aPort);
#else
		GrafPtr port;
		mOriginalSurface->GetGrafPtr(&port);
		if(port==nsnull)
			mOriginalSurface->Init(aPort);
#endif

 	// select the surface
	mFrontSurface->Init(aPort);
	SelectDrawingSurface(mFrontSurface);

  return NS_OK;
}

//------------------------------------------------------------------------

void	nsRenderingContextMac::SelectDrawingSurface(DrawingSurface* aSurface)
{
	if (! aSurface)
		return;

	mCurrentSurface = aSurface;
#ifdef OLDDRAWINGSURFACE
	mPort	= aSurface->GetPort();
	mGS		= aSurface->GetGS();
#else
		GrafPtr port;
		aSurface->GetGrafPtr(&mPort);
		mGS		= aSurface->GetGS();
#endif

	// quickdraw initialization
  ::SetPort(mPort);

  ::SetOrigin(-mGS->mOffx, -mGS->mOffy);		// line order...

	::SetClip(mGS->mClipRegion);							// ...does matter

	::PenNormal();
	::PenMode(patCopy);
	::TextMode(srcOr);

  this->SetColor(mGS->mColor);

	if (mGS->mFontMetrics)
		SetFont(mGS->mFontMetrics);
	
	if (!mContext) return;
	
	// GS and context initializations
  ((nsDeviceContextMac *)mContext)->SetDrawingSurface(mPort);
#if 0
	((nsDeviceContextMac *)mContext)->InstallColormap();
#endif

  mContext->GetDevUnitsToAppUnits(mP2T);

  if (!mGS->mTMatrix)
  {
 		mGS->mTMatrix = new nsTransform2D();
	  if (mGS->mTMatrix)
	  {
			// apply the new scaling
		  float app2dev;
		  mContext->GetAppUnitsToDevUnits(app2dev);
	  	mGS->mTMatrix->AddScale(app2dev, app2dev);
		}
	}
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetPortTextState()
{
	NS_PRECONDITION(mGS->mFontMetrics != nsnull, "No font metrics in SetPortTextState");
	
	if (nsnull == mGS->mFontMetrics)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(mContext != nsnull, "No device context in SetPortTextState");
	
	if (nsnull == mContext)
		return NS_ERROR_NULL_POINTER;

	TextStyle		theStyle;
	nsFontMetricsMac::GetNativeTextStyle(*mGS->mFontMetrics, *mContext, theStyle);

	::TextFont(theStyle.tsFont);
	::TextSize(theStyle.tsSize);
	::TextFace(theStyle.tsFace);

	return NS_OK;
}


#pragma mark -

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PushState(void)
{
	// create a GS
  GraphicState * gs = new GraphicState();

	// copy the current GS into it
	gs->Duplicate(mGS);

	// put the new GS at the end of the stack
  mGSStack->AppendElement(gs);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PopState(PRBool &aClipEmpty)
{
  PRUint32 cnt = mGSStack->Count();
  if (cnt > 0) 
  {
    // get the GS from the stack
    GraphicState* gs = (GraphicState *)mGSStack->ElementAt(cnt - 1);

		// copy the GS into the current one and tell the current surface to use it
		mGS->Duplicate(gs);
		SelectDrawingSurface(mCurrentSurface);

    // remove the GS object from the stack and delete it
    mGSStack->RemoveElementAt(cnt - 1);
    delete gs;
	}

  aClipEmpty = (::EmptyRgn(mGS->mClipRegion));

  return NS_OK;
}

#pragma mark -

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::UnlockDrawingSurface(void)
{
  return NS_OK;
}

//------------------------------------------------------------------------


NS_IMETHODIMP nsRenderingContextMac::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{  
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);

  if (surface != nsnull)
		SelectDrawingSurface(surface);				// select the offscreen surface...
  else
		SelectDrawingSurface(mFrontSurface);	// ...or get back to the window port

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetDrawingSurface(nsDrawingSurface *aSurface)
{  
  *aSurface = mCurrentSurface;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
	// hack: shortcut to bypass empty frames
	// or frames entirely recovered with other frames
	if ((aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) == 0)
		if (::EmptyRgn(mFrontSurface->GetGS()->mMainRegion))
			return NS_OK;

	// retrieve the surface
	DrawingSurface* srcSurface = static_cast<DrawingSurface*>(aSrcSurf);
#ifdef OLDDRAWINGSURFACE
	GrafPtr srcPort = srcSurface->GetPort();
#else
		GrafPtr srcPort;
		srcSurface->GetGrafPtr(&srcPort);
#endif

	// apply the selected transformations
  PRInt32	x = aSrcX;
  PRInt32	y = aSrcY;
  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mGS->mTMatrix->TransformCoord(&x, &y);

  nsRect dstRect = aDestBounds;
  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mGS->mTMatrix->TransformCoord(&dstRect.x, &dstRect.y, &dstRect.width, &dstRect.height);

	// get the source and destination rectangles
  Rect macSrcRect, macDstRect;
  ::SetRect(&macSrcRect,
  		x,
  		y,
  		dstRect.width,
  		dstRect.height);

  ::SetRect(&macDstRect, 
	    dstRect.x, 
	    dstRect.y, 
	    dstRect.x + dstRect.width, 
	    dstRect.y + dstRect.height);
  
	// get the source clip region
	RgnHandle clipRgn;
  if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
  	clipRgn = srcPort->clipRgn;
  else
  	clipRgn = nil;

	// get the destination port and surface
  GrafPtr destPort;
	DrawingSurface* destSurface;
  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
  {
    destSurface	= mCurrentSurface;
    destPort		= mPort;
    NS_ASSERTION((destPort != nsnull), "no back buffer");
  }
  else
  {
    destSurface	= mFrontSurface;
#ifdef OLDDRAWINGSURFACE
    destPort		= mFrontSurface->GetPort();
#else
		mFrontSurface->GetGrafPtr(&destPort);
#endif
	}

	// select the destination surface to set the colors
	DrawingSurface* saveSurface = nsnull;
	if (mCurrentSurface != destSurface)
	{
		saveSurface = mCurrentSurface;
		SelectDrawingSurface(destSurface);
	}

	// set the right colors for CopyBits
  RGBColor foreColor;
  Boolean changedForeColor = false;
  ::GetForeColor(&foreColor);
  if ((foreColor.red != 0x0000) || (foreColor.green != 0x0000) || (foreColor.blue != 0x0000))
  {
	  RGBColor rgbBlack = {0x0000,0x0000,0x0000};
		::RGBForeColor(&rgbBlack);
		changedForeColor = true;
	}

  RGBColor backColor;
  Boolean changedBackColor = false;
  ::GetBackColor(&backColor);
  if ((backColor.red != 0xFFFF) || (backColor.green != 0xFFFF) || (backColor.blue != 0xFFFF))
  {
	  RGBColor rgbWhite = {0xFFFF,0xFFFF,0xFFFF};
		::RGBBackColor(&rgbWhite);
		changedBackColor = true;
	}

	// copy the bits now
	::CopyBits(
		  &srcPort->portBits,
		  &destPort->portBits,
		  &macSrcRect,
		  &macDstRect,
		  srcCopy,
		  clipRgn);

	// restore colors and surface
	if (changedForeColor)
		::RGBForeColor(&foreColor);
	if (changedBackColor)
			::RGBBackColor(&backColor);

	if (saveSurface != nsnull)
		SelectDrawingSurface(saveSurface);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
#ifdef OLDDRAWINGSURFACE
	// get depth
  PRUint32 depth = 8;
  if (mContext)
  	mContext->GetDepth(depth);

	// get rect
  Rect macRect;
  if (aBounds != nsnull)
  {
  	// fyi, aBounds->x and aBounds->y are always 0 here
  	::SetRect(&macRect, aBounds->x, aBounds->y, aBounds->XMost(), aBounds->YMost());
  }
  else
  	::SetRect(&macRect, 0, 0, 2, 2);

	// create offscreen
  GWorldPtr offscreenGWorld;
  QDErr osErr = ::NewGWorld(&offscreenGWorld, depth, &macRect, nil, nil, 0);
  if (osErr != noErr)
  	return NS_ERROR_FAILURE;

	// keep the pixels locked... that's how it works on Windows and  we are forced to do
	// the same because the API doesn't give us any hook to do it at drawing time.
  ::LockPixels(::GetGWorldPixMap(offscreenGWorld));

	// erase the offscreen area
	GrafPtr savePort;
	::GetPort(&savePort);
	::SetPort((GrafPtr)offscreenGWorld);
	::EraseRect(&macRect);
	::SetPort(savePort);

	// return the offscreen surface
	DrawingSurface* surface = new DrawingSurface();
	surface->Init((GrafPtr)offscreenGWorld);
 
	aSurface = surface;
#else

  PRUint32 depth = 8;
  if (mContext)
  	mContext->GetDepth(depth);

	// get rect
  Rect macRect;
  if (aBounds != nsnull)
  {
  	// fyi, aBounds->x and aBounds->y are always 0 here
  	::SetRect(&macRect, aBounds->x, aBounds->y, aBounds->XMost(), aBounds->YMost());
  }
  else
  	::SetRect(&macRect, 0, 0, 2, 2);

	nsDrawingSurfaceMac* surface = new nsDrawingSurfaceMac();
	surface->Init(depth,macRect.right,macRect.bottom,aSurfFlags);
	aSurface = surface;	

#endif

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DestroyDrawingSurface(nsDrawingSurface aSurface)
{
	if (!aSurface)
  	return NS_ERROR_FAILURE;

	// if that surface is still the current one, select the front surface

	if (aSurface == mCurrentSurface)
		SelectDrawingSurface(mFrontSurface);

	// delete the offscreen
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);
#ifdef OLDDRAWINGSURFACE
  GWorldPtr offscreenGWorld = (GWorldPtr)surface->GetPort();
#else
		GWorldPtr offscreenGWorld;
		mOriginalSurface->GetGrafPtr(&(GrafPtr)offscreenGWorld);
#endif
	::UnlockPixels(::GetGWorldPixMap(offscreenGWorld));
	::DisposeGWorld(offscreenGWorld);

	// delete the surface
	delete surface;

  return NS_OK;
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  // QuickDraw is prefered over to ATSUI for drawing 7-bit text
  // (it's not 8-bit: the name of the constant is misleading)
  result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

  aResult = result;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: Reset(void)
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetClipRectInPixels(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
	Rect macRect;
	::SetRect(&macRect, aRect.x, aRect.y, aRect.x + aRect.width, aRect.y + aRect.height);

	RgnHandle rectRgn = ::NewRgn();
	::RectRgn(rectRgn, &macRect);

	RgnHandle clipRgn = mGS->mClipRegion;
	if (clipRgn == nsnull)
		clipRgn = ::NewRgn();

	switch (aCombine)
	{
	  case nsClipCombine_kIntersect:
	  	::SectRgn(clipRgn, rectRgn, clipRgn);
	  	break;

	  case nsClipCombine_kUnion:
	  	::UnionRgn(clipRgn, rectRgn, clipRgn);
	  	break;

	  case nsClipCombine_kSubtract:
	  	::DiffRgn(clipRgn, rectRgn, clipRgn);
	  	break;

	  case nsClipCombine_kReplace:
	  	::CopyRgn(rectRgn, clipRgn);
	  	break;
	}
	::DisposeRgn(rectRgn);

	StartDraw();
		::SetClip(clipRgn);
	EndDraw();

	mGS->mClipRegion = clipRgn;
	aClipEmpty = ::EmptyRgn(clipRgn);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect  trect = aRect;

  mGS->mTMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);

  return SetClipRectInPixels(trect, aCombine, aClipEmpty);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  Rect	cliprect;

  if (mGS->mClipRegion != nsnull) 
  {
  	cliprect = (**mGS->mClipRegion).rgnBBox;
    aRect.SetRect(cliprect.left, cliprect.top, cliprect.right-cliprect.left, cliprect.bottom-cliprect.top);
    aClipValid = PR_TRUE;
 	} 
 	else 
	{
    aRect.SetRect(0,0,0,0);
    aClipValid = PR_FALSE;
 	}

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
	RgnHandle regionH;
	aRegion.GetNativeRegion(regionH);

	RgnHandle clipRgn = mGS->mClipRegion;
	if (clipRgn == nsnull)
		clipRgn = ::NewRgn();

	switch (aCombine)
	{
	  case nsClipCombine_kIntersect:
	  	::SectRgn(clipRgn, regionH, clipRgn);
	  	break;

	  case nsClipCombine_kUnion:
	  	::UnionRgn(clipRgn, regionH, clipRgn);
	  	break;

	  case nsClipCombine_kSubtract:
	  	::DiffRgn(clipRgn, regionH, clipRgn);
	  	break;

	  case nsClipCombine_kReplace:
	  	::CopyRgn(regionH, clipRgn);
	  	break;
	}

	StartDraw();
		::SetClip(clipRgn);
	EndDraw();

	mGS->mClipRegion = clipRgn;
	aClipEmpty = ::EmptyRgn(clipRgn);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetClipRegion(nsIRegion **aRegion)
{
  nsresult  rv = NS_OK;

  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

  if (nsnull == *aRegion)
  {
    nsRegionMac *rgn = new nsRegionMac();

    if (nsnull != rgn)
    {
      NS_ADDREF(rgn);

      rv = rgn->Init();

      if (NS_OK != rv)
        NS_RELEASE(rgn);
      else
        *aRegion = rgn;
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (rv == NS_OK)
  {
		nsRegionMac** macRegion = (nsRegionMac**)aRegion;
		(*macRegion)->SetNativeRegion(mGS->mClipRegion);
  }

  return rv;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetColor(nscolor aColor)
{
	StartDraw();

	#define COLOR8TOCOLOR16(color8)	 ((color8 << 8) | color8)

  RGBColor	thecolor;
	thecolor.red = COLOR8TOCOLOR16(NS_GET_R(aColor));
	thecolor.green = COLOR8TOCOLOR16(NS_GET_G(aColor));
	thecolor.blue = COLOR8TOCOLOR16(NS_GET_B(aColor));
	::RGBForeColor(&thecolor);
  mGS->mColor = aColor ;
 
  mChanges |= kColorChanged;

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetColor(nscolor &aColor) const
{
  aColor = mGS->mColor;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetLineStyle(nsLineStyle aLineStyle)
{
	// note: the line style must be saved in the GraphicState like font, color, etc...
	NS_NOTYETIMPLEMENTED("nsRenderingContextMac::SetLineStyle");//¥TODO
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetLineStyle(nsLineStyle &aLineStyle)
{
	NS_NOTYETIMPLEMENTED("nsRenderingContextMac::GetLineStyle");//¥TODO
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetFont(const nsFont& aFont)
{
	NS_IF_RELEASE(mGS->mFontMetrics);

	if (mContext)
		mContext->GetMetricsFor(aFont, mGS->mFontMetrics);
  mChanges |= kFontChanged;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetFont(nsIFontMetrics *aFontMetrics)
{
	NS_IF_RELEASE(mGS->mFontMetrics);
	mGS->mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mGS->mFontMetrics);
  mChanges |= kFontChanged;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mGS->mFontMetrics);
  aFontMetrics = mGS->mFontMetrics;
  return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextMac :: Translate(nscoord aX, nscoord aY)
{
  mGS->mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextMac :: Scale(float aSx, float aSy)
{
  mGS->mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mGS->mTMatrix;
  return NS_OK;
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
	StartDraw();

  mGS->mTMatrix->TransformCoord(&aX0,&aY0);
  mGS->mTMatrix->TransformCoord(&aX1,&aY1);
	::MoveTo(aX0, aY0);
	::LineTo(aX1, aY1);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	NS_NOTYETIMPLEMENTED("nsRenderingContextMac::DrawPolyline");	//¥TODO
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawRect(const nsRect& aRect)
{
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StartDraw();
	
  nscoord x,y,w,h;
  Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
	::SetRect(&therect,x,y,x+w,y+h);
	::FrameRect(&therect);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillRect(const nsRect& aRect)
{
	return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StartDraw();

  nscoord  x,y,w,h;
  Rect     therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  // TODO - cps - must debug and fix this 
  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);

	::SetRect(&therect,x,y,x+w,y+h);
	::PaintRect(&therect);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	StartDraw();

  PRUint32   i;
  PolyHandle thepoly;
  PRInt32    x,y;

  thepoly = ::OpenPoly();
	
  x = aPoints[0].x;
  y = aPoints[0].y;
  mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
  ::MoveTo(x,y);

  for (i = 1; i < aNumPoints; i++)
  {
    x = aPoints[i].x;
    y = aPoints[i].y;
		
		mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	ClosePoly();
	
	::FramePoly(thepoly);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	StartDraw();

  PRUint32   i;
  PolyHandle thepoly;
  PRInt32    x,y;

  thepoly = ::OpenPoly();
	
  x = aPoints[0].x;
  y = aPoints[0].y;
  mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
  ::MoveTo(x,y);

  for (i = 1; i < aNumPoints; i++)
  {
    x = aPoints[i].x;
    y = aPoints[i].y;
		mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	::ClosePoly();
	
	::PaintPoly(thepoly);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StartDraw();

  nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,y+h);
  ::FrameOval(&therect);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillEllipse(const nsRect& aRect)
{
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StartDraw();

	nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,y+h);
  ::PaintOval(&therect);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
	StartDraw();

  nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  
  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,y+h);
  ::FrameArc(&therect,aStartAngle,aEndAngle);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
	StartDraw();

  nscoord x,y,w,h;
  Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  
  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,y+h);
  ::PaintArc(&therect,aStartAngle,aEndAngle);

	EndDraw();
  return NS_OK;
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(char ch, nscoord &aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth, aFontID);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const nsString& aString, nscoord &aWidth, PRInt32 *aFontID)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const char *aString, nscoord &aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsRenderingContextMac :: GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
	StartDraw();

	// set native font and attributes
	SetPortTextState();

//   below is a bad assert, aString is not guaranteed null terminated...
//    NS_ASSERTION(strlen(aString) >= aLength, "Getting width on garbage string");
  
	// measure text
	short textWidth = ::TextWidth(aString, 0, aLength);
	aWidth = NSToCoordRound(float(textWidth) * mP2T);

	EndDraw();
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: qdGetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID)
{
  // XXX Unicode Broken!!!
  // we should use TEC here to convert Unicode to different script run
  nsString nsStr;
  nsStr.SetString(aString, aLength);
  char* cStr = nsStr.ToNewCString();
  GetWidth(cStr, aLength, aWidth);
  delete[] cStr;
  if (nsnull != aFontID)
    *aFontID = 0;
  return NS_OK;
}

#define FloatToFixed(a)		((Fixed)((float)(a) * fixed1))

ATSUTextLayout nsRenderingContextMac::atsuiGetTextLayout()
{ 
	if( 0 != mChanges )
	{		
		ATSUTextLayout txLayout;
		OSStatus err;
		
		nsFont *font;
		nsFontHandle fontNum;
		mGS->mFontMetrics->GetFont(font);
		mGS->mFontMetrics->GetFontHandle(fontNum);
		PRBool aBold = font->weight > NS_FONT_WEIGHT_NORMAL;
		PRBool aItalic = (NS_FONT_STYLE_ITALIC ==  font->style) || (NS_FONT_STYLE_OBLIQUE ==  font->style);		
		
		if(! gTxLayoutCache->Get((short)fontNum, font->size, aBold, aItalic, mGS->mColor,  &txLayout) )
		{
			UniChar dmy[1];
			err = ATSUCreateTextLayoutWithTextPtr (dmy, 0,0,0,0,NULL, NULL, &txLayout);

	 		NS_ASSERTION(noErr == err, "ATSUCreateTextLayoutWithTextPtr failed");
	 	
			ATSUStyle				theStyle;
			err = ATSUCreateStyle(&theStyle);
	 		NS_ASSERTION(noErr == err, "ATSUCreateStyle failed");

			ATSUAttributeTag 		theTag[3];
			ByteCount				theValueSize[3];
			ATSUAttributeValuePtr 	theValue[3];

	 		//--- Font ID & Face -----		
			ATSUFontID atsuFontID;
			
			err = ATSUFONDtoFontID((short)fontNum, (StyleField)((aBold ? bold : normal) | (aItalic ? italic : normal)), &atsuFontID);
	 		NS_ASSERTION(noErr == err, "ATSUFONDtoFontID failed");

			theTag[0] = kATSUFontTag;
			theValueSize[0] = (ByteCount) sizeof(ATSUFontID);
			theValue[0] = (ATSUAttributeValuePtr) &atsuFontID;
	 		//--- Font ID & Face  -----		
	 		
	 		//--- Size -----		
	 		float  dev2app;
	 		short fontsize = font->size;

			mContext->GetDevUnitsToAppUnits(dev2app);
	 		Fixed size = FloatToFixed( roundf(float(fontsize) / dev2app));
	 		if( FixRound ( size ) < 9 )
	 			size = X2Fix(9);

			theTag[1] = kATSUSizeTag;
			theValueSize[1] = (ByteCount) sizeof(Fixed);
			theValue[1] = (ATSUAttributeValuePtr) &size;
	 		//--- Size -----		
	 		
	 		//--- Color -----		
	 		RGBColor color;
	 		
			color.red = COLOR8TOCOLOR16(NS_GET_R(mGS->mColor));
			color.green = COLOR8TOCOLOR16(NS_GET_G(mGS->mColor));
			color.blue = COLOR8TOCOLOR16(NS_GET_B(mGS->mColor));				
			theTag[2] = kATSUColorTag;
			theValueSize[2] = (ByteCount) sizeof(RGBColor);
			theValue[2] = (ATSUAttributeValuePtr) &color;
	 		//--- Color -----		

			err =  ATSUSetAttributes(theStyle, 3, theTag, theValueSize, theValue);
	 		NS_ASSERTION(noErr == err, "ATSUSetAttributes failed");
			 	
	 		err = ATSUSetRunStyle(txLayout, theStyle, kATSUFromTextBeginning, kATSUToTextEnd);
	 		NS_ASSERTION(noErr == err, "ATSUSetRunStyle failed");
		
		
			err = ATSUSetTransientFontMatching(txLayout, true);
	 		NS_ASSERTION(noErr == err, "ATSUSetTransientFontMatching failed");
	 		
	 		gTxLayoutCache->Set((short)fontNum, font->size, aBold, aItalic, mGS->mColor,  txLayout);	
		} 		
		mLastTextLayout = txLayout;
		mChanges = 0;
	}
	return mLastTextLayout;
}

NS_IMETHODIMP nsRenderingContextMac :: atsuiGetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID)
{
  OSStatus err = noErr;
  StartDraw();

	// set native font and attributes
  SetPortTextState();
  
  ATSUTextLayout aTxtLayout = atsuiGetTextLayout();
  ATSUTextMeasurement iAfter; 
  err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString, 0, aLength, aLength);
  NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");
  err = ATSUMeasureText( aTxtLayout, 0, aLength, NULL, &iAfter, NULL, NULL );
  NS_ASSERTION(noErr == err, "ATSUMeasureText failed");
  
  aWidth = NSToCoordRound(Fix2Long(FixMul(iAfter , X2Fix(mP2T))));      
  EndDraw();

  return NS_OK;
}
PRBool convertToMacRoman(const PRUnichar *aString, PRUint32 aLength, char* macroman, PRBool onlyAllowASCII);

PRBool convertToMacRoman(const PRUnichar *aString, PRUint32 aLength, char* macroman, PRBool onlyAllowASCII)
{

	static char map[0x80] = {
0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,
0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,  0x00, 0x00,
0xCA, 0xC1,  0xA2, 0xA3,  0x00, 0xB4,  0x00, 0xA4,  0xAC, 0xA9,  0xBB, 0xC7,  0xC2, 0xD0,  0xA8, 0xF8,
0xA1, 0xB1,  0x00, 0x00,  0xAB, 0xB5,  0xA6, 0xE1,  0xFC, 0x00,  0xBC, 0xC8,  0x00, 0x00,  0x00, 0xC0,
0xCB, 0xE7,  0xE5, 0xCC,  0x80, 0x81,  0xAE, 0x82,  0xE9, 0x83,  0xE6, 0xE8,  0xED, 0xEA,  0xEB, 0xEC,
0x00, 0x84,  0xF1, 0xEE,  0xEF, 0xCD,  0x85, 0x78,  0xAF, 0xF4,  0xF2, 0xF3,  0x86, 0x00,  0x00, 0xA7,
0x88, 0x87,  0x89, 0x8B,  0x8A, 0x8C,  0xBE, 0x8D,  0x8F, 0x8E,  0x90, 0x91,  0x93, 0x92,  0x94, 0x95,
0x00, 0x96,  0x98, 0x97,  0x99, 0x9B,  0x9A, 0xD6,  0xBF, 0x9D,  0x9C, 0x9E,  0x9F, 0x00,  0x00, 0xD8
	
	};
	static char g2010map[0x40] = {
0x00,0x00,0x00,0xd0,0xd1,0x00,0x00,0x00,0xd4,0xd5,0xe2,0x00,0xd2,0xd3,0xe3,0x00,  
0xa0,0xe0,0xa5,0x00,0x00,0x00,0xc9,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  
0xe4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xdc,0xdd,0x00,0x00,0x00,0x00,0x00,  
0x00,0x00,0x05,0x00,0xda,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  
    };
	const PRUnichar *u = aString;
	char *c = macroman;
	if(onlyAllowASCII)
	{
		for(PRUint32 i = 0 ; i < aLength ; i++) {
			if( (*u) & 0xFF80 )
				return PR_FALSE;
			*c++ = *u++;
		}
	} else {
	    // XXX we should clean up these by calling the Unicode Converter after cata add x-mac-roman converters there.
		for(PRUint32 i = 0 ; i < aLength ; i++) {
			if( (*u) & 0xFF80 ) {
				char ch;
				switch( (*u) & 0xFF00 ) 
				{
				  case 0x0000:
				  	ch = map[(*u) & 0x007F];
					break;
				  case 0x2000:
				    if( 0x2202 == *u) {
				       ch = 0xDB;
					} else {
					  if( (*u < 0x2013)  || ( *u > 0x2044))
				        return PR_FALSE;
				      ch = g2010map[*u - 0x2010];
				      if( 0 == ch)
				        return PR_FALSE;
				    }
				    break;
				  case 0x2100:
				    if( 0x2122 == *u) ch = 0xAA;
				    else if( 0x2126 == *u) ch = 0xBD;
				    else return PR_FALSE;
				    break;
				  case 0x2200:
				  	switch(*u) {
					    case 0x2202: ch = 0xB6; break;
					    case 0x2206: ch = 0xBD; break;
					    case 0x220F: ch = 0xC6; break;
					    case 0x2211: ch = 0xB8; break;
					    case 0x221A: ch = 0xB7; break;
					    case 0x221E: ch = 0xC3; break;
					    case 0x222B: ch = 0xBA; break;
					    case 0x2248: ch = 0xC5; break;
					    case 0x2260: ch = 0xAD; break;
					    case 0x2264: ch = 0xB2; break;
					    case 0x2265: ch = 0xB3; break;
				  		default: return PR_FALSE;
				  	};

				    break;
				  case 0x2500:
				    if( 0x25CA == *u) ch = 0xD7;
				    else return PR_FALSE;
				    break;
				  case 0xF800:
				    if( 0xF8FF == *u) ch = 0xF0;
				    else return PR_FALSE;
				    break;
				  case 0xFB00:
				    if( 0xFB01 == *u) ch = 0xDE;
				    else if( 0xFB02 == *u) ch = 0xDF;
				    else return PR_FALSE;
				    break;
				  default:
				    return PR_FALSE;
				}
				u++;
				if(ch == 0)
					return PR_FALSE;				
				*c++ = ch;
			} else {
				*c++ = *u++;
			}
		}
	}
	return PR_TRUE;
}

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID)
{
	PRBool isMacRomanFont = PR_TRUE; // XXX we need to set up this value latter.
	nsresult res = NS_OK;
	if(aLength< 500)
	{
		char buf[500];
		if(convertToMacRoman(aString, aLength, buf, ! isMacRomanFont))
		{
			res = GetWidth(buf, aLength, aWidth);
			return res;
		}
	} else {
        char* buf = new char[aLength];
		PRBool useQD = convertToMacRoman(aString, aLength, buf, ! isMacRomanFont);
		if(useQD)
			res = GetWidth(buf, aLength, aWidth);
		delete [] buf;
		if(useQD)
			return res;
	}

	if(gATSUI) {
		return atsuiGetWidth(aString, aLength, aWidth, aFontID);	
	}
	else {
		return qdGetWidth(aString, aLength, aWidth, aFontID);
	}
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const char *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY,
                                         const nscoord* aSpacing)
{
	StartDraw();

	PRInt32 x = aX;
	PRInt32 y = aY;
	
  if (mGS->mFontMetrics)
  {
		// set native font and attributes
		SetPortTextState();

		// substract ascent since drawing specifies baseline
		nscoord ascent = 0;
		mGS->mFontMetrics->GetMaxAscent(ascent);
		y += ascent;
	}

  mGS->mTMatrix->TransformCoord(&x,&y);

	::MoveTo(x,y);
  if ( aSpacing == NULL )
		::DrawText(aString,0,aLength);
  else
  {
		int buffer[500];
		int* spacing = NULL;

		if (aLength > 500)
		  spacing = new int[aLength];
		else
		  spacing = buffer;
		
		mGS->mTMatrix->ScaleXCoords(aSpacing, aLength, spacing);
		PRInt32 currentX = x;
		for ( PRInt32 i = 0; i< aLength; i++ )
		{
			::DrawChar( aString[i] );
			currentX += spacing[ i ];
			::MoveTo( currentX, y );
		}
		// clean up and restore settings
		if ( (spacing != buffer))
			delete [] spacing; 
  }

  EndDraw();
  return NS_OK;
}



//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: qdDrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
	// XXX Unicode Broken!!!
    // we should use TEC here to convert Unicode to different script run
  	nsString nsStr;
	nsStr.SetString(aString, aLength);
	char* cStr = nsStr.ToNewCString();

	nsresult rv = DrawString(cStr, aLength, aX, aY, aSpacing);

	delete[] cStr;
	return rv;
}

NS_IMETHODIMP nsRenderingContextMac :: atsuiDrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
  OSStatus err = noErr;
  
  PRInt32 x = aX;
  PRInt32 y = aY;
  if (mGS->mFontMetrics)
  {
		// set native font and attributes
		SetPortTextState();

		// substract ascent since drawing specifies baseline
		nscoord ascent = 0;
		mGS->mFontMetrics->GetMaxAscent(ascent);
		y += ascent;
	}
  mGS->mTMatrix->TransformCoord(&x,&y);

  ATSUTextLayout aTxtLayout = atsuiGetTextLayout();
  if(NULL == aSpacing) 
  {
    err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString, 0, aLength, aLength);
    NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");
  	err = ATSUDrawText( aTxtLayout, 0, aLength, Long2Fix(x), Long2Fix(y));
    NS_ASSERTION(noErr == err, "ATSUMeasureText failed");
  }
  else
  {
    if(aLength < 500)
    {
     	int spacing[500];
	  	mGS->mTMatrix->ScaleXCoords(aSpacing, aLength, spacing);  	
	    for(PRInt32 i = 0; i < aLength; i++)
	    {
          err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString+i, 0, 1, 1);
          NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");
	      err = ATSUDrawText( aTxtLayout, 0, 1, Long2Fix(x), Long2Fix(y));
	      NS_ASSERTION(noErr == err, "ATSUMeasureText failed");  	
	      x += spacing[i];
	    }
	}
    else
    {
      	int *spacing = new int[aLength];
	    NS_ASSERTION(NULL != spacing, "memalloc failed");  	
	  	mGS->mTMatrix->ScaleXCoords(aSpacing, aLength, spacing);  	
	    for(PRInt32 i = 0; i < aLength; i++)
	    {
          err = ATSUSetTextPointerLocation( aTxtLayout, (ConstUniCharArrayPtr)aString+i, 0, 1, 1);
          NS_ASSERTION(noErr == err, "ATSUSetTextPointerLocation failed");
	      err = ATSUDrawText( aTxtLayout, 0, 1, Long2Fix(x), Long2Fix(y));
	      NS_ASSERTION(noErr == err, "ATSUMeasureText failed");  	
	      x += spacing[i];
	    }
      	delete spacing;
    }

  }
  return NS_OK;

}
NS_IMETHODIMP nsRenderingContextMac :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{

	// First, let's try to convert to MacRoman and draw by Quick Draw
		
	PRBool isMacRomanFont = PR_TRUE; // XXX we need to set up this value latter.
	nsresult res = NS_OK;
	if(aLength < 500)
	{
		char buf[500];
		if(convertToMacRoman(aString, aLength, buf, ! isMacRomanFont))
		{
			res = DrawString(buf, aLength, aX, aY, aSpacing);
			return res;
		}
	} else {
        char* buf = new char[aLength ];
		PRBool useQD = convertToMacRoman(aString, aLength, buf, ! isMacRomanFont);
		if(useQD)
			res = DrawString(buf, aLength, aX, aY, aSpacing);
		delete [] buf;
		if(useQD)
			return res;
	}

	// The data cannot be convert to MacRoman, let's draw by using ATSUI if available
	
	if(gATSUI) {
		return atsuiDrawString(aString, aLength, aX, aY, aFontID, aSpacing);	
	}
	else {
		return qdDrawString(aString, aLength, aX, aY, aFontID, aSpacing);
	}
	
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
 	nsresult rv = DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aFontID, aSpacing);
	return rv;
}




#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  nscoord width,height;

  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());
  
  return DrawImage(aImage,aX,aY,width,height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  nsRect	tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage,tr);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
	StartDraw();

  nsRect sr = aSRect;
  nsRect dr = aDRect;
  mGS->mTMatrix->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);
  mGS->mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);
  
  nsresult result =  aImage->Draw(*this,mPort,sr.x,sr.y,sr.width,sr.height,
                      dr.x,dr.y,dr.width,dr.height);

	EndDraw();
	return result;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
	StartDraw();

  nsRect tr = aRect;
  mGS->mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  
	nsresult result = aImage->Draw(*this,mPort,tr.x,tr.y,tr.width,tr.height);

	EndDraw();
	return result;
}
