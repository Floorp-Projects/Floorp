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

//   drawable.h - Classes used for maintaining the notion of 
//                offscreen and onscreen drawables, so that 
//                document contents can be drawn to either
//                location. Currently used for LAYERS.

#include <QDOffscreen.h>
#include "ntypes.h"
#include "structs.h"

#include "layers.h"
#include "cl_types.h"

#ifndef _DRAWABLE_H_
#define _DRAWABLE_H_


#ifdef LAYERS 

extern CL_DrawableVTable mfe_drawable_vtable;

class CDrawable
{
	public:
    // Constructor and destructor
								CDrawable();
								~CDrawable();
 
    // Lock the drawable for use, based on the state passed in.
    // Does nothing in the super class.
		virtual PRBool			LockDrawable(
									CL_Drawable*			/*pCLDrawable*/, 
									CL_DrawableState		/*newState*/) { return PR_TRUE; }

    // Calls to indicate a new client or the relinquishing of the drawable
    // by a client.
		virtual void			InitDrawable(
									CL_Drawable*			pCLDrawable);
									
		virtual void			RelinquishDrawable(
									CL_Drawable*			pCLDrawable);

    // Get and set the origin of the drawable
    	virtual void			SetLayerOrigin(
    								int32					inX,
    								int32					inY);
    	virtual void			GetLayerOrigin(
    								int32*					outX,
    								int32*					outY);

    // Set the drawable's clip. A value of NULL indicates that the
    // clip should be restored to its initial value.
		virtual void			SetLayerClip(
    								FE_Region				inClipRgn);

		virtual FE_Region		GetLayerClip() { return mClipRgn; }
		
        virtual Boolean			HasClipChanged();
        
    // Call to copy pixels from the given drawable
		virtual void			CopyPixels(
    								CDrawable *				inSrcDrawable,
    								FE_Region				hCopyRgn);

    // Call to set the width and height of a drawable;
		virtual void			SetDimensions(
    								int32					/*inWidth*/,
    								int32					/*inHeight*/) {}
    	
    	virtual GWorldPtr		GetDrawableOffscreen() { return mGWorld; }

	// Set the parent drawable
		virtual void			SetParent(
									CDrawable*				parent );
									
  		CDrawable*				mParent;
		GWorldPtr				mGWorld;
		Uint32					mRefCnt;
		int32					mOriginX;
		int32					mOriginY;
		RgnHandle				mClipRgn;
		Boolean					mClipChanged;
};

class COnscreenDrawable :
		public CDrawable
{
	public:
		COnscreenDrawable();
		~COnscreenDrawable() {}
};

class CRouterDrawable :
		public CDrawable
{
	public:
		CRouterDrawable();
		~CRouterDrawable() {}

    	virtual void			SetLayerOrigin(
    								int32					inX,
    								int32					inY);
    	virtual void			GetLayerOrigin(
    								int32*					outX,
    								int32*					outY);

		virtual void			SetLayerClip(
    								FE_Region				inClipRgn);
		virtual FE_Region		GetLayerClip();
		
        virtual Boolean			HasClipChanged();
        
		virtual void			CopyPixels(
    								CDrawable *				inSrcDrawable,
    								FE_Region				hCopyRgn);
};

class COffscreenDrawable :
		public CDrawable
{

	public:
								COffscreenDrawable();
								~COffscreenDrawable();
    
    // Factory for creating offscreen drawables
		static COffscreenDrawable* AllocateOffscreen();

		virtual void			RelinquishDrawable(
									CL_Drawable*			pCLDrawable);

		virtual PRBool			LockDrawable(
    								CL_Drawable*			pCLDrawable, 
									CL_DrawableState		newState);

		virtual void			SetDimensions(
    								int32					inWidth,
    								int32					inHeight);
    
  private:
		void					delete_offscreen();

	protected:
		CL_Drawable*			mOwner;
		int32					mWidth;
		int32					mHeight;
		
    // In this implementation, we only have a single offscreen drawable
		static COffscreenDrawable *	mOffscreenDrawable;
};

#endif // LAYERS 
#endif // _DRAWABLE_H_ 
