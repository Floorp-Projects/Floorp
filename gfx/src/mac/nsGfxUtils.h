/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
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
