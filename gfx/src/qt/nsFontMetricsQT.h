/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *		John C. Griggs <jcgriggs@sympatico.ca>
 *		Jean Claude Batista <jcb@macadamian.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFontMetricsQT_h__
#define nsFontMetricsQT_h__

#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsICharRepresentable.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsVoidArray.h"

#include <qintdict.h>
#include <qfontmetrics.h>

class nsFont;
class nsString;
class nsRenderingContextQT;
class nsDrawingSurfaceQT;
class nsFontMetricsQT;
class nsFontQTUserDefined;
class QString;
class QFont;
class QFontInfo;
class QFontDatabase;

#undef FONT_HAS_GLYPH
#define FONT_HAS_GLYPH(map,char) IS_REPRESENTABLE(map,char)

typedef struct nsFontCharSetInfo nsFontCharSetInfo;
 
class nsFontQT
{
public:
  nsFontQT();
  nsFontQT(QFont *aQFont);
  virtual ~nsFontQT();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
 
  virtual QFont   *GetQFont(void);
  virtual PRUint32 *GetCharSetMap();
  PRBool          IsUnicodeFont();
 
  inline int HasChar(PRUnichar aChar)
  {
    return(mFontMetrics && mFontMetrics->inFont(QChar(aChar)));
  };
  virtual int SupportsChar(PRUnichar aChar);
 
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual int DrawString(nsRenderingContextQT* aContext,
                         nsDrawingSurfaceQT* aSurface, nscoord aX,
                         nscoord aY, const PRUnichar* aString,
                         PRUint32 aLength) = 0;
#ifdef MOZ_MATHML
  // bounding metrics for a string
  // remember returned values are not in app units
  // - to emulate GetWidth () above
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics) = 0;
#endif

  QFont*                 mFont;
  QFontInfo*             mFontInfo;
  QFontMetrics*          mFontMetrics;
  nsFontCharSetInfo*     mCharSetInfo;
};

class nsFontMetricsQT : public nsIFontMetrics
{
public:
    nsFontMetricsQT();
    virtual ~nsFontMetricsQT();

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
    NS_IMETHOD  GetFont(const nsFont *&aFont);
    NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
    NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);

    NS_IMETHOD  GetSpaceWidth(nscoord &aSpaceWidth);
 
    nsFontQT*  FindFont(PRUnichar aChar);
    nsFontQT*  FindUserDefinedFont(PRUnichar aChar);
    nsFontQT*  FindLangGroupPrefFont(nsIAtom *aLangGroup,PRUnichar aChar);
    nsFontQT*  FindLocalFont(PRUnichar aChar);
    nsFontQT*  FindGenericFont(PRUnichar aChar);
    nsFontQT*  FindGlobalFont(PRUnichar aChar);
    nsFontQT*  FindSubstituteFont(PRUnichar aChar); 

    nsFontQT*  LookUpFontPref(nsCAutoString &aName,PRUnichar aChar);
    nsFontQT*  LoadFont(QString &aName,PRUnichar aChar);
    nsFontQT*  LoadFont(QString &aName,const QString &aCharSet,
                        PRUnichar aChar);
    QFont*     LoadQFont(QString &aName,const QString &aCharSet);

    static nsresult FamilyExists(const nsString& aFontName);

    nsFontQT    **mLoadedFonts;
    PRUint16    mLoadedFontsAlloc;
    PRUint16    mLoadedFontsCount;

    nsFontQT               *mSubstituteFont;
    nsFontQTUserDefined    *mUserDefinedFont;

    nsCOMPtr<nsIAtom> mLangGroup;
    nsCStringArray    mFonts;
    PRUint16          mFontsIndex;
    nsVoidArray       mFontIsGeneric;
    nsCAutoString     mDefaultFont;
    nsCString         *mGeneric; 
    nsCAutoString     mUserDefined;

    PRUint8 mTriedAllGenerics;
    PRUint8 mIsUserDefined;

    static QFontDatabase *GetQFontDB();

protected:
    void RealizeFont();

    nsIDeviceContext *mDeviceContext;
    nsFont           *mFont;
    nsFontQT         *mWesternFont; 

    QString          *mQStyle;
    PRUint16         mPixelSize;
    PRUint16         mWeight;

    QIntDict<char>   mCharSubst;

    nscoord          mLeading;
    nscoord          mEmHeight;
    nscoord          mEmAscent;
    nscoord          mEmDescent;
    nscoord          mMaxHeight;
    nscoord          mMaxAscent;
    nscoord          mMaxDescent;
    nscoord          mMaxAdvance;
    nscoord          mXHeight;
    nscoord          mSuperscriptOffset;
    nscoord          mSubscriptOffset;
    nscoord          mStrikeoutSize;
    nscoord          mStrikeoutOffset;
    nscoord          mUnderlineSize;
    nscoord          mUnderlineOffset;
    nscoord          mSpaceWidth;

    static QFontDatabase    *mQFontDB;
};

class nsFontEnumeratorQT : public nsIFontEnumerator
{
public:
  nsFontEnumeratorQT();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTENUMERATOR
};

#endif
