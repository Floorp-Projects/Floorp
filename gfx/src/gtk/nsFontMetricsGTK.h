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

#ifndef nsFontMetricsGTK_h__
#define nsFontMetricsGTK_h__

#include "nsDeviceContextGTK.h"
#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsRenderingContextGTK.h"
#include "nsICharRepresentable.h"
#include "nsCompressedCharMap.h"
#include "nsIFontMetricsGTK.h"
#include "nsIFontCatalogService.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#undef FONT_HAS_GLYPH
#define FONT_HAS_GLYPH(map, char) IS_REPRESENTABLE(map, char)
#define WEIGHT_INDEX(weight) (((weight) / 100) - 1)

typedef struct nsFontCharSetInfo nsFontCharSetInfo;

typedef gint (*nsFontCharSetConverter)(nsFontCharSetInfo* aSelf,
  XFontStruct* aFont, const PRUnichar* aSrcBuf, PRInt32 aSrcLen,
  char* aDestBuf, PRInt32 aDestLen);

struct nsFontCharSet;
struct nsFontFamily;
struct nsFontNode;
struct nsFontStretch;
struct nsFontWeight;
class nsXFont;

class nsFontGTKUserDefined;
class nsFontMetricsGTK;
class nsFreeTypeFace;
class nsFontGTK;

struct nsFontStretch
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void SortSizes(void);

  nsFontGTK**        mSizes;
  PRUint16           mSizesAlloc;
  PRUint16           mSizesCount;

  char*              mScalable;
  PRBool             mOutlineScaled;
  nsVoidArray        mScaledFonts;
  nsITrueTypeFontCatalogEntry*   mFreeTypeFaceID;
};

struct nsFontStyle
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillWeightHoles(void);

  nsFontWeight* mWeights[9];
};

struct nsFontWeight
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStretchHoles(void);

  nsFontStretch* mStretches[9];
};

struct nsFontNode
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStyleHoles(void);

  nsCAutoString      mName;
  nsFontCharSetInfo* mCharSetInfo;
  nsFontStyle*       mStyles[3];
  PRUint8            mHolesFilled;
  PRUint8            mDummy;
};

class nsFontNodeArray : public nsAutoVoidArray
{
public:
  nsFontNodeArray() {};

  nsFontNode* GetElement(PRInt32 aIndex)
  {
    return (nsFontNode*) ElementAt(aIndex);
  };
};

/*
 * Font Language Groups
 *
 * These Font Language Groups (FLG) indicate other related
 * encodings to look at when searching for glyphs
 *
 */
typedef struct nsFontLangGroup {
  const char *mFontLangGroupName;
  nsIAtom*    mFontLangGroupAtom;
} nsFontLangGroup;

struct nsFontCharSetMap
{
  const char*        mName;
  nsFontLangGroup*   mFontLangGroup;
  nsFontCharSetInfo* mInfo;
};

class nsFontGTK
{
public:
  nsFontGTK();
  nsFontGTK(nsFontGTK*);
  virtual ~nsFontGTK();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void LoadFont(void);
  PRBool IsEmptyFont(XFontStruct*);

  inline int SupportsChar(PRUint32 aChar)
    { return mCCMap && CCMAP_HAS_CHAR_EXT(mCCMap, aChar); };

  virtual GdkFont* GetGDKFont(void);
  virtual nsXFont* GetXFont(void);
  virtual PRBool   GetXFontIs10646(void);
  virtual PRBool   IsFreeTypeFont(void);
  virtual gint GetWidth(const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual gint DrawString(nsRenderingContextGTK* aContext,
                          nsDrawingSurfaceGTK* aSurface, nscoord aX,
                          nscoord aY, const PRUnichar* aString,
                          PRUint32 aLength) = 0;
#ifdef MOZ_MATHML
  // bounding metrics for a string 
  // remember returned values are not in app units 
  // - to emulate GetWidth () above
  virtual nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#endif

  PRUint16*              mCCMap;
  nsFontCharSetInfo*     mCharSetInfo;
  char*                  mName;
  nsFontGTKUserDefined*  mUserDefinedFont;
  PRUint16               mSize;
  PRUint16               mAABaseSize;
  PRInt16                mBaselineAdjust;

  // these values are not in app units, they need to be scaled with 
  // nsIDeviceContext::DevUnitsToAppUnits()
  PRInt16                mMaxAscent;
  PRInt16                mMaxDescent;

protected:
  GdkFont*               mFont;
  GdkFont*               mFontHolder;
  nsXFont*               mXFont;
  PRBool                 mAlreadyCalledLoadFont;
};

struct nsFontSwitchGTK {
  // Simple wrapper on top of nsFontGTK for the moment
  // Could hold other attributes of the font
  nsFontGTK* mFontGTK;
};

typedef PRBool (*PR_CALLBACK nsFontSwitchCallbackGTK)
               (const nsFontSwitchGTK *aFontSwitch,
                const PRUnichar       *aSubstring,
                PRUint32               aSubstringLength,
                void                  *aData);

class nsFontMetricsGTK : public nsIFontMetricsGTK
{
public:
  nsFontMetricsGTK();
  virtual ~nsFontMetricsGTK();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(const nsFont& aFont, nsIAtom* aLangGroup,
                   nsIDeviceContext* aContext);
  NS_IMETHOD  Destroy();

  NS_IMETHOD  GetXHeight(nscoord& aResult);
  NS_IMETHOD  GetSuperscriptOffset(nscoord& aResult);
  NS_IMETHOD  GetSubscriptOffset(nscoord& aResult);
  NS_IMETHOD  GetStrikeout(nscoord& aOffset, nscoord& aSize);
  NS_IMETHOD  GetUnderline(nscoord& aOffset, nscoord& aSize);

  NS_IMETHOD  GetHeight(nscoord &aHeight);
  NS_IMETHOD  GetNormalLineHeight(nscoord &aHeight);
  NS_IMETHOD  GetLeading(nscoord &aLeading);
  NS_IMETHOD  GetEmHeight(nscoord &aHeight);
  NS_IMETHOD  GetEmAscent(nscoord &aAscent);
  NS_IMETHOD  GetEmDescent(nscoord &aDescent);
  NS_IMETHOD  GetMaxHeight(nscoord &aHeight);
  NS_IMETHOD  GetMaxAscent(nscoord &aAscent);
  NS_IMETHOD  GetMaxDescent(nscoord &aDescent);
  NS_IMETHOD  GetMaxAdvance(nscoord &aAdvance);
  NS_IMETHOD  GetAveCharWidth(nscoord &aAveCharWidth);
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  
  NS_IMETHOD  GetSpaceWidth(nscoord &aSpaceWidth);
  NS_IMETHOD  ResolveForwards(const PRUnichar* aString, PRUint32 aLength,
                              nsFontSwitchCallbackGTK aFunc, void* aData);

  nsFontGTK*  FindFont(PRUint32 aChar);
  nsFontGTK*  FindUserDefinedFont(PRUint32 aChar);
  nsFontGTK*  FindStyleSheetSpecificFont(PRUint32 aChar);
  nsFontGTK*  FindStyleSheetGenericFont(PRUint32 aChar);
  nsFontGTK*  FindLangGroupPrefFont(nsIAtom* aLangGroup, PRUint32 aChar);
  nsFontGTK*  FindLangGroupFont(nsIAtom* aLangGroup, PRUint32 aChar, nsCString* aName);
  nsFontGTK*  FindAnyFont(PRUint32 aChar);
  nsFontGTK*  FindSubstituteFont(PRUint32 aChar);

  nsFontGTK*  SearchNode(nsFontNode* aNode, PRUint32 aChar);
  nsFontGTK*  TryAliases(nsCString* aName, PRUint32 aChar);
  nsFontGTK*  TryFamily(nsCString* aName, PRUint32 aChar);
  nsFontGTK*  TryNode(nsCString* aName, PRUint32 aChar);
  nsFontGTK*  TryNodes(nsACString &aFFREName, PRUint32 aChar);
  nsFontGTK*  TryLangGroup(nsIAtom* aLangGroup, nsCString* aName, PRUint32 aChar);

  nsFontGTK*  AddToLoadedFontsList(nsFontGTK* aFont);
  nsFontGTK*  FindNearestSize(nsFontStretch* aStretch, PRUint16 aSize);
  nsFontGTK*  GetAASBBaseFont(nsFontStretch* aStretch, 
                              nsFontCharSetInfo* aCharSet);
  nsFontGTK*  PickASizeAndLoad(nsFontStretch* aStretch,
                               nsFontCharSetInfo* aCharSet, 
                               PRUint32 aChar,
                               const char *aName);

  // nsIFontMetricsGTK (calls from the font rendering layer)
  virtual nsresult GetWidth(const char* aString, PRUint32 aLength,
                            nscoord& aWidth,
                            nsRenderingContextGTK *aContext);
  virtual nsresult GetWidth(const PRUnichar* aString, PRUint32 aLength,
                            nscoord& aWidth, PRInt32 *aFontID,
                            nsRenderingContextGTK *aContext);
  
  virtual nsresult GetTextDimensions(const PRUnichar* aString,
                                     PRUint32 aLength,
                                     nsTextDimensions& aDimensions, 
                                     PRInt32* aFontID,
                                     nsRenderingContextGTK *aContext);
  virtual nsresult GetTextDimensions(const char*         aString,
                                     PRInt32             aLength,
                                     PRInt32             aAvailWidth,
                                     PRInt32*            aBreaks,
                                     PRInt32             aNumBreaks,
                                     nsTextDimensions&   aDimensions,
                                     PRInt32&            aNumCharsFit,
                                     nsTextDimensions&   aLastWordDimensions,
                                     PRInt32*            aFontID,
                                     nsRenderingContextGTK *aContext);
  virtual nsresult GetTextDimensions(const PRUnichar*    aString,
                                     PRInt32             aLength,
                                     PRInt32             aAvailWidth,
                                     PRInt32*            aBreaks,
                                     PRInt32             aNumBreaks,
                                     nsTextDimensions&   aDimensions,
                                     PRInt32&            aNumCharsFit,
                                     nsTextDimensions&   aLastWordDimensions,
                                     PRInt32*            aFontID,
                                     nsRenderingContextGTK *aContext);

  virtual nsresult DrawString(const char *aString, PRUint32 aLength,
                              nscoord aX, nscoord aY,
                              const nscoord* aSpacing,
                              nsRenderingContextGTK *aContext,
                              nsDrawingSurfaceGTK *aSurface);
  virtual nsresult DrawString(const PRUnichar* aString, PRUint32 aLength,
                              nscoord aX, nscoord aY,
                              PRInt32 aFontID,
                              const nscoord* aSpacing,
                              nsRenderingContextGTK *aContext,
                              nsDrawingSurfaceGTK *aSurface);

#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                      nsBoundingMetrics &aBoundingMetrics,
                                      nsRenderingContextGTK *aContext);
  virtual nsresult GetBoundingMetrics(const PRUnichar *aString,
                                      PRUint32 aLength,
                                      nsBoundingMetrics &aBoundingMetrics,
                                      PRInt32 *aFontID,
                                      nsRenderingContextGTK *aContext);
#endif /* MOZ_MATHML */

  virtual GdkFont* GetCurrentGDKFont(void);

  virtual nsresult SetRightToLeftText(PRBool aIsRTL);

  static nsresult FamilyExists(nsIDeviceContext *aDevice, const nsString& aName);
  static PRUint32 GetHints(void);

  //friend struct nsFontGTK;

  nsFontGTK   **mLoadedFonts;
  PRUint16    mLoadedFontsAlloc;
  PRUint16    mLoadedFontsCount;

  nsFontGTK   *mSubstituteFont;

  nsCStringArray mFonts;
  PRUint16       mFontsIndex;
  nsAutoVoidArray   mFontIsGeneric;

  nsCAutoString     mDefaultFont;
  nsCString         *mGeneric;
  nsCOMPtr<nsIAtom> mLangGroup;
  nsCAutoString     mUserDefined;

  PRUint8 mTriedAllGenerics;
  PRUint8 mIsUserDefined;

protected:
  void RealizeFont();
  nsFontGTK* LocateFont(PRUint32 aChar, PRInt32 & aCount);

  nsIDeviceContext    *mDeviceContext;
  nsFont              *mFont;
  nsFontGTK           *mWesternFont;
  nsFontGTK           *mCurrentFont;

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
  nscoord             mAveCharWidth;

  PRUint16            mPixelSize;
  PRUint8             mStretchIndex;
  PRUint8             mStyleIndex;
  nsFontCharSetConverter mDocConverterType;
};

class nsFontEnumeratorGTK : public nsIFontEnumerator
{
public:
  nsFontEnumeratorGTK();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTENUMERATOR
};

class nsHashKey;
PRBool FreeNode(nsHashKey* aKey, void* aData, void* aClosure);
nsFontCharSetInfo *GetCharSetInfo(const char *aCharSetName);
void CharSetNameToCodeRangeBits(const char*, PRUint32*, PRUint32*);
nsFontCharSetMap *GetCharSetMap(const char *aCharSetName);




#endif
