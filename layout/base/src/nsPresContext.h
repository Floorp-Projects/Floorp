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
#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "nsIPresContext.h"
#include "nsIDeviceContext.h"
#include "nsVoidArray.h"
#include "nsFont.h"
#include "nsCRT.h"
class nsIImageGroup;

// Base class for concrete presentation context classes
class nsPresContext : public nsIPresContext {
public:
  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIPresContext methods
  NS_IMETHOD Init(nsIDeviceContext* aDeviceContext, nsIPref* aPrefs);
  NS_IMETHOD Stop(void);
  NS_IMETHOD SetShell(nsIPresShell* aShell);
  virtual nsIPresShell* GetShell();
  NS_IMETHOD GetPrefs(nsIPref*& aPrefs);
  NS_IMETHOD GetCompatibilityMode(nsCompatibility& aMode);
  NS_IMETHOD SetCompatibilityMode(nsCompatibility aMode);
  NS_IMETHOD GetBaseURL(nsIURL*& aURL);
  NS_IMETHOD ResolveStyleContextFor(nsIContent* aContent,
                                    nsIStyleContext* aParentContext,
                                    nsIStyleContext** aStyleContext,
                                    PRBool aForceUnique = PR_FALSE);
  NS_IMETHOD ResolvePseudoStyleContextFor(nsIContent* aParentContent,
                                          nsIAtom* aPseudoTag,
                                          nsIStyleContext* aParentContext,
                                          nsIStyleContext** aStyleContext,
                                          PRBool aForceUnique = PR_FALSE);
  virtual nsIStyleContext* ProbePseudoStyleContextFor(nsIContent* aParentContent,
                                                      nsIAtom* aPseudoTag,
                                                      nsIStyleContext* aParentContext,
                                                      PRBool aForceUnique = PR_FALSE);
  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont);
  virtual const nsFont& GetDefaultFont(void);
  virtual const nsFont& GetDefaultFixedFont(void);
  NS_IMETHOD GetFontScaler(PRInt32& aResult);
  NS_IMETHOD SetFontScaler(PRInt32 aScaler);
  NS_IMETHOD GetDefaultColor(nscolor& aColor);
  NS_IMETHOD GetDefaultBackgroundColor(nscolor& aColor);
  NS_IMETHOD SetDefaultColor(const nscolor& aColor);
  NS_IMETHOD SetDefaultBackgroundColor(const nscolor& aColor);
  NS_IMETHOD GetImageGroup(nsIImageGroup*& aGroupResult);
  NS_IMETHOD StartLoadImage(const nsString& aURL,
                            const nscolor* aBackgroundColor,
                            nsIFrame* aTargetFrame,
                            nsFrameImageLoaderCB aCallBack,
                            PRBool aNeedSizeUpdate,
                            PRBool aNeedErrorNotification,
                            nsIFrameImageLoader*& aLoader);
  NS_IMETHOD StopLoadImage(nsIFrame* aForFrame);
  NS_IMETHOD SetContainer(nsISupports* aContainer);
  NS_IMETHOD GetContainer(nsISupports** aResult);
  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHander);
  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult);
  NS_IMETHOD GetVisibleArea(nsRect& aResult);
  NS_IMETHOD SetVisibleArea(const nsRect& r);
  virtual float GetPixelsToTwips() const;
  virtual float GetTwipsToPixels() const;
  NS_IMETHOD GetScaledPixelsToTwips(float &aScale) const;
  virtual nsIDeviceContext* GetDeviceContext() const;
  NS_IMETHOD GetEventStateManager(nsIEventStateManager** aManager);

protected:
  nsPresContext();
  virtual ~nsPresContext();

  nsIPresShell*         mShell;
  nsIPref*              mPrefs;
  nsRect                mVisibleArea;
  nsIDeviceContext*     mDeviceContext;
  nsIImageGroup*        mImageGroup;
  nsILinkHandler*       mLinkHandler;
  nsISupports*          mContainer;
  nsFont                mDefaultFont;
  nsFont                mDefaultFixedFont;
  PRInt32               mFontScaler;
  nscolor               mDefaultColor;
  nscolor               mDefaultBackgroundColor;
  nsVoidArray           mImageLoaders;
  nsIEventStateManager* mEventManager;
  nsCompatibility       mCompatibilityMode;
  nsIURL*               mBaseURL;
  PRBool                mStopped;

#ifdef DEBUG
  PRBool                mInitialized;
#endif

protected:
  void   GetUserPreferences();

private:
  friend int PrefChangedCallback(const char*, void*);
  void   PreferenceChanged(const char* aPrefName);
};

#endif /* nsPresContext_h___ */
