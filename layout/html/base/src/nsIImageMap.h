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
#ifndef nsIImageMap_h___
#define nsIImageMap_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsCoord.h"
class nsIAtom;
class nsIPresContext;
class nsIRenderingContext;
class nsIURL;
class nsString;

/* db6ca200-d0a6-11d1-89b1-006008911b81 */
#define NS_IIMAGEMAP_IID \
{0xdb6ca200, 0xd0a6, 0x11d1, {0x89, 0xb1, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81}}

class nsIImageMap : public nsISupports {
public:
  NS_IMETHOD SetName(const nsString& aMapName) = 0;

  NS_IMETHOD GetName(nsString& aResult) = 0;

  NS_IMETHOD AddArea(const nsString& aBaseURL,
                     const nsString& aShape,
                     const nsString& aCoords,
                     const nsString& aHREF,
                     const nsString& aTarget,
                     const nsString& aAltText,
                     PRBool aSuppress) = 0;

  /**
   * See if the given aX,aY <b>pixel</b> coordinates are in the image
   * map. If they are then NS_OK is returned and aAbsURL, aTarget,
   * aAltText, aSuppress are filled in with the values from the
   * underlying area tag. If the coordinates are not in the map
   * then NS_NOT_INSIDE is returned.
   */
  NS_IMETHOD IsInside(nscoord aX, nscoord aY,
                      nsIURL* aDocURL,
                      nsString& aAbsURL,
                      nsString& aTarget,
                      nsString& aAltText,
                      PRBool* aSuppress) = 0;

  /**
   * See if the given aX,aY <b>pixel</b> coordinates are in the image
   * map. If they are then NS_OK is returned otherwise NS_NOT_INSIDE
   * is returned.
   */
  NS_IMETHOD IsInside(nscoord aX, nscoord aY) = 0;

  NS_IMETHOD Draw(nsIPresContext& aCX, nsIRenderingContext& aRC) = 0;
};

// XXX get an error space to alloc from
#define NS_NOT_INSIDE   1

extern NS_HTML nsresult NS_NewImageMap(nsIImageMap** aInstancePtrResult,
                                       nsIAtom* aAtom);

#endif /* nsIImageMap_h___ */
