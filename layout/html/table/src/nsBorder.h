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
#ifndef nsBorder_h__
#define nsBorder_h__

#include "nsColor.h"
#include "nsCoord.h"
#include "nsStyleConsts.h"


#define BORDER_PRECEDENT_EQUAL 0
#define BORDER_PRECEDENT_LOWER 1
#define BORDER_PRECEDENT_HIGHER 2


/** an encapsulation of border edge info 
  * 
  */
struct nsBorderEdge
{
  nscoord mWidth;
  PRUint8 mStyle;  
  nscolor mColor;
  PRUint8 mSide;

  nsBorderEdge();
};

inline nsBorderEdge::nsBorderEdge()
{
  mWidth=0;
  mStyle=NS_STYLE_BORDER_STYLE_NONE;
  mColor=0;
  mSide=NS_SIDE_LEFT;
};


#endif
