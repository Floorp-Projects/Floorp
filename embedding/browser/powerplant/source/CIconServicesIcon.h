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

#ifndef __CIconServicesIcon_h__
#define __CIconServicesIcon_h__

#include <LControl.h>

class CIconServicesIcon : public LControl
{
public:
	enum { class_ID = FOUR_CHAR_CODE('CISC') };

                           CIconServicesIcon(const SPaneInfo&	inPaneInfo,
	                                         MessageT			inValueMessage,
                                             OSType             inIconType,
                                             SInt16             inIconResID);
						   CIconServicesIcon(LStream*	inStream);

	virtual				   ~CIconServicesIcon();

    // LPane
    virtual void            DrawSelf();
    virtual void            EnableSelf();
    virtual void            DisableSelf();
    
    // LControl
    SInt16                  FindHotSpot(Point	inPoint) const;
    Boolean                 PointInHotSpot(Point		inPoint,
								           SInt16	    inHotSpot) const;
    void                    HotSpotAction(SInt16    inHotSpot,
	                                      Boolean	inCurrInside,
	                                      Boolean	inPrevInside);
    void		            HotSpotResult(SInt16 inHotSpot);	
	
	// CIconServicesIcon
protected:
    void                    Init();              

    void                    AdjustIconRect(Rect& ioRect) const;

	void                    GetIconRef();
	void                    ReleaseIconRef();
	    
protected:
    OSType                  mIconType;
    SInt16                  mIconResID;
    IconAlignmentType       mAlignmentType;
    
    IconRef                 mIconRef;
    bool                    mbIsPressed;

    static OSType           mgAppCreator;
    static FSSpec           mgIconFileSpec;
};

#endif // __CIconServicesIcon_h__
