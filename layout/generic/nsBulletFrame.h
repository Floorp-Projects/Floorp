/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for list-item bullets */

#ifndef nsBulletFrame_h___
#define nsBulletFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"
#include "nsStyleContext.h"

#include "imgIRequest.h"
#include "imgINotificationObserver.h"

class imgRequestProxy;

#define BULLET_FRAME_IMAGE_LOADING NS_FRAME_STATE_BIT(63)
#define BULLET_FRAME_HAS_FONT_INFLATION NS_FRAME_STATE_BIT(62)

class nsBulletFrame;

class nsBulletListener : public imgINotificationObserver
{
public:
  nsBulletListener();
  virtual ~nsBulletListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void SetFrame(nsBulletFrame *frame) { mFrame = frame; }

private:
  nsBulletFrame *mFrame;
};

/**
 * A simple class that manages the layout and rendering of html bullets.
 * This class also supports the CSS list-style properties.
 */
class nsBulletFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsBulletFrame(nsStyleContext* aContext)
    : nsFrame(aContext)
  {
  }
  virtual ~nsBulletFrame();

  NS_IMETHOD Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData);

  // nsIFrame
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus) MOZ_OVERRIDE;
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  // nsBulletFrame
  int32_t SetListItemOrdinal(int32_t aNextOrdinal, bool* aChanged,
                             int32_t aIncrement);


  /* get list item text, without '.' */
  static bool AppendCounterText(int32_t aListStyleType,
                                  int32_t aOrdinal,
                                  nsString& aResult);

  /* get list item text, with '.' */
  bool GetListItemText(const nsStyleList& aStyleList,
                         nsString& aResult);
                         
  void PaintBullet(nsRenderingContext& aRenderingContext, nsPoint aPt,
                   const nsRect& aDirtyRect);
  
  virtual bool IsEmpty() MOZ_OVERRIDE;
  virtual bool IsSelfEmpty() MOZ_OVERRIDE;
  virtual nscoord GetBaseline() const;

  float GetFontSizeInflation() const;
  bool HasFontSizeInflation() const {
    return (GetStateBits() & BULLET_FRAME_HAS_FONT_INFLATION) != 0;
  }
  void SetFontSizeInflation(float aInflation);

  int32_t GetOrdinal() { return mOrdinal; }

protected:
  nsresult OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);

  void GetDesiredSize(nsPresContext* aPresContext,
                      nsRenderingContext *aRenderingContext,
                      nsHTMLReflowMetrics& aMetrics,
                      float aFontSizeInflation);

  void GetLoadGroup(nsPresContext *aPresContext, nsILoadGroup **aLoadGroup);

  nsMargin mPadding;
  nsRefPtr<imgRequestProxy> mImageRequest;
  nsRefPtr<nsBulletListener> mListener;

  nsSize mIntrinsicSize;
  int32_t mOrdinal;
  bool mTextIsRTL;

private:

  // This is a boolean flag indicating whether or not the current image request
  // has been registered with the refresh driver.
  bool mRequestRegistered;
};

#endif /* nsBulletFrame_h___ */
