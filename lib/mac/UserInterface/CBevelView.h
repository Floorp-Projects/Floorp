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

#pragma once

#include <LView.h>
#include <LArray.h>
#include <URegions.h>

const ResType ResType_BevelDescList = 'BevL';

typedef struct SSubBevel {
	PaneIDT		paneID;
	Int16		bevelLevel;
	Rect		cachedLocalFrame;
} SSubBevel;

class CBevelView : public LOffscreenView
{
	public:
		enum { class_ID = 'BvVw' };
	
							CBevelView(LStream *inStream);
		virtual				~CBevelView();
	
		virtual void		ResizeFrameBy(
									Int16 				inWidthDelta,
									Int16				inHeightDelta,
									Boolean 			inRefresh);

		virtual	void		CalcBevelRegion(void);
		virtual	void		CalcBevelMask(void);
		
		virtual	void		SubPanesChanged(Boolean inRefresh = true);

		static void			SubPanesChanged(LPane* subPane, Boolean inRefresh);
			// This is a safe, but ugly way of telling any enclosing GrayBevelView
			// that the panes have moved.  The subPane is the starting point for
			// an upwards search
		
		// 97-06-05 pkc -- methods to control bevelling of grow icon area
		void				BevelGrowIcon();
		void				DontBevelGrowIcon();
		
	protected:

		virtual void		FinishCreateSelf(void);

		virtual	void		DrawSelf(void);
		virtual	void		DrawBeveledFill(void) = 0;
		virtual	void		DrawBeveledFrame(void) = 0;
		virtual	void		DrawBeveledSub(const SSubBevel&	inDesc) = 0;
									
		virtual	void		InvalMainFrame(void);
		virtual void		InvalSubFrames(void);

		virtual	Boolean		CalcSubpaneLocalRect(PaneIDT inPaneID, Rect& subFrame);

		Int16				mMainBevel;
		LArray				mBevelList;
		StRegion			mBevelRegion;		// Local
		StRegion			mBevelMaskRegion;	// Port Relative
		Boolean				mNeedsRecalc;
		Boolean				mBevelGrowIcon;
};

