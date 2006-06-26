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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsFontMetricsPh_h__
#define nsFontMetricsPh_h__

#include <Pt.h>

#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsDeviceContextPh.h"
#include "nsCOMPtr.h"

class nsFontMetricsPh : public nsIFontMetrics
{
public:
  nsFontMetricsPh();
  virtual ~nsFontMetricsPh();

   NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(const nsFont& aFont, nsIAtom* aLangGroup,
                   nsIDeviceContext* aContext);

	inline
  NS_IMETHODIMP GetLangGroup(nsIAtom** aLangGroup)
		{
		if( !aLangGroup ) return NS_ERROR_NULL_POINTER;
		*aLangGroup = mLangGroup;
		NS_IF_ADDREF(*aLangGroup);
		return NS_OK;
		}

  NS_IMETHODIMP  Destroy()
		{
		mDeviceContext = nsnull;
		return NS_OK;
		}

  inline NS_IMETHODIMP  GetXHeight(nscoord& aResult)
		{
		aResult = mXHeight;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetSuperscriptOffset(nscoord& aResult)
		{
		aResult = mSuperscriptOffset;
		return NS_OK;
		}
  inline NS_IMETHOD  GetSubscriptOffset(nscoord& aResult)
		{
		aResult = mSubscriptOffset;
		return NS_OK;
		}
  inline NS_IMETHOD  GetStrikeout(nscoord& aOffset, nscoord& aSize)
		{
		aOffset = mStrikeoutOffset;
		aSize = mStrikeoutSize;
		return NS_OK;
		}
  inline NS_IMETHOD  GetUnderline(nscoord& aOffset, nscoord& aSize)
		{
		aOffset = mUnderlineOffset;
		aSize = mUnderlineSize;
		return NS_OK;
		}

  inline NS_IMETHODIMP  GetHeight(nscoord &aHeight)
		{
		aHeight = mHeight;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetNormalLineHeight(nscoord &aHeight)
		{
		aHeight = mEmHeight + mLeading;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetLeading(nscoord &aLeading)
		{
		aLeading = mLeading;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetEmHeight(nscoord &aHeight)
		{
		aHeight = mEmHeight;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetEmAscent(nscoord &aAscent)
		{
		aAscent = mEmAscent;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetEmDescent(nscoord &aDescent)
		{
		aDescent = mEmDescent;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetMaxHeight(nscoord &aHeight)
		{
		aHeight = mMaxHeight;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetMaxAscent(nscoord &aAscent)
		{
		aAscent = mMaxAscent;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetMaxDescent(nscoord &aDescent)
		{
		aDescent = mMaxDescent;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetMaxAdvance(nscoord &aAdvance)
		{
		aAdvance = mMaxAdvance;
		return NS_OK;
		}
	inline NS_IMETHODIMP  GetAveCharWidth(nscoord &aAveCharWidth)
		{
		aAveCharWidth = mAveCharWidth;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetFontHandle(nsFontHandle &aHandle)
		{
		aHandle = (nsFontHandle) mFontHandle;
		return NS_OK;
		}
  inline NS_IMETHODIMP  GetSpaceWidth(nscoord &aSpaceWidth)
		{
		aSpaceWidth = mSpaceWidth;
		return NS_OK;
		}
  // No known string length limits on Photon
  virtual PRInt32 GetMaxStringLength() { return PR_INT32_MAX; }
  
protected:
  void RealizeFont();

  nsIDeviceContext    *mDeviceContext;
  char                *mFontHandle;		/* Photon Fonts are just a string */
  nscoord             mHeight;
  nscoord             mAscent;
  nscoord             mDescent;
  nscoord             mLeading;
  nscoord             mEmHeight;
  nscoord             mEmAscent;
  nscoord             mEmDescent;
  nscoord             mMaxHeight;
  nscoord             mMaxAscent;
  nscoord             mMaxDescent;
  nscoord             mMaxAdvance;
  nscoord             mXHeight;
  nscoord             mSuperscriptOffset;
  nscoord             mSubscriptOffset;
  nscoord             mStrikeoutSize;
  nscoord             mStrikeoutOffset;
  nscoord             mUnderlineSize;
  nscoord             mUnderlineOffset;
  nscoord             mSpaceWidth;
  nscoord			mAveCharWidth;
  // No known string length limits on Photon
  virtual PRInt32 GetMaxStringLength() { return PR_INT32_MAX; }

  nsCOMPtr<nsIAtom>   mLangGroup;
};

class nsFontEnumeratorPh : public nsIFontEnumerator
{
public:
  nsFontEnumeratorPh();
  NS_DECL_ISUPPORTS
//  NS_DECL_NSIFONTENUMERATOR

	inline NS_IMETHODIMP HaveFontFor(const char* aLangGroup, PRBool* aResult)
		{
		  NS_ENSURE_ARG_POINTER(aResult);
		  *aResult = PR_TRUE; // always return true for now.
		  return NS_OK;
		}


	inline NS_IMETHODIMP GetDefaultFont(const char *aLangGroup, const char *aGeneric, PRUnichar **aResult)
		{
		  // aLangGroup=null or ""  means any (i.e., don't care)
		  // aGeneric=null or ""  means any (i.e, don't care)
		  NS_ENSURE_ARG_POINTER(aResult);
		  *aResult = nsnull;
		  return NS_OK;
		}

	inline NS_IMETHODIMP UpdateFontList(PRBool *updateFontList)
		{
		  *updateFontList = PR_FALSE; // always return false for now
		  return NS_OK;
		}

	inline NS_IMETHODIMP EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
		{
		  return EnumerateFonts( nsnull, nsnull, aCount, aResult );
		}

	NS_IMETHOD EnumerateFonts( const char* aLangGroup, const char* aGeneric,
								PRUint32* aCount, PRUnichar*** aResult );

};

#endif
