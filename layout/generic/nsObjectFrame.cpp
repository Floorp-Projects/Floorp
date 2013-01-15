/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering objects for replaced elements implemented by a plugin */

#include "mozilla/plugins/PluginMessageUtils.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMDragEvent.h"
#include "nsPluginHost.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsGkAtoms.h"
#include "nsIAppShell.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIPluginInstanceOwner.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIPluginTagInfo.h"
#include "plstr.h"
#include "nsILinkHandler.h"
#include "nsIScrollPositionListener.h"
#include "nsITimer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsDocShellCID.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIDocumentEncoder.h"
#include "nsXPIDLString.h"
#include "nsIDOMRange.h"
#include "nsIPluginWidget.h"
#include "nsGUIEvent.h"
#include "nsRenderingContext.h"
#include "npapi.h"
#include "nsTransform2D.h"
#include "nsIImageLoadingContent.h"
#include "nsIObjectLoadingContent.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsAttrName.h"
#include "nsDataHashtable.h"
#include "nsDOMClassInfo.h"
#include "nsFocusManager.h"
#include "nsLayoutUtils.h"
#include "nsFrameManager.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIScrollableFrame.h"
#include "mozilla/Preferences.h"
#include "sampler.h"
#include <algorithm>

// headers for plugin scriptability
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsIClassInfo.h"
#include "nsIDOMClientRect.h"

#include "nsObjectFrame.h"
#include "nsIObjectFrame.h"
#include "nsPluginNativeWindow.h"
#include "nsIPluginDocument.h"
#include "FrameLayerBuilder.h"

#include "nsThreadUtils.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "ImageLayers.h"

#ifdef XP_WIN
#include "gfxWindowsNativeDrawing.h"
#include "gfxWindowsSurface.h"
#endif

#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "Layers.h"
#include "ReadbackLayer.h"

// accessibility support
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1 /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include <errno.h>

#include "nsContentCID.h"
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef XP_MACOSX
#include "gfxQuartzNativeDrawing.h"
#include "nsPluginUtilsOSX.h"
#include "mozilla/gfx/QuartzSupport.h"
#endif

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "gfxXlibNativeRenderer.h"
#endif

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
using mozilla::DefaultXDisplay;
#endif

#ifdef XP_WIN
#include <wtypes.h>
#include <winuser.h>
#endif

#ifdef XP_OS2
#define INCL_PM
#define INCL_GPI
#include <os2.h>
#include "gfxOS2Surface.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "GLContext.h"
#endif

#ifdef CreateEvent // Thank you MS.
#undef CreateEvent
#endif

#ifdef PR_LOGGING 
static PRLogModuleInfo *
GetObjectFrameLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("nsObjectFrame");
  return sLog;
}
#endif /* PR_LOGGING */

#if defined(XP_MACOSX) && !defined(__LP64__)

// The header files QuickdrawAPI.h and QDOffscreen.h are missing on OS X 10.7
// and up (though the QuickDraw APIs defined in them are still present) -- so
// we need to supply the relevant parts of their contents here.  It's likely
// that Apple will eventually remove the APIs themselves (probably in OS X
// 10.8), so we need to make them weak imports, and test for their presence
// before using them.
extern "C" {
  #if !defined(__QUICKDRAWAPI__)
  extern void SetRect(
    Rect * r,
    short  left,
    short  top,
    short  right,
    short  bottom)
    __attribute__((weak_import));
  #endif /* __QUICKDRAWAPI__ */

  #if !defined(__QDOFFSCREEN__)
  extern QDErr NewGWorldFromPtr(
    GWorldPtr *   offscreenGWorld,
    UInt32        PixelFormat,
    const Rect *  boundsRect,
    CTabHandle    cTable,                /* can be NULL */
    GDHandle      aGDevice,              /* can be NULL */
    GWorldFlags   flags,
    Ptr           newBuffer,
    SInt32        rowBytes)
    __attribute__((weak_import));
  extern void DisposeGWorld(GWorldPtr offscreenGWorld)
    __attribute__((weak_import));
  #endif /* __QDOFFSCREEN__ */
}

#endif /* #if defined(XP_MACOSX) && !defined(__LP64__) */

using namespace mozilla;
using namespace mozilla::plugins;
using namespace mozilla::layers;

class PluginBackgroundSink : public ReadbackSink {
public:
  PluginBackgroundSink(nsObjectFrame* aFrame, uint64_t aStartSequenceNumber)
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
  nsObjectFrame* mFrame;
};

nsObjectFrame::nsObjectFrame(nsStyleContext* aContext)
  : nsObjectFrameSuper(aContext)
  , mReflowCallbackPosted(false)
{
  PR_LOG(GetObjectFrameLog(), PR_LOG_DEBUG,
         ("Created new nsObjectFrame %p\n", this));
}

nsObjectFrame::~nsObjectFrame()
{
  PR_LOG(GetObjectFrameLog(), PR_LOG_DEBUG,
         ("nsObjectFrame %p deleted\n", this));
}

NS_QUERYFRAME_HEAD(nsObjectFrame)
  NS_QUERYFRAME_ENTRY(nsObjectFrame)
  NS_QUERYFRAME_ENTRY(nsIObjectFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsObjectFrameSuper)

#ifdef ACCESSIBILITY
a11y::AccType
nsObjectFrame::AccessibleType()
{
  return a11y::ePluginType;
}

#ifdef XP_WIN
NS_IMETHODIMP nsObjectFrame::GetPluginPort(HWND *aPort)
{
  *aPort = (HWND) mInstanceOwner->GetPluginPortFromWidget();
  return NS_OK;
}
#endif
#endif

NS_IMETHODIMP 
nsObjectFrame::Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow)
{
  PR_LOG(GetObjectFrameLog(), PR_LOG_DEBUG,
         ("Initializing nsObjectFrame %p for content %p\n", this, aContent));

  nsresult rv = nsObjectFrameSuper::Init(aContent, aParent, aPrevInFlow);

  return rv;
}

void
nsObjectFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (mReflowCallbackPosted) {
    PresContext()->PresShell()->CancelReflowCallback(this);
  }

  // Tell content owner of the instance to disconnect its frame.
  nsCOMPtr<nsIObjectLoadingContent> objContent(do_QueryInterface(mContent));
  NS_ASSERTION(objContent, "Why not an object loading content?");
  objContent->DisconnectFrame();

  if (mBackgroundSink) {
    mBackgroundSink->Destroy();
  }

  if (mInstanceOwner) {
    mInstanceOwner->SetFrame(nullptr);
  }
  SetInstanceOwner(nullptr);

  nsObjectFrameSuper::DestroyFrom(aDestructRoot);
}

/* virtual */ void
nsObjectFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
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

  nsObjectFrameSuper::DidSetStyleContext(aOldStyleContext);
}

nsIAtom*
nsObjectFrame::GetType() const
{
  return nsGkAtoms::objectFrame; 
}

#ifdef DEBUG
NS_IMETHODIMP
nsObjectFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ObjectFrame"), aResult);
}
#endif

nsresult
nsObjectFrame::PrepForDrawing(nsIWidget *aWidget)
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

    nsRefPtr<nsDeviceContext> dx;
    viewMan->GetDeviceContext(*getter_AddRefs(dx));
    mInnerView->AttachWidgetEventHandler(mWidget);

#ifdef XP_MACOSX
    // On Mac, we need to invalidate ourselves since even windowed
    // plugins are painted through Thebes and we need to ensure
    // the Thebes layer containing the plugin is updated.
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

#ifndef XP_MACOSX
    RegisterPluginForGeometryUpdates();
#endif
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
nsObjectFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;

  if (!IsHidden(false)) {
    nsIAtom *atom = mContent->Tag();
    if (atom == nsGkAtoms::applet || atom == nsGkAtoms::embed) {
      result = nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_WIDTH);
    }
  }

  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsObjectFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  return nsObjectFrame::GetMinWidth(aRenderingContext);
}

void
nsObjectFrame::GetDesiredSize(nsPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics)
{
  // By default, we have no area
  aMetrics.width = 0;
  aMetrics.height = 0;

  if (IsHidden(false)) {
    return;
  }
  
  aMetrics.width = aReflowState.ComputedWidth();
  aMetrics.height = aReflowState.ComputedHeight();

  // for EMBED and APPLET, default to 240x200 for compatibility
  nsIAtom *atom = mContent->Tag();
  if (atom == nsGkAtoms::applet || atom == nsGkAtoms::embed) {
    if (aMetrics.width == NS_UNCONSTRAINEDSIZE) {
      aMetrics.width = clamped(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_WIDTH),
                               aReflowState.mComputedMinWidth,
                               aReflowState.mComputedMaxWidth);
    }
    if (aMetrics.height == NS_UNCONSTRAINEDSIZE) {
      aMetrics.height = clamped(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_HEIGHT),
                                aReflowState.mComputedMinHeight,
                                aReflowState.mComputedMaxHeight);
    }

#if defined (MOZ_WIDGET_GTK2)
    // We need to make sure that the size of the object frame does not
    // exceed the maximum size of X coordinates.  See bug #225357 for
    // more information.  In theory Gtk2 can handle large coordinates,
    // but underlying plugins can't.
    aMetrics.height = std::min(aPresContext->DevPixelsToAppUnits(INT16_MAX), aMetrics.height);
    aMetrics.width = std::min(aPresContext->DevPixelsToAppUnits(INT16_MAX), aMetrics.width);
#endif
  }

  // At this point, the width has an unconstrained value only if we have
  // nothing to go on (no width set, no information from the plugin, nothing).
  // Make up a number.
  if (aMetrics.width == NS_UNCONSTRAINEDSIZE) {
    aMetrics.width =
      (aReflowState.mComputedMinWidth != NS_UNCONSTRAINEDSIZE) ?
        aReflowState.mComputedMinWidth : 0;
  }

  // At this point, the height has an unconstrained value only in two cases:
  // a) We are in standards mode with percent heights and parent is auto-height
  // b) We have no height information at all.
  // In either case, we have to make up a number.
  if (aMetrics.height == NS_UNCONSTRAINEDSIZE) {
    aMetrics.height =
      (aReflowState.mComputedMinHeight != NS_UNCONSTRAINEDSIZE) ?
        aReflowState.mComputedMinHeight : 0;
  }

  // XXXbz don't add in the border and padding, because we screw up our
  // plugin's size and positioning if we do...  Eventually we _do_ want to
  // paint borders, though!  At that point, we will need to adjust the desired
  // size either here or in Reflow....  Further, we will need to fix Paint() to
  // call the superclass in all cases.
}

NS_IMETHODIMP
nsObjectFrame::Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsObjectFrame");
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
    return NS_OK;
  }

  // if we are printing or print previewing, bail for now
  if (aPresContext->Medium() == nsGkAtoms::print) {
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  nsRect r(0, 0, aMetrics.width, aMetrics.height);
  r.Deflate(aReflowState.mComputedBorderPadding);

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
  return NS_OK;
}

///////////// nsIReflowCallback ///////////////

bool
nsObjectFrame::ReflowFinished()
{
  mReflowCallbackPosted = false;
  CallSetWindow();
  return true;
}

void
nsObjectFrame::ReflowCallbackCanceled()
{
  mReflowCallbackPosted = false;
}

void
nsObjectFrame::FixupWindow(const nsSize& aSize)
{
  nsPresContext* presContext = PresContext();

  if (!mInstanceOwner)
    return;

  NPWindow *window;
  mInstanceOwner->GetWindow(window);

  NS_ENSURE_TRUE_VOID(window);

#ifdef XP_MACOSX
  nsWeakFrame weakFrame(this);
  mInstanceOwner->FixUpPluginWindow(nsPluginInstanceOwner::ePluginPaintDisable);
  if (!weakFrame.IsAlive()) {
    return;
  }
#endif

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

  // on the Mac we need to set the clipRect to { 0, 0, 0, 0 } for now. This will keep
  // us from drawing on screen until the widget is properly positioned, which will not
  // happen until we have finished the reflow process.
#ifdef XP_MACOSX
  window->clipRect.top = 0;
  window->clipRect.left = 0;
  window->clipRect.bottom = 0;
  window->clipRect.right = 0;
#else
  mInstanceOwner->UpdateWindowPositionAndClipRect(false);
#endif

  NotifyPluginReflowObservers();
}

nsresult
nsObjectFrame::CallSetWindow(bool aCheckIsHidden)
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
#ifdef XP_MACOSX
  nsWeakFrame weakFrame(this);
  mInstanceOwner->FixUpPluginWindow(nsPluginInstanceOwner::ePluginPaintDisable);
  if (!weakFrame.IsAlive()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
#endif

  if (aCheckIsHidden && IsHidden())
    return NS_ERROR_FAILURE;

  // refresh the plugin port as well
  window->window = mInstanceOwner->GetPluginPortFromWidget();

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
nsObjectFrame::RegisterPluginForGeometryUpdates()
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
nsObjectFrame::UnregisterPluginForGeometryUpdates()
{
  if (!mRootPresContextRegisteredWith) {
    // Not registered...
    return;
  }
  mRootPresContextRegisteredWith->UnregisterPluginForGeometryUpdates(mContent);
  mRootPresContextRegisteredWith = nullptr;
}

void
nsObjectFrame::SetInstanceOwner(nsPluginInstanceOwner* aOwner)
{
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
nsObjectFrame::IsFocusable(int32_t *aTabIndex, bool aWithMouse)
{
  if (aTabIndex)
    *aTabIndex = -1;
  return nsObjectFrameSuper::IsFocusable(aTabIndex, aWithMouse);
}

bool
nsObjectFrame::IsHidden(bool aCheckVisibilityStyle) const
{
  if (aCheckVisibilityStyle) {
    if (!GetStyleVisibility()->IsVisibleOrCollapsed())
      return true;    
  }

  // only <embed> tags support the HIDDEN attribute
  if (mContent->Tag() == nsGkAtoms::embed) {
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

nsIntPoint nsObjectFrame::GetWindowOriginInPixels(bool aWindowless)
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

  return nsIntPoint(PresContext()->AppUnitsToDevPixels(origin.x),
                    PresContext()->AppUnitsToDevPixels(origin.y));
}

NS_IMETHODIMP
nsObjectFrame::DidReflow(nsPresContext*            aPresContext,
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

  nsresult rv = nsObjectFrameSuper::DidReflow(aPresContext, aReflowState, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (aStatus != nsDidReflowStatus::FINISHED) 
    return rv;

  if (HasView()) {
    nsView* view = GetView();
    nsViewManager* vm = view->GetViewManager();
    if (vm)
      vm->SetViewVisibility(view, IsHidden() ? nsViewVisibility_kHide : nsViewVisibility_kShow);
  }

  return rv;
}

/* static */ void
nsObjectFrame::PaintPrintPlugin(nsIFrame* aFrame, nsRenderingContext* aCtx,
                                const nsRect& aDirtyRect, nsPoint aPt)
{
  nsPoint pt = aPt + aFrame->GetContentRectRelativeToSelf().TopLeft();
  nsRenderingContext::AutoPushTranslation translate(aCtx, pt);
  // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
  static_cast<nsObjectFrame*>(aFrame)->PrintPlugin(*aCtx, aDirtyRect);
}

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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap);
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion);

  NS_DISPLAY_DECL_NAME("PluginReadback", TYPE_PLUGIN_READBACK)

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters)
  {
    return static_cast<nsObjectFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters)
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

bool
nsDisplayPluginReadback::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aAllowVisibleRegionExpansion)
{
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion))
    return false;

  nsRect expand;
  bool snap;
  expand.IntersectRect(aAllowVisibleRegionExpansion, GetBounds(aBuilder, &snap));
  // *Add* our bounds to the visible region so that stuff underneath us is
  // likely to be made visible, so we can use it for a background! This is
  // a bit crazy since we normally only subtract from the visible region.
  aVisibleRegion->Or(*aVisibleRegion, expand);
  return true;
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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap);
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion);

  NS_DISPLAY_DECL_NAME("PluginVideo", TYPE_PLUGIN_VIDEO)

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters)
  {
    return static_cast<nsObjectFrame*>(mFrame)->BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters)
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

bool
nsDisplayPluginVideo::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aAllowVisibleRegionExpansion)
{
  return nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                          aAllowVisibleRegionExpansion);
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
  nsObjectFrame* f = static_cast<nsObjectFrame*>(mFrame);
  bool snap;
  f->PaintPlugin(aBuilder, *aCtx, mVisibleRect, GetBounds(aBuilder, &snap));
}

bool
nsDisplayPlugin::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion)
{
  if (aBuilder->IsForPluginGeometry()) {
    nsObjectFrame* f = static_cast<nsObjectFrame*>(mFrame);
    if (!aBuilder->IsInTransform() || f->IsPaintedByGecko()) {
      // Since transforms induce reference frames, we don't need to worry
      // about this method fluffing out due to non-rectilinear transforms.
      nsRect rAncestor = nsLayoutUtils::TransformFrameRectToAncestor(f,
          f->GetContentRectRelativeToSelf(), ReferenceFrame());
      nscoord appUnitsPerDevPixel =
        ReferenceFrame()->PresContext()->AppUnitsPerDevPixel();
      f->mNextConfigurationBounds = rAncestor.ToNearestPixels(appUnitsPerDevPixel);

      bool snap;
      nsRegion visibleRegion;
      visibleRegion.And(*aVisibleRegion, GetBounds(aBuilder, &snap));
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

  return nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                          aAllowVisibleRegionExpansion);
}

nsRegion
nsDisplayPlugin::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                 bool* aSnap)
{
  *aSnap = false;
  nsRegion result;
  nsObjectFrame* f = static_cast<nsObjectFrame*>(mFrame);
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
      // in ThebesLayers there.
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
nsObjectFrame::PluginEventNotifier::Run() {
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  obsSvc->NotifyObservers(nullptr, "plugin-changed-event", mEventType.get());
  return NS_OK;
}

void
nsObjectFrame::NotifyPluginReflowObservers()
{
  nsContentUtils::AddScriptRunner(new PluginEventNotifier(NS_LITERAL_STRING("reflow")));
}

void
nsObjectFrame::DidSetWidgetGeometry()
{
#if defined(XP_MACOSX)
  if (mInstanceOwner) {
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
nsObjectFrame::IsOpaque() const
{
#if defined(XP_MACOSX)
  // ???
  return false;
#elif defined(MOZ_WIDGET_ANDROID)
  // We don't know, so just assume transparent
  return false;
#else
  return !IsTransparentMode();
#endif
}

bool
nsObjectFrame::IsTransparentMode() const
{
#if defined(XP_MACOSX)
  // ???
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

NS_IMETHODIMP
nsObjectFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  // XXX why are we painting collapsed object frames?
  if (!IsVisibleOrCollapsedForPainting(aBuilder))
    return NS_OK;

  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPresContext::nsPresContextType type = PresContext()->Type();

  // If we are painting in Print Preview do nothing....
  if (type == nsPresContext::eContext_PrintPreview)
    return NS_OK;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsObjectFrame");

#ifndef XP_MACOSX
  if (mWidget && aBuilder->IsInTransform()) {
    // Windowed plugins should not be rendered inside a transform.
    return NS_OK;
  }
#endif

  nsDisplayList replacedContent;

  if (aBuilder->IsForPainting() && mInstanceOwner && mInstanceOwner->UseAsyncRendering()) {
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

  // determine if we are printing
  if (type == nsPresContext::eContext_Print) {
    rv = replacedContent.AppendNewToTop(new (aBuilder)
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
      rv = replacedContent.AppendNewToTop(new (aBuilder)
          nsDisplayPluginReadback(aBuilder, this));
      NS_ENSURE_SUCCESS(rv, rv);
    }
#endif

#if MOZ_WIDGET_ANDROID
    if (aBuilder->IsPaintingToWindow() &&
        state == LAYER_ACTIVE) {

      nsTArray<nsNPAPIPluginInstance::VideoInfo*> videos;
      mInstanceOwner->GetVideos(videos);

      for (uint32_t i = 0; i < videos.Length(); i++) {
        rv = replacedContent.AppendNewToTop(new (aBuilder)
          nsDisplayPluginVideo(aBuilder, this, videos[i]));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
#endif

    rv = replacedContent.AppendNewToTop(new (aBuilder)
        nsDisplayPlugin(aBuilder, this));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  WrapReplacedContentForBorderRadius(aBuilder, &replacedContent, aLists);

  return NS_OK;
}

#ifdef XP_OS2
static void *
GetPSFromRC(nsRenderingContext& aRenderingContext)
{
  nsRefPtr<gfxASurface>
    surf = aRenderingContext.ThebesContext()->CurrentSurface();
  if (!surf || surf->CairoStatus())
    return nullptr;
  return (void *)(static_cast<gfxOS2Surface*>
                  (static_cast<gfxASurface*>(surf.get()))->GetPS());
}
#endif

void
nsObjectFrame::PrintPlugin(nsRenderingContext& aRenderingContext,
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
#if defined(XP_MACOSX) && !defined(__LP64__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  // Don't use this code if any of the QuickDraw APIs it currently requires
  // are missing (as they probably will be on OS X 10.8 and up).
  if (!&::SetRect || !&::NewGWorldFromPtr || !&::DisposeGWorld) {
    NS_WARNING("Cannot print plugin -- required QuickDraw APIs are missing!");
    return;
  }

  nsSize contentSize = GetContentRectRelativeToSelf().Size();
  window.x = 0;
  window.y = 0;
  window.width = presContext->AppUnitsToDevPixels(contentSize.width);
  window.height = presContext->AppUnitsToDevPixels(contentSize.height);

  gfxContext *ctx = aRenderingContext.ThebesContext();
  if (!ctx)
    return;
  gfxContextAutoSaveRestore save(ctx);

  ctx->NewPath();

  gfxRect rect(window.x, window.y, window.width, window.height);

  ctx->Rectangle(rect);
  ctx->Clip();

  gfxQuartzNativeDrawing nativeDraw(ctx, rect);
  CGContextRef cgContext = nativeDraw.BeginNativeDrawing();
  if (!cgContext) {
    nativeDraw.EndNativeDrawing();
    return;
  }

  window.clipRect.right = window.width;
  window.clipRect.bottom = window.height;
  window.type = NPWindowTypeDrawable;

  ::Rect gwBounds;
  ::SetRect(&gwBounds, 0, 0, window.width, window.height);

  nsTArray<char> buffer(window.width * window.height * 4);
  CGColorSpaceRef cspace = ::CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
  if (!cspace) {
    nativeDraw.EndNativeDrawing();
    return;
  }
  CGContextRef cgBuffer =
    ::CGBitmapContextCreate(buffer.Elements(), 
                            window.width, window.height, 8, window.width * 4,
                            cspace, kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedFirst);
  ::CGColorSpaceRelease(cspace);
  if (!cgBuffer) {
    nativeDraw.EndNativeDrawing();
    return;
  }
  GWorldPtr gWorld;
  if (::NewGWorldFromPtr(&gWorld, k32ARGBPixelFormat, &gwBounds, NULL, NULL, 0,
                         buffer.Elements(), window.width * 4) != noErr) {
    ::CGContextRelease(cgBuffer);
    nativeDraw.EndNativeDrawing();
    return;
  }

  window.clipRect.right = window.width;
  window.clipRect.bottom = window.height;
  window.type = NPWindowTypeDrawable;
  // Setting nsPluginPrint/NPPrint.print.embedPrint.window.window to
  // &GWorldPtr and nsPluginPrint/NPPrint.print.embedPrint.platformPrint to
  // GWorldPtr isn't any kind of standard (it's not documented anywhere).
  // But that's what WebKit does.  And it's what the Flash plugin (apparently
  // the only NPAPI plugin on OS X to support printing) seems to expect.  So
  // we do the same.  The Flash plugin uses the CoreGraphics drawing mode.
  // But a GWorldPtr should be usable in either CoreGraphics or QuickDraw
  // drawing mode.  See bug 191046.
  window.window = &gWorld;
  npprint.print.embedPrint.platformPrint = gWorld;
  npprint.print.embedPrint.window = window;
  pi->Print(&npprint);

  ::CGContextTranslateCTM(cgContext, 0.0f, float(window.height));
  ::CGContextScaleCTM(cgContext, 1.0f, -1.0f);
  CGImageRef image = ::CGBitmapContextCreateImage(cgBuffer);
  if (!image) {
    ::CGContextRestoreGState(cgContext);
    ::CGContextRelease(cgBuffer);
    ::DisposeGWorld(gWorld);
    nativeDraw.EndNativeDrawing();
    return;
  }
  ::CGContextDrawImage(cgContext,
                       ::CGRectMake(0, 0, window.width, window.height),
                       image);
  ::CGImageRelease(image);
  ::CGContextRelease(cgBuffer);

  ::DisposeGWorld(gWorld);

  nativeDraw.EndNativeDrawing();
#pragma clang diagnostic warning "-Wdeprecated-declarations"
#elif defined(XP_UNIX)

  /* XXX this just flat-out doesn't work in a thebes world --
   * RenderEPS is a no-op.  So don't bother to do any work here.
   */
  (void)window;
  (void)npprint;

#elif defined(XP_OS2)
  void *hps = GetPSFromRC(aRenderingContext);
  if (!hps)
    return;

  npprint.print.embedPrint.platformPrint = hps;
  npprint.print.embedPrint.window = window;
  // send off print info to plugin
  pi->Print(&npprint);
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
nsObjectFrame::GetPaintedRect(nsDisplayPlugin* aItem)
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

void
nsObjectFrame::UpdateImageLayer(const gfxRect& aRect)
{
  if (!mInstanceOwner) {
    return;
  }

#ifdef XP_MACOSX
  if (!mInstanceOwner->UseAsyncRendering()) {
    mInstanceOwner->DoCocoaEventDrawRect(aRect, nullptr);
  }
#endif
}

LayerState
nsObjectFrame::GetLayerState(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager)
{
  if (!mInstanceOwner)
    return LAYER_NONE;

#ifdef XP_MACOSX
  if (!mInstanceOwner->UseAsyncRendering() &&
      mInstanceOwner->IsRemoteDrawingCoreAnimation() &&
      mInstanceOwner->GetEventModel() == NPEventModelCocoa) {
    return LAYER_ACTIVE;
  }
#endif

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
nsObjectFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                          LayerManager* aManager,
                          nsDisplayItem* aItem,
                          const ContainerParameters& aContainerParameters)
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
  gfxIntSize size(window->width * intScaleFactor, window->height * intScaleFactor);

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
    UpdateImageLayer(r);

    imglayer->SetScaleToSize(size, ImageLayer::SCALE_STRETCH);
    imglayer->SetContainer(container);
    gfxPattern::GraphicsFilter filter =
      nsLayoutUtils::GetGraphicsFilterForFrame(this);
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    if (!aManager->IsCompositingCheap()) {
      // Pixman just horrible with bilinear filter scaling
      filter = gfxPattern::FILTER_NEAREST;
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
    NS_ABORT_IF_FALSE(!IsOpaque(), "Opaque plugins don't use backgrounds");

    if (!layer) {
      layer = aManager->CreateReadbackLayer();
      if (!layer)
        return nullptr;
    }
    NS_ASSERTION(layer->GetType() == Layer::TYPE_READBACK, "Bad layer type");

    ReadbackLayer* readback = static_cast<ReadbackLayer*>(layer.get());
    if (readback->GetSize() != nsIntSize(size.width, size.height)) {
      // This will destroy any old background sink and notify us that the
      // background is now unknown
      readback->SetSink(nullptr);
      readback->SetSize(nsIntSize(size.width, size.height));

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
  gfxMatrix transform;
  transform.Translate(r.TopLeft() + aContainerParameters.mOffset);

  layer->SetBaseTransform(gfx3DMatrix::From2D(transform));
  layer->SetVisibleRegion(nsIntRect(0, 0, size.width, size.height));
  return layer.forget();
}

void
nsObjectFrame::PaintPlugin(nsDisplayListBuilder* aBuilder,
                           nsRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect, const nsRect& aPluginRect)
{
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
      ctx->Translate(offset);

      gfxQuartzNativeDrawing nativeDrawing(ctx, nativeClipRect - offset);

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
      // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
      nsRenderingContext::AutoPushTranslation
        translate(&aRenderingContext, aPluginRect.TopLeft());

      // this rect is used only in the CoreGraphics drawing model
      gfxRect tmpRect(0, 0, 0, 0);
      mInstanceOwner->Paint(tmpRect, NULL);
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
      ctx->IdentityMatrix();
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
        pluginEvent.event = DoublePassRenderingEvent();
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
#elif defined(XP_OS2)
  nsRefPtr<nsNPAPIPluginInstance> inst;
  GetPluginInstance(getter_AddRefs(inst));
  if (inst) {
    // Look if it's windowless
    NPWindow *window;
    mInstanceOwner->GetWindow(window);

    if (window->type == NPWindowTypeDrawable) {
      // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
      nsRenderingContext::AutoPushTranslation
        translate(&aRenderingContext, aPluginRect.TopLeft());

      // check if we need to call SetWindow with updated parameters
      bool doupdatewindow = false;
      // the offset of the DC
      nsIntPoint origin;

      /*
       * Layout now has an optimized way of painting. Now we always get
       * a new drawing surface, sized to be just what's needed. Windowless
       * plugins need a transform applied to their origin so they paint
       * in the right place. Since |SetWindow| is no longer being used
       * to tell the plugin where it is, we dispatch a NPWindow through
       * |HandleEvent| to tell the plugin when its window moved
       */
      gfxContext *ctx = aRenderingContext.ThebesContext();

      gfxMatrix ctxMatrix = ctx->CurrentMatrix();
      if (ctxMatrix.HasNonTranslation()) {
        // soo; in the future, we should be able to render
        // the object content to an offscreen DC, and then
        // composite it in with the right transforms.

        // But, we don't bother doing that, because we don't
        // have the event handling story figured out yet.
        // Instead, let's just bail.

        return;
      }

      origin.x = NSToIntRound(ctxMatrix.GetTranslation().x);
      origin.y = NSToIntRound(ctxMatrix.GetTranslation().y);

      /* Need to force the clip to be set */
      ctx->UpdateSurfaceClip();

      /* Set the device offsets as appropriate, for whatever our current group offsets might be */
      gfxFloat xoff, yoff;
      nsRefPtr<gfxASurface> surf = ctx->CurrentSurface(&xoff, &yoff);

      if (surf->CairoStatus() != 0) {
        NS_WARNING("Plugin is being asked to render to a surface that's in error!");
        return;
      }

      // check if we need to update the PS
      HPS hps = (HPS)GetPSFromRC(aRenderingContext);
      if (reinterpret_cast<HPS>(window->window) != hps) {
        window->window = reinterpret_cast<void*>(hps);
        doupdatewindow = true;
      }
      LONG lPSid = GpiSavePS(hps);
      RECTL rclViewport;
      if (GpiQueryDevice(hps) != NULLHANDLE) { // ensure that we have an associated HDC
        if (GpiQueryPageViewport(hps, &rclViewport)) {
          rclViewport.xLeft += (LONG)xoff;
          rclViewport.xRight += (LONG)xoff;
          rclViewport.yBottom += (LONG)yoff;
          rclViewport.yTop += (LONG)yoff;
          GpiSetPageViewport(hps, &rclViewport);
        }
      }

      if ((window->x != origin.x) || (window->y != origin.y)) {
        window->x = origin.x;
        window->y = origin.y;
        doupdatewindow = true;
      }

      // if our location or visible area has changed, we need to tell the plugin
      if (doupdatewindow) {
        inst->SetWindow(window);        
      }

      mInstanceOwner->Paint(aDirtyRect, hps);
      if (lPSid >= 1) {
        GpiRestorePS(hps, lPSid);
      }
      surf->MarkDirty();
    }
  }
#endif
}

NS_IMETHODIMP
nsObjectFrame::HandleEvent(nsPresContext* aPresContext,
                           nsGUIEvent*     anEvent,
                           nsEventStatus*  anEventStatus)
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

#ifdef XP_MACOSX
  if (anEvent->message == NS_PLUGIN_RESOLUTION_CHANGED) {
    double scaleFactor = 1.0;
    mInstanceOwner->GetContentsScaleFactor(&scaleFactor);
    mInstanceOwner->ContentsScaleFactorChanged(scaleFactor);
    return NS_OK;
  }
#endif

  if (mInstanceOwner->SendNativeEvents() &&
      NS_IS_PLUGIN_EVENT(anEvent)) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    return rv;
  }

#ifdef XP_WIN
  rv = nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
  return rv;
#endif

#ifdef XP_MACOSX
  // we want to process some native mouse events in the cocoa event model
  if ((anEvent->message == NS_MOUSE_ENTER ||
       anEvent->message == NS_WHEEL_WHEEL) &&
      mInstanceOwner->GetEventModel() == NPEventModelCocoa) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    return rv;
  }
#endif

  return nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
}

#ifdef XP_MACOSX
// Needed to make the routing of mouse events while dragging conform to
// standard OS X practice, and to the Cocoa NPAPI spec.  See bug 525078.
NS_IMETHODIMP
nsObjectFrame::HandlePress(nsPresContext* aPresContext,
                           nsGUIEvent*    anEvent,
                           nsEventStatus* anEventStatus)
{
  nsIPresShell::SetCapturingContent(GetContent(), CAPTURE_IGNOREALLOWED);
  return nsObjectFrameSuper::HandlePress(aPresContext, anEvent, anEventStatus);
}
#endif

nsresult
nsObjectFrame::GetPluginInstance(nsNPAPIPluginInstance** aPluginInstance)
{
  *aPluginInstance = nullptr;

  if (!mInstanceOwner) {
    return NS_OK;
  }

  return mInstanceOwner->GetInstance(aPluginInstance);
}

NS_IMETHODIMP
nsObjectFrame::GetCursor(const nsPoint& aPoint, nsIFrame::Cursor& aCursor)
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

  return nsObjectFrameSuper::GetCursor(aPoint, aCursor);
}

void
nsObjectFrame::SetIsDocumentActive(bool aIsActive)
{
#ifndef XP_MACOSX
  if (mInstanceOwner) {
    mInstanceOwner->UpdateDocumentActiveState(aIsActive);
  }
#endif
}

// static
nsIObjectFrame *
nsObjectFrame::GetNextObjectFrame(nsPresContext* aPresContext, nsIFrame* aRoot)
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
nsObjectFrame::BeginSwapDocShells(nsIContent* aContent, void*)
{
  NS_PRECONDITION(aContent, "");

  // This function is called from a document content enumerator so we need
  // to filter out the nsObjectFrames and ignore the rest.
  nsIObjectFrame* obj = do_QueryFrame(aContent->GetPrimaryFrame());
  if (!obj)
    return;

  nsObjectFrame* objectFrame = static_cast<nsObjectFrame*>(obj);
  NS_ASSERTION(!objectFrame->mWidget || objectFrame->mWidget->GetParent(),
               "Plugin windows must not be toplevel");
  objectFrame->UnregisterPluginForGeometryUpdates();
}

/*static*/ void
nsObjectFrame::EndSwapDocShells(nsIContent* aContent, void*)
{
  NS_PRECONDITION(aContent, "");

  // This function is called from a document content enumerator so we need
  // to filter out the nsObjectFrames and ignore the rest.
  nsIObjectFrame* obj = do_QueryFrame(aContent->GetPrimaryFrame());
  if (!obj)
    return;

  nsObjectFrame* objectFrame = static_cast<nsObjectFrame*>(obj);
  nsRootPresContext* rootPC = objectFrame->PresContext()->GetRootPresContext();
  NS_ASSERTION(rootPC, "unable to register the plugin frame");
  nsIWidget* widget = objectFrame->mWidget;
  if (widget) {
    // Reparent the widget.
    nsIWidget* parent =
      rootPC->PresShell()->GetRootFrame()->GetNearestWidget();
    widget->SetParent(parent);
    objectFrame->CallSetWindow();

    // Register for geometry updates and make a request.
    objectFrame->RegisterPluginForGeometryUpdates();
  }
}

nsIFrame*
NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsObjectFrame(aContext);
}

bool
nsObjectFrame::IsPaintedByGecko() const
{
#ifdef XP_MACOSX
  return true;
#else
  return !mWidget;
#endif
}

NS_IMPL_FRAMEARENA_HELPERS(nsObjectFrame)
