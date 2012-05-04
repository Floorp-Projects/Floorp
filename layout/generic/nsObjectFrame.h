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

/* rendering objects for replaced elements implemented by a plugin */

#ifndef nsObjectFrame_h___
#define nsObjectFrame_h___

#include "nsPluginInstanceOwner.h"
#include "nsIObjectFrame.h"
#include "nsFrame.h"
#include "nsRegion.h"
#include "nsDisplayList.h"
#include "nsIReflowCallback.h"
#include "Layers.h"
#include "ImageLayers.h"

#ifdef ACCESSIBILITY
class nsIAccessible;
#endif

class nsPluginHost;
class nsPresContext;
class nsDisplayPlugin;
class nsIOSurface;
class PluginBackgroundSink;

#define nsObjectFrameSuper nsFrame

class nsObjectFrame : public nsObjectFrameSuper,
                      public nsIObjectFrame,
                      public nsIReflowCallback {
public:
  typedef mozilla::LayerState LayerState;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::ImageContainer ImageContainer;

  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_QUERYFRAME

  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame* aParent,
                  nsIFrame* aPrevInFlow);
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD DidReflow(nsPresContext* aPresContext,
                       const nsHTMLReflowState* aReflowState,
                       nsDidReflowStatus aStatus);
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD  HandleEvent(nsPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus);

#ifdef XP_MACOSX
  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         nsGUIEvent*    aEvent,
                         nsEventStatus* aEventStatus);
#endif

  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsObjectFrameSuper::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

  virtual bool NeedsView() { return true; }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  NS_METHOD GetPluginInstance(nsNPAPIPluginInstance** aPluginInstance);

  virtual void SetIsDocumentActive(bool aIsActive);

  NS_IMETHOD GetCursor(const nsPoint& aPoint, nsIFrame::Cursor& aCursor);

  // Compute the desired position of the plugin's widget, on the assumption
  // that it is not visible (clipped out or covered by opaque content).
  // This will only be called for plugins which have been registered
  // with the root pres context for geometry updates.
  // The widget, its new position, size and (empty) clip region are appended
  // as a Configuration record to aConfigurations.
  // If there is no widget associated with the plugin, this
  // simply does nothing.
  void GetEmptyClipConfiguration(nsTArray<nsIWidget::Configuration>* aConfigurations) {
    ComputeWidgetGeometry(nsRegion(), nsPoint(0,0), aConfigurations);
  }

  void DidSetWidgetGeometry();

  // accessibility support
#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#ifdef XP_WIN
  NS_IMETHOD GetPluginPort(HWND *aPort);
#endif
#endif

  //local methods
  nsresult PrepForDrawing(nsIWidget *aWidget);

  // for a given aRoot, this walks the frame tree looking for the next outFrame
  static nsIObjectFrame* GetNextObjectFrame(nsPresContext* aPresContext,
                                            nsIFrame* aRoot);

  // nsIReflowCallback
  virtual bool ReflowFinished();
  virtual void ReflowCallbackCanceled();

  void UpdateImageLayer(const gfxRect& aRect);

  /**
   * Builds either an ImageLayer or a ReadbackLayer, depending on the type
   * of aItem (TYPE_PLUGIN or TYPE_PLUGIN_READBACK respectively).
   */
  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     nsDisplayItem* aItem);

  LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                           LayerManager* aManager);

  already_AddRefed<ImageContainer> GetImageContainer();
  /**
   * Get the rectangle (relative to this frame) which it will paint. Normally
   * the frame's content-box but may be smaller if the plugin is rendering
   * asynchronously and has a different-sized image temporarily.
   */
  nsRect GetPaintedRect(nsDisplayPlugin* aItem);

  /**
   * If aContent has a nsObjectFrame, then prepare it for a DocShell swap.
   * @see nsSubDocumentFrame::BeginSwapDocShells.
   * There will be a call to EndSwapDocShells after we were moved to the
   * new view tree.
   */
  static void BeginSwapDocShells(nsIContent* aContent, void*);
  /**
   * If aContent has a nsObjectFrame, then set it up after a DocShell swap.
   * @see nsSubDocumentFrame::EndSwapDocShells.
   */
  static void EndSwapDocShells(nsIContent* aContent, void*);

  bool PaintedByGecko();

  nsIWidget* GetWidget() { return mWidget; }

  /**
   * Adjust the plugin's idea of its size, using aSize as its new size.
   * (aSize must be in twips)
   */
  void FixupWindow(const nsSize& aSize);

  /*
   * Sets up the plugin window and calls SetWindow on the plugin.
   */
  nsresult CallSetWindow(bool aCheckIsHidden = true);

  void SetInstanceOwner(nsPluginInstanceOwner* aOwner);

protected:
  nsObjectFrame(nsStyleContext* aContext);
  virtual ~nsObjectFrame();

  // NOTE:  This frame class does not inherit from |nsLeafFrame|, so
  // this is not a virtual method implementation.
  void GetDesiredSize(nsPresContext* aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsHTMLReflowMetrics& aDesiredSize);

  bool IsFocusable(PRInt32 *aTabIndex = nsnull, bool aWithMouse = false);

  // check attributes and optionally CSS to see if we should display anything
  bool IsHidden(bool aCheckVisibilityStyle = true) const;

  bool IsOpaque() const;
  bool IsTransparentMode() const;

  nsIntPoint GetWindowOriginInPixels(bool aWindowless);

  static void PaintPrintPlugin(nsIFrame* aFrame,
                               nsRenderingContext* aRenderingContext,
                               const nsRect& aDirtyRect, nsPoint aPt);
  void PrintPlugin(nsRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  void PaintPlugin(nsDisplayListBuilder* aBuilder,
                   nsRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect, const nsRect& aPluginRect);

  /**
   * Get the widget geometry for the plugin. aRegion is in some appunits
   * coordinate system whose origin is device-pixel-aligned (if possible),
   * and aPluginOrigin gives the top-left of the plugin frame's content-rect
   * in that coordinate system. It doesn't matter what that coordinate
   * system actually is, as long as aRegion and aPluginOrigin are consistent.
   * This will append a Configuration object to aConfigurations
   * containing the widget, its desired position, size and clip region.
   */
  void ComputeWidgetGeometry(const nsRegion& aRegion,
                             const nsPoint& aPluginOrigin,
                             nsTArray<nsIWidget::Configuration>* aConfigurations);

  void NotifyPluginReflowObservers();

  friend class nsPluginInstanceOwner;
  friend class nsDisplayPlugin;
  friend class PluginBackgroundSink;

private:
  
  class PluginEventNotifier : public nsRunnable {
  public:
    PluginEventNotifier(const nsString &aEventType) : 
      mEventType(aEventType) {}
    
    NS_IMETHOD Run();
  private:
    nsString mEventType;
  };

  nsPluginInstanceOwner*          mInstanceOwner; // WEAK
  nsIView*                        mInnerView;
  nsCOMPtr<nsIWidget>             mWidget;
  nsIntRect                       mWindowlessRect;
  /**
   * This is owned by the ReadbackLayer for this nsObjectFrame. It is
   * automatically cleared if the PluginBackgroundSink is destroyed.
   */
  PluginBackgroundSink*           mBackgroundSink;

  bool mReflowCallbackPosted;

  // A reference to the ImageContainer which contains the current frame
  // of plugin to display.
  nsRefPtr<ImageContainer> mImageContainer;
};

class nsDisplayPlugin : public nsDisplayItem {
public:
  nsDisplayPlugin(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayPlugin);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayPlugin() {
    MOZ_COUNT_DTOR(nsDisplayPlugin);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap);
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap);
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion);

  NS_DISPLAY_DECL_NAME("Plugin", TYPE_PLUGIN)

  // Compute the desired position and clip region of the plugin's widget.
  // This will only be called for plugins which have been registered
  // with the root pres context for geometry updates.
  // The widget, its new position, size and clip region are appended as
  // a Configuration record to aConfigurations.
  // If the plugin has no widget, no configuration is added, but
  // the plugin visibility state may be adjusted.
  void GetWidgetConfiguration(nsDisplayListBuilder* aBuilder,
                              nsTArray<nsIWidget::Configuration>* aConfigurations);

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters)
  {
    return static_cast<nsObjectFrame*>(mFrame)->BuildLayer(aBuilder,
                                                           aManager, 
                                                           this);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters)
  {
    return static_cast<nsObjectFrame*>(mFrame)->GetLayerState(aBuilder,
                                                              aManager);
  }

private:
  nsRegion mVisibleRegion;
};

#endif /* nsObjectFrame_h___ */
