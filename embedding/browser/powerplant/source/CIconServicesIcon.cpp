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

#include <Processes.h>

//*****************************************************************************
// CIconServicesIcon
//*****************************************************************************   

OSType CIconServicesIcon::mgAppCreator;
FSSpec CIconServicesIcon::mgAppFileSpec;

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
	Rect	iconRect;
	CalcPortFrameRect(iconRect);
    AdjustIconRect(iconRect);
    	
	IconTransformType transform;
	if (mbIsPressed)
	    transform = kTransformSelected;
	else if (mEnabled != triState_On)
	    transform = kTransformDisabled;
	else
	    transform = kTransformNone;
		  
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
	Point	portPt = inPoint;
	LocalToPortPoint(portPt);

	Rect	iconRect;
	CalcPortFrameRect(iconRect);
    AdjustIconRect(iconRect);

    return ::PtInIconRef(&portPt, &iconRect, mAlignmentType, kIconServicesNormalUsageFlag, mIconRef);
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
        info.processAppSpec = &mgAppFileSpec;
        err = ::GetProcessInformation(&psn, &info);
        ThrowIfError_(err);
        mgAppCreator = info.processSignature;
        
        gInitialized = true;
    }
    GetIconRef();
}

void CIconServicesIcon::AdjustIconRect(Rect& ioRect) const
{
	ioRect.top += ((ioRect.bottom - ioRect.top) - 32) / 2;
	ioRect.left += ((ioRect.right - ioRect.left) - 32) / 2;
    ioRect.right = ioRect.left + 32;
    ioRect.bottom = ioRect.top + 32;
}
	
void CIconServicesIcon::GetIconRef()
{
    OSErr   err;
    IconRef iconRef;
    
    // We would like to first see if the icon is already registered
    // But, for some reason, the following call always returns noErr and the wrong icon.
    // err = ::GetIconRef(mgAppFileSpec.vRefNum, mgAppCreator, mIconType, &iconRef);    
    // if (err != noErr)

    err = ::RegisterIconRefFromResource(mgAppCreator, mIconType, &mgAppFileSpec, mIconResID, &iconRef);
    ThrowIfError_(err);
    mIconRef = iconRef;
}

void CIconServicesIcon::ReleaseIconRef()
{
    if (mIconRef)
    {
        ::ReleaseIconRef(mIconRef);
        mIconRef = nil;
    }
}
