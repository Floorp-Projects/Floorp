/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIRenderingContextPh_h___
#define nsIRenderingContextPh_h___

#include "nsIRenderingContext.h"
#include <Pt.h>

// IID for the nsIRenderingContext interface
#define NS_IRENDERING_CONTEXT_PH_IID \
{ 0x0fcde820, 0x8ae2, 0x11d2, \
{ 0xa8, 0x48, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

// RenderingContextPh interface
class nsIRenderingContextPh : public nsISupports
{
public:
  /**
   * Create a new drawing surface to represent an HDC.
   * @param aDC Windows HDC.
   * @param aSurface out parameter for new drawing surface
   * @result error status
   */
  NS_IMETHOD CreateDrawingSurface(PhGC_t &aGC, nsDrawingSurface &aSurface) = 0;
};

#endif /* nsIRenderingContextPh_h___ */
