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

#include "nsRegionPool.h"


NS_EXPORT nsNativeRegionPool sNativeRegionPool;

//------------------------------------------------------------------------

nsNativeRegionPool::nsNativeRegionPool()
{
	mRegionSlots = nsnull;
	mEmptySlots = nsnull;
}

//------------------------------------------------------------------------

nsNativeRegionPool::~nsNativeRegionPool()
{
	// Release all of the regions.
	if (mRegionSlots != nsnull) {
		nsRegionSlot* slot = mRegionSlots;
		while (slot != nsnull) {
			::DisposeRgn(slot->mRegion);
			nsRegionSlot* next = slot->mNext;
			delete slot;
			slot = next;
		}
	}
	
	// Release all empty slots.
	if (mEmptySlots != nsnull) {
		nsRegionSlot* slot = mEmptySlots;
		while (slot != nsnull) {
			nsRegionSlot* next = slot->mNext;
			delete slot;
			slot = next;
		}
	}
}

//------------------------------------------------------------------------

RgnHandle nsNativeRegionPool::GetNewRegion()
{
	nsRegionSlot* slot = mRegionSlots;
	if (slot != nsnull) {
		RgnHandle region = slot->mRegion;

		// remove this slot from the free list.
		mRegionSlots = slot->mNext;
		
		// transfer this slot to the empty slot list for reuse.
		slot->mRegion = nsnull;
		slot->mNext = mEmptySlots;
		mEmptySlots = slot;
		
		// initialize the region. 
		::SetEmptyRgn(region);
		return region;
	}
	
	// return a fresh new region. a slot will be created to hold it
	// if and when the region is released.
	return (::NewRgn());
}

//------------------------------------------------------------------------

void nsNativeRegionPool::ReleaseRegion(RgnHandle aRgnHandle)
{
	nsRegionSlot* slot = mEmptySlots;
	if (slot != nsnull)
		mEmptySlots = slot->mNext;
	else
		slot = new nsRegionSlot;
	
	if (slot != nsnull) {
		// put this region on the region list.
		slot->mRegion = aRgnHandle;
		slot->mNext = mRegionSlots;
		mRegionSlots = slot;
	} else {
		// couldn't allocate a slot, toss the region.
		::DisposeRgn(aRgnHandle);
	}
}

