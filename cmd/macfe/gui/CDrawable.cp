/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

//   drawable.cpp - Classes used for maintaining the notion of 
//                  offscreen and onscreen drawables, so that 
//                  document contents can be drawn to either
//                  location. Currently used for LAYERS.

//
// To Do:
//		1.	Allocate a GWorld that matches the depth of the screen the window is
//			currently being displayed on. This will involve cooperation from
//			CHTMLView.cp and could well be done through the Lock mechanism assuming
//			there's a way to get back at where the fe window is.
//
//		2.	Keep the GWorld around and mark the pixels purgeable. LockDrawable would
//			then call UpdateGWorld before it locked the pixels.
//
//		3.	Take out the parent stuff. It isn't useful anymore unless it ends up
//			being needed for item 1 above (which I don't think it will).
//
//		4.	Allocate a COffscreenDrawable per CHTMLView but then share the actual
//			GWorld between them all. That would allow the CHTMLView to update the
//			drawable whenever the scrolling origin changes. COffscreenDrawable can
//			then offset the clip when it gets set as opposed to doing it both in
//			SetLayerClip and FocusDraw (and CHTMLView wouldn't need to override
//			SetLayerClip anymore).
//

#ifdef LAYERS

#include "CDrawable.h"

COffscreenDrawable *COffscreenDrawable::mOffscreenDrawable = NULL;

// CDrawable base class
CDrawable::CDrawable()
{
	mParent = NULL;
	mGWorld = NULL;
	mRefCnt = 0;
	mOriginX = mOriginY = 0;
	mClipRgn = NewRgn();
	mClipChanged = false;
}

// CDrawable base class
CDrawable::~CDrawable() 
{
	DisposeRgn ( mClipRgn );
}

// Each user should call InitDrawable before using the drawable
// and RelinquishDrawable when it is done. This allows us to
// maintain a reference count of users.
void CDrawable::InitDrawable(
	CL_Drawable*			/* inCLDrawable */)
{
	mRefCnt++;
}
 
void CDrawable::RelinquishDrawable(
	CL_Drawable*			/* inCLDrawable */)
{
	Assert_(mRefCnt > 0);

	if (mRefCnt)
		{
		 mRefCnt--;
		}
}

// Set and get the origin of the drawable 
void CDrawable::SetLayerOrigin(
	int32					inX,
	int32					inY)
{
	mOriginX = inX;
	mOriginY = inY;
}

void CDrawable::GetLayerOrigin(
	int32*					outX,
	int32*					outY)
{
	*outX = mOriginX;
	*outY = mOriginY;
}

// Set the clip region of the drawable.
void CDrawable::SetLayerClip(
	FE_Region				inClipRgn)
{

	if ( inClipRgn != NULL )
		{
		CopyRgn ( FE_GetMDRegion(inClipRgn), mClipRgn );
		mClipChanged = true;

		// if we're the current port, make the change be immediate. I wish layers
		// would tell the drawable directly that it's being made active rather than
		// calling a global FE entry point
		if ( UQDGlobals::GetCurrentPort() == (GrafPtr) mGWorld )
			{
			mClipChanged = false;
			
			::OffsetRgn ( mClipRgn, -mGWorld->portRect.left, -mGWorld->portRect.top );
			
			::SetClip ( mClipRgn );

			::OffsetRgn ( mClipRgn, mGWorld->portRect.left, mGWorld->portRect.top );
			}
		}
}

Boolean CDrawable::HasClipChanged()
{
	Boolean changed;
	
	changed = mClipChanged;
	mClipChanged = false;
	
	return changed;
}

void CDrawable::CopyPixels(
	CDrawable*				/* inSrcDrawable */,
	FE_Region				/* inCopyRgn */)
{
}

void CDrawable::SetParent(
	CDrawable*				parent )
{
	mParent = parent;
}


COnscreenDrawable::COnscreenDrawable()
{
}


CRouterDrawable::CRouterDrawable()
{
}

void CRouterDrawable::SetLayerOrigin(
	int32					inX,
	int32					inY)
{
	if ( mParent != NULL )
		{
		mParent->SetLayerOrigin ( inX, inY );
		}
}

void CRouterDrawable::GetLayerOrigin(
	int32*					outX,
	int32*					outY)
{
	if ( mParent != NULL )
		{
		mParent->GetLayerOrigin ( outX, outY );
		}
}

void CRouterDrawable::SetLayerClip(
	FE_Region				inClipRgn)
{
	if ( mParent != NULL )
		{
		mParent->SetLayerClip ( inClipRgn );
		}
}

FE_Region CRouterDrawable::GetLayerClip()
{
	FE_Region clip;
	
	clip = mClipRgn;
	if ( mParent != NULL )
		{
		clip = mParent->GetLayerClip();
		}
	
	return clip;
}

Boolean CRouterDrawable::HasClipChanged()
{
	Boolean changed;
	
	changed = mClipChanged;
	if ( mParent != NULL )
		{
		changed = mParent->HasClipChanged();
		}
	
	return changed;
}

void CRouterDrawable::CopyPixels(
	CDrawable *				inSrcDrawable,
	FE_Region				hCopyRgn)
{
	if ( mParent != NULL )
		{
		mParent->CopyPixels ( inSrcDrawable, hCopyRgn );
		}
}

// For the current implementation, only a single offscreen drawable
// is created using this factory method.
COffscreenDrawable * COffscreenDrawable::AllocateOffscreen()
{
	if (mOffscreenDrawable == NULL)
		{
		mOffscreenDrawable = new COffscreenDrawable();
		}
	
	return mOffscreenDrawable;
}

COffscreenDrawable::COffscreenDrawable()
{
	mGWorld = NULL;
	mOwner = NULL;
	mWidth = mHeight = 0;
}

// Get rid of any offscreen constructs if they exist
void COffscreenDrawable::delete_offscreen()
{
	/* we should probably just mark ourselves purgable... */
	DisposeGWorld ( mGWorld );
	mGWorld = NULL;
	mWidth = mHeight = 0;
	mOwner = NULL;
}

COffscreenDrawable::~COffscreenDrawable()
{
	delete_offscreen();
}

void 
COffscreenDrawable::RelinquishDrawable(
	CL_Drawable*			inCLDrawable)
{
	CDrawable::RelinquishDrawable(inCLDrawable);

	// There are no clients left. Let's get rid of the offscreen
	// bitmap.
	if (mRefCnt == 0)
		{
		delete_offscreen();
		}
}

// Called before using the drawable.
PRBool COffscreenDrawable::LockDrawable(
	CL_Drawable*			inCLDrawable, 
	CL_DrawableState		inState)
{
	if (inState == CL_UNLOCK_DRAWABLE)
		{
		return PR_TRUE;
		}

	/* 
	 * Check to see if this CL_Drawable was the last one to use
	 * this drawable. If not, someone else might have modified
	 * the bits since the last time this CL_Drawable wrote to
	 * to them.
	 */
	if ( (inState & CL_LOCK_DRAWABLE_FOR_READ) && (mOwner != inCLDrawable) )
		{
		return PR_FALSE;
		}

	mOwner = inCLDrawable;

	if ( mGWorld == NULL )
		{
		return PR_FALSE;
		}
	
	LockPixels ( GetGWorldPixMap ( mGWorld ) );
	
	return PR_TRUE;
}
 
// Set the required dimensions of the drawable. 
void 
COffscreenDrawable::SetDimensions(int32 inWidth, int32 inHeight)
{

	if ( (inWidth > mWidth) || (inHeight > mHeight) )
		{
		/* 
		 * If there is only one client of the backing store,
		 * we can resize it to the dimensions specified.
		 * Otherwise, we can make it larger, but not smaller
		 */
		if (mRefCnt > 1)
			{
			if (inWidth < mWidth)
				{
				inWidth = mWidth;
				}
		
			if (inHeight < mHeight)
				{
				inHeight = mHeight;
				}
			}
	
		/*
		 * Create our offscreen. We need to try to find the deepest device that
		 * intersects the current screen area. Not sure how to do that at present.
		 * [hint: DeviceLoop]
		 *
		 * If a drawable is always set as the current drawable before its
		 * SetDimensions method is called, then we can have an explicit SetScreenArea
		 * method that allows us to track the current deepest screen that intersects
		 * us.
		 * 
		 * We should also be able to keep the GWorld allocated and just reallocate the
		 * pixels rather than the whole damn thing. This will be a slight memory hit
		 * but a big speed win.
		 */
		
		OSErr err;
		Rect r = { 0, 0, inHeight, inWidth };
		GDHandle deepestDevice;
		GDHandle gdList;
		UInt32 deviceDepth;
		UInt32 maxDepth;
		Boolean maxIsColor;
		
		/*
		 * Find the deepest device, or at least a color one
		 */
		maxIsColor = false;
		maxDepth = 0;
		deepestDevice = 0L;
		gdList = GetDeviceList();
		
		while ( gdList )
			{
			deviceDepth = (*(*gdList)->gdPMap)->pixelSize;
			
			if ( deviceDepth > maxDepth )
				{
				// our new device is deeper, use it
				maxDepth = deviceDepth;
				maxIsColor = (*gdList)->gdFlags & 1;
				deepestDevice = gdList;
				}
			else
			if ( deviceDepth == maxDepth && !maxIsColor )
				{
				if ( ( (*gdList)->gdFlags & 1 ) != 0 )
					{
					// our new device is color, use it
					maxIsColor = true;
					deepestDevice = gdList;
					}
				}
			
			gdList = GetNextDevice ( gdList );
			}
			
		Assert_ ( deepestDevice != NULL );
		
		/* if we didn't find anything, then we just do an 8 bit color device */
		if ( deepestDevice == NULL )
			{
			maxDepth = 8;
			}
		
		if ( mGWorld == NULL )
			{
			err = NewGWorld ( &mGWorld, maxDepth, &r, 0L, deepestDevice, noNewDevice + useTempMem );
			}
		else
			{
			err = UpdateGWorld ( &mGWorld, maxDepth, &r, 0L, deepestDevice, 0 );
			}
		
		if ( err != noErr )
			{
			mGWorld = NULL;
			}
		else
			{
			GDHandle gd;
			CGrafPtr gp;
			
			GetGWorld ( &gp, &gd );
			SetGWorld ( mGWorld, NULL );
			
			// DEBUG - so we know for sure when we draw unitialized pixels!
			ForeColor ( redColor );
			PaintRect ( &mGWorld->portRect );
			
			SetGWorld ( gp, gd );
			
			mWidth = inWidth;
			mHeight = inHeight;
			}
		}
}


/*
 * These are all our fe callback routines
 */
static PRBool fe_lock_drawable(CL_Drawable *drawable, CL_DrawableState state)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	return pDrawable->LockDrawable(drawable, state);
}

static void fe_init_drawable(CL_Drawable *drawable)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	pDrawable->InitDrawable(drawable);
}

static void fe_relinquish_drawable(CL_Drawable *drawable)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	pDrawable->RelinquishDrawable(drawable);
}

static void fe_set_drawable_origin(CL_Drawable *drawable, int32 x_offset, int32 y_offset)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	pDrawable->SetLayerOrigin(x_offset, y_offset);
}

static void fe_get_drawable_origin(CL_Drawable *drawable, int32 *x_offset, int32 *y_offset)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	pDrawable->GetLayerOrigin(x_offset, y_offset);
}

static void fe_set_drawable_clip(CL_Drawable *drawable, FE_Region clip_region)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	pDrawable->SetLayerClip(clip_region);
}

static void fe_restore_drawable_clip(CL_Drawable *drawable)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	pDrawable->SetLayerClip(NULL);
}

static void fe_copy_pixels(CL_Drawable *drawable_src, CL_Drawable *drawable_dest, FE_Region region)
{
	CDrawable *pSrcDrawable = (CDrawable *)CL_GetDrawableClientData(drawable_src);
	CDrawable *pDstDrawable = (CDrawable *)CL_GetDrawableClientData(drawable_dest);

	pDstDrawable->CopyPixels(pSrcDrawable, region);
}

static void fe_set_drawable_dimensions(CL_Drawable *drawable, uint32 width, uint32 height)
{
	CDrawable *pDrawable = (CDrawable *)CL_GetDrawableClientData(drawable);
	pDrawable->SetDimensions(width, height);
}

CL_DrawableVTable mfe_drawable_vtable = {
	fe_lock_drawable,
	fe_init_drawable,
	fe_relinquish_drawable,
	NULL,
	fe_set_drawable_origin,
	fe_get_drawable_origin,
	fe_set_drawable_clip,
	fe_restore_drawable_clip,
	fe_copy_pixels,
	fe_set_drawable_dimensions
};


#endif // LAYERS
