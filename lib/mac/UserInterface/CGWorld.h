/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	This is a replacement class for PowerPlant's LGWorld.  It allows for the
//	resizing and reorienting of the offscreen world, which LGWorld won't let
//	you do.  Because the CGWorld can be updated, the pixmap can be made
//	purgeable by passing in the pixPurge flag in the constructor.  LGWorld
//	would let you do this, however, if the pixels ever got purged you'd be
//	completely hosed.
//
//	This class has ben submitted to metrowerks for a potential replacement
//	of LGWorld.  In the event that metrowerks adds the extended functionality
//	that CGWorld offers, we can make this module obsolete.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LSharable.h>

#ifndef __QDOFFSCREEN__
#include <QDOffscreen.h>
#endif

class CGWorld
{
	public:
						CGWorld();
						CGWorld(
							const Rect&		inBounds,
							Int16 			inPixelDepth = 0,
							GWorldFlags 	inFlags = 0,
							CTabHandle 		inCTableH = nil,
							GDHandle 		inGDeviceH = nil);
							
						~CGWorld();
						
		Boolean			BeginDrawing(void);
		void			EndDrawing(void);
		
		void			CopyImage(
								GrafPtr 	inDestPort,
								const Rect&	inDestRect,
								Int16 		inXferMode = srcCopy,
								RgnHandle 	inMaskRegion = nil);

		GWorldPtr		GetMacGWorld(void);

		void			SetBounds(const Rect &inBounds);
		const Rect&		GetBounds(void) const;

		Boolean			IsConsistent(void) const;
		
		void			AdjustGWorld(const Rect& inToBounds);

		
	virtual	void			UpdateBounds(const Rect& inBounds);
		void			UpdateOrigin(const Point& inOrigin);

	protected:

		GWorldPtr		mMacGWorld;
		QDErr			mGWorldStatus;
		Rect			mBounds;
		Int16			mDepth;
		
		CGrafPtr		mSavePort;
		GDHandle		mSaveDevice;
};

inline const Rect& CGWorld::GetBounds(void) const
	{	return mBounds;									}
inline Boolean CGWorld::IsConsistent(void) const
	{	return (mGWorldStatus == noErr);				}
inline GWorldPtr CGWorld::GetMacGWorld(void)
	{	return mMacGWorld;								}
	

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CSharedWorld : public CGWorld, public LSharable
{
	public:
						CSharedWorld(
								const Rect& 	inFrame,
								Int16 			inDepth,
								GWorldFlags 	inFlags);
};

