/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering objects for replaced elements implemented by a plugin */

#include "nsPluginFrame.h"

#include "gfx2DGlue.h"
#include "gfxMatrix.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/BasicEvents.h"
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
#include "nsRenderingContext.h"
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

// accessibility support
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1 /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

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

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "GLContext.h"
#endif

#include "mozilla/dom/TabChild.h"

#ifdef CreateEvent // Thank you MS.
#undef CreateEvent
#endif

#ifdef PR_LOGGING 
static PRLogModuleInfo *
GetObjectFrameLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("nsPluginFrame");
  return sLog;
}
#endif /* PR_LOGGING */

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

class PluginBackgroundSink : public ReadbackSink {
public:
  PluginBackgroundSink(nsPluginFrame* aFrame, uint64_t aStartSequenceNumber)
    : mLastSequenceNumber(aStartSequenceNumber), mFrame(aFrame) {}
  ~PluginBackgroundSink()
  {
    if (mFrame) {
      mFrame->mBackgroundSink = nullptr;
    }
  }

  virtual void SetUnknown(uint64_t aSequenceNumber)
  {
    if (!AcceptUpdate(aSequenceNumber))
      return;
    mFrame->mInstanceOwner->SetBackgroundUnknown();
  }

  virtual already_AddRefed<gfxContext>
      BeginUpdate(const nsIntRect& aRect, uint64_t aSequenceNumber)
  {
    if (!AcceptUpdate(aSequenceNumber))
      return nullptr;
    return mFrame->mInstanceOwner->BeginUpdateBackground(aRect);
  }

  virtual void EndUpdate(gfxContext* aContext, const nsIntRect& aRect)
  {
    return mFrame->mInstanceOwner->EndUpdateBackground(aContext, aRect);
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
  : nsPluginFrameSuper(aContext)
  , mReflowCallbackPosted(false)
{
  PR_LOG(GetObjectFrameLog(), PR_LOG_DEBUG,
         ("Created new nsPluginFrame %p\n", this));
}

nsPluginFrame::~nsPluginFrame()
{
  PR_LOG(GetObjectFrameLog(), PR_LOG_DEBUG,
         ("nsPluginFrame %p deleted\n", this));
}

NS_QUERYFRAME_HEAD(nsPluginFrame)
  NS_QUERYFRAME_ENTRY(nsPluginFrame)
  NS_QUERYFRAME_ENTRY(nsIObjectFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsPluginFrameSuper)

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
  PR_LOG(GetObjectFrameLog(), PR_LOG_DEBUG,
         ("Initializing nsPluginFrame %p for content %p\n", this, aContent));

  nsPluginFrameSuper::Init(aContent, aParent, aPrevInFlow);
}

void
nsPluginFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (mReflowCallbackPosted) {
    PresContext()->PresShell()->CancelReflowCallback(this);
  }

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

  nsPluginFrameSuper::DestroyFrom(aDestructRoot);
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

  nsPluginFrameSuper::DidSetStyleContext(aOldStyleContext);
}

nsIAtom*
nsPluginFrame::GetType() const
{
  return nsGkAtoms::objectFrame; 
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

    mInnerView = viewMan->CreateView(GetContentRectRelativeToSelf(), view);
    if (!mInnerView) {
      NS_ERROR("Could not create inner view");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    viewMan->InsertChild(view, mInnerView, nullptr, true);

    mWidget->SetParent(parentWidget);
    mWidget->Show(true);
    mWidget->Enable(true);

    // Set the plugin window to have an empty clip region until we know
    // what our true position, size and clip region are. These
    // will be reset when nsRootPresContext computes our true
    // geometry. The plugin window does need to have a good size here, so
    // set the size explicitly to a reasonable guess.
    nsAutoTArray<nsIWidget::Configuration,1> configurations;
    nsIWidget::Configuration* configuration = configurations.AppendElement();
    nscoord appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    configuration->mChild = mWidget;
    configuration->mBounds.width = NSAppUnitsToIntPixels(mRect.width, appUnitsPerDevPixel);
    configuration->mBounds.height = NSAppUnitsToIntPixels(mRect.height, appUnitsPerDevPixel);
    parentWidget->ConfigureChildren(configurations);

    nsRefPtr<nsDeviceContext> dx = viewMan->GetDeviceContext();
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
      nscolor bgcolor =
        frame->GetVisitedDependentColor(eCSSProperty_background_color);
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
    accService->RecreateAccessible(PresContext()->PresShell(), mContent);
  }
#endif

  return NS_OK;
}

#define EMBED_DEF_WIDTH 240
#define EMBED_DEF_HEIGHT 200

/* virtual */ nscoord
nsPluginFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;

  if (!IsHidden(false)) {
    if (mContent->IsAnyOfHTMLElements(nsGkAtoms::applet,
                                      nsGkAtoms::embed)) {
      bool vertical = GetWritingMode().IsVertical();
      result = nsPresContext::CSSPixelsToAppUnits(
        vertical ? EMBED_DEF_HEIGHT : EMBED_DEF_WIDTH);
    }
  }

  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsPluginFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
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
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    configuration->mWindowID = (uintptr_t)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
    configuration->mVisible = mWidget->IsVisible();
  }
#endif // defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
}

void
nsPluginFrame::GetDesiredSize(nsPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics)
{
  // By default, we have no area
  aMetrics.ClearSize();

  if (IsHidden(false)) {
    return;
  }

  aMetrics.Width() = aReflowState.ComputedWidth();
  aMetrics.Height() = aReflowState.ComputedHeight();

  // for EMBED and APPLET, default to 240x200 for compatibility
  if (mContent->IsAnyOfHTMLElements(nsGkAtoms::applet,
                                    nsGkAtoms::embed)) {
    if (aMetrics.Width() == NS_UNCONSTRAINEDSIZE) {
      aMetrics.Width() = clamped(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_WIDTH),
                               aReflowState.ComputedMinWidth(),
                               aReflowState.ComputedMaxWidth());
    }
    if (aMetrics.Height() == NS_UNCONSTRAINEDSIZE) {
      aMetrics.Height() = clamped(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_HEIGHT),
                                aReflowState.ComputedMinHeight(),
                                aReflowState.ComputedMaxHeight());
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
      (aReflowState.ComputedMinWidth() != NS_UNCONSTRAINEDSIZE) ?
        aReflowState.ComputedMinWidth() : 0;
  }

  // At this point, the height has an unconstrained value only in two cases:
  // a) We are in standards mode with percent heights and parent is auto-height
  // b) We have no height information at all.
  // In either case, we have to make up a number.
  if (aMetrics.Height() == NS_UNCONSTRAINEDSIZE) {
    aMetrics.Height() =
      (aReflowState.ComputedMinHeight() != NS_UNCONSTRAINEDSIZE) ?
        aReflowState.ComputedMinHeight() : 0;
  }

  // XXXbz don't add in the border and padding, because we screw up our
  // plugin's size and positioning if we do...  Eventually we _do_ want to
  // paint borders, though!  At that point, we will need to adjust the desired
  // size either here or in Reflow....  Further, we will need to fix Paint() to
  // call the superclass in all cases.
}

void
nsPluginFrame::Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsPluginFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);

  // Get our desired size
  GetDesiredSize(aPresContext, aReflowState, aMetrics);
  aMetrics.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aMetrics);

  // delay plugin instantiation until all children have
  // arrived. Otherwise there may be PARAMs or other stuff that the
  // plugin needs to see that haven't arrived yet.
  if (!GetContent()->IsDoneAddingChildren()) {
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  // if we are printing or print previewing, bail for now
  if (aPresContext->Medium() == nsGkAtoms::print) {
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  nsRect r(0, 0, aMetrics.Width(), aMetrics.Height());
  r.Deflate(aReflowState.ComputedPhysicalBorderPadding());

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

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
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
  double scaleFactor = 1.0;
  if (NS_FAILED(mInstanceOwner->GetContentsScaleFactor(&scaleFactor))) {
    scaleFactor = 1.0;
  }
  int intScaleFactor = ceil(scaleFactor);
  window->x = origin.x / intScaleFactor;
  window->y = origin.y / intScaleFactor;
  window->width = presContext->AppUnitsToDevPixels(aSize.width) / intScaleFactor;
  window->height = presContext->AppUnitsToDevPixels(aSize.height) / intScaleFactor;

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
  nsRefPtr<nsNPAPIPluginInstance> pi;
  if (!mInstanceOwner ||
      NS_FAILED(rv = mInstanceOwner->GetInstance(getter_AddRefs(pi))) ||
      !pi ||
      NS_FAILED(rv = mInstanceOwner->GetWindow(win)) || 
      !win)
    return rv;

  nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;

  if (aCheckIsHidden && IsHidden())
    return NS_ERROR_FAILURE;

  // refresh the plugin port as well
#ifdef XP_MACOSX
  mInstanceOwner->FixUpPluginWindow(nsPluginInstanceOwner::ePluginPaintEnable);
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

  // window must be in "display pixels"
  double scaleFactor = 1.0;
  if (NS_FAILED(mInstanceOwner->GetContentsScaleFactor(&scaleFactor))) {
    scaleFactor = 1.0;
  }
  size_t intScaleFactor = ceil(scaleFactor);
  window->x = intBounds.x / intScaleFactor;
  window->y = intBounds.y / intScaleFactor;
  window->width = intBounds.width / intScaleFactor;
  window->height = intBounds.height / intScaleFactor;

  // Calling SetWindow might destroy this frame. We need to use the instance
  // owner to clean up so hold a ref.
  nsRefPtr<nsPluginInstanceOwner> instanceOwnerRef(mInstanceOwner);

  // This will call pi->SetWindow and take care of window subclassing
  // if needed, see bug 132759. Calling SetWindow can destroy this frame
  // so check for that before doing anything else with this frame's memory.
  if (mInstanceOwner->UseAsyncRendering()) {
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
  return nsPluginFrameSuper::IsFocusable(aTabIndex, aWithMouse);
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
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(GetContent()->OwnerDoc()->GetWindow());
    if (window) {
      nsCOMPtr<nsIDOMWindow> topWindow;
      window->GetTop(getter_AddRefs(topWindow));
      if (topWindow) {
        dom::TabChild* tc = dom::TabChild::GetFrom(topWindow);
        if (tc) {
          LayoutDeviceIntPoint chromeOffset;
          tc->SendGetTabOffset(&chromeOffset);
          offset -= chromeOffset;
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
nsPluginFrame::DidReflow(nsPresContext*            aPresContext,
                         const nsHTMLReflowState*  aReflowState,
                         nsDidReflowStatus         aStatus)
{
  // Do this check before calling the superclass, as that clears
  // NS_FRAME_FIRST_REFLOW
  if (aStatus == nsDidReflowStatus::FINISHED &&
      (GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    nsCOMPtr<nsIObjectLoadingContent> objContent(do_QueryInterface(mContent));
    NS_ASSERTION(objContent, "Why not an object loading content?");
    objContent->HasNewFrame(this);
  }

  nsPluginFrameSuper::DidReflow(aPresContext, aReflowState, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (aStatus != nsDidReflowStatus::FINISHED)
    return;

  if (HasView()) {
    nsView* view = GetView();
    nsViewManager* vm = view->GetViewManager();
    if (vm)
      vm->SetViewVisibility(view, IsHidden() ? nsViewVisibility_kHide : nsViewVisibility_kShow);
  }
}

/* static */ void
nsPluginFrame::PaintPrintPlugin(nsIFrame* aFrame, nsRenderingContext* aCtx,
                                const nsRect& aDirtyRect, nsPoint aPt)
{
  gfxContext* ctx = aCtx->ThebesContext();

  // Translate the context:
  nsPoint pt = aPt + aFrame->GetContentRectRelativeToSelf().TopLeft();
  gfxPoint devPixelPt =
    nsLayoutUtils::PointToGfxPoint(pt, aFrame->PresContext()->AppUnitsPerDevPixel());

  gfxContextMatrixAutoSaveRestore autoSR(ctx);
  ctx->SetMatrix(ctx->CurrentMatrix().Translate(devPixelPt));

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
  virtual ~nsDisplayPluginReadback() {
    MOZ_COUNT_DTOR(nsDisplayPluginReadback);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override;

  NS_DISPLAY_DECL_NAME("PluginReadback", TYPE_PLUGIN_READBACK)

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override
  {
    return static_cast<nsPluginFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return LAYER_ACTIVE;
  }
};

static nsRect
GetDisplayItemBounds(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem, nsIFrame* aFrame)
{
  // XXX For slightly more accurate region computations we should pixel-snap this
  return aFrame->GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
}

nsRect
nsDisplayPluginReadback::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = false;
  return GetDisplayItemBounds(aBuilder, this, mFrame);
}

#ifdef MOZ_WIDGET_ANDROID

class nsDisplayPluginVideo : public nsDisplayItem {
public:
  nsDisplayPluginVideo(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsNPAPIPluginInstance::VideoInfo* aVideoInfo)
    : nsDisplayItem(aBuilder, aFrame), mVideoInfo(aVideoInfo)
  {
    MOZ_COUNT_CTOR(nsDisplayPluginVideo);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayPluginVideo() {
    MOZ_COUNT_DTOR(nsDisplayPluginVideo);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override;

  NS_DISPLAY_DECL_NAME("PluginVideo", TYPE_PLUGIN_VIDEO)

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override
  {
    return static_cast<nsPluginFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return LAYER_ACTIVE;
  }

  nsNPAPIPluginInstance::VideoInfo* VideoInfo() { return mVideoInfo; }

private:
  nsNPAPIPluginInstance::VideoInfo* mVideoInfo;
};

nsRect
nsDisplayPluginVideo::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = false;
  return GetDisplayItemBounds(aBuilder, this, mFrame);
}

#endif

nsRect
nsDisplayPlugin::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = true;
  return GetDisplayItemBounds(aBuilder, this, mFrame);
}

void
nsDisplayPlugin::Paint(nsDisplayListBuilder* aBuilder,
                       nsRenderingContext* aCtx)
{
  nsPluginFrame* f = static_cast<nsPluginFrame*>(mFrame);
  bool snap;
  f->PaintPlugin(aBuilder, *aCtx, mVisibleRect, GetBounds(aBuilder, &snap));
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
      f->mNextConfigurationBounds = rAncestor.ToNearestPixels(appUnitsPerDevPixel);

      nsRegion visibleRegion;
      visibleRegion.And(*aVisibleRegion, GetClippedBounds(aBuilder));
      // Make visibleRegion relative to f
      visibleRegion.MoveBy(-ToReferenceFrame());

      f->mNextConfigurationClipRegion.Clear();
      nsRegionRectIterator iter(visibleRegion);
      for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
        nsRect rAncestor =
          nsLayoutUtils::TransformFrameRectToAncestor(f, *r, ReferenceFrame());
        nsIntRect rPixels = rAncestor.ToNearestPixels(appUnitsPerDevPixel)
            - f->mNextConfigurationBounds.TopLeft();
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
                                 bool* aSnap)
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
#elif defined(MOZ_WIDGET_ANDROID)
  // We don't know, so just assume transparent
  return false;
#else
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
  nsRefPtr<nsNPAPIPluginInstance> pi;
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
                                const nsRect&           aDirtyRect,
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
#ifdef XP_MACOSX
    mInstanceOwner->ResolutionMayHaveChanged();
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
    aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(aBuilder, this, PaintPrintPlugin, "PrintPlugin",
                       nsDisplayItem::TYPE_PRINT_PLUGIN));
  } else {
    LayerState state = GetLayerState(aBuilder, nullptr);
    if (state == LAYER_INACTIVE &&
        nsDisplayItem::ForceActiveLayers()) {
      state = LAYER_ACTIVE;
    }
    // We don't need this on Android, and it just confuses things
#if !MOZ_WIDGET_ANDROID
    if (aBuilder->IsPaintingToWindow() &&
        state == LAYER_ACTIVE &&
        IsTransparentMode()) {
      aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayPluginReadback(aBuilder, this));
    }
#endif

#if MOZ_WIDGET_ANDROID
    if (aBuilder->IsPaintingToWindow() &&
        state == LAYER_ACTIVE) {

      nsTArray<nsNPAPIPluginInstance::VideoInfo*> videos;
      mInstanceOwner->GetVideos(videos);

      for (uint32_t i = 0; i < videos.Length(); i++) {
        aLists.Content()->AppendNewToTop(new (aBuilder)
          nsDisplayPluginVideo(aBuilder, this, videos[i]));
      }
    }
#endif

    aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayPlugin(aBuilder, this));
  }
}

void
nsPluginFrame::PrintPlugin(nsRenderingContext& aRenderingContext,
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
  nsRefPtr<nsNPAPIPluginInstance> pi;
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

  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

  /* Make sure plugins don't do any damage outside of where they're supposed to */
  ctx->NewPath();
  gfxRect r(window.x, window.y, window.width, window.height);
  ctx->Rectangle(r);
  ctx->Clip();

  gfxWindowsNativeDrawing nativeDraw(ctx, r);
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

  ctx->Restore();
#endif

  // XXX Nav 4.x always sent a SetWindow call after print. Should we do the same?
  // XXX Calling DidReflow here makes no sense!!!
  nsDidReflowStatus status = nsDidReflowStatus::FINISHED; // should we use a special status?
  frame->DidReflow(presContext,
                   nullptr, status);  // DidReflow will take care of it
}

nsRect
nsPluginFrame::GetPaintedRect(nsDisplayPlugin* aItem)
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

#ifdef MOZ_WIDGET_ANDROID
  // We always want a layer on Honeycomb and later
  if (AndroidBridge::Bridge()->GetAPIVersion() >= 11)
    return LAYER_ACTIVE;
#endif

  if (!mInstanceOwner->UseAsyncRendering()) {
    return LAYER_NONE;
  }

  return LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsPluginFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                          LayerManager* aManager,
                          nsDisplayItem* aItem,
                          const ContainerLayerParameters& aContainerParameters)
{
  if (!mInstanceOwner)
    return nullptr;

  NPWindow* window = nullptr;
  mInstanceOwner->GetWindow(window);
  if (!window)
    return nullptr;

  if (window->width <= 0 || window->height <= 0)
    return nullptr;

  // window is in "display pixels", but size needs to be in device pixels
  double scaleFactor = 1.0;
  if (NS_FAILED(mInstanceOwner->GetContentsScaleFactor(&scaleFactor))) {
    scaleFactor = 1.0;
  }
  int intScaleFactor = ceil(scaleFactor);
  IntSize size(window->width * intScaleFactor, window->height * intScaleFactor);

  nsRect area = GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
  gfxRect r = nsLayoutUtils::RectToGfxRect(area, PresContext()->AppUnitsPerDevPixel());
  // to provide crisper and faster drawing.
  r.Round();
  nsRefPtr<Layer> layer =
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));

  if (aItem->GetType() == nsDisplayItem::TYPE_PLUGIN) {
    // Create image
    nsRefPtr<ImageContainer> container = mInstanceOwner->GetImageContainer();
    if (!container) {
      // This can occur if our instance is gone.
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
    GraphicsFilter filter =
      nsLayoutUtils::GetGraphicsFilterForFrame(this);
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    if (!aManager->IsCompositingCheap()) {
      // Pixman just horrible with bilinear filter scaling
      filter = GraphicsFilter::FILTER_NEAREST;
    }
#endif
    imglayer->SetFilter(filter);

    layer->SetContentFlags(IsOpaque() ? Layer::CONTENT_OPAQUE : 0);
#ifdef MOZ_WIDGET_ANDROID
  } else if (aItem->GetType() == nsDisplayItem::TYPE_PLUGIN_VIDEO) {
    nsDisplayPluginVideo* videoItem = reinterpret_cast<nsDisplayPluginVideo*>(aItem);
    nsNPAPIPluginInstance::VideoInfo* videoInfo = videoItem->VideoInfo();

    nsRefPtr<ImageContainer> container = mInstanceOwner->GetImageContainerForVideo(videoInfo);
    if (!container)
      return nullptr;

    if (!layer) {
      // Initialize ImageLayer
      layer = aManager->CreateImageLayer();
      if (!layer)
        return nullptr;
    }

    ImageLayer* imglayer = static_cast<ImageLayer*>(layer.get());
    imglayer->SetContainer(container);

    layer->SetContentFlags(IsOpaque() ? Layer::CONTENT_OPAQUE : 0);

    // Set the offset and size according to the video dimensions
    r.MoveBy(videoInfo->mDimensions.TopLeft());
    size.width = videoInfo->mDimensions.width;
    size.height = videoInfo->mDimensions.height;
#endif
  } else {
    NS_ASSERTION(aItem->GetType() == nsDisplayItem::TYPE_PLUGIN_READBACK,
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
                           nsRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect, const nsRect& aPluginRect)
{
#if defined(XP_MACOSX)
  DrawTarget& aDrawTarget = *aRenderingContext.GetDrawTarget();
#endif

#if defined(MOZ_WIDGET_ANDROID)
  if (mInstanceOwner) {
    gfxRect frameGfxRect =
      PresContext()->AppUnitsToGfxUnits(aPluginRect);
    gfxRect dirtyGfxRect =
      PresContext()->AppUnitsToGfxUnits(aDirtyRect);

    gfxContext* ctx = aRenderingContext.ThebesContext();

    mInstanceOwner->Paint(ctx, frameGfxRect, dirtyGfxRect);
    return;
  }
#endif

  // Screen painting code
#if defined(XP_MACOSX)
  // delegate all painting to the plugin instance.
  if (mInstanceOwner) {
    if (mInstanceOwner->GetDrawingModel() == NPDrawingModelCoreGraphics ||
        mInstanceOwner->GetDrawingModel() == NPDrawingModelCoreAnimation ||
        mInstanceOwner->GetDrawingModel() == 
                                  NPDrawingModelInvalidatingCoreAnimation) {
      int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();
      // Clip to the content area where the plugin should be drawn. If
      // we don't do this, the plugin can draw outside its bounds.
      nsIntRect contentPixels = aPluginRect.ToNearestPixels(appUnitsPerDevPixel);
      nsIntRect dirtyPixels = aDirtyRect.ToOutsidePixels(appUnitsPerDevPixel);
      nsIntRect clipPixels;
      clipPixels.IntersectRect(contentPixels, dirtyPixels);

      // Don't invoke the drawing code if the clip is empty.
      if (clipPixels.IsEmpty())
        return;

      gfxRect nativeClipRect(clipPixels.x, clipPixels.y,
                             clipPixels.width, clipPixels.height);
      gfxContext* ctx = aRenderingContext.ThebesContext();

      gfxContextAutoSaveRestore save(ctx);
      ctx->NewPath();
      ctx->Rectangle(nativeClipRect);
      ctx->Clip();
      gfxPoint offset(contentPixels.x, contentPixels.y);
      ctx->SetMatrix(
        ctx->CurrentMatrix().Translate(offset));

      gfxQuartzNativeDrawing nativeDrawing(aDrawTarget,
                                           ToRect(nativeClipRect - offset));

      CGContextRef cgContext = nativeDrawing.BeginNativeDrawing();
      if (!cgContext) {
        NS_WARNING("null CGContextRef during PaintPlugin");
        return;
      }

      nsRefPtr<nsNPAPIPluginInstance> inst;
      GetPluginInstance(getter_AddRefs(inst));
      if (!inst) {
        NS_WARNING("null plugin instance during PaintPlugin");
        nativeDrawing.EndNativeDrawing();
        return;
      }
      NPWindow* window;
      mInstanceOwner->GetWindow(window);
      if (!window) {
        NS_WARNING("null plugin window during PaintPlugin");
        nativeDrawing.EndNativeDrawing();
        return;
      }
      NP_CGContext* cgPluginPortCopy =
                static_cast<NP_CGContext*>(mInstanceOwner->GetPluginPortCopy());
      if (!cgPluginPortCopy) {
        NS_WARNING("null plugin port copy during PaintPlugin");
        nativeDrawing.EndNativeDrawing();
        return;
      }

      mInstanceOwner->BeginCGPaint();
      if (mInstanceOwner->GetDrawingModel() == NPDrawingModelCoreAnimation ||
          mInstanceOwner->GetDrawingModel() == 
                                   NPDrawingModelInvalidatingCoreAnimation) {
        // CoreAnimation is updated, render the layer and perform a readback.
        mInstanceOwner->RenderCoreAnimation(cgContext, window->width, window->height);
      } else {
        mInstanceOwner->Paint(nativeClipRect - offset, cgContext);
      }
      mInstanceOwner->EndCGPaint();

      nativeDrawing.EndNativeDrawing();
    } else {
      gfxContext* ctx = aRenderingContext.ThebesContext();

      // Translate the context:
      gfxPoint devPixelPt =
        nsLayoutUtils::PointToGfxPoint(aPluginRect.TopLeft(),
                                       PresContext()->AppUnitsPerDevPixel());

      gfxContextMatrixAutoSaveRestore autoSR(ctx);
      ctx->SetMatrix(ctx->CurrentMatrix().Translate(devPixelPt));

      // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?

      // this rect is used only in the CoreGraphics drawing model
      gfxRect tmpRect(0, 0, 0, 0);
      mInstanceOwner->Paint(tmpRect, nullptr);
    }
  }
#elif defined(MOZ_X11)
  if (mInstanceOwner) {
    NPWindow *window;
    mInstanceOwner->GetWindow(window);
    if (window->type == NPWindowTypeDrawable) {
      gfxRect frameGfxRect =
        PresContext()->AppUnitsToGfxUnits(aPluginRect);
      gfxRect dirtyGfxRect =
        PresContext()->AppUnitsToGfxUnits(aDirtyRect);
      gfxContext* ctx = aRenderingContext.ThebesContext();

      mInstanceOwner->Paint(ctx, frameGfxRect, dirtyGfxRect);
    }
  }
#elif defined(XP_WIN)
  nsRefPtr<nsNPAPIPluginInstance> inst;
  GetPluginInstance(getter_AddRefs(inst));
  if (inst) {
    gfxRect frameGfxRect =
      PresContext()->AppUnitsToGfxUnits(aPluginRect);
    gfxRect dirtyGfxRect =
      PresContext()->AppUnitsToGfxUnits(aDirtyRect);
    gfxContext *ctx = aRenderingContext.ThebesContext();
    gfxMatrix currentMatrix = ctx->CurrentMatrix();

    if (ctx->UserToDevicePixelSnapped(frameGfxRect, false)) {
      dirtyGfxRect = ctx->UserToDevice(dirtyGfxRect);
      ctx->SetMatrix(gfxMatrix());
    }
    dirtyGfxRect.RoundOut();

    // Look if it's windowless
    NPWindow *window;
    mInstanceOwner->GetWindow(window);

    if (window->type == NPWindowTypeDrawable) {
      // the offset of the DC
      nsPoint origin;

      gfxWindowsNativeDrawing nativeDraw(ctx, frameGfxRect);
      if (nativeDraw.IsDoublePass()) {
        // OOP plugin specific: let the shim know before we paint if we are doing a
        // double pass render. If this plugin isn't oop, the register window message
        // will be ignored.
        NPEvent pluginEvent;
        pluginEvent.event = plugins::DoublePassRenderingEvent();
        pluginEvent.wParam = 0;
        pluginEvent.lParam = 0;
        if (pluginEvent.event)
          inst->HandleEvent(&pluginEvent, nullptr);
      }
      do {
        HDC hdc = nativeDraw.BeginNativeDrawing();
        if (!hdc)
          return;

        RECT dest;
        nativeDraw.TransformToNativeRect(frameGfxRect, dest);
        RECT dirty;
        nativeDraw.TransformToNativeRect(dirtyGfxRect, dirty);

        window->window = hdc;
        window->x = dest.left;
        window->y = dest.top;
        window->clipRect.left = 0;
        window->clipRect.top = 0;
        // if we're painting, we're visible.
        window->clipRect.right = window->width;
        window->clipRect.bottom = window->height;

        // Windowless plugins on windows need a special event to update their location,
        // see bug 135737.
        //
        // bug 271442: note, the rectangle we send is now purely the bounds of the plugin
        // relative to the window it is contained in, which is useful for the plugin to
        // correctly translate mouse coordinates.
        //
        // this does not mesh with the comments for bug 135737 which imply that the rectangle
        // must be clipped in some way to prevent the plugin attempting to paint over areas
        // it shouldn't.
        //
        // since the two uses of the rectangle are mutually exclusive in some cases, and
        // since I don't see any incorrect painting (at least with Flash and ViewPoint -
        // the originator of bug 135737), it seems that windowless plugins are not relying
        // on information here for clipping their drawing, and we can safely use this message
        // to tell the plugin exactly where it is in all cases.

        nsIntPoint origin = GetWindowOriginInPixels(true);
        nsIntRect winlessRect = nsIntRect(origin, nsIntSize(window->width, window->height));

        if (!mWindowlessRect.IsEqualEdges(winlessRect)) {
          mWindowlessRect = winlessRect;

          WINDOWPOS winpos;
          memset(&winpos, 0, sizeof(winpos));
          winpos.x = mWindowlessRect.x;
          winpos.y = mWindowlessRect.y;
          winpos.cx = mWindowlessRect.width;
          winpos.cy = mWindowlessRect.height;

          // finally, update the plugin by sending it a WM_WINDOWPOSCHANGED event
          NPEvent pluginEvent;
          pluginEvent.event = WM_WINDOWPOSCHANGED;
          pluginEvent.wParam = 0;
          pluginEvent.lParam = (LPARAM)&winpos;
          inst->HandleEvent(&pluginEvent, nullptr);
        }

        inst->SetWindow(window);

        mInstanceOwner->Paint(dirty, hdc);
        nativeDraw.EndNativeDrawing();
      } while (nativeDraw.ShouldRenderAgain());
      nativeDraw.PaintToContext();
    }

    ctx->SetMatrix(currentMatrix);
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

  if (anEvent->message == NS_PLUGIN_ACTIVATE) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(GetContent());
    if (fm && elem)
      return fm->SetFocus(elem, 0);
  }
  else if (anEvent->message == NS_PLUGIN_FOCUS) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm)
      return fm->FocusPlugin(GetContent());
  }

  if (mInstanceOwner->SendNativeEvents() &&
      anEvent->IsNativeEventDelivererForPlugin()) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    // Due to plugin code reentering Gecko, this frame may be dead at this
    // point.
    return rv;
  }

#ifdef XP_WIN
  rv = nsPluginFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
  return rv;
#endif

#ifdef XP_MACOSX
  // we want to process some native mouse events in the cocoa event model
  if ((anEvent->message == NS_MOUSE_ENTER ||
       anEvent->message == NS_WHEEL_WHEEL) &&
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
  if (anEvent->message == NS_MOUSE_BUTTON_DOWN) {
    nsIPresShell::SetCapturingContent(GetContent(), CAPTURE_IGNOREALLOWED);
  }
#endif

  rv = nsPluginFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);

  // We need to be careful from this point because the call to
  // nsPluginFrameSuper::HandleEvent() might have killed us.

#ifdef XP_MACOSX
  if (anEvent->message == NS_MOUSE_BUTTON_UP) {
    nsIPresShell::SetCapturingContent(nullptr, 0);
  }
#endif

  return rv;
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

  nsRefPtr<nsNPAPIPluginInstance> inst;
  mInstanceOwner->GetInstance(getter_AddRefs(inst));
  if (!inst) {
    return NS_ERROR_FAILURE;
  }

  bool useDOMCursor = static_cast<nsNPAPIPluginInstance*>(inst.get())->UsesDOMForCursor();
  if (!useDOMCursor) {
    return NS_ERROR_FAILURE;
  }

  return nsPluginFrameSuper::GetCursor(aPoint, aCursor);
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
  nsIFrame* child = aRoot->GetFirstPrincipalChild();

  while (child) {
    nsIObjectFrame* outFrame = do_QueryFrame(child);
    if (outFrame) {
      nsRefPtr<nsNPAPIPluginInstance> pi;
      outFrame->GetPluginInstance(getter_AddRefs(pi));  // make sure we have a REAL plugin
      if (pi)
        return outFrame;
    }

    outFrame = GetNextObjectFrame(aPresContext, child);
    if (outFrame)
      return outFrame;
    child = child->GetNextSibling();
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
    nsWeakFrame weakFrame(objectFrame);
    objectFrame->CallSetWindow();
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  objectFrame->RegisterPluginForGeometryUpdates();
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
