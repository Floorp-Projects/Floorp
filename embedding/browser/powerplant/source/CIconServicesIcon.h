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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
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
