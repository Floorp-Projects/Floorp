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
 
#ifndef nsGfxUtils_h_
#define nsGfxUtils_h_

#ifndef CarbonHelpers_h__
#include "nsCarbonHelpers.h"
#endif // CarbonHelpers_h__

#include <LowMem.h>


/** ------------------------------------------------------------
 *	Used to assert that we're not clobbering a bad port
 */
inline PRBool CurrentPortIsWMPort()
{
#if TARGET_CARBON
  return PR_FALSE;
#else
  GrafPtr curPort;
  ::GetPort(&curPort);
  
  return (curPort == ::LMGetWMgrPort());
#endif
}


//------------------------------------------------------------------------
// utility port setting class
//------------------------------------------------------------------------

class StPortSetter
{
public:
	StPortSetter(GrafPtr newPort)
		: mNewPort(newPort), mOldPort(::GetQDGlobalsThePort())
	{
		if (mOldPort != newPort)
			::SetPort(newPort);
	}

#if TARGET_CARBON
	StPortSetter(WindowPtr newWindow)
		: mNewPort(GetWindowPort(newWindow)), mOldPort(::GetQDGlobalsThePort())
	{
		if (mOldPort != mNewPort)
			::SetPort(mNewPort);
	}
#endif    
	
	~StPortSetter()
	{
		if (mOldPort != mNewPort)
			::SetPort(mOldPort);
	}

protected:
	GrafPtr		mNewPort;
	GrafPtr		mOldPort;
};

//------------------------------------------------------------------------
// utility text state save/restore class
//------------------------------------------------------------------------

class StTextStyleSetter
{
public:
	StTextStyleSetter(SInt16 fontID, SInt16 fontSize, SInt16 fontFace)
	{
	  SetPortFontStyle(fontID, fontSize, fontFace);
	}
	
	StTextStyleSetter(TextStyle& theStyle)
	{
	  SetPortFontStyle(theStyle.tsFont, theStyle.tsSize, theStyle.tsFace);
	}
	
	~StTextStyleSetter()
	{
  	::TextFont(mFontID);
  	::TextSize(mFontSize);
  	::TextFace(mFontFace);
	}

protected:

  void SetPortFontStyle(SInt16 fontID, SInt16 fontSize, SInt16 fontFace)
	{
	  GrafPtr curPort;
	  ::GetPort(&curPort);
	  
    NS_ASSERTION(!CurrentPortIsWMPort(), "Setting window manager port font");

    mFontID = ::GetPortTextFont(curPort);
    mFontSize = ::GetPortTextSize(curPort);
    mFontFace = ::GetPortTextFace(curPort);
	  
  	::TextFont(fontID);
  	::TextSize(fontSize);
  	::TextFace(fontFace);
	}

protected:
	SInt16		mFontID;
	SInt16		mFontSize;
	SInt16    mFontFace;
};





/** ------------------------------------------------------------
 *	Utility class for saving, locking, and restoring pixel state
 */

class StPixelLocker
{
public:
				
										StPixelLocker(PixMapHandle thePixMap)
										:	mPixMap(thePixMap)
										,	mPixelState(0)
										{
											if (mPixMap) {
											mPixelState = ::GetPixelsState(mPixMap);
											::LockPixels(mPixMap);
										}
										}
										
										~StPixelLocker()
										{
											if (mPixMap)
											::SetPixelsState(mPixMap, mPixelState);
										}

protected:


		PixMapHandle		mPixMap;
		GWorldFlags			mPixelState;

};

/** ------------------------------------------------------------
 *	Utility class for saving, locking, and restoring handle state
 *  Ok with null handle
 */

class StHandleLocker
{
public:

                    StHandleLocker(Handle theHandle)
                    :	mHandle(theHandle)
                    {
                      if (mHandle)
                      {
                    	  mOldHandleState = ::HGetState(mHandle);
                    	  ::HLock(mHandle);
                      }										  
                    }

                    ~StHandleLocker()
                    {
                      if (mHandle)
                        ::HSetState(mHandle, mOldHandleState);
                    }

protected:

    Handle          mHandle;
    SInt8           mOldHandleState;
};


#endif // nsGfxUtils_h_
