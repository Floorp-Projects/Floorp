/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date           Modified by     Description of modification
 * 03/28/2000   IBM Corp.        Changes to make os2.h file similar to windows.h file
 */

#ifndef _nsFontMetricsOS2_h
#define _nsFontMetricsOS2_h

#include "nsIFontMetrics.h"
#include "nsCRT.h"

class nsIRenderingContext;
class nsDeviceContextOS2;
class nsString;
class nsFont;

// An nsFontHandle is actually a pointer to one of these.
// It knows how to select itself into a ps.
struct nsFontHandleOS2
{
   FATTRS fattrs;
   SIZEF  charbox;

   nsFontHandleOS2();
   void SelectIntoPS( HPS hps, long lcid);
};

class nsFontMetricsOS2 : public nsIFontMetrics
{
 public:
   nsFontMetricsOS2();
   virtual ~nsFontMetricsOS2();

   NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

   NS_DECL_ISUPPORTS

   NS_IMETHOD Init( const nsFont& aFont, nsIDeviceContext *aContext);
   NS_IMETHOD Destroy();

   // Metrics
   NS_IMETHOD  GetXHeight( nscoord &aResult);
   NS_IMETHOD  GetSuperscriptOffset( nscoord &aResult);
   NS_IMETHOD  GetSubscriptOffset( nscoord &aResult);
   NS_IMETHOD  GetStrikeout( nscoord &aOffset, nscoord &aSize);
   NS_IMETHOD  GetUnderline( nscoord &aOffset, nscoord &aSize);

   NS_IMETHOD  GetHeight( nscoord &aHeight);
   NS_IMETHOD  GetLeading( nscoord &aLeading);
   NS_IMETHOD  GetMaxAscent( nscoord &aAscent);
   NS_IMETHOD  GetMaxDescent( nscoord &aDescent);
   NS_IMETHOD  GetMaxAdvance( nscoord &aAdvance);
   NS_IMETHOD  GetFont( const nsFont *&aFont);
//   NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
   NS_IMETHOD  GetFontHandle( nsFontHandle &aHandle);
//   NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);

   // for drawing text
   PRUint32 GetDevMaxAscender() const { return mDevMaxAscent; }
   nscoord  GetSpaceWidth( nsIRenderingContext *aRContext);
 
 protected:
   nsresult RealizeFont();
 
   nsFont  *mFont;
   nscoord  mSuperscriptYOffset;
   nscoord  mSubscriptYOffset;
   nscoord  mStrikeoutPosition;
   nscoord  mStrikeoutSize;
   nscoord  mUnderlinePosition;
   nscoord  mUnderlineSize;
   nscoord  mHeight;
   nscoord  mLeading;
   nscoord  mMaxAscent;
   nscoord  mMaxDescent;
   nscoord  mMaxAdvance;
   nscoord  mSpaceWidth;
   nscoord  mXHeight;

   PRUint32 mDevMaxAscent;

   nsFontHandleOS2    *mFontHandle;
   nsDeviceContextOS2 *mContext;    // sigh.. broken broken broken XP interfaces...
};

#endif
