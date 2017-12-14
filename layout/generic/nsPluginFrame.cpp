/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering objects for replaced elements implemented by a plugin */

#include "nsPluginFrame.h"

#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MouseEvents.h"
#ifdef XP_WIN
// This is needed for DoublePassRenderingEvent.
#include "mozilla/plugins/PluginMessageUtils.h"
#endif

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsString.h"
#include "nsGkAtoms.h"
#include "nsIPluginInstanceOwner.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIDOMElement.h"
#include "npapi.h"
#include "nsIObjectLoadingContent.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsFocusManager.h"
#include "nsLayoutUtils.h"
#include "nsFrameManager.h"
#include "nsIObserverService.h"
#include "GeckoProfiler.h"
#include <algorithm>

#include "nsIObjectFrame.h"
#include "nsPluginNativeWindow.h"
#include "FrameLayerBuilder.h"

#include "ImageLayers.h"
#include "nsPluginInstanceOwner.h"

#ifdef XP_WIN
#include "gfxWindowsNativeDrawing.h"
#include "gfxWindowsSurface.h"
#endif

#include "Layers.h"
#include "ReadbackLayer.h"
#include "ImageContainer.h"
#include "mozilla/layers/WebRenderLayerManager.h"

// accessibility support
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#include "mozilla/Logging.h"

#ifdef XP_MACOSX
#include "gfxQuartzNativeDrawing.h"
#include "mozilla/gfx/QuartzSupport.h"
#endif

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
using mozilla::DefaultXDisplay;
#endif

#ifdef XP_WIN
#include <wtypes.h>
#include <winuser.h>
#endif

#include "mozilla/dom/TabChild.h"

#ifdef CreateEvent // Thank you MS.
#undef CreateEvent
#endif

static mozilla::LazyLogModule sPluginFrameLog("nsPluginFrame");

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

class PluginBackgroundSink : public ReadbackSink {
public:
  PluginBackgroundSink(nsPluginFrame* aFrame, uint64_t aStartSequenceNumber)
    : mLastSequenceNumber(aStartSequenceNumber), mFrame(aFrame) {}
  ~PluginBackgroundSink() override
  {
    if (mFrame) {
      mFrame->mBackgroundSink = nullptr;
    }
  }

  void SetUnknown(uint64_t aSequenceNumber) override
  {
    if (!AcceptUpdate(aSequenceNumber))
      return;
    mFrame->mInstanceOwner->SetBackgroundUnknown();
  }

  already_AddRefed<DrawTarget>
      BeginUpdate(const nsIntRect& aRect, uint64_t aSequenceNumber) override
  {
    if (!AcceptUpdate(aSequenceNumber))
      return nullptr;
    return mFrame->mInstanceOwner->BeginUpdateBackground(aRect);
  }

  void EndUpdate(const nsIntRect& aRect) override
  {
    return mFrame->mInstanceOwner->EndUpdateBackground(aRect);
  }

  void Destroy() { mFrame = nullptr; }

protected:
  bool AcceptUpdate(uint64_t aSequenceNumber) {
    if (aSequenceNumber > mLastSequenceNumber && mFrame &&
        mFrame->mInstanceOwner) {
      mLastSequenceNumber = aSequenceNumber;
      return true;
    }
    return false;
  }

  uint64_t mLastSequenceNumber;
  nsPluginFrame* mFrame;
};

nsPluginFrame::nsPluginFrame(nsStyleContext* aContext)
  : nsFrame(aContext, kClassID)
  , mInstanceOwner(nullptr)
  , mOuterView(nullptr)
  , mInnerView(nullptr)
  , mBackgroundSink(nullptr)
  , mReflowCallbackPosted(false)
{
  MOZ_LOG(sPluginFrameLog, LogLevel::Debug,
         ("Created new nsPluginFrame %p\n", this));
}

nsPluginFrame::~nsPluginFrame()
{
  MOZ_LOG(sPluginFrameLog, LogLevel::Debug,
         ("nsPluginFrame %p deleted\n", this));
}

NS_QUERYFRAME_HEAD(nsPluginFrame)
  NS_QUERYFRAME_ENTRY(nsPluginFrame)
  NS_QUERYFRAME_ENTRY(nsIObjectFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsPluginFrame::AccessibleType()
{
  return a11y::ePluginType;
}

#ifdef XP_WIN
NS_IMETHODIMP nsPluginFrame::GetPluginPort(HWND *aPort)
{
  *aPort = (HWND) mInstanceOwner->GetPluginPort();
  return NS_OK;
}
#endif
#endif

void
nsPluginFrame::Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow)
{
  MOZ_LOG(sPluginFrameLog, LogLevel::Debug,
         ("Initializing nsPluginFrame %p for content %p\n", this, aContent));

  nsFrame::Init(aContent, aParent, aPrevInFlow);
  CreateView();
}

void
nsPluginFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  if (mReflowCallbackPosted) {
    PresShell()->CancelReflowCallback(this);
  }

  // Ensure our DidComposite observer is gone.
  mDidCompositeObserver = nullptr;

  // Tell content owner of the instance to disconnect its frame.
  nsCOMPtr<nsIObjectLoadingContent> objContent(do_QueryInterface(mContent));
  NS_ASSERTION(objContent, "Why not an object loading content?");

  // The content might not have a reference to the instance owner any longer in
  // the case of re-entry during instantiation or teardown, so make sure we're
  // dissociated.
  if (mInstanceOwner) {
    mInstanceOwner->SetFrame(nullptr);
  }
  objContent->HasNewFrame(nullptr);

  if (mBackgroundSink) {
    mBackgroundSink->Destroy();
  }

  nsFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

/* virtual */ void
nsPluginFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  if (HasView()) {
    nsView* view = GetView();
    nsViewManager* vm = view->GetViewManager();
    if (vm) {
      nsViewVisibility visibility =
        IsHidden() ? nsViewVisibility_kHide : nsViewVisibility_kShow;
      vm->SetViewVisibility(view, visibility);
    }
  }

  nsFrame::DidSetStyleContext(aOldStyleContext);
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsPluginFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("PluginFrame"), aResult);
}
#endif

nsresult
nsPluginFrame::PrepForDrawing(nsIWidget *aWidget)
{
  mWidget = aWidget;

  nsView* view = GetView();
  NS_ASSERTION(view, "Object frames must have views");
  if (!view) {
    return NS_ERROR_FAILURE;
  }

  nsViewManager* viewMan = view->GetViewManager();
  // mark the view as hidden since we don't know the (x,y) until Paint
  // XXX is the above comment correct?
  viewMan->SetViewVisibility(view, nsViewVisibility_kHide);

  //this is ugly. it was ripped off from didreflow(). MMP
  // Position and size view relative to its parent, not relative to our
  // parent frame (our parent frame may not have a view).

  nsView* parentWithView;
  nsPoint origin;
  nsRect r(0, 0, mRect.width, mRect.height);

  GetOffsetFromView(origin, &parentWithView);
  viewMan->ResizeView(view, r);
  viewMan->MoveViewTo(view, origin.x, origin.y);

  nsPresContext* presContext = PresContext();
  nsRootPresContext* rpc = presContext->GetRootPresContext();
  if (!rpc) {
    return NS_ERROR_FAILURE;
  }

  if (mWidget) {
    // Disallow windowed plugins in popups
    nsIFrame* rootFrame = rpc->PresShell()->FrameManager()->GetRootFrame();
    nsIWidget* parentWidget = rootFrame->GetNearestWidget();
    if (!parentWidget || nsLayoutUtils::GetDisplayRootFrame(this) != rootFrame) {
      return NS_ERROR_FAILURE;
    }

    // We can already have mInnerView if our instance owner went away and then
    // came back. So clear the old one before creating a new one.
    if (mInnerView) {
      if (mInnerView->GetWidget()) {
        // The widget listener should have already been cleared by
        // SetInstanceOwner (with a null instance owner).
        MOZ_RELEASE_ASSERT(mInnerView->GetWidget()->GetWidgetListener() == nullptr);
      }
      mInnerView->Destroy();
      mInnerView = nullptr;
    }
    mInnerView = viewMan->CreateView(GetContentRectRelativeToSelf(), view);
    if (!mInnerView) {
      NS_ERROR("Could not create inner view");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    viewMan->InsertChild(view, mInnerView, nullptr, true);

    mWidget->SetParent(parentWidget);
    mWidget->Enable(true);
    mWidget->Show(true);

    // Set the plugin window to have an empty clip region until we know
    // what our true position, size and clip region are. These
    // will be reset when nsRootPresContext computes our true
    // geometry. The plugin window does need to have a good size here, so
    // set the size explicitly to a reasonable guess.
    AutoTArray<nsIWidget::Configuration,1> configurations;
    nsIWidget::Configuration* configuration = configurations.AppendElement();
    nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    configuration->mChild = mWidget;
    configuration->mBounds.width = NSAppUnitsToIntPixels(mRect.width, appUnitsPerDevPixel);
    configuration->mBounds.height = NSAppUnitsToIntPixels(mRect.height, appUnitsPerDevPixel);
    parentWidget->ConfigureChildren(configurations);

    mInnerView->AttachWidgetEventHandler(mWidget);

#ifdef XP_MACOSX
    // On Mac, we need to invalidate ourselves since even windowed
    // plugins are painted through Thebes and we need to ensure
    // the PaintedLayer containing the plugin is updated.
    if (parentWidget == GetNearestWidget()) {
      InvalidateFrame();
    }
#endif

    RegisterPluginForGeometryUpdates();

    // Here we set the background color for this widget because some plugins will use
    // the child window background color when painting. If it's not set, it may default to gray
    // Sometimes, a frame doesn't have a background color or is transparent. In this
    // case, walk up the frame tree until we do find a frame with a background color
    for (nsIFrame* frame = this; frame; frame = frame->GetParent()) {
      nscolor bgcolor = frame->
        GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor);
      if (NS_GET_A(bgcolor) > 0) {  // make sure we got an actual color
        mWidget->SetBackgroundColor(bgcolor);
        break;
      }
    }
  } else {
    // Changing to windowless mode changes the NPWindow geometry.
    FixupWindow(GetContentRectRelativeToSelf().Size());
    RegisterPluginForGeometryUpdates();
  }

  if (!IsHidden()) {
    viewMan->SetViewVisibility(view, nsViewVisibility_kShow);
  }

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    accService->RecreateAccessible(PresShell(), mContent);
  }
#endif

  return NS_OK;
}

#define EMBED_DEF_WIDTH 240
#define EMBED_DEF_HEIGHT 200

/* virtual */ nscoord
nsPluginFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result = 0;

  if (!IsHidden(false)) {
    if (mContent->IsHTMLElement(nsGkAtoms::embed)) {
      bool vertical = GetWritingMode().IsVertical();
      result = nsPresContext::CSSPixelsToAppUnits(
        vertical ? EMBED_DEF_HEIGHT : EMBED_DEF_WIDTH);
    }
  }

  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsPluginFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  return nsPluginFrame::GetMinISize(aRenderingContext);
}

void
nsPluginFrame::GetWidgetConfiguration(nsTArray<nsIWidget::Configuration>* aConfigurations)
{
  if (!mWidget) {
    return;
  }

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
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  if (XRE_IsContentProcess()) {
    configuration->mWindowID = (uintptr_t)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
    configuration->mVisible = mWidget->IsVisible();

  }
#endif // defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
}

void
nsPluginFrame::GetDesiredSize(nsPresContext* aPresContext,
                              const ReflowInput& aReflowInput,
                              ReflowOutput& aMetrics)
{
  // By default, we have no area
  aMetrics.ClearSize();

  if (IsHidden(false)) {
    return;
  }

  aMetrics.Width() = aReflowInput.ComputedWidth();
  aMetrics.Height() = aReflowInput.ComputedHeight();

  // for EMBED, default to 240x200 for compatibility
  if (mContent->IsHTMLElement(nsGkAtoms::embed)) {
    if (aMetrics.Width() == NS_UNCONSTRAINEDSIZE) {
      aMetrics.Width() = clamped(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_WIDTH),
                               aReflowInput.ComputedMinWidth(),
                               aReflowInput.ComputedMaxWidth());
    }
    if (aMetrics.Height() == NS_UNCONSTRAINEDSIZE) {
      aMetrics.Height() = clamped(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_HEIGHT),
                                aReflowInput.ComputedMinHeight(),
                                aReflowInput.ComputedMaxHeight());
    }

#if defined(MOZ_WIDGET_GTK)
    // We need to make sure that the size of the object frame does not
    // exceed the maximum size of X coordinates.  See bug #225357 for
    // more information.  In theory Gtk2 can handle large coordinates,
    // but underlying plugins can't.
    aMetrics.Height() = std::min(aPresContext->DevPixelsToAppUnits(INT16_MAX), aMetrics.Height());
    aMetrics.Width() = std::min(aPresContext->DevPixelsToAppUnits(INT16_MAX), aMetrics.Width());
#endif
  }

  // At this point, the width has an unconstrained value only if we have
  // nothing to go on (no width set, no information from the plugin, nothing).
  // Make up a number.
  if (aMetrics.Width() == NS_UNCONSTRAINEDSIZE) {
    aMetrics.Width() =
      (aReflowInput.ComputedMinWidth() != NS_UNCONSTRAINEDSIZE) ?
        aReflowInput.ComputedMinWidth() : 0;
  }

  // At this point, the height has an unconstrained value only in two cases:
  // a) We are in standards mode with percent heights and parent is auto-height
  // b) We have no height information at all.
  // In either case, we have to make up a number.
  if (aMetrics.Height() == NS_UNCONSTRAINEDSIZE) {
    aMetrics.Height() =
      (aReflowInput.ComputedMinHeight() != NS_UNCONSTRAINEDSIZE) ?
        aReflowInput.ComputedMinHeight() : 0;
  }

  // XXXbz don't add in the border and padding, because we screw up our
  // plugin's size and positioning if we do...  Eventually we _do_ want to
  // paint borders, though!  At that point, we will need to adjust the desired
  // size either here or in Reflow....  Further, we will need to fix Paint() to
  // call the superclass in all cases.
}

void
nsPluginFrame::Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aMetrics,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsPluginFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // Get our desired size
  GetDesiredSize(aPresContext, aReflowInput, aMetrics);
  aMetrics.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aMetrics);

  // delay plugin instantiation until all children have
  // arrived. Otherwise there may be PARAMs or other stuff that the
  // plugin needs to see that haven't arrived yet.
  if (!GetContent()->IsDoneAddingChildren()) {
    return;
  }

  // if we are printing or print previewing, bail for now
  if (aPresContext->Medium() == nsGkAtoms::print) {
    return;
  }

  nsRect r(0, 0, aMetrics.Width(), aMetrics.Height());
  r.Deflate(aReflowInput.ComputedPhysicalBorderPadding());

  if (mInnerView) {
    nsViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, r.x, r.y);
    vm->ResizeView(mInnerView, nsRect(nsPoint(0, 0), r.Size()), true);
  }

  FixupWindow(r.Size());
  if (!mReflowCallbackPosted) {
    mReflowCallbackPosted = true;
    aPresContext->PresShell()->PostReflowCallback(this);
  }

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

///////////// nsIReflowCallback ///////////////

bool
nsPluginFrame::ReflowFinished()
{
  mReflowCallbackPosted = false;
  CallSetWindow();
  return true;
}

void
nsPluginFrame::ReflowCallbackCanceled()
{
  mReflowCallbackPosted = false;
}

void
nsPluginFrame::FixupWindow(const nsSize& aSize)
{
  nsPresContext* presContext = PresContext();

  if (!mInstanceOwner)
    return;

  NPWindow *window;
  mInstanceOwner->GetWindow(window);

  NS_ENSURE_TRUE_VOID(window);

  bool windowless = (window->type == NPWindowTypeDrawable);

  nsIntPoint origin = GetWindowOriginInPixels(windowless);

  // window must be in "display pixels"
#if defined(XP_MACOSX)
  // window must be in "display pixels"
  double scaleFactor = 1.0;
  if (NS_FAILED(mInstanceOwner->GetContentsScaleFactor(&scaleFactor))) {
    scaleFactor = 1.0;
  }
  int intScaleFactor = ceil(scaleFactor);
  window->x = origin.x / intScaleFactor;
  window->y = origin.y / intScaleFactor;
  window->width = presContext->AppUnitsToDevPixels(aSize.width) / intScaleFactor;
  window->height = presContext->AppUnitsToDevPixels(aSize.height) / intScaleFactor;
#else
  window->x = origin.x;
  window->y = origin.y;
  window->width = presContext->AppUnitsToDevPixels(aSize.width);
  window->height = presContext->AppUnitsToDevPixels(aSize.height);
#endif

#ifndef XP_MACOSX
  mInstanceOwner->UpdateWindowPositionAndClipRect(false);
#endif

  NotifyPluginReflowObservers();
}

nsresult
nsPluginFrame::CallSetWindow(bool aCheckIsHidden)
{
  NPWindow *win = nullptr;

  nsresult rv = NS_ERROR_FAILURE;
  RefPtr<nsNPAPIPluginInstance> pi;
  if (!mInstanceOwner ||
      NS_FAILED(rv = mInstanceOwner->GetInstance(getter_AddRefs(pi))) ||
      !pi ||
      NS_FAILED(rv = mInstanceOwner->GetWindow(win)) ||
      !win)
    return rv;

  nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;

  if (aCheckIsHidden && IsHidden())
    return NS_ERROR_FAILURE;

  // Calling either nsPluginInstanceOwner::FixUpPluginWindow() (here,
  // on OS X) or SetWindow() (below, on all platforms) can destroy this
  // frame.  (FixUpPluginWindow() calls SetWindow()).  So grab a safe
  // reference to mInstanceOwner which we can use below, if needed.
  RefPtr<nsPluginInstanceOwner> instanceOwnerRef(mInstanceOwner);

  // refresh the plugin port as well
#ifdef XP_MACOSX
  mInstanceOwner->FixUpPluginWindow(nsPluginInstanceOwner::ePluginPaintEnable);
  // Bail now if our frame has been destroyed.
  if (!instanceOwnerRef->GetFrame()) {
    return NS_ERROR_FAILURE;
  }
#endif
  window->window = mInstanceOwner->GetPluginPort();

  // Adjust plugin dimensions according to pixel snap results
  // and reduce amount of SetWindow calls
  nsPresContext* presContext = PresContext();
  nsRootPresContext* rootPC = presContext->GetRootPresContext();
  if (!rootPC)
    return NS_ERROR_FAILURE;
  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  nsIFrame* rootFrame = rootPC->PresShell()->FrameManager()->GetRootFrame();
  nsRect bounds = GetContentRectRelativeToSelf() + GetOffsetToCrossDoc(rootFrame);
  nsIntRect intBounds = bounds.ToNearestPixels(appUnitsPerDevPixel);

  // In e10s, this returns the offset to the top level window, in non-e10s
  // it return 0,0.
  LayoutDeviceIntPoint intOffset = GetRemoteTabChromeOffset();
  intBounds.x += intOffset.x;
  intBounds.y += intOffset.y;

#if defined(XP_MACOSX)
  // window must be in "display pixels"
  double scaleFactor = 1.0;
  if (NS_FAILED(instanceOwnerRef->GetContentsScaleFactor(&scaleFactor))) {
    scaleFactor = 1.0;
  }

  size_t intScaleFactor = ceil(scaleFactor);
  window->x = intBounds.x / intScaleFactor;
  window->y = intBounds.y / intScaleFactor;
  window->width = intBounds.width / intScaleFactor;
  window->height = intBounds.height / intScaleFactor;
#else
  window->x = intBounds.x;
  window->y = intBounds.y;
  window->width = intBounds.width;
  window->height = intBounds.height;
#endif
  // BE CAREFUL: By the time we get here the PluginFrame is sometimes destroyed
  // and poisoned. If we reference local fields (implicit this deref),
  // we will crash.
  instanceOwnerRef->ResolutionMayHaveChanged();

  // This will call pi->SetWindow and take care of window subclassing
  // if needed, see bug 132759. Calling SetWindow can destroy this frame
  // so check for that before doing anything else with this frame's memory.
  if (instanceOwnerRef->UseAsyncRendering()) {
    rv = pi->AsyncSetWindow(window);
  }
  else {
    rv = window->CallSetWindow(pi);
  }

  instanceOwnerRef->ReleasePluginPort(window->window);

  return rv;
}

void
nsPluginFrame::RegisterPluginForGeometryUpdates()
{
  nsRootPresContext* rpc = PresContext()->GetRootPresContext();
  NS_ASSERTION(rpc, "We should have a root pres context!");
  if (mRootPresContextRegisteredWith == rpc || !rpc) {
    // Already registered with current root pres context,
    // or null root pres context...
    return;
  }
  if (mRootPresContextRegisteredWith && mRootPresContextRegisteredWith != rpc) {
    // Registered to some other root pres context. Unregister, and
    // re-register with our current one...
    UnregisterPluginForGeometryUpdates();
  }
  mRootPresContextRegisteredWith = rpc;
  mRootPresContextRegisteredWith->RegisterPluginForGeometryUpdates(mContent);
}

void
nsPluginFrame::UnregisterPluginForGeometryUpdates()
{
  if (!mRootPresContextRegisteredWith) {
    // Not registered...
    return;
  }
  mRootPresContextRegisteredWith->UnregisterPluginForGeometryUpdates(mContent);
  mRootPresContextRegisteredWith = nullptr;
}

void
nsPluginFrame::SetInstanceOwner(nsPluginInstanceOwner* aOwner)
{
  // The ownership model here is historically fuzzy. This should only be called
  // by nsPluginInstanceOwner when it is given a new frame, and
  // nsObjectLoadingContent should be arbitrating frame-ownership via its
  // HasNewFrame callback.
  mInstanceOwner = aOwner;

  // Reset the DidCompositeObserver since the owner changed.
  mDidCompositeObserver = nullptr;

  if (mInstanceOwner) {
    return;
  }

  UnregisterPluginForGeometryUpdates();
  if (mWidget && mInnerView) {
    mInnerView->DetachWidgetEventHandler(mWidget);
    // Make sure the plugin is hidden in case an update of plugin geometry
    // hasn't happened since this plugin became hidden.
    nsIWidget* parent = mWidget->GetParent();
    if (parent) {
      nsTArray<nsIWidget::Configuration> configurations;
      nsIWidget::Configuration* configuration = configurations.AppendElement();
      configuration->mChild = mWidget;
      parent->ConfigureChildren(configurations);

      mWidget->Show(false);
      mWidget->Enable(false);
      mWidget->SetParent(nullptr);
    }
  }
}

bool
nsPluginFrame::IsFocusable(int32_t *aTabIndex, bool aWithMouse)
{
  if (aTabIndex)
    *aTabIndex = -1;
  return nsFrame::IsFocusable(aTabIndex, aWithMouse);
}

bool
nsPluginFrame::IsHidden(bool aCheckVisibilityStyle) const
{
  if (aCheckVisibilityStyle) {
    if (!StyleVisibility()->IsVisibleOrCollapsed())
      return true;
  }

  // only <embed> tags support the HIDDEN attribute
  if (mContent->IsHTMLElement(nsGkAtoms::embed)) {
    // Yes, these are really the kooky ways that you could tell 4.x
    // not to hide the <embed> once you'd put the 'hidden' attribute
    // on the tag...

    // HIDDEN w/ no attributes gets translated as we are hidden for
    // compatibility w/ 4.x and IE so we don't create a non-painting
    // widget in layout. See bug 188959.
    nsAutoString hidden;
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::hidden, hidden) &&
       (hidden.IsEmpty() ||
        (!hidden.LowerCaseEqualsLiteral("false") &&
         !hidden.LowerCaseEqualsLiteral("no") &&
         !hidden.LowerCaseEqualsLiteral("off")))) {
      return true;
    }
  }

  return false;
}

mozilla::LayoutDeviceIntPoint
nsPluginFrame::GetRemoteTabChromeOffset()
{
  LayoutDeviceIntPoint offset;
  if (XRE_IsContentProcess()) {
    if (nsPIDOMWindowOuter* window = GetContent()->OwnerDoc()->GetWindow()) {
      if (nsCOMPtr<nsPIDOMWindowOuter> topWindow = window->GetTop()) {
        dom::TabChild* tc = dom::TabChild::GetFrom(topWindow);
        if (tc) {
          offset += tc->GetChromeOffset();
        }
      }
    }
  }
  return offset;
}

nsIntPoint
nsPluginFrame::GetWindowOriginInPixels(bool aWindowless)
{
  nsView * parentWithView;
  nsPoint origin(0,0);

  GetOffsetFromView(origin, &parentWithView);

  // if it's windowless, let's make sure we have our origin set right
  // it may need to be corrected, like after scrolling
  if (aWindowless && parentWithView) {
    nsPoint offsetToWidget;
    parentWithView->GetNearestWidget(&offsetToWidget);
    origin += offsetToWidget;
  }
  origin += GetContentRectRelativeToSelf().TopLeft();

  nsIntPoint pt(PresContext()->AppUnitsToDevPixels(origin.x),
                PresContext()->AppUnitsToDevPixels(origin.y));

  // If we're in the content process offsetToWidget is tied to the top level
  // widget we can access in the child process, which is the tab. We need the
  // offset all the way up to the top level native window here. (If this is
  // non-e10s this routine will return 0,0.)
  if (aWindowless) {
    mozilla::LayoutDeviceIntPoint lpt = GetRemoteTabChromeOffset();
    pt += nsIntPoint(lpt.x, lpt.y);
  }

  return pt;
}

void
nsPluginFrame::DidReflow(nsPresContext*     aPresContext,
                         const ReflowInput* aReflowInput)
{
  // Do this check before calling the superclass, as that clears
  // NS_FRAME_FIRST_REFLOW
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    nsCOMPtr<nsIObjectLoadingContent> objContent(do_QueryInterface(mContent));
    NS_ASSERTION(objContent, "Why not an object loading content?");
    objContent->HasNewFrame(this);
  }

  nsFrame::DidReflow(aPresContext, aReflowInput);

  if (HasView()) {
    nsView* view = GetView();
    nsViewManager* vm = view->GetViewManager();
    if (vm)
      vm->SetViewVisibility(view, IsHidden() ? nsViewVisibility_kHide : nsViewVisibility_kShow);
  }
}

/* static */ void
nsPluginFrame::PaintPrintPlugin(nsIFrame* aFrame, gfxContext* aCtx,
                                const nsRect& aDirtyRect, nsPoint aPt)
{
  // Translate the context:
  nsPoint pt = aPt + aFrame->GetContentRectRelativeToSelf().TopLeft();
  gfxPoint devPixelPt =
    nsLayoutUtils::PointToGfxPoint(pt, aFrame->PresContext()->AppUnitsPerDevPixel());

  gfxContextMatrixAutoSaveRestore autoSR(aCtx);
  aCtx->SetMatrixDouble(aCtx->CurrentMatrixDouble().PreTranslate(devPixelPt));

  // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?

  static_cast<nsPluginFrame*>(aFrame)->PrintPlugin(*aCtx, aDirtyRect);
}

/**
 * nsDisplayPluginReadback creates an active ReadbackLayer. The ReadbackLayer
 * obtains from the compositor the contents of the window underneath
 * the ReadbackLayer, which we then use as an opaque buffer for plugins to
 * asynchronously draw onto.
 */
class nsDisplayPluginReadback : public nsDisplayItem {
public:
  nsDisplayPluginReadback(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayPluginReadback);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  ~nsDisplayPluginReadback() override {
    MOZ_COUNT_DTOR(nsDisplayPluginReadback);
  }
#endif

  nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                   bool* aSnap) const override;

  NS_DISPLAY_DECL_NAME("PluginReadback", TYPE_PLUGIN_READBACK)

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override
  {
    return static_cast<nsPluginFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }

  LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return LAYER_ACTIVE;
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayPluginGeometry(this, aBuilder);
  }
};

static nsRect
GetDisplayItemBounds(nsDisplayListBuilder* aBuilder,
                     const nsDisplayItem* aItem,
                     nsIFrame* aFrame)
{
  // XXX For slightly more accurate region computations we should pixel-snap this
  return aFrame->GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
}

nsRect
nsDisplayPluginReadback::GetBounds(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const
{
  *aSnap = false;
  return GetDisplayItemBounds(aBuilder, this, mFrame);
}

nsRect
nsDisplayPlugin::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const
{
  *aSnap = true;
  return GetDisplayItemBounds(aBuilder, this, mFrame);
}

void
nsDisplayPlugin::Paint(nsDisplayListBuilder* aBuilder,
                       gfxContext* aCtx)
{
  nsPluginFrame* f = static_cast<nsPluginFrame*>(mFrame);
  bool snap;
  f->PaintPlugin(aBuilder, *aCtx, mVisibleRect, GetBounds(aBuilder, &snap));
}

static nsRect
GetClippedBoundsIncludingAllScrollClips(nsDisplayItem* aItem,
                                        nsDisplayListBuilder* aBuilder)
{
  nsRect r = aItem->GetClippedBounds(aBuilder);
  for (auto* sc = aItem->GetClipChain(); sc; sc = sc->mParent) {
    r = sc->mClip.ApplyNonRoundedIntersection(r);
  }
  return r;
}

bool
nsDisplayPlugin::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion)
{
  if (aBuilder->IsForPluginGeometry()) {
    nsPluginFrame* f = static_cast<nsPluginFrame*>(mFrame);
    if (!aBuilder->IsInTransform() || f->IsPaintedByGecko()) {
      // Since transforms induce reference frames, we don't need to worry
      // about this method fluffing out due to non-rectilinear transforms.
      nsRect rAncestor = nsLayoutUtils::TransformFrameRectToAncestor(f,
          f->GetContentRectRelativeToSelf(), ReferenceFrame());
      nscoord appUnitsPerDevPixel =
        ReferenceFrame()->PresContext()->AppUnitsPerDevPixel();
      f->mNextConfigurationBounds = LayoutDeviceIntRect::FromUnknownRect(
        rAncestor.ToNearestPixels(appUnitsPerDevPixel));

      nsRegion visibleRegion;
      // Apply all scroll clips when computing the clipped bounds of this item.
      // We hide windowed plugins during APZ scrolling, so there never is an
      // async transform that we need to take into account when clipping.
      visibleRegion.And(*aVisibleRegion, GetClippedBoundsIncludingAllScrollClips(this, aBuilder));
      // Make visibleRegion relative to f
      visibleRegion.MoveBy(-ToReferenceFrame());

      f->mNextConfigurationClipRegion.Clear();
      for (auto iter = visibleRegion.RectIter(); !iter.Done(); iter.Next()) {
        nsRect rAncestor =
          nsLayoutUtils::TransformFrameRectToAncestor(f, iter.Get(), ReferenceFrame());
        LayoutDeviceIntRect rPixels =
          LayoutDeviceIntRect::FromUnknownRect(rAncestor.ToNearestPixels(appUnitsPerDevPixel)) -
          f->mNextConfigurationBounds.TopLeft();
        if (!rPixels.IsEmpty()) {
          f->mNextConfigurationClipRegion.AppendElement(rPixels);
        }
      }
    }

    if (f->mInnerView) {
      // This should produce basically the same rectangle (but not relative
      // to the root frame). We only call this here for the side-effect of
      // setting mViewToWidgetOffset on the view.
      f->mInnerView->CalcWidgetBounds(eWindowType_plugin);
    }
  }

  return nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion);
}

nsRegion
nsDisplayPlugin::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                 bool* aSnap) const
{
  *aSnap = false;
  nsRegion result;
  nsPluginFrame* f = static_cast<nsPluginFrame*>(mFrame);
  if (!aBuilder->IsForPluginGeometry()) {
    nsIWidget* widget = f->GetWidget();
    if (widget) {
      // Be conservative and treat plugins with widgets as not opaque,
      // because that's simple and we might need the content under the widget
      // if the widget is unexpectedly clipped away. (As can happen when
      // chrome content over a plugin forces us to clip out the plugin for
      // security reasons.)
      // We shouldn't be repainting the content under plugins much anyway
      // since there generally shouldn't be anything to invalidate or paint
      // in PaintedLayers there.
  	  return result;
    }
  }

  if (f->IsOpaque()) {
    nsRect bounds = GetBounds(aBuilder, aSnap);
    if (aBuilder->IsForPluginGeometry() ||
        (f->GetPaintedRect(this) + ToReferenceFrame()).Contains(bounds)) {
      // We can treat this as opaque
      result = bounds;
    }
  }

  return result;
}

bool
nsDisplayPlugin::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         mozilla::wr::IpcResourceUpdateQueue& aResources,
                                         const StackingContextHelper& aSc,
                                         mozilla::layers::WebRenderLayerManager* aManager,
                                         nsDisplayListBuilder* aDisplayListBuilder)
{
  return static_cast<nsPluginFrame*>(mFrame)->CreateWebRenderCommands(this,
                                                                      aBuilder,
                                                                      aResources,
                                                                      aSc,
                                                                      aManager,
                                                                      aDisplayListBuilder);
}

nsresult
nsPluginFrame::PluginEventNotifier::Run() {
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  obsSvc->NotifyObservers(nullptr, "plugin-changed-event", mEventType.get());
  return NS_OK;
}

void
nsPluginFrame::NotifyPluginReflowObservers()
{
  nsContentUtils::AddScriptRunner(new PluginEventNotifier(NS_LITERAL_STRING("reflow")));
}

void
nsPluginFrame::DidSetWidgetGeometry()
{
#if defined(XP_MACOSX)
  if (mInstanceOwner && !IsHidden()) {
    mInstanceOwner->FixUpPluginWindow(nsPluginInstanceOwner::ePluginPaintEnable);
  }
#else
  if (!mWidget && mInstanceOwner) {
    // UpdateWindowVisibility will notify the plugin of position changes
    // by updating the NPWindow and calling NPP_SetWindow/AsyncSetWindow.
    // We treat windowless plugins inside popups as always visible, since
    // plugins inside popups don't get valid mNextConfigurationBounds
    // set up.
    mInstanceOwner->UpdateWindowVisibility(
      nsLayoutUtils::IsPopup(nsLayoutUtils::GetDisplayRootFrame(this)) ||
      !mNextConfigurationBounds.IsEmpty());
  }
#endif
}

bool
nsPluginFrame::IsOpaque() const
{
#if defined(XP_MACOSX)
  return false;
#else

  if (mInstanceOwner && mInstanceOwner->UseAsyncRendering()) {
    return false;
  }
  return !IsTransparentMode();
#endif
}

bool
nsPluginFrame::IsTransparentMode() const
{
#if defined(XP_MACOSX)
  return false;
#else
  if (!mInstanceOwner)
    return false;

  NPWindow *window = nullptr;
  mInstanceOwner->GetWindow(window);
  if (!window) {
    return false;
  }

  if (window->type != NPWindowTypeDrawable)
    return false;

  nsresult rv;
  RefPtr<nsNPAPIPluginInstance> pi;
  rv = mInstanceOwner->GetInstance(getter_AddRefs(pi));
  if (NS_FAILED(rv) || !pi)
    return false;

  bool transparent = false;
  pi->IsTransparent(&transparent);
  return transparent;
#endif
}

void
nsPluginFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists)
{
  // XXX why are we painting collapsed object frames?
  if (!IsVisibleOrCollapsedForPainting(aBuilder))
    return;

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  nsPresContext::nsPresContextType type = PresContext()->Type();

  // If we are painting in Print Preview do nothing....
  if (type == nsPresContext::eContext_PrintPreview)
    return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsPluginFrame");

#ifndef XP_MACOSX
  if (mWidget && aBuilder->IsInTransform()) {
    // Windowed plugins should not be rendered inside a transform.
    return;
  }
#endif

  if (aBuilder->IsForPainting() && mInstanceOwner) {
    // Update plugin frame for both content scaling and full zoom changes.
    mInstanceOwner->ResolutionMayHaveChanged();
#ifdef XP_MACOSX
    mInstanceOwner->WindowFocusMayHaveChanged();
#endif
    if (mInstanceOwner->UseAsyncRendering()) {
      NPWindow* window = nullptr;
      mInstanceOwner->GetWindow(window);
      bool isVisible = window && window->width > 0 && window->height > 0;
      if (isVisible && aBuilder->ShouldSyncDecodeImages()) {
#ifndef XP_MACOSX
        mInstanceOwner->UpdateWindowVisibility(true);
#endif
      }

      mInstanceOwner->NotifyPaintWaiter(aBuilder);
    }
  }

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this);

  // determine if we are printing
  if (type == nsPresContext::eContext_Print) {
    aLists.Content()->AppendToTop(new (aBuilder)
      nsDisplayGeneric(aBuilder, this, PaintPrintPlugin, "PrintPlugin",
                       DisplayItemType::TYPE_PRINT_PLUGIN));
  } else {
    LayerState state = GetLayerState(aBuilder, nullptr);
    if (state == LAYER_INACTIVE &&
        nsDisplayItem::ForceActiveLayers()) {
      state = LAYER_ACTIVE;
    }
    if (aBuilder->IsPaintingToWindow() &&
        state == LAYER_ACTIVE &&
        IsTransparentMode()) {
      aLists.Content()->AppendToTop(new (aBuilder)
        nsDisplayPluginReadback(aBuilder, this));
    }

    aLists.Content()->AppendToTop(new (aBuilder)
      nsDisplayPlugin(aBuilder, this));
  }
}

void
nsPluginFrame::PrintPlugin(gfxContext& aRenderingContext,
                           const nsRect& aDirtyRect)
{
  nsCOMPtr<nsIObjectLoadingContent> obj(do_QueryInterface(mContent));
  if (!obj)
    return;

  nsIFrame* frame = nullptr;
  obj->GetPrintFrame(&frame);
  if (!frame)
    return;

  nsPresContext* presContext = PresContext();
  // make sure this is REALLY an nsIObjectFrame
  // we may need to go through the children to get it
  nsIObjectFrame* objectFrame = do_QueryFrame(frame);
  if (!objectFrame)
    objectFrame = GetNextObjectFrame(presContext,frame);
  if (!objectFrame)
    return;

  // finally we can get our plugin instance
  RefPtr<nsNPAPIPluginInstance> pi;
  if (NS_FAILED(objectFrame->GetPluginInstance(getter_AddRefs(pi))) || !pi)
    return;

  // now we need to setup the correct location for printing
  NPWindow window;
  window.window = nullptr;

  // prepare embedded mode printing struct
  NPPrint npprint;
  npprint.mode = NP_EMBED;

  // we need to find out if we are windowless or not
  bool windowless = false;
  pi->IsWindowless(&windowless);
  window.type = windowless ? NPWindowTypeDrawable : NPWindowTypeWindow;

  window.clipRect.bottom = 0; window.clipRect.top = 0;
  window.clipRect.left = 0; window.clipRect.right = 0;

// platform specific printing code
#if defined(XP_UNIX) || defined(XP_MACOSX)
  // Doesn't work in a thebes world, or on OS X.
  (void)window;
  (void)npprint;
#elif defined(XP_WIN)

  /* On Windows, we use the win32 printing surface to print.  This, in
   * turn, uses the Cairo paginated surface, which in turn uses the
   * meta surface to record all operations and then play them back.
   * This doesn't work too well for plugins, because if plugins render
   * directly into the DC, the meta surface won't have any knowledge
   * of them, and so at the end when it actually does the replay step,
   * it'll fill the background with white and draw over whatever was
   * rendered before.
   *
   * So, to avoid this, we use PushGroup, which creates a new windows
   * surface, the plugin renders to that, and then we use normal
   * cairo methods to composite that in such that it's recorded using the
   * meta surface.
   */

  /* we'll already be translated into the right spot by gfxWindowsNativeDrawing */
  nsSize contentSize = GetContentRectRelativeToSelf().Size();
  window.x = 0;
  window.y = 0;
  window.width = presContext->AppUnitsToDevPixels(contentSize.width);
  window.height = presContext->AppUnitsToDevPixels(contentSize.height);

  aRenderingContext.Save();

  /* Make sure plugins don't do any damage outside of where they're supposed to */
  aRenderingContext.NewPath();
  gfxRect r(window.x, window.y, window.width, window.height);
  aRenderingContext.Rectangle(r);
  aRenderingContext.Clip();

  gfxWindowsNativeDrawing nativeDraw(&aRenderingContext, r);
  do {
    HDC dc = nativeDraw.BeginNativeDrawing();
    if (!dc)
      return;

    // XXX don't we need to call nativeDraw.TransformToNativeRect here?
    npprint.print.embedPrint.platformPrint = dc;
    npprint.print.embedPrint.window = window;
    // send off print info to plugin
    pi->Print(&npprint);

    nativeDraw.EndNativeDrawing();
  } while (nativeDraw.ShouldRenderAgain());
  nativeDraw.PaintToContext();

  aRenderingContext.Restore();
#endif

  // XXX Nav 4.x always sent a SetWindow call after print. Should we do the same?
  // XXX Calling DidReflow here makes no sense!!!
  frame->DidReflow(presContext, nullptr);  // DidReflow will take care of it
}

nsRect
nsPluginFrame::GetPaintedRect(const nsDisplayPlugin* aItem) const
{
  if (!mInstanceOwner)
    return nsRect();
  nsRect r = GetContentRectRelativeToSelf();
  if (!mInstanceOwner->UseAsyncRendering())
    return r;

  nsIntSize size = mInstanceOwner->GetCurrentImageSize();
  nsPresContext* pc = PresContext();
  r.IntersectRect(r, nsRect(0, 0, pc->DevPixelsToAppUnits(size.width),
                                  pc->DevPixelsToAppUnits(size.height)));
  return r;
}

LayerState
nsPluginFrame::GetLayerState(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager)
{
  if (!mInstanceOwner)
    return LAYER_NONE;

  if (mInstanceOwner->NeedsScrollImageLayer()) {
    return LAYER_ACTIVE;
  }

  if (!mInstanceOwner->UseAsyncRendering()) {
    return LAYER_NONE;
  }

  return LAYER_ACTIVE_FORCE;
}

class PluginFrameDidCompositeObserver final : public DidCompositeObserver
{
public:
  PluginFrameDidCompositeObserver(nsPluginInstanceOwner* aOwner, LayerManager* aLayerManager)
    : mInstanceOwner(aOwner),
      mLayerManager(aLayerManager)
  {
  }
  ~PluginFrameDidCompositeObserver() {
    mLayerManager->RemoveDidCompositeObserver(this);
  }
  void DidComposite() override {
    mInstanceOwner->DidComposite();
  }
  bool IsValid(LayerManager* aLayerManager) {
    return aLayerManager == mLayerManager;
  }

private:
  nsPluginInstanceOwner* mInstanceOwner;
  RefPtr<LayerManager> mLayerManager;
};

bool
nsPluginFrame::GetBounds(nsDisplayItem* aItem, IntSize& aSize, gfxRect& aRect)
{
  if (!mInstanceOwner)
    return false;

  NPWindow* window = nullptr;
  mInstanceOwner->GetWindow(window);
  if (!window)
    return false;

  if (window->width <= 0 || window->height <= 0)
    return false;

#if defined(XP_MACOSX)
  // window is in "display pixels", but size needs to be in device pixels
  // window must be in "display pixels"
  double scaleFactor = 1.0;
  if (NS_FAILED(mInstanceOwner->GetContentsScaleFactor(&scaleFactor))) {
    scaleFactor = 1.0;
  }

  size_t intScaleFactor = ceil(scaleFactor);
#else
  size_t intScaleFactor = 1;
#endif

  aSize = IntSize(window->width * intScaleFactor, window->height * intScaleFactor);

  nsRect area = GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
  aRect = nsLayoutUtils::RectToGfxRect(area, PresContext()->AppUnitsPerDevPixel());
  // to provide crisper and faster drawing.
  aRect.Round();

  return true;
}

bool
nsPluginFrame::CreateWebRenderCommands(nsDisplayItem* aItem,
                                       mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder)
{
  IntSize size;
  gfxRect r;
  if (!GetBounds(aItem, size, r)) {
    return true;
  }

  RefPtr<ImageContainer> container;
  // Image for Windowed plugins that support window capturing for scroll
  // operations or async windowless rendering.
  container = mInstanceOwner->GetImageContainer();
  if (!container) {
    // This can occur if our instance is gone or if the current plugin
    // configuration does not require a backing image layer.
    return true;
  }

#ifdef XP_MACOSX
  if (!mInstanceOwner->UseAsyncRendering()) {
    mInstanceOwner->DoCocoaEventDrawRect(r, nullptr);
  }
#endif

  RefPtr<LayerManager> lm = aDisplayListBuilder->GetWidgetLayerManager();
  if (!mDidCompositeObserver || !mDidCompositeObserver->IsValid(lm)) {
    mDidCompositeObserver = MakeUnique<PluginFrameDidCompositeObserver>(mInstanceOwner, lm);
  }
  lm->AddDidCompositeObserver(mDidCompositeObserver.get());

  LayoutDeviceRect dest(r.x, r.y, size.width, size.height);
  return aManager->CommandBuilder().PushImage(aItem, container, aBuilder, aResources, aSc, dest);
}


already_AddRefed<Layer>
nsPluginFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                          LayerManager* aManager,
                          nsDisplayItem* aItem,
                          const ContainerLayerParameters& aContainerParameters)
{
  IntSize size;
  gfxRect r;
  if (!GetBounds(aItem, size, r)) {
    return nullptr;
  }

  RefPtr<Layer> layer =
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));

  if (aItem->GetType() == DisplayItemType::TYPE_PLUGIN) {
    RefPtr<ImageContainer> container;
    // Image for Windowed plugins that support window capturing for scroll
    // operations or async windowless rendering.
    container = mInstanceOwner->GetImageContainer();
    if (!container) {
      // This can occur if our instance is gone or if the current plugin
      // configuration does not require a backing image layer.
      return nullptr;
    }

    if (!layer) {
      mInstanceOwner->NotifyPaintWaiter(aBuilder);
      // Initialize ImageLayer
      layer = aManager->CreateImageLayer();
      if (!layer)
        return nullptr;
    }

    NS_ASSERTION(layer->GetType() == Layer::TYPE_IMAGE, "Bad layer type");
    ImageLayer* imglayer = static_cast<ImageLayer*>(layer.get());
#ifdef XP_MACOSX
    if (!mInstanceOwner->UseAsyncRendering()) {
      mInstanceOwner->DoCocoaEventDrawRect(r, nullptr);
    }
#endif

    imglayer->SetScaleToSize(size, ScaleMode::STRETCH);
    imglayer->SetContainer(container);
    SamplingFilter samplingFilter = nsLayoutUtils::GetSamplingFilterForFrame(this);
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    if (!aManager->IsCompositingCheap()) {
      // Pixman just horrible with bilinear filter scaling
      samplingFilter = SamplingFilter::POINT;
    }
#endif
    imglayer->SetSamplingFilter(samplingFilter);

    layer->SetContentFlags(IsOpaque() ? Layer::CONTENT_OPAQUE : 0);

    if (aBuilder->IsPaintingToWindow() &&
        aBuilder->GetWidgetLayerManager() &&
        (aBuilder->GetWidgetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT ||
         aBuilder->GetWidgetLayerManager()->GetBackendType() == LayersBackend::LAYERS_WR) &&
        mInstanceOwner->UseAsyncRendering())
    {
      RefPtr<LayerManager> lm = aBuilder->GetWidgetLayerManager();
      if (!mDidCompositeObserver || !mDidCompositeObserver->IsValid(lm)) {
        mDidCompositeObserver = MakeUnique<PluginFrameDidCompositeObserver>(mInstanceOwner, lm);
      }
      lm->AddDidCompositeObserver(mDidCompositeObserver.get());
    }
  } else {
    NS_ASSERTION(aItem->GetType() == DisplayItemType::TYPE_PLUGIN_READBACK,
                 "Unknown item type");
    MOZ_ASSERT(!IsOpaque(), "Opaque plugins don't use backgrounds");

    if (!layer) {
      layer = aManager->CreateReadbackLayer();
      if (!layer)
        return nullptr;
    }
    NS_ASSERTION(layer->GetType() == Layer::TYPE_READBACK, "Bad layer type");

    ReadbackLayer* readback = static_cast<ReadbackLayer*>(layer.get());
    if (readback->GetSize() != size) {
      // This will destroy any old background sink and notify us that the
      // background is now unknown
      readback->SetSink(nullptr);
      readback->SetSize(size);

      if (mBackgroundSink) {
        // Maybe we still have a background sink associated with another
        // readback layer that wasn't recycled for some reason? Unhook it
        // now so that if this frame goes away, it doesn't have a dangling
        // reference to us.
        mBackgroundSink->Destroy();
      }
      mBackgroundSink =
        new PluginBackgroundSink(this,
                                 readback->AllocateSequenceNumber());
      readback->SetSink(mBackgroundSink);
      // The layer has taken ownership of our sink. When either the sink dies
      // or the frame dies, the connection from the surviving object is nulled out.
    }
  }

  // Set a transform on the layer to draw the plugin in the right place
  gfxPoint p = r.TopLeft() + aContainerParameters.mOffset;
  Matrix transform = Matrix::Translation(p.x, p.y);

  layer->SetBaseTransform(Matrix4x4::From2D(transform));
  return layer.forget();
}

void
nsPluginFrame::PaintPlugin(nsDisplayListBuilder* aBuilder,
                           gfxContext& aRenderingContext,
                           const nsRect& aDirtyRect, const nsRect& aPluginRect)
{
#if defined(DEBUG)
  // On Desktop, we should have built a layer as we no longer support in-process
  // plugins or synchronous painting. We can only get here for windowed plugins
  // (which draw themselves), or via some error/unload state.
  if (mInstanceOwner) {
    NPWindow *window = nullptr;
    mInstanceOwner->GetWindow(window);
    MOZ_ASSERT(!window || window->type == NPWindowTypeWindow);
  }
#endif
}

nsresult
nsPluginFrame::HandleEvent(nsPresContext* aPresContext,
                           WidgetGUIEvent* anEvent,
                           nsEventStatus* anEventStatus)
{
  NS_ENSURE_ARG_POINTER(anEvent);
  NS_ENSURE_ARG_POINTER(anEventStatus);
  nsresult rv = NS_OK;

  if (!mInstanceOwner)
    return NS_ERROR_NULL_POINTER;

  mInstanceOwner->ConsiderNewEventloopNestingLevel();

  if (anEvent->mMessage == ePluginActivate) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(GetContent());
    if (fm && elem)
      return fm->SetFocus(elem, 0);
  }
  else if (anEvent->mMessage == ePluginFocus) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsCOMPtr<nsIContent> content = GetContent();
      return fm->FocusPlugin(content);
    }
  }

  if (mInstanceOwner->SendNativeEvents() &&
      anEvent->IsNativeEventDelivererForPlugin()) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    // Due to plugin code reentering Gecko, this frame may be dead at this
    // point.
    return rv;
  }

#ifdef XP_WIN
  rv = nsFrame::HandleEvent(aPresContext, anEvent, anEventStatus);
  return rv;
#endif

#ifdef XP_MACOSX
  // we want to process some native mouse events in the cocoa event model
  if ((anEvent->mMessage == eMouseEnterIntoWidget ||
       anEvent->mMessage == eWheel) &&
      mInstanceOwner->GetEventModel() == NPEventModelCocoa) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    // Due to plugin code reentering Gecko, this frame may be dead at this
    // point.
    return rv;
  }

  // These two calls to nsIPresShell::SetCapturingContext() (on mouse-down
  // and mouse-up) are needed to make the routing of mouse events while
  // dragging conform to standard OS X practice, and to the Cocoa NPAPI spec.
  // See bug 525078 and bug 909678.
  if (anEvent->mMessage == eMouseDown) {
    nsIPresShell::SetCapturingContent(GetContent(), CAPTURE_IGNOREALLOWED);
  }
#endif

  rv = nsFrame::HandleEvent(aPresContext, anEvent, anEventStatus);

  // We need to be careful from this point because the call to
  // nsFrame::HandleEvent() might have killed us.

#ifdef XP_MACOSX
  if (anEvent->mMessage == eMouseUp) {
    nsIPresShell::SetCapturingContent(nullptr, 0);
  }
#endif

  return rv;
}

void
nsPluginFrame::HandleWheelEventAsDefaultAction(WidgetWheelEvent* aWheelEvent)
{
  MOZ_ASSERT(WantsToHandleWheelEventAsDefaultAction());
  MOZ_ASSERT(!aWheelEvent->DefaultPrevented());

  if (NS_WARN_IF(!mInstanceOwner) ||
      NS_WARN_IF(aWheelEvent->mMessage != eWheel)) {
    return;
  }

  // If the wheel event has native message, it should may be handled by
  // HandleEvent() in the future.  In such case, we should do nothing here.
  if (NS_WARN_IF(!!aWheelEvent->mPluginEvent)) {
    return;
  }

  mInstanceOwner->ProcessEvent(*aWheelEvent);
  // We need to assume that the event is always consumed/handled by the
  // plugin.  There is no way to know if it's actually consumed/handled.
  aWheelEvent->mViewPortIsOverscrolled = false;
  aWheelEvent->mOverflowDeltaX = 0;
  aWheelEvent->mOverflowDeltaY = 0;
  // Consume the event explicitly.
  aWheelEvent->PreventDefault();
}

bool
nsPluginFrame::WantsToHandleWheelEventAsDefaultAction() const
{
#ifdef XP_WIN
  if (!mInstanceOwner) {
    return false;
  }
  NPWindow* window = nullptr;
  mInstanceOwner->GetWindow(window);
  // On Windows, only when the plugin is windowless, we need to send wheel
  // events as default action.
  return window->type == NPWindowTypeDrawable;
#else
  return false;
#endif
}

nsresult
nsPluginFrame::GetPluginInstance(nsNPAPIPluginInstance** aPluginInstance)
{
  *aPluginInstance = nullptr;

  if (!mInstanceOwner) {
    return NS_OK;
  }

  return mInstanceOwner->GetInstance(aPluginInstance);
}

nsresult
nsPluginFrame::GetCursor(const nsPoint& aPoint, nsIFrame::Cursor& aCursor)
{
  if (!mInstanceOwner) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsNPAPIPluginInstance> inst;
  mInstanceOwner->GetInstance(getter_AddRefs(inst));
  if (!inst) {
    return NS_ERROR_FAILURE;
  }

  bool useDOMCursor = static_cast<nsNPAPIPluginInstance*>(inst.get())->UsesDOMForCursor();
  if (!useDOMCursor) {
    return NS_ERROR_FAILURE;
  }

  return nsFrame::GetCursor(aPoint, aCursor);
}

void
nsPluginFrame::SetIsDocumentActive(bool aIsActive)
{
  if (mInstanceOwner) {
    mInstanceOwner->UpdateDocumentActiveState(aIsActive);
  }
}

// static
nsIObjectFrame *
nsPluginFrame::GetNextObjectFrame(nsPresContext* aPresContext, nsIFrame* aRoot)
{
  for (nsIFrame* child : aRoot->PrincipalChildList()) {
    nsIObjectFrame* outFrame = do_QueryFrame(child);
    if (outFrame) {
      RefPtr<nsNPAPIPluginInstance> pi;
      outFrame->GetPluginInstance(getter_AddRefs(pi));  // make sure we have a REAL plugin
      if (pi)
        return outFrame;
    }

    outFrame = GetNextObjectFrame(aPresContext, child);
    if (outFrame)
      return outFrame;
  }

  return nullptr;
}

/*static*/ void
nsPluginFrame::BeginSwapDocShells(nsISupports* aSupports, void*)
{
  NS_PRECONDITION(aSupports, "");
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (!content) {
    return;
  }

  // This function is called from a document content enumerator so we need
  // to filter out the nsPluginFrames and ignore the rest.
  nsIObjectFrame* obj = do_QueryFrame(content->GetPrimaryFrame());
  if (!obj)
    return;

  nsPluginFrame* objectFrame = static_cast<nsPluginFrame*>(obj);
  NS_ASSERTION(!objectFrame->mWidget || objectFrame->mWidget->GetParent(),
               "Plugin windows must not be toplevel");
  objectFrame->UnregisterPluginForGeometryUpdates();
}

/*static*/ void
nsPluginFrame::EndSwapDocShells(nsISupports* aSupports, void*)
{
  NS_PRECONDITION(aSupports, "");
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (!content) {
    return;
  }

  // This function is called from a document content enumerator so we need
  // to filter out the nsPluginFrames and ignore the rest.
  nsIObjectFrame* obj = do_QueryFrame(content->GetPrimaryFrame());
  if (!obj)
    return;

  nsPluginFrame* objectFrame = static_cast<nsPluginFrame*>(obj);
  nsRootPresContext* rootPC = objectFrame->PresContext()->GetRootPresContext();
  NS_ASSERTION(rootPC, "unable to register the plugin frame");
  nsIWidget* widget = objectFrame->mWidget;
  if (widget) {
    // Reparent the widget.
    nsIWidget* parent =
      rootPC->PresShell()->GetRootFrame()->GetNearestWidget();
    widget->SetParent(parent);
    AutoWeakFrame weakFrame(objectFrame);
    objectFrame->CallSetWindow();
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  if (objectFrame->mInstanceOwner) {
    objectFrame->RegisterPluginForGeometryUpdates();
  }
}

nsIFrame*
NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPluginFrame(aContext);
}

bool
nsPluginFrame::IsPaintedByGecko() const
{
#ifdef XP_MACOSX
  return true;
#else
  return !mWidget;
#endif
}

NS_IMPL_FRAMEARENA_HELPERS(nsPluginFrame)
