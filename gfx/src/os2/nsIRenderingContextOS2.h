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

#ifndef nsIRenderingContextOS2_h___
#define nsIRenderingContextOS2_h___

#include "nsIRenderingContext.h"
#include <os2.h>

// IID for the nsIRenderingContext interface
#define NS_IRENDERING_CONTEXT_OS2_IID \
{ 0x0fcde820, 0x8ae2, 0x11d2, \
{ 0xa8, 0x48, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

// RenderingContextWin interface
class nsIRenderingContextOS2 : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRENDERING_CONTEXT_OS2_IID)
  /**
   * Create a new drawing surface to represent an HPS.
   * @param aPS Windows HPS.
   * @param aSurface out parameter for new drawing surface
   * @result error status
   */
  NS_IMETHOD CreateDrawingSurface(HPS aPS, nsDrawingSurface &aSurface, nsIWidget *aWidget) = 0;
};

#endif /* nsIRenderingContextOS2_h___ */
