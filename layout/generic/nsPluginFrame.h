/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering objects for replaced elements implemented by a plugin */

#ifndef nsPluginFrame_h___
#define nsPluginFrame_h___

#include "mozilla/Attributes.h"
#include "nsIObjectFrame.h"
#include "nsFrame.h"
#include "nsRegion.h"
#include "nsDisplayList.h"
#include "nsIReflowCallback.h"
#include "Units.h"

#ifdef XP_WIN
#include <windows.h> // For HWND :(
// Undo the windows.h damage
#undef GetMessage
#undef CreateEvent
#undef GetClassName
#undef GetBinaryType
#undef RemoveDirectory
#undef LoadIcon
#endif

class nsPresContext;
class nsRootPresContext;
class nsDisplayPlugin;
class PluginBackgroundSink;
class nsPluginInstanceOwner;

namespace mozilla {
namespace layers {
class ImageContainer;
class Layer;
class LayerManager;
} // namespace layers
} // namespace mozilla

typedef nsFrame nsPluginFrameSuper;

class nsPluginFrame : public nsPluginFrameSuper,
                      public nsIObjectFrame,
                      public nsIReflowCallback {
public:
  typedef mozilla::LayerState LayerState;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;

  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(nsPluginFrame)

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) override;
  virtual void DidReflow(nsPresContext* aPresContext,
                         const nsHTMLReflowState* aReflowState,
                         nsDidReflowStatus aStatus) override;
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual nsresult  HandleEvent(nsPresContext* aPresContext,
                                mozilla::WidgetGUIEvent* aEvent,
                                nsEventStatus* aEventStatus) override;

  virtual nsIAtom* GetType() const override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsPluginFrameSuper::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

  virtual bool NeedsView() override { return true; }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  NS_METHOD GetPluginInstance(nsNPAPIPluginInstance** aPluginInstance) override;

  virtual void SetIsDocumentActive(bool aIsActive) override;

  virtual nsresult GetCursor(const nsPoint& aPoint, 
                             nsIFrame::Cursor& aCursor) override;

  // APIs used by nsRootPresContext to set up the widget position/size/clip
  // region.
  /**
   * Set the next widget configuration for the plugin to the desired
   * position of the plugin's widget, on the assumption that it is not visible
   * (clipped out or covered by opaque content).
   * This will only be called for plugins which have been registered
   * with the root pres context for geometry updates.
   * If there is no widget associated with the plugin, this will have no effect.
   */
  void SetEmptyWidgetConfiguration()
  {
    mNextConfigurationBounds = nsIntRect(0,0,0,0);
    mNextConfigurationClipRegion.Clear();
  }
  /**
   * Append the desired widget configuration to aConfigurations.
   */
  void GetWidgetConfiguration(nsTArray<nsIWidget::Configuration>* aConfigurations);

  nsIntRect GetWidgetlessClipRect() {
    return RegionFromArray(mNextConfigurationClipRegion).GetBounds();
  }

  /**
   * Called after all widget position/size/clip regions have been changed
   * (even if there isn't a widget for this plugin).
   */
  void DidSetWidgetGeometry();

  // accessibility support
#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
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
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

  /**
   * Builds either an ImageLayer or a ReadbackLayer, depending on the type
   * of aItem (TYPE_PLUGIN or TYPE_PLUGIN_READBACK respectively).
   */
  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     nsDisplayItem* aItem,
                                     const ContainerLayerParameters& aContainerParameters);

  LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                           LayerManager* aManager);

  /**
   * Get the rectangle (relative to this frame) which it will paint. Normally
   * the frame's content-box but may be smaller if the plugin is rendering
   * asynchronously and has a different-sized image temporarily.
   */
  nsRect GetPaintedRect(nsDisplayPlugin* aItem);

  /**
   * If aSupports has a nsPluginFrame, then prepare it for a DocShell swap.
   * @see nsSubDocumentFrame::BeginSwapDocShells.
   * There will be a call to EndSwapDocShells after we were moved to the
   * new view tree.
   */
  static void BeginSwapDocShells(nsISupports* aSupports, void*);
  /**
   * If aSupports has a nsPluginFrame, then set it up after a DocShell swap.
   * @see nsSubDocumentFrame::EndSwapDocShells.
   */
  static void EndSwapDocShells(nsISupports* aSupports, void*);

  nsIWidget* GetWidget() override { return mInnerView ? mWidget : nullptr; }

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

  /**
   * Helper for hiding windowed plugins during async scroll operations.
   */
  void SetScrollVisibility(bool aState);

protected:
  explicit nsPluginFrame(nsStyleContext* aContext);
  virtual ~nsPluginFrame();

  // NOTE:  This frame class does not inherit from |nsLeafFrame|, so
  // this is not a virtual method implementation.
  void GetDesiredSize(nsPresContext* aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsHTMLReflowMetrics& aDesiredSize);

  bool IsFocusable(int32_t *aTabIndex = nullptr, 
                   bool aWithMouse = false) override;

  // check attributes and optionally CSS to see if we should display anything
  bool IsHidden(bool aCheckVisibilityStyle = true) const;

  bool IsOpaque() const;
  bool IsTransparentMode() const;
  bool IsPaintedByGecko() const;

  nsIntPoint GetWindowOriginInPixels(bool aWindowless);
  
  /*
   * If this frame is in a remote tab, return the tab offset to
   * the origin of the chrome window. In non-e10s, this return 0,0.
   * This api sends a sync ipc request so be careful about use.
   */
  mozilla::LayoutDeviceIntPoint GetRemoteTabChromeOffset();

  static void PaintPrintPlugin(nsIFrame* aFrame,
                               nsRenderingContext* aRenderingContext,
                               const nsRect& aDirtyRect, nsPoint aPt);
  void PrintPlugin(nsRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  void PaintPlugin(nsDisplayListBuilder* aBuilder,
                   nsRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect, const nsRect& aPluginRect);

  void NotifyPluginReflowObservers();

  friend class nsPluginInstanceOwner;
  friend class nsDisplayPlugin;
  friend class PluginBackgroundSink;

private:
  // Registers the plugin for a geometry update, and requests a geometry
  // update. This caches the root pres context in
  // mRootPresContextRegisteredWith, so that we can be sure we unregister
  // from the right root prest context in UnregisterPluginForGeometryUpdates.
  void RegisterPluginForGeometryUpdates();

  // Unregisters the plugin for geometry updated with the root pres context
  // stored in mRootPresContextRegisteredWith.
  void UnregisterPluginForGeometryUpdates();

  static const nsIntRegion RegionFromArray(const nsTArray<nsIntRect>& aRects)
  {
    nsIntRegion region;
    for (uint32_t i = 0; i < aRects.Length(); ++i) {
      region.Or(region, aRects[i]);
    }
    return region;
  }

  class PluginEventNotifier : public nsRunnable {
  public:
    explicit PluginEventNotifier(const nsString &aEventType) : 
      mEventType(aEventType) {}
    
    NS_IMETHOD Run() override;
  private:
    nsString mEventType;
  };

  nsPluginInstanceOwner*          mInstanceOwner; // WEAK
  nsView*                        mInnerView;
  nsCOMPtr<nsIWidget>             mWidget;
  nsIntRect                       mWindowlessRect;
  /**
   * This is owned by the ReadbackLayer for this nsPluginFrame. It is
   * automatically cleared if the PluginBackgroundSink is destroyed.
   */
  PluginBackgroundSink*           mBackgroundSink;

  /**
   * Bounds that we should set the plugin's widget to in the next composite,
   * for plugins with widgets. For plugins without widgets, bounds in device
   * pixels relative to the nearest frame that's a display list reference frame.
   */
  nsIntRect                       mNextConfigurationBounds;
  /**
   * Clip region that we should set the plugin's widget to
   * in the next composite. Only meaningful for plugins with widgets.
   */
  nsTArray<nsIntRect>             mNextConfigurationClipRegion;

  bool mReflowCallbackPosted;

  // We keep this reference to ensure we can always unregister the
  // plugins we register on the root PresContext.
  // This is only non-null while we have a plugin registered for geometry
  // updates.
  nsRefPtr<nsRootPresContext> mRootPresContextRegisteredWith;

  // Tracks windowed plugin visibility during scroll operations. See
  // SetScrollVisibility.
  bool mIsHiddenDueToScroll;
};

class nsDisplayPlugin : public nsDisplayItem {
public:
  nsDisplayPlugin(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayPlugin);
    aBuilder->SetContainsPluginItem();
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayPlugin() {
    MOZ_COUNT_DTOR(nsDisplayPlugin);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;

  NS_DISPLAY_DECL_NAME("Plugin", TYPE_PLUGIN)

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override
  {
    return static_cast<nsPluginFrame*>(mFrame)->BuildLayer(aBuilder,
                                                           aManager, 
                                                           this,
                                                           aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return static_cast<nsPluginFrame*>(mFrame)->GetLayerState(aBuilder,
                                                              aManager);
  }
};

#endif /* nsPluginFrame_h___ */
