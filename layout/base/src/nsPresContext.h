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
class nsIImageGroup;

// Base class for concrete presentation context classes
class nsPresContext : public nsIPresContext {
public:
  NS_DECL_ISUPPORTS

  virtual void SetShell(nsIPresShell* aShell);
  virtual nsIPresShell* GetShell();
  virtual nsIStyleContext* ResolveStyleContextFor(nsIContent* aContent,
                                                  nsIFrame* aParentFrame);
  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont);
  virtual const nsFont& GetDefaultFont(void);
  virtual nsIImage* LoadImage(const nsString&, nsIFrame* aForFrame);
  virtual void StopLoadImage(nsIFrame* aForFrame);

  NS_IMETHOD SetContainer(nsISupports* aContainer);

  NS_IMETHOD GetContainer(nsISupports** aResult);

  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHander);

  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult);

  virtual nsRect GetVisibleArea();
  virtual void SetVisibleArea(const nsRect& r);

  virtual float GetPixelsToTwips() const;
  virtual float GetTwipsToPixels() const;

  virtual nsIDeviceContext* GetDeviceContext() const;

protected:
  nsPresContext();
  virtual ~nsPresContext();

  nsIPresShell*     mShell;
  nsRect            mVisibleArea;
  nsIDeviceContext* mDeviceContext;
  nsIImageGroup*    mImageGroup;
  nsILinkHandler*   mLinkHandler;
  nsISupports*      mContainer;
  nsFont            mDefaultFont;

  nsVoidArray       mImageLoaders;/* XXX temporary */

private:
  // Not supported and not implemented
  nsPresContext(const nsPresContext&);
  nsPresContext& operator=(const nsPresContext&);
};

#endif /* nsPresContext_h___ */
