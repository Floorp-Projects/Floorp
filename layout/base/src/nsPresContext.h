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
 *   IBM Corporation 
 * 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/20/2000   IBM Corp.       BiDi - ability to change the default direction of the browser
 * 04/20/2000   IBM Corp.       OS/2 VisualAge build.
 *
 */
#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "nsIPresContext.h"
#include "nsIDeviceContext.h"
#include "nsVoidArray.h"
#include "nsFont.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsIImageGroup.h"
#include "nsIPref.h"
#include "nsICharsetConverterManager.h"
#include "nsILanguageAtomService.h"
#include "nsIURL.h"
#include "nsIEventStateManager.h"
#include "nsIObserver.h"
#include "nsILookAndFeel.h"

// Base class for concrete presentation context classes
class nsPresContext : public nsIPresContext, public nsIObserver {
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIPresContext methods
  NS_IMETHOD Init(nsIDeviceContext* aDeviceContext);
  NS_IMETHOD Stop(void);
  NS_IMETHOD SetShell(nsIPresShell* aShell);
  NS_IMETHOD GetShell(nsIPresShell** aResult);
  NS_IMETHOD GetCompatibilityMode(nsCompatibility* aModeResult);
  NS_IMETHOD SetCompatibilityMode(nsCompatibility aMode);
  NS_IMETHOD GetWidgetRenderingMode(nsWidgetRendering* aModeResult);
  NS_IMETHOD SetWidgetRenderingMode(nsWidgetRendering aMode);
  NS_IMETHOD GetImageAnimationMode(nsImageAnimation* aModeResult);
  NS_IMETHOD SetImageAnimationMode(nsImageAnimation aMode);
  NS_IMETHOD GetLookAndFeel(nsILookAndFeel** aLookAndFeel);
  NS_IMETHOD GetBaseURL(nsIURI** aURLResult);
  NS_IMETHOD GetMedium(nsIAtom** aMediumResult) = 0;
  NS_IMETHOD RemapStyleAndReflow(void);
  NS_IMETHOD ResolveStyleContextFor(nsIContent* aContent,
                                    nsIStyleContext* aParentContext,
                                    PRBool aForceUnique,
                                    nsIStyleContext** aResult);
  NS_IMETHOD ResolvePseudoStyleContextFor(nsIContent* aParentContent,
                                          nsIAtom* aPseudoTag,
                                          nsIStyleContext* aParentContext,
                                          PRBool aForceUnique,
                                          nsIStyleContext** aResult);
  NS_IMETHOD ProbePseudoStyleContextFor(nsIContent* aParentContent,
                                        nsIAtom* aPseudoTag,
                                        nsIStyleContext* aParentContext,
                                        PRBool aForceUnique,
                                        nsIStyleContext** aResult);
  NS_IMETHOD ReParentStyleContext(nsIFrame* aFrame, 
                                  nsIStyleContext* aNewParentContext);
  NS_IMETHOD GetMetricsFor(const nsFont& aFont, nsIFontMetrics** aResult);
  NS_IMETHOD GetDefaultFont(nsFont& aResult);
  NS_IMETHOD SetDefaultFont(const nsFont& aFont);
  virtual const nsFont& GetDefaultFontDeprecated();
  NS_IMETHOD GetDefaultFixedFont(nsFont& aResult);
  NS_IMETHOD SetDefaultFixedFont(const nsFont& aFont);
  virtual const nsFont& GetDefaultFixedFontDeprecated();
  NS_IMETHOD GetFontScaler(PRInt32* aResult);
  NS_IMETHOD SetFontScaler(PRInt32 aScaler);
  NS_IMETHOD GetDefaultColor(nscolor* aColor);
  NS_IMETHOD GetDefaultBackgroundColor(nscolor* aColor);
  NS_IMETHOD GetDefaultBackgroundImage(nsString& aImage);
  NS_IMETHOD GetDefaultBackgroundImageRepeat(PRUint8* aRepeat);
  NS_IMETHOD GetDefaultBackgroundImageOffset(nscoord* aX, nscoord* aY);
  NS_IMETHOD GetDefaultBackgroundImageAttachment(PRUint8* aRepeat);

  NS_IMETHOD SetDefaultColor(nscolor aColor);
  NS_IMETHOD SetDefaultBackgroundColor(nscolor aColor);
  NS_IMETHOD SetDefaultBackgroundImage(const nsString& aImage);
  NS_IMETHOD SetDefaultBackgroundImageRepeat(PRUint8 aRepeat);
  NS_IMETHOD SetDefaultBackgroundImageOffset(nscoord aX, nscoord aY);
  NS_IMETHOD SetDefaultBackgroundImageAttachment(PRUint8 aRepeat);

  NS_IMETHOD GetImageGroup(nsIImageGroup** aGroupResult);
  NS_IMETHOD StartLoadImage(const nsString& aURL,
                            const nscolor* aBackgroundColor,
                            const nsSize* aDesiredSize,
                            nsIFrame* aTargetFrame,
                            nsIFrameImageLoaderCB aCallBack,
                            void* aClosure,
                            void* aKey,
                            nsIFrameImageLoader** aResult);
  NS_IMETHOD StopLoadImage(void* aKey, nsIFrameImageLoader* aLoader);
  NS_IMETHOD StopAllLoadImagesFor(nsIFrame* aTargetFrame, void* aKey);
  NS_IMETHOD SetContainer(nsISupports* aContainer);
  NS_IMETHOD GetContainer(nsISupports** aResult);
  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHandler);
  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult);
  NS_IMETHOD GetVisibleArea(nsRect& aResult);
  NS_IMETHOD SetVisibleArea(const nsRect& r);
  NS_IMETHOD IsPaginated(PRBool* aResult) = 0;
  NS_IMETHOD GetPageWidth(nscoord* aResult) = 0;
  NS_IMETHOD GetPageHeight(nscoord* aResult) = 0;
  NS_IMETHOD GetPixelsToTwips(float* aResult) const;
  NS_IMETHOD GetTwipsToPixels(float* aResult) const;
  NS_IMETHOD GetScaledPixelsToTwips(float* aScale) const;
  NS_IMETHOD GetDeviceContext(nsIDeviceContext** aResult) const;
  NS_IMETHOD GetEventStateManager(nsIEventStateManager** aManager);
  NS_IMETHOD GetDefaultDirection(PRUint8* aDirection);
  NS_IMETHOD SetDefaultDirection(PRUint8 aDirection);
  NS_IMETHOD GetLanguage(nsILanguageAtom** aLanguage);

#ifdef MOZ_REFLOW_PERF
  NS_IMETHOD CountReflows(const char * aName, PRUint32 aType);
#endif

  // nsIObserver method
  NS_IMETHOD Observe(nsISupports* aSubject, const PRUnichar* aTopic,
                     const PRUnichar* aData);

protected:
  nsPresContext();
  virtual ~nsPresContext();

  // IMPORTANT: The ownership implicit in the following member variables has been 
  // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface 
  // pointers for weak (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).
  
  nsIPresShell*         mShell;         // [WEAK]
  nsCOMPtr<nsIPref>     mPrefs;
  nsRect                mVisibleArea;
  nsCOMPtr<nsIDeviceContext>  mDeviceContext; // could be weak, but better safe than sorry. Cannot reintroduce cycles
                                              // since there is no dependency from gfx back to layout.
  nsCOMPtr<nsILanguageAtomService> mLangService;
  nsCOMPtr<nsILanguageAtom> mLanguage;
  nsCOMPtr<nsIImageGroup> mImageGroup;
  nsILinkHandler*       mLinkHandler;   // [WEAK]
  nsISupports*          mContainer;     // [WEAK]
  nsCOMPtr<nsILookAndFeel> mLookAndFeel;
  nsFont                mDefaultFont;
  nsFont                mDefaultFixedFont;
  PRInt32               mFontScaler;
  nscolor               mDefaultColor;
  nscolor               mDefaultBackgroundColor;
  nsString              mDefaultBackgroundImage;
  PRUint8               mDefaultBackgroundImageRepeat;
  nscoord               mDefaultBackgroundImageOffsetX;
  nscoord               mDefaultBackgroundImageOffsetY;
  PRUint8               mDefaultBackgroundImageAttachment;
  nsVoidArray           mImageLoaders;
  nsCOMPtr<nsIEventStateManager> mEventManager;
  nsCOMPtr<nsIURI>      mBaseURL;
  nsCompatibility       mCompatibilityMode;
  PRPackedBool          mCompatibilityLocked;
  nsWidgetRendering     mWidgetRenderingMode;
  nsImageAnimation      mImageAnimationMode;
  PRPackedBool          mImageAnimationStopped;   // image animation stopped
  PRPackedBool          mStopped;                 // loading stopped
  PRUint8               mDefaultDirection;

#ifdef DEBUG
  PRBool                mInitialized;
#endif

protected:
  void   GetUserPreferences();
  void   GetFontPreferences();

private:
  friend int PR_CALLBACK PrefChangedCallback(const char*, void*);
  void   PreferenceChanged(const char* aPrefName);
};

#endif /* nsPresContext_h___ */
