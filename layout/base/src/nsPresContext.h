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
#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "nsIPresContext.h"
#include "nsIDeviceContext.h"
#include "nsVoidArray.h"
#include "nsFont.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsICharsetConverterManager.h"
#include "nsILanguageAtomService.h"
#include "nsIURL.h"
#include "nsIObserver.h"
#ifdef IBMBIDI
#include "nsBidiUtils.h"
#endif

#include "nsHashtable.h"
#include "nsIContent.h"
#include "nsITheme.h"
#include "nsWeakReference.h"

// Base class for concrete presentation context classes
class nsPresContext : public nsIPresContext, public nsIObserver {
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIPresContext methods
  NS_IMETHOD Init(nsIDeviceContext* aDeviceContext);
  NS_IMETHOD SetShell(nsIPresShell* aShell);
  virtual void SetCompatibilityMode(nsCompatibility aMode);
  virtual void SetImageAnimationMode(PRUint16 aMode);
  virtual void ClearStyleDataAndReflow();

  virtual nsresult GetXBLBindingURL(nsIContent* aContent, nsIURI** aResult);
  NS_IMETHOD GetMetricsFor(const nsFont& aFont, nsIFontMetrics** aResult);
  virtual const nsFont* GetDefaultFont(PRUint8 aFontID) const;

  virtual nsresult LoadImage(imgIRequest* aImage,
                             nsIFrame* aTargetFrame,
                             imgIRequest **aRequest);

  virtual void StopImagesFor(nsIFrame* aTargetFrame);
  virtual void SetContainer(nsISupports* aContainer);
  virtual already_AddRefed<nsISupports>  GetContainer();
  virtual void GetPageDim(nsRect* aActualRect, nsRect* aAdjRect) = 0;
  virtual void SetPageDim(nsRect* aRect) = 0;
  NS_IMETHOD GetTwipsToPixelsForFonts(float* aResult) const;
  NS_IMETHOD GetScaledPixelsToTwips(float* aScale) const;

#ifdef MOZ_REFLOW_PERF
  NS_IMETHOD CountReflows(const char * aName, PRUint32 aType, nsIFrame * aFrame);
  NS_IMETHOD PaintCount(const char * aName, nsIRenderingContext* aRenderingContext, nsIFrame * aFrame, PRUint32 aColor);
#endif

  // nsIObserver method
  NS_IMETHOD Observe(nsISupports* aSubject, 
                     const char* aTopic,
                     const PRUnichar* aData);

#ifdef IBMBIDI
  virtual PRBool BidiEnabled() const;
  virtual void SetBidiEnabled(PRBool aBidiEnabled) const;
  NS_IMETHOD GetBidiUtils(nsBidiPresUtils** aBidiUtils);
  NS_IMETHOD SetBidi(PRUint32 aSource, PRBool aForceReflow = PR_FALSE);
  NS_IMETHOD GetBidi(PRUint32* aDest) const;
#endif // IBMBIDI

  NS_IMETHOD GetTheme(nsITheme** aResult);
  NS_IMETHOD ThemeChanged();
  NS_IMETHOD SysColorChanged();

protected:
  nsPresContext();
  virtual ~nsPresContext();

  nsCOMPtr<nsILanguageAtomService> mLangService;
  nsWeakPtr             mContainer;

  nsFont                mDefaultVariableFont;
  nsFont                mDefaultFixedFont;
  nsFont                mDefaultSerifFont;
  nsFont                mDefaultSansSerifFont;
  nsFont                mDefaultMonospaceFont;
  nsFont                mDefaultCursiveFont;
  nsFont                mDefaultFantasyFont;

  nsSupportsHashtable   mImageLoaders;

  PRBool                mEnableJapaneseTransform;

#ifdef IBMBIDI
  nsBidiPresUtils*      mBidiUtils;
  PRUint32              mBidi;
  nsCAutoString         mCharset;                 // the charset we are using
#endif // IBMBIDI

#ifdef DEBUG
  PRBool                mInitialized;
#endif

  PRUint16      mImageAnimationModePref;

  nsCOMPtr<nsITheme> mTheme;

protected:
  void   GetUserPreferences();
  void   GetFontPreferences();
  void   GetDocumentColorPreferences();
  void   UpdateCharSet(const char* aCharSet);
  void SetImgAnimations(nsIContent *aParent, PRUint16 aMode);

private:
  static int PR_CALLBACK PrefChangedCallback(const char*, void*);
  void   PreferenceChanged(const char* aPrefName);

  // these are private, use the list in nsFont.h if you want a public list
  enum {
    eDefaultFont_Variable,
    eDefaultFont_Fixed,
    eDefaultFont_Serif,
    eDefaultFont_SansSerif,
    eDefaultFont_Monospace,
    eDefaultFont_Cursive,
    eDefaultFont_Fantasy,
    eDefaultFont_COUNT
  };
};

#endif /* nsPresContext_h___ */
