/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */
 
#include "CIconServicesIcon.h"

#include "UResourceMgr.h"
#include "UMemoryMgr.h"

#include <Processes.h>

//*****************************************************************************
// CIconServicesIcon
//*****************************************************************************   

OSType CIconServicesIcon::mgAppCreator;
FSSpec CIconServicesIcon::mgIconFileSpec;

CIconServicesIcon::CIconServicesIcon(const SPaneInfo&	inPaneInfo,
	                                 MessageT			inValueMessage,
                                     OSType             inIconType,
                                     SInt16             inIconResID) :
    LControl(inPaneInfo, inValueMessage),
    mIconType(inIconType), mIconResID(inIconResID),
    mAlignmentType(kAlignAbsoluteCenter), mIconRef(nil),
    mbIsPressed(false)
{
    Init();
}


CIconServicesIcon::CIconServicesIcon(LStream*	inStream) :
    LControl(inStream),
    mAlignmentType(kAlignAbsoluteCenter), mIconRef(nil),
    mbIsPressed(false)
{
    *inStream >> mIconType;
    *inStream >> mIconResID;
    
    Init();
}


CIconServicesIcon::~CIconServicesIcon()
{
    ReleaseIconRef();
}

void CIconServicesIcon::DrawSelf()
{
  if (!mIconRef)
    return;
    
	Rect	iconRect;
	CalcLocalFrameRect(iconRect);
    AdjustIconRect(iconRect);
    	
	IconTransformType transform;
	if (mbIsPressed)
	    transform = kTransformSelected;
	else if (mEnabled != triState_On)
	    transform = kTransformDisabled;
	else
	    transform = kTransformNone;
	
	// Because the icon may be translucent, clear out the
	// region under it. The icon will draw differently
	// depending on the background. This makes it consistent.
	StRegion cleanRgn;
	if (::IconRefToRgn(cleanRgn,
	                   &iconRect,
	                   mAlignmentType,
	                   kIconServicesNormalUsageFlag,
                       mIconRef) == noErr)
        ::EraseRgn(cleanRgn);
    
    ::PlotIconRef(&iconRect,
                  mAlignmentType,
                  transform,
                  kIconServicesNormalUsageFlag,
                  mIconRef);
}

void CIconServicesIcon::EnableSelf()
{
    Refresh();
}

void CIconServicesIcon::DisableSelf()
{
    Refresh();
}

SInt16 CIconServicesIcon::FindHotSpot(Point	inPoint) const
{
	  Boolean inHotSpot = PointInHotSpot(inPoint, 0);
	  return inHotSpot ? 1 : 0;
}

Boolean CIconServicesIcon::PointInHotSpot(Point		inPoint,
								          SInt16	inHotSpot) const
{
  if (!mIconRef)
    return false;
    
	Rect	iconRect;
	CalcLocalFrameRect(iconRect);
    AdjustIconRect(iconRect);

    return ::PtInIconRef(&inPoint, &iconRect, mAlignmentType, kIconServicesNormalUsageFlag, mIconRef);
}

void CIconServicesIcon::HotSpotAction(SInt16		/* inHotSpot */,
	                                  Boolean		inCurrInside,
	                                  Boolean		inPrevInside)
{
	if (inCurrInside != inPrevInside)
	{
	    mbIsPressed = inCurrInside;
	    Draw(nil);	
    }
}

void CIconServicesIcon::HotSpotResult(SInt16	/* inHotSpot */)
{
	BroadcastValueMessage();
}

void CIconServicesIcon::Init()
{
    static bool gInitialized;
    
    if (!gInitialized)
    {
        OSErr err;
        
        // Since this a part of mozilla, which requires System 8.6,
        // we can be sure of having this. Just in case...
        long response;
        err = ::Gestalt(gestaltIconUtilitiesAttr, &response);
        ThrowIfError_(err);
        if (!(response & gestaltIconUtilitiesHasIconServices))
            Throw_(-12345);
        
        ProcessSerialNumber psn;
        err = ::GetCurrentProcess(&psn);
        ThrowIfError_(err);
        
        ProcessInfoRec info;
        info.processInfoLength = sizeof(info);
        info.processName = nil;
        info.processAppSpec = nil;
        err = ::GetProcessInformation(&psn, &info);
        ThrowIfError_(err);
        mgAppCreator = info.processSignature;
        
        // RegisterIconRefFromResource() needs to be given an FSSpec of
        // the file containing the 'icns' resource of the icon being
        // registered. The following will track down the file no matter
        // how our application is packaged.
        
        StResLoad resLoadState(false);
        StResource resHandle('icns', mIconResID); // throws if N/A
        SInt16 resRefNum = ::HomeResFile(resHandle);
        if (resRefNum != -1)
        {
          FCBPBRec pb;
      
          pb.ioNamePtr = mgIconFileSpec.name;
          pb.ioVRefNum = 0;
          pb.ioRefNum = resRefNum;
          pb.ioFCBIndx = 0;
          err = PBGetFCBInfoSync(&pb);
          if (err == noErr)
          {
              mgIconFileSpec.vRefNum = pb.ioFCBVRefNum;
              mgIconFileSpec.parID = pb.ioFCBParID;
          }
        }
        gInitialized = true;
    }
    GetIconRef();
}

void CIconServicesIcon::AdjustIconRect(Rect& ioRect) const
{
    SDimension16 frameSize;
    GetFrameSize(frameSize);
    SInt16 iconSize = (frameSize.width <= 16 && frameSize.height <= 16) ? 16 : 32;
    
	ioRect.top += ((ioRect.bottom - ioRect.top) - iconSize) / 2;
	ioRect.left += ((ioRect.right - ioRect.left) - iconSize) / 2;
    ioRect.right = ioRect.left + iconSize;
    ioRect.bottom = ioRect.top + iconSize;
}
	
void CIconServicesIcon::GetIconRef()
{
    // We would like to first see if the icon is already registered
    // But, for some reason, the following call always returns noErr and the wrong icon.
    // err = ::GetIconRef(mgIconFileSpec.vRefNum, mgAppCreator, mIconType, &iconRef);    
    // if (err != noErr)
    ::RegisterIconRefFromResource(mgAppCreator, mIconType, &mgIconFileSpec, mIconResID, &mIconRef);
}

void CIconServicesIcon::ReleaseIconRef()
{
    if (mIconRef)
    {
        ::ReleaseIconRef(mIconRef);
        mIconRef = nil;
    }
}
