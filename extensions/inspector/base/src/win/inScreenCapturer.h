/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __inScreenCapturer_h__
#define __inScreenCapturer_h__

#include "inIScreenCapturer.h"
#include <windows.h>

class inScreenCapturer : public inIScreenCapturer                            
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INISCREENCAPTURER

  inScreenCapturer();
  ~inScreenCapturer();

protected:
  static NS_IMETHODIMP DoCopy8(PRUint8* aBits, HDC aHDC, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  static NS_IMETHODIMP DoCopy16(PRUint8* aBits, HDC aHDC, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
  static NS_IMETHODIMP DoCopy32(PRUint8* aBits, HDC aHDC, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);

};

#endif // __inScreenCapturer_h__
