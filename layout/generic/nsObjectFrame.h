/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering objects for replaced elements implemented by a plugin */

#ifndef nsObjectFrame_h___
#define nsObjectFrame_h___

#include "mozilla/Attributes.h"
#include "nsIObjectFrame.h"
#include "nsFrame.h"
#include "nsRegion.h"
#include "nsDisplayList.h"
#include "nsIReflowCallback.h"

#ifdef XP_WIN
#include <windows.h> // For HWND :(
#endif

class nsPresContext;
class nsRootPresContext;
class nsDisplayPlugin;
class nsIOSurface;
class PluginBackgroundSink;
class nsPluginInstanceOwner;

namespace mozilla {
namespace layers {
class ImageContainer;
class Layer;
class LayerManager;
}
}

#define nsObjectFrameSuper nsFrame

class nsObjectFrame : public nsObjectFrameSuper,
                      public nsIObjectFrame,
                      public nsIReflowCallback {
public:
  typedef mozilla::LayerState LayerState;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::FrameLayerBuilder::ContainerParameters ContainerParameters;

  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(nsObjectFrame)

  virtual void Init(nsIContent* aContent,
                    nsIFrame* aParent,
                    nsIFrame* aPrevInFlow) MOZ_OVERRIDE;
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD DidReflow(nsPresContext* aPresContext,
                       const nsHTMLReflowState* aReflowState,
                       nsDidReflowStatus aStatus);
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  NS_IMETHOD  HandleEvent(nsPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus);

#ifdef XP_MACOSX
  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         nsGUIEvent*    aEvent,
                         nsEventStatus* aEventStatus);
#endif

  virtual nsIAtom* GetType() const;

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return nsObjectFrameSuper::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

  virtual bool NeedsView() { return true; }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  NS_METHOD GetPluginInstance(nsNPAPIPluginInstance** aPluginInstance) MOZ_OVERRIDE;

  virtual void SetIsDocumentActive(bool aIsActive) MOZ_OVERRIDE;

  NS_IMETHOD GetCursor(const nsPoint& aPoint, nsIFrame::Cursor& aCursor);

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
  void GetWidgetConfiguration(nsTArray<nsIWidget::Configuration>* aConfigurations)
  {
    if (mWidget) {
      if (!mWidget->GetParent()) {
        // Plugin widgets should not be toplevel except when they're out of the
        // document, in which case the plugin should not be registered for
        // geometry updates and this should not be called. But apparently we
        // have bugs where mWidget sometimes is toplevel here. Bail out.
        NS_ERROR("Plugin widgets registered for geometry updates should not be toplevel");
        return;
      }
      nsIWidget::Configuration* configuration = aConfigurations->AppendElement();
      configuration->mChild = mWidget;
      configuration->mBounds = mNextConfigurationBounds;
      configuration->mClipRegion = mNextConfigurationClipRegion;
    }
  }
  /**
   * Called after all widget position/size/clip regions have been changed
   * (even if there isn't a widget for this plugin).
   */
  void DidSetWidgetGeometry();

  // accessibility support
#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
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
  virtual bool ReflowFinished() MOZ_OVERRIDE;
  virtual void ReflowCallbackCanceled() MOZ_OVERRIDE;

  /**
   * Builds either an ImageLayer or a ReadbackLayer, depending on the type
   * of aItem (TYPE_PLUGIN or TYPE_PLUGIN_READBACK respectively).
   */
  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     nsDisplayItem* aItem,
                                     const ContainerParameters& aContainerParameters);

  LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                           LayerManager* aManager);

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

  nsIWidget* GetWidget() MOZ_OVERRIDE { return mInnerView ? mWidget : nullptr; }

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

  bool IsFocusable(int32_t *aTabIndex = nullptr, bool aWithMouse = false);

  // check attributes and optionally CSS to see if we should display anything
  bool IsHidden(bool aCheckVisibilityStyle = true) const;

  bool IsOpaque() const;
  bool IsTransparentMode() const;
  bool IsPaintedByGecko() const;

  nsIntPoint GetWindowOriginInPixels(bool aWindowless);

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

  class PluginEventNotifier : public nsRunnable {
  public:
    PluginEventNotifier(const nsString &aEventType) : 
      mEventType(aEventType) {}
    
    NS_IMETHOD Run() MOZ_OVERRIDE;
  private:
    nsString mEventType;
  };

  nsPluginInstanceOwner*          mInstanceOwner; // WEAK
  nsView*                        mInnerView;
  nsCOMPtr<nsIWidget>             mWidget;
  nsIntRect                       mWindowlessRect;
  /**
   * This is owned by the ReadbackLayer for this nsObjectFrame. It is
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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;

  NS_DISPLAY_DECL_NAME("Plugin", TYPE_PLUGIN)

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE
  {
    return static_cast<nsObjectFrame*>(mFrame)->BuildLayer(aBuilder,
                                                           aManager, 
                                                           this,
                                                           aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE
  {
    return static_cast<nsObjectFrame*>(mFrame)->GetLayerState(aBuilder,
                                                              aManager);
  }
};

#endif /* nsObjectFrame_h___ */
