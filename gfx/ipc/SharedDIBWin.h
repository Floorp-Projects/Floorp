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

#ifndef gfx_SharedDIBWin_h__
#define gfx_SharedDIBWin_h__

#include <windows.h>

#include "SharedDIB.h"

namespace mozilla {
namespace gfx {

class SharedDIBWin : public SharedDIB
{
public:
  SharedDIBWin();
  ~SharedDIBWin();

  // Allocate a new win32 dib section compatible with an hdc. The dib will
  // be selected into the hdc on return.
  nsresult Create(HDC aHdc, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aDepth);

  // Wrap a dib section around an existing shared memory object. aHandle should
  // point to a section large enough for the dib's memory, otherwise this call
  // will fail.
  nsresult Attach(Handle aHandle, PRUint32 aWidth, PRUint32 aHeight, PRUint32 aDepth);

  // Destroy or release resources associated with this dib.
  nsresult Close();

  // Return the HDC of the shared dib.
  HDC GetHDC() { return mSharedHdc; }

private:
  HDC                 mSharedHdc;
  HBITMAP             mSharedBmp;
  HGDIOBJ             mOldObj;

  PRUint32 SetupBitmapHeader(PRUint32 aWidth, PRUint32 aHeight, PRUint32 aDepth, BITMAPINFOHEADER *aHeader);
  nsresult SetupSurface(HDC aHdc, BITMAPINFOHEADER *aHdr);
};

} // gfx
} // mozilla

#endif
