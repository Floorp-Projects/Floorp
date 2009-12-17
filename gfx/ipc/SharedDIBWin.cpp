/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
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

#include "SharedDIBWin.h"
#include "nsMathUtils.h"
#include "nsDebug.h"

namespace mozilla {
namespace gfx {

SharedDIBWin::SharedDIBWin() :
    mSharedHdc(nsnull)
  , mSharedBmp(nsnull)
  , mOldObj(nsnull)
{
}

SharedDIBWin::~SharedDIBWin()
{
  Close();
}

nsresult
SharedDIBWin::Close()
{
  if (mSharedHdc && mOldObj)
    ::SelectObject(mSharedHdc, mOldObj);

  if (mSharedHdc)
    ::DeleteObject(mSharedHdc);

  if (mSharedBmp)
    ::DeleteObject(mSharedBmp);

  mSharedHdc = NULL;
  mOldObj = mSharedBmp = NULL;

  SharedDIB::Close();

  return NS_OK;
}

nsresult
SharedDIBWin::Create(HDC aHdc, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aDepth)
{
  Close();

  // create the offscreen shared dib
  BITMAPINFOHEADER bmih;
  PRUint32 size = SetupBitmapHeader(aWidth, aHeight, aDepth, &bmih);

  nsresult rv = SharedDIB::Create(size);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(SetupSurface(aHdc, &bmih))) {
    Close();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
SharedDIBWin::Attach(Handle aHandle, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aDepth)
{
  Close();

  BITMAPINFOHEADER bmih;
  SetupBitmapHeader(aWidth, aHeight, aDepth, &bmih);

  nsresult rv = SharedDIB::Attach(aHandle, 0);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(SetupSurface(NULL, &bmih))) {
    Close();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

PRUint32
SharedDIBWin::SetupBitmapHeader(PRUint32 aWidth, PRUint32 aHeight, PRUint32 aDepth, BITMAPINFOHEADER *aHeader)
{
  NS_ASSERTION(aDepth == 32, "Invalid SharedDIBWin depth");

  memset((void*)aHeader, 0, sizeof(BITMAPINFOHEADER));
  aHeader->biSize        = sizeof(BITMAPINFOHEADER);
  aHeader->biWidth       = aWidth;
  aHeader->biHeight      = aHeight;
  aHeader->biPlanes      = 1;
  aHeader->biBitCount    = aDepth;
  aHeader->biCompression = BI_RGB;

  // deal better with varying depths. (we currently only ask for 32 bit)
  return (sizeof(BITMAPINFOHEADER) + (aHeader->biHeight * aHeader->biWidth * (PRUint32)NS_ceil(aDepth/8)));
}

nsresult
SharedDIBWin::SetupSurface(HDC aHdc, BITMAPINFOHEADER *aHdr)
{
  mSharedHdc = ::CreateCompatibleDC(aHdc);

  if (!mSharedHdc)
    return NS_ERROR_FAILURE;

  void* ppvBits = nsnull;
  mSharedBmp = ::CreateDIBSection(mSharedHdc,
                                  (BITMAPINFO*)aHdr,
                                  DIB_RGB_COLORS,
                                  (void**)&ppvBits,
                                  mShMem->handle(),
                                  (unsigned long)sizeof(BITMAPINFOHEADER));
  if (!mSharedBmp)
    return NS_ERROR_FAILURE;

  mOldObj = SelectObject(mSharedHdc, mSharedBmp);

  return NS_OK;
}


} // gfx
} // mozilla
