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

#ifndef nsFontMetricsPS_h__
#define nsFontMetricsPS_h__

#include "gfx-config.h"
#include "nsIFontMetrics.h"
#include "nsAFMObject.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCompressedCharMap.h"
#include "nsPostScriptObj.h"
#ifdef MOZ_ENABLE_XFT
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_CACHE_H
#include FT_CACHE_IMAGE_H
#include FT_OUTLINE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H
#else
#ifdef MOZ_ENABLE_FREETYPE2
#include "nsIFontCatalogService.h"
#endif
#endif
#include "nsVoidArray.h"
#include "nsHashtable.h"

class nsPSFontGenerator;
class nsDeviceContextPS;
class nsRenderingContextPS;
class nsFontPS;

class nsFontMetricsPS : public nsIFontMetrics
{
public:
  nsFontMetricsPS();
  virtual ~nsFontMetricsPS();

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
  NS_IMETHOD  GetSpaceWidth(nscoord& aAveCharWidth);
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  NS_IMETHOD  GetStringWidth(const char *String,nscoord &aWidth,nscoord aLength);
  NS_IMETHOD  GetStringWidth(const PRUnichar *aString,nscoord &aWidth,nscoord aLength);
  
  inline void SetXHeight(nscoord aXHeight) { mXHeight = aXHeight; };
  inline void SetSuperscriptOffset(nscoord aSuperscriptOffset) { mSuperscriptOffset = aSuperscriptOffset; };
  inline void SetSubscriptOffset(nscoord aSubscriptOffset) { mSubscriptOffset = aSubscriptOffset; };
  inline void SetStrikeout(nscoord aOffset, nscoord aSize) { mStrikeoutOffset = aOffset; mStrikeoutSize = aSize; };
  inline void SetUnderline(nscoord aOffset, nscoord aSize) { mUnderlineOffset = aOffset; mUnderlineSize = aSize; };
  inline void SetHeight(nscoord aHeight) { mHeight = aHeight; };
  inline void SetLeading(nscoord aLeading) { mLeading = aLeading; };
  inline void SetAscent(nscoord aAscent) { mAscent = aAscent; };
  inline void SetDescent(nscoord aDescent) { mDescent = aDescent; };
  inline void SetEmHeight(nscoord aEmHeight) { mEmHeight = aEmHeight; };
  inline void SetEmAscent(nscoord aEmAscent) { mEmAscent = aEmAscent; };
  inline void SetEmDescent(nscoord aEmDescent) { mEmDescent = aEmDescent; };
  inline void SetMaxHeight(nscoord aMaxHeight) { mMaxHeight = aMaxHeight; };
  inline void SetMaxAscent(nscoord aMaxAscent) { mMaxAscent = aMaxAscent; };
  inline void SetMaxDescent(nscoord aMaxDescent) { mMaxDescent = aMaxDescent; };
  inline void SetMaxAdvance(nscoord aMaxAdvance) { mMaxAdvance = aMaxAdvance; };
  inline void SetAveCharWidth(nscoord aAveCharWidth) { mAveCharWidth = aAveCharWidth; };
  inline void SetSpaceWidth(nscoord aSpaceWidth) { mSpaceWidth = aSpaceWidth; };

  inline nsDeviceContextPS* GetDeviceContext() { return mDeviceContext; }
  inline nsFont* GetFont() { return mFont; };
  inline nsVoidArray* GetFontsPS() { return mFontsPS; };
  inline nsHashtable *GetFontsAlreadyLoadedList() {return mFontsAlreadyLoaded;};
  inline int GetFontPSState() { return mFontPSState; };
  inline void IncrementFontPSState() { mFontPSState++; };

#if defined(XP_WIN)
// this routine is defined here so the PostScript module can be debugged
// on the windows platform

  /**
   * Returns the average character width
   */
  NS_IMETHOD  GetAveCharWidth(nscoord& aAveCharWidth){return NS_OK;}
#endif


private:
  nsCOMPtr<nsIAtom> mLangGroup;

protected:
  void RealizeFont();

  nsDeviceContextPS   *mDeviceContext;
  nsFont              *mFont;
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
  nscoord             mAveCharWidth;

  nsVoidArray         *mFontsPS;
  nsHashtable         *mFontsAlreadyLoaded;
  int                 mFontPSState;
};

class nsFontPS
{
public:
  nsFontPS();
  nsFontPS(const nsFont& aFont, nsFontMetricsPS* aFontMetrics);
  virtual ~nsFontPS();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  static nsFontPS* FindFont(char aChar, const nsFont& aFont, 
                            nsFontMetricsPS* aFontMetrics);
  static nsFontPS* FindFont(PRUnichar aChar, const nsFont& aFont, 
                            nsFontMetricsPS* aFontMetrics);
  static nsPSFontGenerator* GetPSFontGenerator(nsFontMetricsPS* aFontMetrics,
                                               nsCStringKey& aKey);
  inline PRInt32 SupportsChar(PRUnichar aChar)
    { return mCCMap && CCMAP_HAS_CHAR(mCCMap, aChar); };

  virtual nscoord GetWidth(const char* aString, PRUint32 aLength) = 0;
  virtual nscoord GetWidth(const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual nscoord DrawString(nsRenderingContextPS* aContext,
                             nscoord aX, nscoord aY,
                             const char* aString, PRUint32 aLength) = 0;
  virtual nscoord DrawString(nsRenderingContextPS* aContext,
                             nscoord aX, nscoord aY,
                             const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual nsresult RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app) = 0;
  virtual nsresult SetupFont(nsRenderingContextPS* aContext) = 0;

#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(const char*        aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
  virtual nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#endif

protected:
  nsFont*                  mFont;
  PRUint16*                mCCMap;
  nsFontMetricsPS*         mFontMetrics;
};

class nsFontPSAFM : public nsFontPS
{
public:
  static nsFontPS* FindFont(const nsFont& aFont, nsFontMetricsPS* aFontMetrics);

  nsFontPSAFM(const nsFont& aFont, nsAFMObject* aAFMInfo,
              PRInt16 aFontIndex, nsFontMetricsPS* aFontMetrics);
  virtual ~nsFontPSAFM();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nscoord GetWidth(const char* aString, PRUint32 aLength);
  nscoord GetWidth(const PRUnichar* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const char* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const PRUnichar* aString, PRUint32 aLength);
  nsresult RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app);
  nsresult SetupFont(nsRenderingContextPS* aContext);

#ifdef MOZ_MATHML
  nsresult
  GetBoundingMetrics(const char*        aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
  nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#endif

  nsAFMObject* mAFMInfo;
  PRInt16      mFontIndex;
  nsString     mFamilyName;
};


#ifdef MOZ_ENABLE_XFT

#include <X11/Xft/Xft.h>

class nsXftEntry
{
public:
  nsXftEntry(FcPattern *aFontPattern);
  ~nsXftEntry() {}; 

  FT_Face       mFace;
  int           mFaceIndex;
  nsCString     mFontFileName;
  nsCString     mFamilyName;
  nsCString     mStyleName;

protected:
  nsXftEntry() {};
};

struct fontps {
  nsXftEntry *entry;
  nsFontPS   *fontps;
  FcCharSet  *charset;
};

struct fontPSInfo {
  nsVoidArray     *fontps;
  const nsFont*    nsfont;
  nsCAutoString    lang;
  nsHashtable     *alreadyLoaded;
  nsCStringArray   mFontList;
  nsAutoVoidArray  mFontIsGeneric;
  nsCString       *mGenericFont;
};

class nsFontPSXft : public nsFontPS
{
public:
  static nsFontPS* FindFont(PRUnichar aChar, const nsFont& aFont,
                            nsFontMetricsPS* aFontMetrics);
  nsresult         Init(nsXftEntry* aEntry,
                        nsPSFontGenerator* aPSFontGen);
  static PRBool CSSFontEnumCallback(const nsString& aFamily, PRBool aGeneric,
                                    void* aFpi);

  nsFontPSXft(const nsFont& aFont, nsFontMetricsPS* aFontMetrics);
  virtual ~nsFontPSXft();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nscoord GetWidth(const char* aString, PRUint32 aLength);
  nscoord GetWidth(const PRUnichar* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const char* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const PRUnichar* aString, PRUint32 aLength);
  nsresult RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app);
  nsresult SetupFont(nsRenderingContextPS* aContext);

#ifdef MOZ_MATHML
  nsresult
  GetBoundingMetrics(const char*        aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
  nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#endif

  nsXftEntry *mEntry;
  FT_Face getFTFace();

protected:
  PRUint16        mPixelSize;
  FTC_Image_Desc  mImageDesc;
  FT_Library      mFreeTypeLibrary;
  FTC_Manager     mFTCacheManager;
  FTC_Image_Cache mImageCache;

  int     ascent();
  int     descent();
  PRBool  getXHeight(unsigned long &aVal);
  int     max_ascent();
  int     max_descent();
  int     max_width();
  PRBool  superscript_y(long &aVal);
  PRBool  subscript_y(long &aVal);
  PRBool  underlinePosition(long &aVal);
  PRBool  underline_thickness(unsigned long &aVal);
  nsPSFontGenerator*  mPSFontGenerator;
};

#else

#ifdef MOZ_ENABLE_FREETYPE2
#include "nsIFreeType2.h"

typedef struct {
  nsITrueTypeFontCatalogEntry *entry;
  nsFontPS *fontps;
  unsigned short *ccmap;
} fontps;

typedef struct {
  nsVoidArray *fontps;
  const nsFont* nsfont;
  nsCAutoString lang;
  nsHashtable *alreadyLoaded;
  PRUint16 slant;
  PRUint16 weight;
} fontPSInfo;

class nsFontPSFreeType : public nsFontPS
{
public:
  static nsFontPS* FindFont(PRUnichar aChar, const nsFont& aFont,
                            nsFontMetricsPS* aFontMetrics);
  static nsresult  FindFontEntry(const nsFont& aFont, nsIAtom* aLanguage,
                                 nsITrueTypeFontCatalogEntry** aEntry);
  static nsresult  AddFontEntries(nsACString& aFamilyName,
                                  nsACString& aLanguage,
                                  PRUint16 aWeight, PRUint16 aWidth,
                                  PRUint16 aSlant, PRUint16 aSpacing,
                                  fontPSInfo* aFpi);
  static PRBool CSSFontEnumCallback(const nsString& aFamily, PRBool aGeneric,
                                    void* aFpi);
  nsresult         Init(nsITrueTypeFontCatalogEntry* aEntry,
                        nsPSFontGenerator* aPSFontGen);

  nsFontPSFreeType(const nsFont& aFont, nsFontMetricsPS* aFontMetrics);
  virtual ~nsFontPSFreeType();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nscoord GetWidth(const char* aString, PRUint32 aLength);
  nscoord GetWidth(const PRUnichar* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const char* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const PRUnichar* aString, PRUint32 aLength);
  nsresult RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app);
  nsresult SetupFont(nsRenderingContextPS* aContext);

#ifdef MOZ_MATHML
  nsresult
  GetBoundingMetrics(const char*        aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
  nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#endif

  nsCOMPtr<nsITrueTypeFontCatalogEntry> mEntry;
  FT_Face getFTFace();

protected:
  nsCOMPtr<nsITrueTypeFontCatalogEntry> mFaceID;
  nsCOMPtr<nsIFreeType2> mFt2;
  PRUint16        mPixelSize;
  FTC_Image_Desc  mImageDesc;


  static PRBool AddUserPref(nsIAtom *aLang, const nsFont& aFont,
                            fontPSInfo *aFpi);
  int     ascent();
  int     descent();
  PRBool  getXHeight(unsigned long &aVal);
  int     max_ascent();
  int     max_descent();
  int     max_width();
  PRBool  superscript_y(long &aVal);
  PRBool  subscript_y(long &aVal);
  PRBool  underlinePosition(long &aVal);
  PRBool  underline_thickness(unsigned long &aVal);
  nsPSFontGenerator*  mPSFontGenerator;
};

#else // !FREETYPE2 && !XFT
typedef struct {
  nsFontPS   *fontps;
} fontps;
#endif // MOZ_ENABLE_FREETYPE2
#endif   // MOZ_ENABLE_XFT

class nsPSFontGenerator {
public:
  nsPSFontGenerator();
  virtual ~nsPSFontGenerator();
  virtual void  GeneratePSFont(FILE* aFile);
  void  AddToSubset(const PRUnichar* aString, PRUint32 aLength);
  void  AddToSubset(const char* aString, PRUint32 aLength);

protected:
  nsString mSubset;
};


#ifdef MOZ_ENABLE_XFT

class nsXftType8Generator : public nsPSFontGenerator {
public:
  nsXftType8Generator();
  ~nsXftType8Generator();
  nsresult Init(nsXftEntry* aFce);
  void  GeneratePSFont(FILE* aFile);

protected:
  nsXftEntry *mEntry;
  FTC_Image_Desc  mImageDesc;
  FT_Library      mFreeTypeLibrary;
  FTC_Manager     mFTCacheManager;
};
#else
#ifdef MOZ_ENABLE_FREETYPE2
class nsFT2Type8Generator : public nsPSFontGenerator {
public:
  nsFT2Type8Generator();
  ~nsFT2Type8Generator();
  nsresult Init(nsITrueTypeFontCatalogEntry* aFce);
  void  GeneratePSFont(FILE* aFile);

protected:
  nsCOMPtr<nsITrueTypeFontCatalogEntry> mEntry;
  nsCOMPtr<nsIFreeType2> mFt2;
  FTC_Image_Desc  mImageDesc;
};
#endif   // MOZ_ENABLE_FREETYPE2
#endif   // MOZ_ENABLE_XFT

#endif

