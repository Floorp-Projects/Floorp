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

#if DEBUG && !defined(XP_MACOSX)
#include "macstdlibextras.h"
#endif

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
// ValidateDrawingState
// 
// Test that the current drawing environment is good, which means that
// we have a valid port (as far as we can tell)
//------------------------------------------------------------------------
inline PRBool ValidateDrawingState()
{
  CGrafPtr    curPort;
  GDHandle    curDevice;
  
  GetGWorld(&curPort, &curDevice);
  
  // if we have a window, but we're set to the WM port, things are bad
  if (CurrentPortIsWMPort() && (FrontWindow() != nil))
    return false;

#if TARGET_CARBON
  //if (! IsValidPort(curPort))   // rather slow
  //  return false;
#else
  // all our ports should be onscreen or offscreen color graphics ports
  // Onscreen ports have portVersion 0xC000, GWorlds have 0xC001.
#if DEBUG
  if ((((UInt16)curPort->portVersion & 0xC000) != 0xC000) && !IsSIOUXWindow((GrafPtr)curPort))
    return false;
#else
  if ((((UInt16)curPort->portVersion & 0xC000) != 0xC000))
    return false;
#endif
#endif

  // see if the device is in the device list. If not, it probably means that
  // it's the device for an offscreen GWorld. In that case, the current port
  // should be set to that GWorld too.
  {
    GDHandle    thisDevice = GetDeviceList();
    while (thisDevice)
    {
      if (thisDevice == curDevice)
         break;
    
      thisDevice = GetNextDevice(thisDevice);
    }

    if ((thisDevice == nil) && !IsPortOffscreen(curPort))    // nil device is OK only with GWorld
      return false;
  }

  return true;
}


//------------------------------------------------------------------------
// static graphics utility methods
//------------------------------------------------------------------------
class nsGraphicsUtils
{
public:

  //------------------------------------------------------------------------
  // SafeSetPort
  //
  // Set the port, being sure to set the GDevice to a valid device, since
  // the current GDevice may belong to a GWorld.
  //------------------------------------------------------------------------
  static void SafeSetPort(CGrafPtr newPort)
  {
    ::SetGWorld(newPort, ::IsPortOffscreen(newPort) ? nsnull : ::GetMainDevice());
  }
  
  //------------------------------------------------------------------------
  // SafeSetPortWindowPort
  //
  // Set the port, being sure to set the GDevice to a valid device, since
  // the current GDevice may belong to a GWorld.
  //------------------------------------------------------------------------
  static void SafeSetPortWindowPort(WindowPtr window)
  {
    SafeSetPort(::GetWindowPort(window));
  }

  //------------------------------------------------------------------------
  // SetPortToKnownGoodPort
  //
  // Set the port to a known good port, if possible.
  //------------------------------------------------------------------------
  static void SetPortToKnownGoodPort()
  {
    WindowPtr firstWindow = GetTheWindowList();
    if (firstWindow)
      ::SetGWorld(::GetWindowPort(firstWindow), ::GetMainDevice());
  }

};


//------------------------------------------------------------------------
// utility port setting class
//
// This code has to deal with the situation where the current port
// is a GWorld, and the current devices that GWorld's device. So
// when setting the port to an onscreen part, we always reset the
// current device to the main device.
//------------------------------------------------------------------------
class StPortSetter
{
public:
	StPortSetter(CGrafPtr newPort)
	{
		InitSetter(newPort);
	}

	StPortSetter(WindowPtr window)
	{
		InitSetter(GetWindowPort(window));
	}
	
	~StPortSetter()
	{
	  if (mPortChanged)
  		::SetGWorld(mOldPort, mOldDevice);
	  NS_ASSERTION(ValidateDrawingState(), "Bad drawing state");
	}

protected:
  void InitSetter(CGrafPtr newPort)
	{
	  NS_ASSERTION(ValidateDrawingState(), "Bad drawing state");
	  // we assume that if the port has been set, then the port/GDevice are
	  // valid, and do nothing (for speed)
	  mPortChanged = (newPort != CGrafPtr(GetQDGlobalsThePort));
	  if (mPortChanged)
	  {
  		::GetGWorld(&mOldPort, &mOldDevice);
  		::SetGWorld(newPort, ::IsPortOffscreen(newPort) ? nsnull : ::GetMainDevice());
		}
	}

protected:
  Boolean     mPortChanged;
	CGrafPtr		mOldPort;
	GDHandle    mOldDevice;
};


//------------------------------------------------------------------------
// utility class to temporarily set and restore the origin.
// Assumes that the port has already been set up.
//------------------------------------------------------------------------
class StOriginSetter
{
public:

  StOriginSetter(WindowRef wind, const Point* newOrigin = nsnull)
  {
    ::GetWindowPortBounds(wind, &mSavePortRect);
    if (newOrigin)
      ::SetOrigin(newOrigin->h, newOrigin->v);
    else
      ::SetOrigin(0, 0);
  }
  
  StOriginSetter(CGrafPtr grafPort, const Point* newOrigin = nsnull)
  {
    ::GetPortBounds(grafPort, &mSavePortRect);
    if (newOrigin)
      ::SetOrigin(newOrigin->h, newOrigin->v);
    else
      ::SetOrigin(0, 0);
  }
  
  ~StOriginSetter()
  {
    ::SetOrigin(mSavePortRect.left, mSavePortRect.top);
  }

protected:
  
  Rect    mSavePortRect;

};

//------------------------------------------------------------------------
// utility GWorld port setting class
//
// This should *only* be used to set the port temporarily to the
// GWorld, and then restore it.
//------------------------------------------------------------------------

class StGWorldPortSetter
{
public:
	StGWorldPortSetter(GWorldPtr destGWorld)
	{
	  NS_ASSERTION(::IsPortOffscreen(destGWorld), "StGWorldPortSetter should only be used for GWorlds");
	  ::GetGWorld(&mOldPort, &mOldDevice);
		::SetGWorld(destGWorld, nsnull);
	}
	
	~StGWorldPortSetter()
	{
    ::SetGWorld(mOldPort, mOldDevice);
	  NS_ASSERTION(ValidateDrawingState(), "Bad drawing state");
	}

protected:
	GWorldPtr		mOldPort;
  GDHandle    mOldDevice;
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
	  CGrafPtr curPort;
	  ::GetPort((GrafPtr*)&curPort);
	  
	  NS_ASSERTION(ValidateDrawingState(), "Bad drawing state");

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
 *	Utility class stack-based handle ownership
 */

class StHandleOwner
{
public:
                    StHandleOwner(Handle inHandle)
                    : mHandle(inHandle)
                    {
                    }

                    ~StHandleOwner()
                    {
                      if (mHandle)
                        ::DisposeHandle(mHandle);
                    }

  Handle            GetHandle() { return mHandle; }

  void              ClearHandle(Boolean disposeIt = false)
                    {
                      if (disposeIt)
                        ::DisposeHandle(mHandle);
                      
                      mHandle = nsnull;
                    }

protected:

  Handle            mHandle;

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
