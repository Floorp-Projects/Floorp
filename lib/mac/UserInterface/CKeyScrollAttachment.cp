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


#include "CKeyScrollAttachment.h"


CKeyScrollAttachment::CKeyScrollAttachment(LStream* inStream)
	: LAttachment(inStream)
{
	mViewToScroll = dynamic_cast<LView*>(GetOwnerHost());
	ThrowIfNULL_(mViewToScroll);
	
	Assert_(mMessage == msg_KeyPress);
}


void
CKeyScrollAttachment::ExecuteSelf(
	MessageT	/* inMessage */,
	void		*ioParam)
{
	mExecuteHost = false;		// We handle navigation keys
	Int16 theKey = ((EventRecord*) ioParam)->message & charCodeMask;
	
	switch (theKey)
		{
		case char_Home:			// Scroll to top left
			mViewToScroll->ScrollImageTo(0, 0, true);
			break;
			
		case char_End:
			{					// Scroll to bottom right
			SDimension16	frameSize;
			SDimension32	imageSize;
			mViewToScroll->GetFrameSize(frameSize);
			mViewToScroll->GetImageSize(imageSize);
			// Obviously we don't want to scroll a small image to the bottom of
			// the frame...
			SInt16 deltaV = imageSize.height - frameSize.height;
			if (deltaV < 0)
				deltaV = 0;
			SInt16 deltaH = imageSize.width - frameSize.width;
			if (deltaH < 0)
				deltaH = 0;
			if (deltaV || deltaH)
				mViewToScroll->ScrollImageTo(deltaH, deltaV, true);
			}
			break;
			
		case char_PageUp:
			{					// Scroll up by height of Frame,
								//   but not past top of Image
			SPoint32		frameLoc;
			SPoint32		imageLoc;
			mViewToScroll->GetFrameLocation(frameLoc);
			mViewToScroll->GetImageLocation(imageLoc);
			
			Int32	upMax = frameLoc.v - imageLoc.v;
			if (upMax > 0)
				{
				SPoint32		scrollUnit;
				SDimension16	frameSize;
				mViewToScroll->GetScrollUnit(scrollUnit);
				mViewToScroll->GetFrameSize(frameSize);

				Int32	up = (frameSize.height - 1) / scrollUnit.v;
				if (up <= 0)
					up = 1;

				up *= scrollUnit.v;
				if (up > upMax)
					up = upMax;

				mViewToScroll->ScrollImageBy(0, -up, true);
				}
			}
			break;
			
		case char_PageDown:
			{					// Scroll down by height of Frame,
								//   but not past bottom of Image
			SPoint32		frameLoc;
			SPoint32		imageLoc;
			SDimension16	frameSize;
			SDimension32	imageSize;
			mViewToScroll->GetFrameLocation(frameLoc);
			mViewToScroll->GetImageLocation(imageLoc);
			mViewToScroll->GetFrameSize(frameSize);
			mViewToScroll->GetImageSize(imageSize);
			
			Int32	downMax = imageSize.height - frameSize.height -
								(frameLoc.v - imageLoc.v);
			if (downMax > 0) {
				SPoint32		scrollUnit;
				mViewToScroll->GetScrollUnit(scrollUnit);

				Int32	down = (frameSize.height - 1) / scrollUnit.v;
				if (down <= 0) {
					down = 1;
				}
				down *= scrollUnit.v;
				if (down > downMax) {
					down = downMax;
				}
				mViewToScroll->ScrollImageBy(0, down, true);
			}
			break;
		}
			
		default:
			mExecuteHost = true;	// Some other key, let host respond
			break;
	}
}
