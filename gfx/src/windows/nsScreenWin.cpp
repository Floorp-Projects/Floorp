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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// We have to do this in order to have access to the multiple-monitor
// APIs that are only defined when WINVER is >= 0x0500. Don't worry,
// these won't actually be called unless they are present.
//
#undef WINVER
#define WINVER 0x0500

#include "nsScreenWin.h"


// needed because there are unicode/ansi versions of this routine
// and we need to make sure we get the correct one.
#ifdef UNICODE
#define GetMonitorInfoQuoted "GetMonitorInfoW"
#else
#define GetMonitorInfoQuoted "GetMonitorInfoA"
#endif


typedef BOOL (*GetMonitorInfoProc)(HMONITOR inMon, LPMONITORINFO ioInfo); 


nsScreenWin :: nsScreenWin ( HDC inContext, void* inScreen )
  : mContext(inContext), mScreen(inScreen), mHasMultiMonitorAPIs(PR_FALSE),
      mGetMonitorInfoProc(nsnull)
{
  NS_INIT_REFCNT();

  NS_ASSERTION ( inContext, "Passing null device to nsScreenWin" );
  NS_ASSERTION ( ::GetDeviceCaps(inContext, TECHNOLOGY) == DT_RASDISPLAY, "Not a display screen");
  
  // figure out if we can call the multiple monitor APIs that are only
  // available on Win98/2000.
  HMODULE lib = GetModuleHandle("user32.dll");
  if ( lib ) {
    mGetMonitorInfoProc = GetProcAddress ( lib, GetMonitorInfoQuoted );
    if ( mGetMonitorInfoProc )
      mHasMultiMonitorAPIs = PR_TRUE;
  }
  printf("has multiple monitor apis is %ld\n", mHasMultiMonitorAPIs);

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenWin :: ~nsScreenWin()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenWin, NS_GET_IID(nsIScreen))


NS_IMETHODIMP 
nsScreenWin :: GetWidth(PRInt32 *aWidth)
{
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    BOOL success = (*proc)( (HMONITOR)mScreen, &info );
    if ( success ) 
      *aWidth = info.rcMonitor.right - info.rcMonitor.left;
  }
  else
    *aWidth = ::GetDeviceCaps(mContext, HORZRES);
  return NS_OK;

} // GetWidth


NS_IMETHODIMP 
nsScreenWin :: GetHeight(PRInt32 *aHeight)
{
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    BOOL success = (*proc)( (HMONITOR)mScreen, &info );
    if ( success ) 
      *aHeight = info.rcMonitor.bottom - info.rcMonitor.top;
  }
  else
    *aHeight = ::GetDeviceCaps(mContext, VERTRES);
  return NS_OK;

} // GetHeight


NS_IMETHODIMP 
nsScreenWin :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  //XXX not sure how to get this info for multiple monitors, this might be ok...
  *aPixelDepth = ::GetDeviceCaps(mContext, BITSPIXEL);
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenWin :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth(aColorDepth);

} // GetColorDepth


NS_IMETHODIMP 
nsScreenWin :: GetAvailWidth(PRInt32 *aAvailWidth)
{
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    BOOL success = (*proc)( (HMONITOR)mScreen, &info );
    if ( success ) 
      *aAvailWidth = info.rcWork.right - info.rcWork.left;
  }
  else {
    RECT workArea;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    *aAvailWidth = workArea.right - workArea.left;
  }
  return NS_OK;

} // GetAvailWidth


NS_IMETHODIMP 
nsScreenWin :: GetAvailHeight(PRInt32 *aAvailHeight)
{
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    BOOL success = (*proc)( (HMONITOR)mScreen, &info );
    if ( success ) 
      *aAvailHeight = info.rcWork.bottom - info.rcWork.top;
  }
  else {
    RECT workArea;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    *aAvailHeight = workArea.bottom - workArea.top;
  }
  return NS_OK;

} // GetAvailHeight


NS_IMETHODIMP 
nsScreenWin :: GetAvailLeft(PRInt32 *aAvailLeft)
{
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    BOOL success = (*proc)( (HMONITOR)mScreen, &info );
    if ( success ) 
      *aAvailLeft = info.rcWork.left;
  }
  else {
    RECT workArea;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    *aAvailLeft = workArea.left;
  }
  return NS_OK;

} // GetAvailLeft


NS_IMETHODIMP 
nsScreenWin :: GetAvailTop(PRInt32 *aAvailTop)
{
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    BOOL success = (*proc)( (HMONITOR)mScreen, &info );
    if ( success ) 
      *aAvailTop = info.rcWork.top;
  }
  else {
    RECT workArea;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    *aAvailTop = workArea.top;
  }
  return NS_OK;

} // GetAvailTop

