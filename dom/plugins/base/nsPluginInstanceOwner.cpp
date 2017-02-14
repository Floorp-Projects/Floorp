/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_X11
#include <cairo-xlib.h>
#include "gfxXlibSurface.h"
/* X headers suck */
enum { XKeyPress = KeyPress };
#include "mozilla/X11Util.h"
using mozilla::DefaultXDisplay;
#endif

#include "nsPluginInstanceOwner.h"

#include "gfxUtils.h"
#include "nsIRunnable.h"
#include "nsContentUtils.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsDisplayList.h"
#include "ImageLayers.h"
#include "GLImages.h"
#include "nsPluginFrame.h"
#include "nsIPluginDocument.h"
#include "nsIStringStream.h"
#include "nsNetUtil.h"
#include "mozilla/Preferences.h"
#include "nsILinkHandler.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebBrowserChrome.h"
#include "nsLayoutUtils.h"
#include "nsIPluginWidget.h"
#include "nsViewManager.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIAppShell.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIObjectLoadingContent.h"
#include "nsObjectLoadingContent.h"
#include "nsAttrName.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIDOMDragEvent.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableFrame.h"
#include "nsIDocShell.h"
#include "ImageContainer.h"
#include "nsIDOMHTMLCollection.h"
#include "GLContext.h"
#include "EGLUtils.h"
#include "nsIContentInlines.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/TabChild.h"
#include "nsFrameSelection.h"
#include "PuppetWidget.h"
#include "nsPIWindowRoot.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/TextComposition.h"
#include "mozilla/AutoRestore.h"

#include "nsContentCID.h"
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef XP_WIN
#include <wtypes.h>
#include <winuser.h>
#include "mozilla/widget/WinMessages.h"
#endif // #ifdef XP_WIN

#ifdef XP_MACOSX
#include "ComplexTextInputPanel.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "ANPBase.h"
#include "AndroidBridge.h"
#include "ClientLayerManager.h"
#include "nsWindow.h"

static nsPluginInstanceOwner* sFullScreenInstance = nullptr;

using namespace mozilla::dom;

#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

static inline nsPoint AsNsPoint(const nsIntPoint &p) {
  return nsPoint(p.x, p.y);
}

// special class for handeling DOM context menu events because for
// some reason it starves other mouse events if implemented on the
// same class
class nsPluginDOMContextMenuListener : public nsIDOMEventListener
{
  virtual ~nsPluginDOMContextMenuListener();

public:
  explicit nsPluginDOMContextMenuListener(nsIContent* aContent);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void Destroy(nsIContent* aContent);

  nsEventStatus ProcessEvent(const WidgetGUIEvent& anEvent)
  {
    return nsEventStatus_eConsumeNoDefault;
  }
};

class AsyncPaintWaitEvent : public Runnable
{
public:
  AsyncPaintWaitEvent(nsIContent* aContent, bool aFinished) :
    mContent(aContent), mFinished(aFinished)
  {
  }

  NS_IMETHOD Run() override
  {
    nsContentUtils::DispatchTrustedEvent(mContent->OwnerDoc(), mContent,
        mFinished ? NS_LITERAL_STRING("MozPaintWaitFinished") : NS_LITERAL_STRING("MozPaintWait"),
        true, true);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIContent> mContent;
  bool                 mFinished;
};

void
nsPluginInstanceOwner::NotifyPaintWaiter(nsDisplayListBuilder* aBuilder)
{
  // This is notification for reftests about async plugin paint start
  if (!mWaitingForPaint && !IsUpToDate() && aBuilder->ShouldSyncDecodeImages()) {
    nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(content, false);
    // Run this event as soon as it's safe to do so, since listeners need to
    // receive it immediately
    nsContentUtils::AddScriptRunner(event);
    mWaitingForPaint = true;
  }
}

#if MOZ_WIDGET_ANDROID
static void
AttachToContainerAsSurfaceTexture(ImageContainer* container,
                                  nsNPAPIPluginInstance* instance,
                                  const LayoutDeviceRect& rect,
                                  RefPtr<Image>* out_image)
{
  MOZ_ASSERT(out_image);
  MOZ_ASSERT(!*out_image);

  mozilla::gl::AndroidSurfaceTexture* surfTex = instance->AsSurfaceTexture();
  if (!surfTex) {
    return;
  }

  RefPtr<Image> img = new SurfaceTextureImage(
    surfTex,
    gfx::IntSize::Truncate(rect.width, rect.height),
    instance->OriginPos());
  *out_image = img;
}
#endif

bool
nsPluginInstanceOwner::NeedsScrollImageLayer()
{
#if defined(XP_WIN)
  // If this is a windowed plugin and we're doing layout in the content
  // process, force the creation of an image layer for the plugin. We'll
  // paint to this when scrolling.
  return XRE_IsContentProcess() &&
         mPluginWindow &&
         mPluginWindow->type == NPWindowTypeWindow;
#else
  return false;
#endif
}

already_AddRefed<ImageContainer>
nsPluginInstanceOwner::GetImageContainer()
{
  if (!mInstance)
    return nullptr;

  RefPtr<ImageContainer> container;

#if MOZ_WIDGET_ANDROID
  LayoutDeviceRect r = GetPluginRect();

  // NotifySize() causes Flash to do a bunch of stuff like ask for surfaces to render
  // into, set y-flip flags, etc, so we do this at the beginning.
  float resolution = mPluginFrame->PresContext()->PresShell()->GetCumulativeResolution();
  ScreenSize screenSize = (r * LayoutDeviceToScreenScale(resolution)).Size();
  mInstance->NotifySize(nsIntSize::Truncate(screenSize.width, screenSize.height));

  container = LayerManager::CreateImageContainer();

  if (r.width && r.height) {
    // Try to get it as an EGLImage first.
    RefPtr<Image> img;
    AttachToContainerAsSurfaceTexture(container, mInstance, r, &img);

    if (img) {
      container->SetCurrentImageInTransaction(img);
    }
  }
#else
  if (NeedsScrollImageLayer()) {
    // windowed plugin under e10s
#if defined(XP_WIN)
    mInstance->GetScrollCaptureContainer(getter_AddRefs(container));
#endif
  } else {
    // async windowless rendering
    mInstance->GetImageContainer(getter_AddRefs(container));
  }
#endif

  return container.forget();
}

void
nsPluginInstanceOwner::DidComposite()
{
  if (mInstance) {
    mInstance->DidComposite();
  }
}

void
nsPluginInstanceOwner::SetBackgroundUnknown()
{
  if (mInstance) {
    mInstance->SetBackgroundUnknown();
  }
}

already_AddRefed<mozilla::gfx::DrawTarget>
nsPluginInstanceOwner::BeginUpdateBackground(const nsIntRect& aRect)
{
  nsIntRect rect = aRect;
  RefPtr<DrawTarget> dt;
  if (mInstance &&
      NS_SUCCEEDED(mInstance->BeginUpdateBackground(&rect, getter_AddRefs(dt)))) {
    return dt.forget();
  }
  return nullptr;
}

void
nsPluginInstanceOwner::EndUpdateBackground(const nsIntRect& aRect)
{
  nsIntRect rect = aRect;
  if (mInstance) {
    mInstance->EndUpdateBackground(&rect);
  }
}

bool
nsPluginInstanceOwner::UseAsyncRendering()
{
#ifdef XP_MACOSX
  if (mUseAsyncRendering) {
    return true;
  }
#endif

  bool isOOP;
  bool result = (mInstance &&
          NS_SUCCEEDED(mInstance->GetIsOOP(&isOOP)) && isOOP
#ifndef XP_MACOSX
          && (!mPluginWindow ||
           mPluginWindow->type == NPWindowTypeDrawable)
#endif
          );

#ifdef XP_MACOSX
  if (result) {
    mUseAsyncRendering = true;
  }
#endif

  return result;
}

nsIntSize
nsPluginInstanceOwner::GetCurrentImageSize()
{
  nsIntSize size(0,0);
  if (mInstance) {
    mInstance->GetImageSize(&size);
  }
  return size;
}

nsPluginInstanceOwner::nsPluginInstanceOwner()
  : mPluginWindow(nullptr)
{
  // create nsPluginNativeWindow object, it is derived from NPWindow
  // struct and allows to manipulate native window procedure
  nsCOMPtr<nsIPluginHost> pluginHostCOM = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  mPluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (mPluginHost)
    mPluginHost->NewPluginNativeWindow(&mPluginWindow);

  mPluginFrame = nullptr;
  mWidgetCreationComplete = false;
#ifdef XP_MACOSX
  mSentInitialTopLevelWindowEvent = false;
  mLastWindowIsActive = false;
  mLastContentFocused = false;
  mLastScaleFactor = 1.0;
  mShouldBlurOnActivate = false;
#endif
  mLastCSSZoomFactor = 1.0;
  mContentFocused = false;
  mWidgetVisible = true;
  mPluginWindowVisible = false;
  mPluginDocumentActiveState = true;
  mLastMouseDownButtonType = -1;

#ifdef XP_MACOSX
#ifndef NP_NO_CARBON
  // We don't support Carbon, but it is still the default model for i386 NPAPI.
  mEventModel = NPEventModelCarbon;
#else
  mEventModel = NPEventModelCocoa;
#endif
  mUseAsyncRendering = false;
#endif

  mWaitingForPaint = false;

#ifdef MOZ_WIDGET_ANDROID
  mFullScreen = false;
  mJavaView = nullptr;
#endif

#ifdef XP_WIN
  mGotCompositionData = false;
  mSentStartComposition = false;
  mPluginDidNotHandleIMEComposition = false;
#endif
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  if (mWaitingForPaint) {
    nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
    if (content) {
      // We don't care when the event is dispatched as long as it's "soon",
      // since whoever needs it will be waiting for it.
      nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(content, true);
      NS_DispatchToMainThread(event);
    }
  }

  mPluginFrame = nullptr;

  PLUG_DeletePluginNativeWindow(mPluginWindow);
  mPluginWindow = nullptr;

#ifdef MOZ_WIDGET_ANDROID
  RemovePluginView();
#endif

  if (mInstance) {
    mInstance->SetOwner(nullptr);
  }
}

NS_IMPL_ISUPPORTS(nsPluginInstanceOwner,
                  nsIPluginInstanceOwner,
                  nsIDOMEventListener,
                  nsIPrivacyTransitionObserver,
                  nsIKeyEventInPluginCallback,
                  nsISupportsWeakReference)

nsresult
nsPluginInstanceOwner::SetInstance(nsNPAPIPluginInstance *aInstance)
{
  NS_ASSERTION(!mInstance || !aInstance, "mInstance should only be set or unset!");

  // If we're going to null out mInstance after use, be sure to call
  // mInstance->SetOwner(nullptr) here, since it now won't be called
  // from our destructor.  This fixes bug 613376.
  if (mInstance && !aInstance) {
    mInstance->SetOwner(nullptr);

#ifdef MOZ_WIDGET_ANDROID
    RemovePluginView();
#endif
  }

  mInstance = aInstance;

  nsCOMPtr<nsIDocument> doc;
  GetDocument(getter_AddRefs(doc));
  if (doc) {
    if (nsCOMPtr<nsPIDOMWindowOuter> domWindow = doc->GetWindow()) {
      nsCOMPtr<nsIDocShell> docShell = domWindow->GetDocShell();
      if (docShell)
        docShell->AddWeakPrivacyTransitionObserver(this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetWindow(NPWindow *&aWindow)
{
  NS_ASSERTION(mPluginWindow, "the plugin window object being returned is null");
  aWindow = mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMode(int32_t *aMode)
{
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = GetDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIPluginDocument> pDoc (do_QueryInterface(doc));

  if (pDoc) {
    *aMode = NP_FULL;
  } else {
    *aMode = NP_EMBED;
  }

  return rv;
}

void nsPluginInstanceOwner::GetAttributes(nsTArray<MozPluginParameter>& attributes)
{
  nsCOMPtr<nsIObjectLoadingContent> content = do_QueryReferent(mContent);
  nsObjectLoadingContent *loadingContent =
    static_cast<nsObjectLoadingContent*>(content.get());

  loadingContent->GetPluginAttributes(attributes);
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDOMElement(nsIDOMElement* *result)
{
  return CallQueryReferent(mContent.get(), result);
}

nsresult nsPluginInstanceOwner::GetInstance(nsNPAPIPluginInstance **aInstance)
{
  NS_ENSURE_ARG_POINTER(aInstance);

  *aInstance = mInstance;
  NS_IF_ADDREF(*aInstance);
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetURL(const char *aURL,
                                            const char *aTarget,
                                            nsIInputStream *aPostStream,
                                            void *aHeadersData,
                                            uint32_t aHeadersDataLen,
                                            bool aDoCheckLoadURIChecks)
{
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (!content) {
    return NS_ERROR_NULL_POINTER;
  }

  if (content->IsEditable()) {
    return NS_OK;
  }

  nsIDocument *doc = content->GetUncomposedDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  nsIPresShell *presShell = doc->GetShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext) {
    return NS_ERROR_FAILURE;
  }

  // the container of the pres context will give us the link handler
  nsCOMPtr<nsISupports> container = presContext->GetContainerWeak();
  NS_ENSURE_TRUE(container,NS_ERROR_FAILURE);
  nsCOMPtr<nsILinkHandler> lh = do_QueryInterface(container);
  NS_ENSURE_TRUE(lh, NS_ERROR_FAILURE);

  nsAutoString unitarget;
  if ((0 == PL_strcmp(aTarget, "newwindow")) ||
      (0 == PL_strcmp(aTarget, "_new"))) {
    unitarget.AssignASCII("_blank");
  }
  else if (0 == PL_strcmp(aTarget, "_current")) {
    unitarget.AssignASCII("_self");
  }
  else {
    unitarget.AssignASCII(aTarget); // XXX could this be nonascii?
  }

  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  // Create an absolute URL
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, baseURI);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIInputStream> headersDataStream;
  if (aPostStream && aHeadersData) {
    if (!aHeadersDataLen)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIStringInputStream> sis = do_CreateInstance("@mozilla.org/io/string-input-stream;1");
    if (!sis)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = sis->SetData((char *)aHeadersData, aHeadersDataLen);
    NS_ENSURE_SUCCESS(rv, rv);
    headersDataStream = do_QueryInterface(sis);
  }

  int32_t blockPopups =
    Preferences::GetInt("privacy.popups.disable_from_plugins");
  nsAutoPopupStatePusher popupStatePusher((PopupControlState)blockPopups);


  // if security checks (in particular CheckLoadURIWithPrincipal) needs
  // to be skipped we are creating a codebasePrincipal to make sure
  // that security check succeeds. Please note that we do not want to
  // fall back to using the systemPrincipal, because that would also
  // bypass ContentPolicy checks which should still be enforced.
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  if (!aDoCheckLoadURIChecks) {
    mozilla::OriginAttributes attrs =
      BasePrincipal::Cast(content->NodePrincipal())->OriginAttributesRef();
    triggeringPrincipal = BasePrincipal::CreateCodebasePrincipal(uri, attrs);
  }

  rv = lh->OnLinkClick(content, uri, unitarget.get(), NullString(),
                       aPostStream, headersDataStream, true, triggeringPrincipal);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDocument(nsIDocument* *aDocument)
{
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (!aDocument || !content) {
    return NS_ERROR_NULL_POINTER;
  }

  // XXX sXBL/XBL2 issue: current doc or owner doc?
  // But keep in mind bug 322414 comment 33
  NS_IF_ADDREF(*aDocument = content->OwnerDoc());
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRect(NPRect *invalidRect)
{
  // If our object frame has gone away, we won't be able to determine
  // up-to-date-ness, so just fire off the event.
  if (mWaitingForPaint && (!mPluginFrame || IsUpToDate())) {
    nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
    // We don't care when the event is dispatched as long as it's "soon",
    // since whoever needs it will be waiting for it.
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(content, true);
    NS_DispatchToMainThread(event);
    mWaitingForPaint = false;
  }

  if (!mPluginFrame || !invalidRect || !mWidgetVisible)
    return NS_ERROR_FAILURE;

#if defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID)
  // Each time an asynchronously-drawing plugin sends a new surface to display,
  // the image in the ImageContainer is updated and InvalidateRect is called.
  // There are different side effects for (sync) Android plugins.
  RefPtr<ImageContainer> container;
  mInstance->GetImageContainer(getter_AddRefs(container));
#endif

#ifndef XP_MACOSX
  // Invalidate for windowed plugins needs to work.
  if (mWidget) {
    mWidget->Invalidate(
      LayoutDeviceIntRect(invalidRect->left, invalidRect->top,
                          invalidRect->right - invalidRect->left,
                          invalidRect->bottom - invalidRect->top));
    // Plugin instances also call invalidate when plugin windows are hidden
    // during scrolling. In this case fall through so we invalidate the
    // underlying layer.
    if (!NeedsScrollImageLayer()) {
      return NS_OK;
    }
  }
#endif
  nsIntRect rect(invalidRect->left,
                 invalidRect->top,
                 invalidRect->right - invalidRect->left,
                 invalidRect->bottom - invalidRect->top);
  // invalidRect is in "display pixels".  In non-HiDPI modes "display pixels"
  // are device pixels.  But in HiDPI modes each display pixel corresponds
  // to more than one device pixel.
  double scaleFactor = 1.0;
  GetContentsScaleFactor(&scaleFactor);
  rect.ScaleRoundOut(scaleFactor);
  mPluginFrame->InvalidateLayer(nsDisplayItem::TYPE_PLUGIN, &rect);
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRegion(NPRegion invalidRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginInstanceOwner::RedrawPlugin()
{
  if (mPluginFrame) {
    mPluginFrame->InvalidateLayer(nsDisplayItem::TYPE_PLUGIN);
  }
  return NS_OK;
}

#if defined(XP_WIN)
nsIWidget*
nsPluginInstanceOwner::GetContainingWidgetIfOffset()
{
  MOZ_ASSERT(mPluginFrame, "Caller should have checked for null mPluginFrame.");

  // This property is provided to allow a "windowless" plugin to determine the window it is drawing
  // in, so it can translate mouse coordinates it receives directly from the operating system
  // to coordinates relative to itself.

  // The original code returns the document's window, which is OK if the window the "windowless" plugin
  // is drawing into has the same origin as the document's window, but this is not the case for "windowless" plugins inside of scrolling DIVs etc

  // To make sure "windowless" plugins always get the right origin for translating mouse coordinates, this code
  // determines the window handle of the mozilla window containing the "windowless" plugin.

  // Given that this HWND may not be that of the document's window, there is a slight risk
  // of confusing a plugin that is using this HWND for illicit purposes, but since the documentation
  // does not suggest this HWND IS that of the document window, rather that of the window
  // the plugin is drawn in, this seems like a safe fix.

  // we only attempt to get the nearest window if this really is a "windowless" plugin so as not
  // to change any behaviour for the much more common windowed plugins,
  // though why this method would even be being called for a windowed plugin escapes me.
  if (!XRE_IsContentProcess() &&
      mPluginWindow && mPluginWindow->type == NPWindowTypeDrawable) {
    // it turns out that flash also uses this window for determining focus, and is currently
    // unable to show a caret correctly if we return the enclosing window. Therefore for
    // now we only return the enclosing window when there is an actual offset which
    // would otherwise cause coordinates to be offset incorrectly. (i.e.
    // if the enclosing window if offset from the document window)
    //
    // fixing both the caret and ability to interact issues for a windowless control in a non document aligned windw
    // does not seem to be possible without a change to the flash plugin

    nsIWidget* win = mPluginFrame->GetNearestWidget();
    if (win) {
      nsView *view = nsView::GetViewFor(win);
      NS_ASSERTION(view, "No view for widget");
      nsPoint offset = view->GetOffsetTo(nullptr);

      if (offset.x || offset.y) {
        // in the case the two windows are offset from eachother, we do go ahead and return the correct enclosing window
        // so that mouse co-ordinates are not messed up.
        return win;
      }
    }
  }

  return nullptr;
}

static already_AddRefed<nsIWidget>
GetRootWidgetForPluginFrame(const nsPluginFrame* aPluginFrame)
{
  MOZ_ASSERT(aPluginFrame);

  nsViewManager* vm =
    aPluginFrame->PresContext()->GetPresShell()->GetViewManager();
  if (!vm) {
    NS_WARNING("Could not find view manager for plugin frame.");
    return nullptr;
  }

  nsCOMPtr<nsIWidget> rootWidget;
  vm->GetRootWidget(getter_AddRefs(rootWidget));
  return rootWidget.forget();
}
#endif

NS_IMETHODIMP nsPluginInstanceOwner::GetNetscapeWindow(void *value)
{
  if (!mPluginFrame) {
    NS_WARNING("plugin owner has no owner in getting doc's window handle");
    return NS_ERROR_FAILURE;
  }

#if defined(XP_WIN)
  void** pvalue = (void**)value;
  nsIWidget* offsetContainingWidget = GetContainingWidgetIfOffset();
  if (offsetContainingWidget) {
    *pvalue = (void*)offsetContainingWidget->GetNativeData(NS_NATIVE_WINDOW);
    if (*pvalue) {
      return NS_OK;
    }
  }

  // simply return the topmost document window
  nsCOMPtr<nsIWidget> widget = GetRootWidgetForPluginFrame(mPluginFrame);
  if (widget) {
    *pvalue = widget->GetNativeData(NS_NATIVE_SHAREABLE_WINDOW);
  } else {
    NS_ASSERTION(widget, "couldn't get doc's widget in getting doc's window handle");
  }

  return NS_OK;
#elif defined(MOZ_WIDGET_GTK) && defined(MOZ_X11)
  // X11 window managers want the toplevel window for WM_TRANSIENT_FOR.
  nsIWidget* win = mPluginFrame->GetNearestWidget();
  if (!win)
    return NS_ERROR_FAILURE;
  *static_cast<Window*>(value) = (long unsigned int)win->GetNativeData(NS_NATIVE_SHAREABLE_WINDOW);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#if defined(XP_WIN)
void
nsPluginInstanceOwner::SetWidgetWindowAsParent(HWND aWindowToAdopt)
{
  if (!mWidget) {
    NS_ERROR("mWidget should exist before this gets called.");
    return;
  }

  mWidget->SetNativeData(NS_NATIVE_CHILD_WINDOW,
                         reinterpret_cast<uintptr_t>(aWindowToAdopt));
}

nsresult
nsPluginInstanceOwner::SetNetscapeWindowAsParent(HWND aWindowToAdopt)
{
  if (!mPluginFrame) {
    NS_WARNING("Plugin owner has no plugin frame.");
    return NS_ERROR_FAILURE;
  }

  // If there is a containing window that is offset then ask that to adopt.
  nsIWidget* offsetWidget = GetContainingWidgetIfOffset();
  if (offsetWidget) {
    offsetWidget->SetNativeData(NS_NATIVE_CHILD_WINDOW,
                                reinterpret_cast<uintptr_t>(aWindowToAdopt));
    return NS_OK;
  }

  // Otherwise ask the topmost document window to adopt.
  nsCOMPtr<nsIWidget> rootWidget = GetRootWidgetForPluginFrame(mPluginFrame);
  if (!rootWidget) {
    NS_ASSERTION(rootWidget, "Couldn't get topmost document's widget.");
    return NS_ERROR_FAILURE;
  }

  rootWidget->SetNativeData(NS_NATIVE_CHILD_OF_SHAREABLE_WINDOW,
                            reinterpret_cast<uintptr_t>(aWindowToAdopt));
  return NS_OK;
}

bool
nsPluginInstanceOwner::GetCompositionString(uint32_t aType,
                                            nsTArray<uint8_t>* aDist,
                                            int32_t* aLength)
{
  // Mark pkugin calls ImmGetCompositionStringW correctly
  mGotCompositionData = true;

  RefPtr<TextComposition> composition = GetTextComposition();
  if (NS_WARN_IF(!composition)) {
    return false;
  }

  switch(aType) {
    case GCS_COMPSTR: {
      if (!composition->IsComposing()) {
        *aLength = 0;
        return true;
      }

      uint32_t len = composition->LastData().Length() * sizeof(char16_t);
      if (len) {
        aDist->SetLength(len);
        memcpy(aDist->Elements(), composition->LastData().get(), len);
      }
      *aLength = len;
      return true;
    }

    case GCS_RESULTSTR: {
      if (composition->IsComposing()) {
        *aLength = 0;
        return true;
      }

      uint32_t len = composition->LastData().Length() * sizeof(char16_t);
      if (len) {
        aDist->SetLength(len);
        memcpy(aDist->Elements(), composition->LastData().get(), len);
      }
      *aLength = len;
      return true;
    }

    case GCS_CURSORPOS: {
      *aLength = 0;
      TextRangeArray* ranges = composition->GetLastRanges();
      if (!ranges) {
        return true;
      }
      *aLength = ranges->GetCaretPosition();
      if (*aLength < 0) {
        return false;
      }
      return true;
    }

    case GCS_COMPATTR: {
      TextRangeArray* ranges = composition->GetLastRanges();
      if (!ranges || ranges->IsEmpty()) {
        *aLength = 0;
        return true;
      }

      aDist->SetLength(composition->LastData().Length());
      memset(aDist->Elements(), ATTR_INPUT, aDist->Length());

      for (TextRange& range : *ranges) {
        uint8_t type = ATTR_INPUT;
        switch(range.mRangeType) {
          case TextRangeType::eRawClause:
            type = ATTR_INPUT;
            break;
          case TextRangeType::eSelectedRawClause:
            type = ATTR_TARGET_NOTCONVERTED;
            break;
          case TextRangeType::eConvertedClause:
            type = ATTR_CONVERTED;
            break;
          case TextRangeType::eSelectedClause:
            type = ATTR_TARGET_CONVERTED;
            break;
          default:
            continue;
        }

        size_t minLen = std::min<size_t>(range.mEndOffset, aDist->Length());
        for (size_t i = range.mStartOffset; i < minLen; i++) {
          (*aDist)[i] = type;
        }
      }
      *aLength = aDist->Length();
      return true;
    }

    case GCS_COMPCLAUSE: {
      RefPtr<TextRangeArray> ranges = composition->GetLastRanges();
      if (!ranges || ranges->IsEmpty()) {
        aDist->SetLength(sizeof(uint32_t));
        memset(aDist->Elements(), 0, sizeof(uint32_t));
        *aLength = aDist->Length();
        return true;
      }
      AutoTArray<uint32_t, 16> clauses;
      clauses.AppendElement(0);
      for (TextRange& range : *ranges) {
        if (!range.IsClause()) {
          continue;
        }
        clauses.AppendElement(range.mEndOffset);
      }

      aDist->SetLength(clauses.Length() * sizeof(uint32_t));
      memcpy(aDist->Elements(), clauses.Elements(), aDist->Length());
      *aLength = aDist->Length();
      return true;
    }

    case GCS_RESULTREADSTR: {
      // When returning error causes unexpected error, so we return 0 instead.
      *aLength = 0;
      return true;
    }

    case GCS_RESULTCLAUSE: {
      // When returning error causes unexpected error, so we return 0 instead.
      *aLength = 0;
      return true;
    }

    default:
      NS_WARNING(
        nsPrintfCString("Unsupported type %x of ImmGetCompositionStringW hook",
                        aType).get());
      break;
  }

  return false;
}

bool
nsPluginInstanceOwner::SetCandidateWindow(
    const widget::CandidateWindowPosition& aPosition)
{
  if (NS_WARN_IF(!mPluginFrame)) {
    return false;
  }

  nsCOMPtr<nsIWidget> widget = GetContainingWidgetIfOffset();
  if (!widget) {
    widget = GetRootWidgetForPluginFrame(mPluginFrame);
    if (NS_WARN_IF(!widget)) {
      return false;
    }
  }

  widget->SetCandidateWindowForPlugin(aPosition);
  return true;
}

bool
nsPluginInstanceOwner::RequestCommitOrCancel(bool aCommitted)
{
  nsCOMPtr<nsIWidget> widget = GetContainingWidgetIfOffset();
  if (!widget) {
    widget = GetRootWidgetForPluginFrame(mPluginFrame);
    if (NS_WARN_IF(!widget)) {
      return false;
    }
  }

  if (aCommitted) {
    widget->NotifyIME(widget::REQUEST_TO_COMMIT_COMPOSITION);
  } else {
    widget->NotifyIME(widget::REQUEST_TO_CANCEL_COMPOSITION);
  }
  return true;
}

#endif // #ifdef XP_WIN

void
nsPluginInstanceOwner::HandledWindowedPluginKeyEvent(
                         const NativeEventData& aKeyEventData,
                         bool aIsConsumed)
{
  if (NS_WARN_IF(!mInstance)) {
    return;
  }
  DebugOnly<nsresult> rv =
    mInstance->HandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "HandledWindowedPluginKeyEvent fail");
}

void
nsPluginInstanceOwner::OnWindowedPluginKeyEvent(
                         const NativeEventData& aKeyEventData)
{
  if (NS_WARN_IF(!mPluginFrame)) {
    // Notifies the plugin process of the key event being not consumed by us.
    HandledWindowedPluginKeyEvent(aKeyEventData, false);
    return;
  }

  nsCOMPtr<nsIWidget> widget = mPluginFrame->PresContext()->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    // Notifies the plugin process of the key event being not consumed by us.
    HandledWindowedPluginKeyEvent(aKeyEventData, false);
    return;
  }

  nsresult rv = widget->OnWindowedPluginKeyEvent(aKeyEventData, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Notifies the plugin process of the key event being not consumed by us.
    HandledWindowedPluginKeyEvent(aKeyEventData, false);
    return;
  }

  // If the key event is posted to another process, we need to wait a call
  // of HandledWindowedPluginKeyEvent().  So, nothing to do here in this case.
  if (rv == NS_SUCCESS_EVENT_HANDLED_ASYNCHRONOUSLY) {
    return;
  }

  // Otherwise, the key event is handled synchronously.  Let's notify the
  // plugin process of the key event's result.
  bool consumed = (rv == NS_SUCCESS_EVENT_CONSUMED);
  HandledWindowedPluginKeyEvent(aKeyEventData, consumed);
}

NS_IMETHODIMP nsPluginInstanceOwner::SetEventModel(int32_t eventModel)
{
#ifdef XP_MACOSX
  mEventModel = static_cast<NPEventModel>(eventModel);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#ifdef XP_MACOSX
NPBool nsPluginInstanceOwner::ConvertPointPuppet(PuppetWidget *widget,
                                                 nsPluginFrame* pluginFrame,
                                                 double sourceX, double sourceY,
                                                 NPCoordinateSpace sourceSpace,
                                                 double *destX, double *destY,
                                                 NPCoordinateSpace destSpace)
{
  NS_ENSURE_TRUE(widget && widget->GetOwningTabChild() && pluginFrame, false);
  // Caller has to want a result.
  NS_ENSURE_TRUE(destX || destY, false);

  if (sourceSpace == destSpace) {
    if (destX) {
      *destX = sourceX;
    }
    if (destY) {
      *destY = sourceY;
    }
    return true;
  }

  nsPresContext* presContext = pluginFrame->PresContext();
  double scaleFactor = double(nsPresContext::AppUnitsPerCSSPixel())/
    presContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();

  PuppetWidget *puppetWidget = static_cast<PuppetWidget*>(widget);
  PuppetWidget *rootWidget = static_cast<PuppetWidget*>(widget->GetTopLevelWidget());
  if (!rootWidget) {
    return false;
  }
  nsPoint chromeSize = AsNsPoint(rootWidget->GetChromeDimensions()) / scaleFactor;
  nsIntSize intScreenDims = rootWidget->GetScreenDimensions();
  nsSize screenDims = nsSize(intScreenDims.width / scaleFactor,
                             intScreenDims.height / scaleFactor);
  int32_t screenH = screenDims.height;
  nsPoint windowPosition = AsNsPoint(rootWidget->GetWindowPosition()) / scaleFactor;

  // Window size is tab size + chrome size.
  LayoutDeviceIntRect tabContentBounds = puppetWidget->GetBounds();
  tabContentBounds.ScaleInverseRoundOut(scaleFactor);
  int32_t windowH = tabContentBounds.height + int(chromeSize.y);

  nsPoint pluginPosition = AsNsPoint(pluginFrame->GetScreenRect().TopLeft());

  // Convert (sourceX, sourceY) to 'real' (not PuppetWidget) screen space.
  // In OSX, the Y-axis increases upward, which is the reverse of ours.
  // We want OSX coordinates for window and screen so those equations are swapped.
  nsPoint sourcePoint(sourceX, sourceY);
  nsPoint screenPoint;
  switch (sourceSpace) {
    case NPCoordinateSpacePlugin:
      screenPoint = sourcePoint + pluginPosition +
        pluginFrame->GetContentRectRelativeToSelf().TopLeft() / nsPresContext::AppUnitsPerCSSPixel();
      break;
    case NPCoordinateSpaceWindow:
      screenPoint = nsPoint(sourcePoint.x, windowH-sourcePoint.y) +
        windowPosition;
      break;
    case NPCoordinateSpaceFlippedWindow:
      screenPoint = sourcePoint + windowPosition;
      break;
    case NPCoordinateSpaceScreen:
      screenPoint = nsPoint(sourcePoint.x, screenH-sourcePoint.y);
      break;
    case NPCoordinateSpaceFlippedScreen:
      screenPoint = sourcePoint;
      break;
    default:
      return false;
  }

  // Convert from screen to dest space.
  nsPoint destPoint;
  switch (destSpace) {
    case NPCoordinateSpacePlugin:
      destPoint = screenPoint - pluginPosition -
        pluginFrame->GetContentRectRelativeToSelf().TopLeft() / nsPresContext::AppUnitsPerCSSPixel();
      break;
    case NPCoordinateSpaceWindow:
      destPoint = screenPoint - windowPosition;
      destPoint.y = windowH - destPoint.y;
      break;
    case NPCoordinateSpaceFlippedWindow:
      destPoint = screenPoint - windowPosition;
      break;
    case NPCoordinateSpaceScreen:
      destPoint = nsPoint(screenPoint.x, screenH-screenPoint.y);
      break;
    case NPCoordinateSpaceFlippedScreen:
      destPoint = screenPoint;
      break;
    default:
      return false;
  }

  if (destX) {
    *destX = destPoint.x;
  }
  if (destY) {
    *destY = destPoint.y;
  }

  return true;
}

NPBool nsPluginInstanceOwner::ConvertPointNoPuppet(nsIWidget *widget,
                                                   nsPluginFrame* pluginFrame,
                                                   double sourceX, double sourceY,
                                                   NPCoordinateSpace sourceSpace,
                                                   double *destX, double *destY,
                                                   NPCoordinateSpace destSpace)
{
  NS_ENSURE_TRUE(widget && pluginFrame, false);
  // Caller has to want a result.
  NS_ENSURE_TRUE(destX || destY, false);

  if (sourceSpace == destSpace) {
    if (destX) {
      *destX = sourceX;
    }
    if (destY) {
      *destY = sourceY;
    }
    return true;
  }

  nsPresContext* presContext = pluginFrame->PresContext();
  double scaleFactor = double(nsPresContext::AppUnitsPerCSSPixel())/
    presContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();

  nsCOMPtr<nsIScreenManager> screenMgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (!screenMgr) {
    return false;
  }
  nsCOMPtr<nsIScreen> screen;
  screenMgr->ScreenForNativeWidget(widget->GetNativeData(NS_NATIVE_WINDOW), getter_AddRefs(screen));
  if (!screen) {
    return false;
  }

  int32_t screenX, screenY, screenWidth, screenHeight;
  screen->GetRect(&screenX, &screenY, &screenWidth, &screenHeight);
  screenHeight /= scaleFactor;

  LayoutDeviceIntRect windowScreenBounds = widget->GetScreenBounds();
  windowScreenBounds.ScaleInverseRoundOut(scaleFactor);
  int32_t windowX = windowScreenBounds.x;
  int32_t windowY = windowScreenBounds.y;
  int32_t windowHeight = windowScreenBounds.height;

  nsIntRect pluginScreenRect = pluginFrame->GetScreenRect();

  double screenXGecko, screenYGecko;
  switch (sourceSpace) {
    case NPCoordinateSpacePlugin:
      screenXGecko = pluginScreenRect.x + sourceX;
      screenYGecko = pluginScreenRect.y + sourceY;
      break;
    case NPCoordinateSpaceWindow:
      screenXGecko = windowX + sourceX;
      screenYGecko = windowY + (windowHeight - sourceY);
      break;
    case NPCoordinateSpaceFlippedWindow:
      screenXGecko = windowX + sourceX;
      screenYGecko = windowY + sourceY;
      break;
    case NPCoordinateSpaceScreen:
      screenXGecko = sourceX;
      screenYGecko = screenHeight - sourceY;
      break;
    case NPCoordinateSpaceFlippedScreen:
      screenXGecko = sourceX;
      screenYGecko = sourceY;
      break;
    default:
      return false;
  }

  double destXCocoa, destYCocoa;
  switch (destSpace) {
    case NPCoordinateSpacePlugin:
      destXCocoa = screenXGecko - pluginScreenRect.x;
      destYCocoa = screenYGecko - pluginScreenRect.y;
      break;
    case NPCoordinateSpaceWindow:
      destXCocoa = screenXGecko - windowX;
      destYCocoa = windowHeight - (screenYGecko - windowY);
      break;
    case NPCoordinateSpaceFlippedWindow:
      destXCocoa = screenXGecko - windowX;
      destYCocoa = screenYGecko - windowY;
      break;
    case NPCoordinateSpaceScreen:
      destXCocoa = screenXGecko;
      destYCocoa = screenHeight - screenYGecko;
      break;
    case NPCoordinateSpaceFlippedScreen:
      destXCocoa = screenXGecko;
      destYCocoa = screenYGecko;
      break;
    default:
      return false;
  }

  if (destX) {
    *destX = destXCocoa;
  }
  if (destY) {
    *destY = destYCocoa;
  }

  return true;
}
#endif // XP_MACOSX

NPBool nsPluginInstanceOwner::ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                           double *destX, double *destY, NPCoordinateSpace destSpace)
{
#ifdef XP_MACOSX
  if (!mPluginFrame) {
    return false;
  }

  MOZ_ASSERT(mPluginFrame->GetNearestWidget());

  if (nsIWidget::UsePuppetWidgets()) {
    return ConvertPointPuppet(static_cast<PuppetWidget*>(mPluginFrame->GetNearestWidget()),
                               mPluginFrame, sourceX, sourceY, sourceSpace,
                               destX, destY, destSpace);
  }

  return ConvertPointNoPuppet(mPluginFrame->GetNearestWidget(),
                              mPluginFrame, sourceX, sourceY, sourceSpace,
                              destX, destY, destSpace);
#else
  return false;
#endif
}

NPError nsPluginInstanceOwner::InitAsyncSurface(NPSize *size, NPImageFormat format,
                                                void *initData, NPAsyncSurface *surface)
{
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
}

NPError nsPluginInstanceOwner::FinalizeAsyncSurface(NPAsyncSurface *)
{
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
}

void nsPluginInstanceOwner::SetCurrentAsyncSurface(NPAsyncSurface *, NPRect*)
{
}

NS_IMETHODIMP nsPluginInstanceOwner::GetTagType(nsPluginTagType *result)
{
  NS_ENSURE_ARG_POINTER(result);

  *result = nsPluginTagType_Unknown;

  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (content->IsHTMLElement(nsGkAtoms::applet))
    *result = nsPluginTagType_Applet;
  else if (content->IsHTMLElement(nsGkAtoms::embed))
    *result = nsPluginTagType_Embed;
  else if (content->IsHTMLElement(nsGkAtoms::object))
    *result = nsPluginTagType_Object;

  return NS_OK;
}

void nsPluginInstanceOwner::GetParameters(nsTArray<MozPluginParameter>& parameters)
{
  nsCOMPtr<nsIObjectLoadingContent> content = do_QueryReferent(mContent);
  nsObjectLoadingContent *loadingContent =
    static_cast<nsObjectLoadingContent*>(content.get());

  loadingContent->GetPluginParameters(parameters);
}

#ifdef XP_MACOSX

static void InitializeNPCocoaEvent(NPCocoaEvent* event)
{
  memset(event, 0, sizeof(NPCocoaEvent));
}

NPDrawingModel nsPluginInstanceOwner::GetDrawingModel()
{
#ifndef NP_NO_QUICKDRAW
  // We don't support the Quickdraw drawing model any more but it's still
  // the default model for i386 per NPAPI.
  NPDrawingModel drawingModel = NPDrawingModelQuickDraw;
#else
  NPDrawingModel drawingModel = NPDrawingModelCoreGraphics;
#endif

  if (!mInstance)
    return drawingModel;

  mInstance->GetDrawingModel((int32_t*)&drawingModel);
  return drawingModel;
}

bool nsPluginInstanceOwner::IsRemoteDrawingCoreAnimation()
{
  if (!mInstance)
    return false;

  bool coreAnimation;
  if (!NS_SUCCEEDED(mInstance->IsRemoteDrawingCoreAnimation(&coreAnimation)))
    return false;

  return coreAnimation;
}

NPEventModel nsPluginInstanceOwner::GetEventModel()
{
  return mEventModel;
}

#define DEFAULT_REFRESH_RATE 20 // 50 FPS

nsCOMPtr<nsITimer>               *nsPluginInstanceOwner::sCATimer = nullptr;
nsTArray<nsPluginInstanceOwner*> *nsPluginInstanceOwner::sCARefreshListeners = nullptr;

void nsPluginInstanceOwner::CARefresh(nsITimer *aTimer, void *aClosure) {
  if (!sCARefreshListeners) {
    return;
  }
  for (size_t i = 0; i < sCARefreshListeners->Length(); i++) {
    nsPluginInstanceOwner* instanceOwner = (*sCARefreshListeners)[i];
    NPWindow *window;
    instanceOwner->GetWindow(window);
    if (!window) {
      continue;
    }
    NPRect r;
    r.left = 0;
    r.top = 0;
    r.right = window->width;
    r.bottom = window->height;
    instanceOwner->InvalidateRect(&r);
  }
}

void nsPluginInstanceOwner::AddToCARefreshTimer() {
  if (!mInstance) {
    return;
  }

  // Flash invokes InvalidateRect for us.
  const char* mime = nullptr;
  if (NS_SUCCEEDED(mInstance->GetMIMEType(&mime)) && mime &&
      nsPluginHost::GetSpecialType(nsDependentCString(mime)) ==
      nsPluginHost::eSpecialType_Flash) {
    return;
  }

  if (!sCARefreshListeners) {
    sCARefreshListeners = new nsTArray<nsPluginInstanceOwner*>();
  }

  if (sCARefreshListeners->Contains(this)) {
    return;
  }

  sCARefreshListeners->AppendElement(this);

  if (!sCATimer) {
    sCATimer = new nsCOMPtr<nsITimer>();
  }

  if (sCARefreshListeners->Length() == 1) {
    *sCATimer = do_CreateInstance("@mozilla.org/timer;1");
    (*sCATimer)->InitWithFuncCallback(CARefresh, nullptr,
                   DEFAULT_REFRESH_RATE, nsITimer::TYPE_REPEATING_SLACK);
  }
}

void nsPluginInstanceOwner::RemoveFromCARefreshTimer() {
  if (!sCARefreshListeners || sCARefreshListeners->Contains(this) == false) {
    return;
  }

  sCARefreshListeners->RemoveElement(this);

  if (sCARefreshListeners->Length() == 0) {
    if (sCATimer) {
      (*sCATimer)->Cancel();
      delete sCATimer;
      sCATimer = nullptr;
    }
    delete sCARefreshListeners;
    sCARefreshListeners = nullptr;
  }
}

void nsPluginInstanceOwner::SetPluginPort()
{
  void* pluginPort = GetPluginPort();
  if (!pluginPort || !mPluginWindow)
    return;
  mPluginWindow->window = pluginPort;
}
#endif
#if defined(XP_MACOSX) || defined(XP_WIN)
nsresult nsPluginInstanceOwner::ContentsScaleFactorChanged(double aContentsScaleFactor)
{
  if (!mInstance) {
    return NS_ERROR_NULL_POINTER;
  }
  return mInstance->ContentsScaleFactorChanged(aContentsScaleFactor);
}
#endif


// static
uint32_t
nsPluginInstanceOwner::GetEventloopNestingLevel()
{
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  uint32_t currentLevel = 0;
  if (appShell) {
    appShell->GetEventloopNestingLevel(&currentLevel);
#ifdef XP_MACOSX
    // Cocoa widget code doesn't process UI events through the normal
    // appshell event loop, so it needs an additional count here.
    currentLevel++;
#endif
  }

  // No idea how this happens... but Linux doesn't consistently
  // process UI events through the appshell event loop. If we get a 0
  // here on any platform we increment the level just in case so that
  // we make sure we always tear the plugin down eventually.
  if (!currentLevel) {
    currentLevel++;
  }

  return currentLevel;
}

#ifdef MOZ_WIDGET_ANDROID

// Modified version of nsFrame::GetOffsetToCrossDoc that stops when it
// hits an element with a displayport (or runs out of frames). This is
// not really the right thing to do, but it's better than what was here before.
static nsPoint
GetOffsetRootContent(nsIFrame* aFrame)
{
  // offset will hold the final offset
  // docOffset holds the currently accumulated offset at the current APD, it
  // will be converted and added to offset when the current APD changes.
  nsPoint offset(0, 0), docOffset(0, 0);
  const nsIFrame* f = aFrame;
  int32_t currAPD = aFrame->PresContext()->AppUnitsPerDevPixel();
  int32_t apd = currAPD;
  while (f) {
    if (f->GetContent() && nsLayoutUtils::HasDisplayPort(f->GetContent()))
      break;

    docOffset += f->GetPosition();
    nsIFrame* parent = f->GetParent();
    if (parent) {
      f = parent;
    } else {
      nsPoint newOffset(0, 0);
      f = nsLayoutUtils::GetCrossDocParentFrame(f, &newOffset);
      int32_t newAPD = f ? f->PresContext()->AppUnitsPerDevPixel() : 0;
      if (!f || newAPD != currAPD) {
        // Convert docOffset to the right APD and add it to offset.
        offset += docOffset.ScaleToOtherAppUnits(currAPD, apd);
        docOffset.x = docOffset.y = 0;
      }
      currAPD = newAPD;
      docOffset += newOffset;
    }
  }

  offset += docOffset.ScaleToOtherAppUnits(currAPD, apd);

  return offset;
}

LayoutDeviceRect nsPluginInstanceOwner::GetPluginRect()
{
  // Get the offset of the content relative to the page
  nsRect bounds = mPluginFrame->GetContentRectRelativeToSelf() + GetOffsetRootContent(mPluginFrame);
  LayoutDeviceIntRect rect = LayoutDeviceIntRect::FromAppUnitsToNearest(bounds, mPluginFrame->PresContext()->AppUnitsPerDevPixel());
  return LayoutDeviceRect(rect);
}

bool nsPluginInstanceOwner::AddPluginView(const LayoutDeviceRect& aRect /* = LayoutDeviceRect(0, 0, 0, 0) */)
{
  if (!mJavaView) {
    mJavaView = mInstance->GetJavaSurface();

    if (!mJavaView)
      return false;

    mJavaView = (void*)jni::GetGeckoThreadEnv()->NewGlobalRef((jobject)mJavaView);
  }

  if (mFullScreen) {
    java::GeckoAppShell::AddFullScreenPluginView(jni::Object::Ref::From(jobject(mJavaView)));
    sFullScreenInstance = this;
  }

  return true;
}

void nsPluginInstanceOwner::RemovePluginView()
{
  if (!mInstance || !mJavaView)
    return;

  if (mFullScreen) {
    java::GeckoAppShell::RemoveFullScreenPluginView(jni::Object::Ref::From(jobject(mJavaView)));
  }
  jni::GetGeckoThreadEnv()->DeleteGlobalRef((jobject)mJavaView);
  mJavaView = nullptr;

  if (mFullScreen)
    sFullScreenInstance = nullptr;
}

void
nsPluginInstanceOwner::GetVideos(nsTArray<nsNPAPIPluginInstance::VideoInfo*>& aVideos)
{
  if (!mInstance)
    return;

  mInstance->GetVideos(aVideos);
}

already_AddRefed<ImageContainer>
nsPluginInstanceOwner::GetImageContainerForVideo(nsNPAPIPluginInstance::VideoInfo* aVideoInfo)
{
  RefPtr<ImageContainer> container = LayerManager::CreateImageContainer();

  if (aVideoInfo->mDimensions.width && aVideoInfo->mDimensions.height) {
    RefPtr<Image> img = new SurfaceTextureImage(
      aVideoInfo->mSurfaceTexture,
      gfx::IntSize::Truncate(aVideoInfo->mDimensions.width, aVideoInfo->mDimensions.height),
      gl::OriginPos::BottomLeft);
    container->SetCurrentImageInTransaction(img);
  }

  return container.forget();
}

void nsPluginInstanceOwner::Invalidate() {
  NPRect rect;
  rect.left = rect.top = 0;
  rect.right = mPluginWindow->width;
  rect.bottom = mPluginWindow->height;
  InvalidateRect(&rect);
}

void nsPluginInstanceOwner::Recomposite() {
  nsIWidget* const widget = mPluginFrame->GetNearestWidget();
  NS_ENSURE_TRUE_VOID(widget);

  LayerManager* const lm = widget->GetLayerManager();
  NS_ENSURE_TRUE_VOID(lm);

  ClientLayerManager* const clm = lm->AsClientLayerManager();
  NS_ENSURE_TRUE_VOID(clm && clm->GetRoot());

  clm->SendInvalidRegion(
      clm->GetRoot()->GetLocalVisibleRegion().ToUnknownRegion().GetBounds());
  clm->Composite();
}

void nsPluginInstanceOwner::RequestFullScreen() {
  if (mFullScreen)
    return;

  // Remove whatever view we currently have (if any, fullscreen or otherwise)
  RemovePluginView();

  mFullScreen = true;
  AddPluginView();

  mInstance->NotifyFullScreen(mFullScreen);
}

void nsPluginInstanceOwner::ExitFullScreen() {
  if (!mFullScreen)
    return;

  RemovePluginView();

  mFullScreen = false;

  int32_t model = mInstance->GetANPDrawingModel();

  if (model == kSurface_ANPDrawingModel) {
    // We need to do this immediately, otherwise Flash
    // sometimes causes a deadlock (bug 762407)
    AddPluginView(GetPluginRect());
  }

  mInstance->NotifyFullScreen(mFullScreen);

  // This will cause Paint() to be called, which is where
  // we normally add/update views and layers
  Invalidate();
}

void nsPluginInstanceOwner::ExitFullScreen(jobject view) {
  JNIEnv* env = jni::GetGeckoThreadEnv();

  if (sFullScreenInstance && sFullScreenInstance->mInstance &&
      env->IsSameObject(view, (jobject)sFullScreenInstance->mInstance->GetJavaSurface())) {
    sFullScreenInstance->ExitFullScreen();
  }
}

#endif

void
nsPluginInstanceOwner::NotifyHostAsyncInitFailed()
{
  nsCOMPtr<nsIObjectLoadingContent> content = do_QueryReferent(mContent);
  content->StopPluginInstance();
}

void
nsPluginInstanceOwner::NotifyHostCreateWidget()
{
  mPluginHost->CreateWidget(this);
#ifdef XP_MACOSX
  FixUpPluginWindow(ePluginPaintEnable);
#else
  if (mPluginFrame) {
    mPluginFrame->InvalidateFrame();
  } else {
    CallSetWindow();
  }
#endif
}

void
nsPluginInstanceOwner::NotifyDestroyPending()
{
  if (!mInstance) {
    return;
  }
  bool isOOP = false;
  if (NS_FAILED(mInstance->GetIsOOP(&isOOP)) || !isOOP) {
    return;
  }
  NPP npp = nullptr;
  if (NS_FAILED(mInstance->GetNPP(&npp)) || !npp) {
    return;
  }
  PluginAsyncSurrogate::NotifyDestroyPending(npp);
}

nsresult nsPluginInstanceOwner::DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent)
{
#ifdef MOZ_WIDGET_ANDROID
  if (mInstance) {
    ANPEvent event;
    event.inSize = sizeof(ANPEvent);
    event.eventType = kLifecycle_ANPEventType;

    nsAutoString eventType;
    aFocusEvent->GetType(eventType);
    if (eventType.EqualsLiteral("focus")) {
      event.data.lifecycle.action = kGainFocus_ANPLifecycleAction;
    }
    else if (eventType.EqualsLiteral("blur")) {
      event.data.lifecycle.action = kLoseFocus_ANPLifecycleAction;
    }
    else {
      NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchFocusToPlugin, wierd eventType");
    }
    mInstance->HandleEvent(&event, nullptr);
  }
#endif

#ifndef XP_MACOSX
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow)) {
    // continue only for cases without child window
    return aFocusEvent->PreventDefault(); // consume event
  }
#endif

  WidgetEvent* theEvent = aFocusEvent->WidgetEventPtr();
  if (theEvent) {
    WidgetGUIEvent focusEvent(theEvent->IsTrusted(), theEvent->mMessage,
                              nullptr);
    nsEventStatus rv = ProcessEvent(focusEvent);
    if (nsEventStatus_eConsumeNoDefault == rv) {
      aFocusEvent->PreventDefault();
      aFocusEvent->StopPropagation();
    }
  }

  return NS_OK;
}

nsresult nsPluginInstanceOwner::ProcessKeyPress(nsIDOMEvent* aKeyEvent)
{
#ifdef XP_MACOSX
  return DispatchKeyToPlugin(aKeyEvent);
#else
  if (SendNativeEvents())
    DispatchKeyToPlugin(aKeyEvent);

  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending keypress to the plugin, since we didn't before.
    aKeyEvent->PreventDefault();
    aKeyEvent->StopPropagation();
  }
  return NS_OK;
#endif
}

nsresult nsPluginInstanceOwner::DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent)
{
#if !defined(XP_MACOSX)
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow))
    return aKeyEvent->PreventDefault(); // consume event
  // continue only for cases without child window
#endif

  if (mInstance) {
    WidgetKeyboardEvent* keyEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
    if (keyEvent && keyEvent->mClass == eKeyboardEventClass) {
      nsEventStatus rv = ProcessEvent(*keyEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aKeyEvent->PreventDefault();
        aKeyEvent->StopPropagation();
      }
    }
  }

  return NS_OK;
}

nsresult
nsPluginInstanceOwner::ProcessMouseDown(nsIDOMEvent* aMouseEvent)
{
#if !defined(XP_MACOSX)
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow))
    return aMouseEvent->PreventDefault(); // consume event
  // continue only for cases without child window
#endif

  // if the plugin is windowless, we need to set focus ourselves
  // otherwise, we might not get key events
  if (mPluginFrame && mPluginWindow &&
      mPluginWindow->type == NPWindowTypeDrawable) {

    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsCOMPtr<nsIDOMElement> elem = do_QueryReferent(mContent);
      fm->SetFocus(elem, 0);
    }
  }

  WidgetMouseEvent* mouseEvent =
    aMouseEvent->WidgetEventPtr()->AsMouseEvent();
  if (mouseEvent && mouseEvent->mClass == eMouseEventClass) {
    mLastMouseDownButtonType = mouseEvent->button;
    nsEventStatus rv = ProcessEvent(*mouseEvent);
    if (nsEventStatus_eConsumeNoDefault == rv) {
      return aMouseEvent->PreventDefault(); // consume event
    }
  }

  return NS_OK;
}

nsresult nsPluginInstanceOwner::DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent,
                                                      bool aAllowPropagate)
{
#if !defined(XP_MACOSX)
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow))
    return aMouseEvent->PreventDefault(); // consume event
  // continue only for cases without child window
#endif
  // don't send mouse events if we are hidden
  if (!mWidgetVisible)
    return NS_OK;

  WidgetMouseEvent* mouseEvent =
    aMouseEvent->WidgetEventPtr()->AsMouseEvent();
  if (mouseEvent && mouseEvent->mClass == eMouseEventClass) {
    nsEventStatus rv = ProcessEvent(*mouseEvent);
    if (nsEventStatus_eConsumeNoDefault == rv) {
      aMouseEvent->PreventDefault();
      if (!aAllowPropagate) {
        aMouseEvent->StopPropagation();
      }
    }
    if (mouseEvent->mMessage == eMouseUp) {
      mLastMouseDownButtonType = -1;
    }
  }
  return NS_OK;
}

#ifdef XP_WIN
void
nsPluginInstanceOwner::CallDefaultProc(const WidgetGUIEvent* aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetContainingWidgetIfOffset();
  if (!widget) {
    widget = GetRootWidgetForPluginFrame(mPluginFrame);
    if (NS_WARN_IF(!widget)) {
      return;
    }
  }

  const NPEvent* npEvent =
    static_cast<const NPEvent*>(aEvent->mPluginEvent);
  if (NS_WARN_IF(!npEvent)) {
    return;
  }

  WidgetPluginEvent pluginEvent(true, ePluginInputEvent, widget);
  pluginEvent.mPluginEvent.Copy(*npEvent);
  widget->DefaultProcOfPluginEvent(pluginEvent);
}

already_AddRefed<TextComposition>
nsPluginInstanceOwner::GetTextComposition()
{
  if (NS_WARN_IF(!mPluginFrame)) {
    return nullptr;
  }

  nsCOMPtr<nsIWidget> widget = GetContainingWidgetIfOffset();
  if (!widget) {
    widget = GetRootWidgetForPluginFrame(mPluginFrame);
    if (NS_WARN_IF(!widget)) {
      return nullptr;
    }
  }

  RefPtr<TextComposition> composition =
    IMEStateManager::GetTextCompositionFor(widget);
  if (NS_WARN_IF(!composition)) {
    return nullptr;
  }

  return composition.forget();
}

void
nsPluginInstanceOwner::HandleNoConsumedCompositionMessage(
  WidgetCompositionEvent* aCompositionEvent,
  const NPEvent* aPluginEvent)
{
  nsCOMPtr<nsIWidget> widget = GetContainingWidgetIfOffset();
  if (!widget) {
    widget = GetRootWidgetForPluginFrame(mPluginFrame);
    if (NS_WARN_IF(!widget)) {
      return;
    }
  }

  NPEvent npevent;
  if (aPluginEvent->lParam & GCS_RESULTSTR) {
    // GCS_RESULTSTR's default proc will generate WM_CHAR. So emulate it.
    for (size_t i = 0; i < aCompositionEvent->mData.Length(); i++) {
      WidgetPluginEvent charEvent(true, ePluginInputEvent, widget);
      npevent.event = WM_CHAR;
      npevent.wParam = aCompositionEvent->mData[i];
      npevent.lParam = 0;
      charEvent.mPluginEvent.Copy(npevent);
      ProcessEvent(charEvent);
    }
    return;
  }
  if (!mSentStartComposition) {
    // We post WM_IME_COMPOSITION to default proc, but
    // WM_IME_STARTCOMPOSITION isn't post yet.  We should post it at first.
    WidgetPluginEvent startEvent(true, ePluginInputEvent, widget);
    npevent.event = WM_IME_STARTCOMPOSITION;
    npevent.wParam = 0;
    npevent.lParam = 0;
    startEvent.mPluginEvent.Copy(npevent);
    CallDefaultProc(&startEvent);
    mSentStartComposition = true;
  }

  CallDefaultProc(aCompositionEvent);
}
#endif

nsresult
nsPluginInstanceOwner::DispatchCompositionToPlugin(nsIDOMEvent* aEvent)
{
#ifdef XP_WIN
  if (!mPluginWindow) {
    // CompositionEvent isn't cancellable.  So it is unnecessary to call
    // PreventDefaults() to consume event
    return NS_OK;
  }
  WidgetCompositionEvent* compositionEvent =
    aEvent->WidgetEventPtr()->AsCompositionEvent();
  if (NS_WARN_IF(!compositionEvent)) {
      return NS_ERROR_INVALID_ARG;
  }

  if (compositionEvent->mMessage == eCompositionChange) {
    RefPtr<TextComposition> composition = GetTextComposition();
    if (NS_WARN_IF(!composition)) {
      return NS_ERROR_FAILURE;
    }
    TextComposition::CompositionChangeEventHandlingMarker
      compositionChangeEventHandlingMarker(composition, compositionEvent);
  }

  const NPEvent* pPluginEvent =
    static_cast<const NPEvent*>(compositionEvent->mPluginEvent);
  if (pPluginEvent && pPluginEvent->event == WM_IME_COMPOSITION &&
      mPluginDidNotHandleIMEComposition) {
    // This is a workaround when running windowed and windowless Flash on
    // same process.
    // Flash with protected mode calls IMM APIs on own render process.  This
    // is a bug of Flash's protected mode.
    // ImmGetCompositionString with GCS_RESULTSTR returns *LAST* committed
    // string.  So when windowed mode Flash handles IME composition,
    // windowless plugin can get windowed mode's commited string by that API.
    // So we never post WM_IME_COMPOSITION when plugin doesn't call
    // ImmGetCompositionString() during WM_IME_COMPOSITION correctly.
    HandleNoConsumedCompositionMessage(compositionEvent, pPluginEvent);
    aEvent->StopImmediatePropagation();
    return NS_OK;
  }

  // Protected mode Flash returns noDefault by NPP_HandleEvent, but
  // composition information into plugin is invalid because plugin's bug.
  // So if plugin doesn't get composition data by WM_IME_COMPOSITION, we
  // recongnize it isn't handled
  AutoRestore<bool> restore(mGotCompositionData);
  mGotCompositionData = false;

  nsEventStatus status = ProcessEvent(*compositionEvent);
  aEvent->StopImmediatePropagation();

  // Composition event isn't handled by plugin, so we have to call default proc.

  if (NS_WARN_IF(!pPluginEvent)) {
    return NS_OK;
  }

  if (pPluginEvent->event == WM_IME_STARTCOMPOSITION) {
    // Flash's protected mode lies that composition event is handled, but it
    // cannot do it well.  So even if handled, we should post this message when
    // no IMM API calls during WM_IME_COMPOSITION.
    if (nsEventStatus_eConsumeNoDefault != status)  {
      CallDefaultProc(compositionEvent);
      mSentStartComposition = true;
    } else {
      mSentStartComposition = false;
    }
    mPluginDidNotHandleIMEComposition = false;
    return NS_OK;
  }

  if (pPluginEvent->event == WM_IME_ENDCOMPOSITION) {
    // Always post WM_END_COMPOSITION to default proc. Because Flash may lie
    // that it doesn't handle composition well, but event is handled.
    // Even if posting this message, default proc do nothing if unnecessary.
    CallDefaultProc(compositionEvent);
    return NS_OK;
  }

  if (pPluginEvent->event == WM_IME_COMPOSITION && !mGotCompositionData) {
    // If plugin doesn't handle WM_IME_COMPOSITION correctly, we don't send
    // composition event until end composition.
    mPluginDidNotHandleIMEComposition = true;

    HandleNoConsumedCompositionMessage(compositionEvent, pPluginEvent);
  }
#endif // #ifdef XP_WIN
  return NS_OK;
}

nsresult
nsPluginInstanceOwner::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(mInstance, "Should have a valid plugin instance or not receive events.");

  nsAutoString eventType;
  aEvent->GetType(eventType);

#ifdef XP_MACOSX
  if (eventType.EqualsLiteral("activate") ||
      eventType.EqualsLiteral("deactivate")) {
    WindowFocusMayHaveChanged();
    return NS_OK;
  }
  if (eventType.EqualsLiteral("MozPerformDelayedBlur")) {
    if (mShouldBlurOnActivate) {
      WidgetGUIEvent blurEvent(true, eBlur, nullptr);
      ProcessEvent(blurEvent);
      mShouldBlurOnActivate = false;
    }
    return NS_OK;
  }
#endif

  if (eventType.EqualsLiteral("focus")) {
    mContentFocused = true;
    return DispatchFocusToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("blur")) {
    mContentFocused = false;
    return DispatchFocusToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("mousedown")) {
    return ProcessMouseDown(aEvent);
  }
  if (eventType.EqualsLiteral("mouseup")) {
    return DispatchMouseToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("mousemove")) {
    return DispatchMouseToPlugin(aEvent, true);
  }
  if (eventType.EqualsLiteral("click") ||
      eventType.EqualsLiteral("dblclick") ||
      eventType.EqualsLiteral("mouseover") ||
      eventType.EqualsLiteral("mouseout")) {
    return DispatchMouseToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("keydown") ||
      eventType.EqualsLiteral("keyup")) {
    return DispatchKeyToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("keypress")) {
    return ProcessKeyPress(aEvent);
  }
  if (eventType.EqualsLiteral("compositionstart") ||
      eventType.EqualsLiteral("compositionend") ||
      eventType.EqualsLiteral("text")) {
    return DispatchCompositionToPlugin(aEvent);
  }

  nsCOMPtr<nsIDOMDragEvent> dragEvent(do_QueryInterface(aEvent));
  if (dragEvent && mInstance) {
    WidgetEvent* ievent = aEvent->WidgetEventPtr();
    if (ievent && ievent->IsTrusted() &&
        ievent->mMessage != eDragEnter && ievent->mMessage != eDragOver) {
      aEvent->PreventDefault();
    }

    // Let the plugin handle drag events.
    aEvent->StopPropagation();
  }
  return NS_OK;
}

#ifdef MOZ_X11
static unsigned int XInputEventState(const WidgetInputEvent& anEvent)
{
  unsigned int state = 0;
  if (anEvent.IsShift()) state |= ShiftMask;
  if (anEvent.IsControl()) state |= ControlMask;
  if (anEvent.IsAlt()) state |= Mod1Mask;
  if (anEvent.IsMeta()) state |= Mod4Mask;
  return state;
}
#endif

#ifdef XP_MACOSX

// Returns whether or not content is the content that is or would be
// focused if the top-level chrome window was active.
static bool
ContentIsFocusedWithinWindow(nsIContent* aContent)
{
  nsPIDOMWindowOuter* outerWindow = aContent->OwnerDoc()->GetWindow();
  if (!outerWindow) {
    return false;
  }

  nsPIDOMWindowOuter* rootWindow = outerWindow->GetPrivateRoot();
  if (!rootWindow) {
    return false;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> focusedFrame;
  nsCOMPtr<nsIContent> focusedContent = fm->GetFocusedDescendant(rootWindow, true, getter_AddRefs(focusedFrame));
  return (focusedContent.get() == aContent);
}

static NPCocoaEventType
CocoaEventTypeForEvent(const WidgetGUIEvent& anEvent, nsIFrame* aObjectFrame)
{
  const NPCocoaEvent* event = static_cast<const NPCocoaEvent*>(anEvent.mPluginEvent);
  if (event) {
    return event->type;
  }

  switch (anEvent.mMessage) {
    case eMouseOver:
      return NPCocoaEventMouseEntered;
    case eMouseOut:
      return NPCocoaEventMouseExited;
    case eMouseMove: {
      // We don't know via information on events from the widget code whether or not
      // we're dragging. The widget code just generates mouse move events from native
      // drag events. If anybody is capturing, this is a drag event.
      if (nsIPresShell::GetCapturingContent()) {
        return NPCocoaEventMouseDragged;
      }

      return NPCocoaEventMouseMoved;
    }
    case eMouseDown:
      return NPCocoaEventMouseDown;
    case eMouseUp:
      return NPCocoaEventMouseUp;
    case eKeyDown:
      return NPCocoaEventKeyDown;
    case eKeyUp:
      return NPCocoaEventKeyUp;
    case eFocus:
    case eBlur:
      return NPCocoaEventFocusChanged;
    case eLegacyMouseLineOrPageScroll:
      return NPCocoaEventScrollWheel;
    default:
      return (NPCocoaEventType)0;
  }
}

static NPCocoaEvent
TranslateToNPCocoaEvent(WidgetGUIEvent* anEvent, nsIFrame* aObjectFrame)
{
  NPCocoaEvent cocoaEvent;
  InitializeNPCocoaEvent(&cocoaEvent);
  cocoaEvent.type = CocoaEventTypeForEvent(*anEvent, aObjectFrame);

  if (anEvent->mMessage == eMouseMove ||
      anEvent->mMessage == eMouseDown ||
      anEvent->mMessage == eMouseUp ||
      anEvent->mMessage == eLegacyMouseLineOrPageScroll ||
      anEvent->mMessage == eMouseOver ||
      anEvent->mMessage == eMouseOut)
  {
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(anEvent, aObjectFrame) -
                 aObjectFrame->GetContentRectRelativeToSelf().TopLeft();
    nsPresContext* presContext = aObjectFrame->PresContext();
    // Plugin event coordinates need to be translated from device pixels
    // into "display pixels" in HiDPI modes.
    double scaleFactor = double(nsPresContext::AppUnitsPerCSSPixel())/
      aObjectFrame->PresContext()->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();
    size_t intScaleFactor = ceil(scaleFactor);
    nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x) / intScaleFactor,
                    presContext->AppUnitsToDevPixels(pt.y) / intScaleFactor);
    cocoaEvent.data.mouse.pluginX = double(ptPx.x);
    cocoaEvent.data.mouse.pluginY = double(ptPx.y);
  }

  switch (anEvent->mMessage) {
    case eMouseDown:
    case eMouseUp: {
      WidgetMouseEvent* mouseEvent = anEvent->AsMouseEvent();
      if (mouseEvent) {
        switch (mouseEvent->button) {
          case WidgetMouseEvent::eLeftButton:
            cocoaEvent.data.mouse.buttonNumber = 0;
            break;
          case WidgetMouseEvent::eRightButton:
            cocoaEvent.data.mouse.buttonNumber = 1;
            break;
          case WidgetMouseEvent::eMiddleButton:
            cocoaEvent.data.mouse.buttonNumber = 2;
            break;
          default:
            NS_WARNING("Mouse button we don't know about?");
        }
        cocoaEvent.data.mouse.clickCount = mouseEvent->mClickCount;
      } else {
        NS_WARNING("eMouseUp/DOWN is not a WidgetMouseEvent?");
      }
      break;
    }
    case eLegacyMouseLineOrPageScroll: {
      WidgetWheelEvent* wheelEvent = anEvent->AsWheelEvent();
      if (wheelEvent) {
        cocoaEvent.data.mouse.deltaX = wheelEvent->mLineOrPageDeltaX;
        cocoaEvent.data.mouse.deltaY = wheelEvent->mLineOrPageDeltaY;
      } else {
        NS_WARNING("eLegacyMouseLineOrPageScroll is not a WidgetWheelEvent? "
                   "(could be, haven't checked)");
      }
      break;
    }
    case eKeyDown:
    case eKeyUp:
    {
      WidgetKeyboardEvent* keyEvent = anEvent->AsKeyboardEvent();

      // That keyEvent->mPluginTextEventString is non-empty is a signal that we should
      // create a text event for the plugin, instead of a key event.
      if (anEvent->mMessage == eKeyDown &&
          !keyEvent->mPluginTextEventString.IsEmpty()) {
        cocoaEvent.type = NPCocoaEventTextInput;
        const char16_t* pluginTextEventString = keyEvent->mPluginTextEventString.get();
        cocoaEvent.data.text.text = (NPNSString*)
          ::CFStringCreateWithCharacters(NULL,
                                         reinterpret_cast<const UniChar*>(pluginTextEventString),
                                         keyEvent->mPluginTextEventString.Length());
      } else {
        cocoaEvent.data.key.keyCode = keyEvent->mNativeKeyCode;
        cocoaEvent.data.key.isARepeat = keyEvent->mIsRepeat;
        cocoaEvent.data.key.modifierFlags = keyEvent->mNativeModifierFlags;
        const char16_t* nativeChars = keyEvent->mNativeCharacters.get();
        cocoaEvent.data.key.characters = (NPNSString*)
          ::CFStringCreateWithCharacters(NULL,
                                         reinterpret_cast<const UniChar*>(nativeChars),
                                         keyEvent->mNativeCharacters.Length());
        const char16_t* nativeCharsIgnoringModifiers = keyEvent->mNativeCharactersIgnoringModifiers.get();
        cocoaEvent.data.key.charactersIgnoringModifiers = (NPNSString*)
          ::CFStringCreateWithCharacters(NULL,
                                         reinterpret_cast<const UniChar*>(nativeCharsIgnoringModifiers),
                                         keyEvent->mNativeCharactersIgnoringModifiers.Length());
      }
      break;
    }
    case eFocus:
    case eBlur:
      cocoaEvent.data.focus.hasFocus = (anEvent->mMessage == eFocus);
      break;
    default:
      break;
  }
  return cocoaEvent;
}

void nsPluginInstanceOwner::PerformDelayedBlurs()
{
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  nsCOMPtr<EventTarget> windowRoot = content->OwnerDoc()->GetWindow()->GetTopWindowRoot();
  nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(),
                                       windowRoot,
                                       NS_LITERAL_STRING("MozPerformDelayedBlur"),
                                       false, false, nullptr);
}

#endif

nsEventStatus nsPluginInstanceOwner::ProcessEvent(const WidgetGUIEvent& anEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (!mInstance || !mPluginFrame) {
    return nsEventStatus_eIgnore;
  }

#ifdef XP_MACOSX
  NPEventModel eventModel = GetEventModel();
  if (eventModel != NPEventModelCocoa) {
    return nsEventStatus_eIgnore;
  }

  // In the Cocoa event model, focus is per-window. Don't tell a plugin it lost
  // focus unless it lost focus within the window. For example, ignore a blur
  // event if it's coming due to the plugin's window deactivating.
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (anEvent.mMessage == eBlur && ContentIsFocusedWithinWindow(content)) {
    mShouldBlurOnActivate = true;
    return nsEventStatus_eIgnore;
  }

  // Also, don't tell the plugin it gained focus again after we've already given
  // it focus. This might happen if it has focus, its window is blurred, then the
  // window is made active again. The plugin never lost in-window focus, so it
  // shouldn't get a focus event again.
  if (anEvent.mMessage == eFocus && mLastContentFocused == true) {
    mShouldBlurOnActivate = false;
    return nsEventStatus_eIgnore;
  }

  // Now, if we're going to send a focus event, update mLastContentFocused and
  // tell any plugins in our window that we have taken focus, so they should
  // perform any delayed blurs.
  if (anEvent.mMessage == eFocus || anEvent.mMessage == eBlur) {
    mLastContentFocused = (anEvent.mMessage == eFocus);
    mShouldBlurOnActivate = false;
    PerformDelayedBlurs();
  }

  NPCocoaEvent cocoaEvent = TranslateToNPCocoaEvent(const_cast<WidgetGUIEvent*>(&anEvent), mPluginFrame);
  if (cocoaEvent.type == (NPCocoaEventType)0) {
    return nsEventStatus_eIgnore;
  }

  if (cocoaEvent.type == NPCocoaEventTextInput) {
    mInstance->HandleEvent(&cocoaEvent, nullptr);
    return nsEventStatus_eConsumeNoDefault;
  }

  int16_t response = kNPEventNotHandled;
  mInstance->HandleEvent(&cocoaEvent,
                         &response,
                         NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
  if ((response == kNPEventStartIME) && (cocoaEvent.type == NPCocoaEventKeyDown)) {
    nsIWidget* widget = mPluginFrame->GetNearestWidget();
    if (widget) {
      const WidgetKeyboardEvent* keyEvent = anEvent.AsKeyboardEvent();
      double screenX, screenY;
      ConvertPoint(0.0, mPluginFrame->GetScreenRect().height,
                   NPCoordinateSpacePlugin, &screenX, &screenY,
                   NPCoordinateSpaceScreen);
      nsAutoString outText;
      if (NS_SUCCEEDED(widget->StartPluginIME(*keyEvent, screenX, screenY, outText)) &&
          !outText.IsEmpty()) {
        CFStringRef cfString =
          ::CFStringCreateWithCharacters(kCFAllocatorDefault,
                                         reinterpret_cast<const UniChar*>(outText.get()),
                                         outText.Length());
        NPCocoaEvent textEvent;
        InitializeNPCocoaEvent(&textEvent);
        textEvent.type = NPCocoaEventTextInput;
        textEvent.data.text.text = (NPNSString*)cfString;
        mInstance->HandleEvent(&textEvent, nullptr);
      }
    }
  }

  bool handled = (response == kNPEventHandled || response == kNPEventStartIME);
  bool leftMouseButtonDown = (anEvent.mMessage == eMouseDown) &&
                             (anEvent.AsMouseEvent()->button == WidgetMouseEvent::eLeftButton);
  if (handled && !(leftMouseButtonDown && !mContentFocused)) {
    rv = nsEventStatus_eConsumeNoDefault;
  }
#endif

#ifdef XP_WIN
  // this code supports windowless plugins
  const NPEvent *pPluginEvent = static_cast<const NPEvent*>(anEvent.mPluginEvent);
  // we can get synthetic events from the EventStateManager... these
  // have no pluginEvent
  NPEvent pluginEvent;
  if (anEvent.mClass == eMouseEventClass ||
      anEvent.mClass == eWheelEventClass) {
    if (!pPluginEvent) {
      // XXX Should extend this list to synthesize events for more event
      // types
      pluginEvent.event = 0;
      bool initWParamWithCurrentState = true;
      switch (anEvent.mMessage) {
      case eMouseMove: {
        pluginEvent.event = WM_MOUSEMOVE;
        break;
      }
      case eMouseDown: {
        static const int downMsgs[] =
          { WM_LBUTTONDOWN, WM_MBUTTONDOWN, WM_RBUTTONDOWN };
        static const int dblClickMsgs[] =
          { WM_LBUTTONDBLCLK, WM_MBUTTONDBLCLK, WM_RBUTTONDBLCLK };
        const WidgetMouseEvent* mouseEvent = anEvent.AsMouseEvent();
        if (mouseEvent->mClickCount == 2) {
          pluginEvent.event = dblClickMsgs[mouseEvent->button];
        } else {
          pluginEvent.event = downMsgs[mouseEvent->button];
        }
        break;
      }
      case eMouseUp: {
        static const int upMsgs[] =
          { WM_LBUTTONUP, WM_MBUTTONUP, WM_RBUTTONUP };
        const WidgetMouseEvent* mouseEvent = anEvent.AsMouseEvent();
        pluginEvent.event = upMsgs[mouseEvent->button];
        break;
      }
      // For plugins which don't support high-resolution scroll, we should
      // generate legacy resolution wheel messages.  I.e., the delta value
      // should be WHEEL_DELTA * n.
      case eWheel: {
        const WidgetWheelEvent* wheelEvent = anEvent.AsWheelEvent();
        int32_t delta = 0;
        if (wheelEvent->mLineOrPageDeltaY) {
          switch (wheelEvent->mDeltaMode) {
            case nsIDOMWheelEvent::DOM_DELTA_PAGE:
              pluginEvent.event = WM_MOUSEWHEEL;
              delta = -WHEEL_DELTA * wheelEvent->mLineOrPageDeltaY;
              break;
            case nsIDOMWheelEvent::DOM_DELTA_LINE: {
              UINT linesPerWheelDelta = 0;
              if (NS_WARN_IF(!::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0,
                                                     &linesPerWheelDelta, 0))) {
                // Use system default scroll amount, 3, when
                // SPI_GETWHEELSCROLLLINES isn't available.
                linesPerWheelDelta = 3;
              }
              if (!linesPerWheelDelta) {
                break;
              }
              pluginEvent.event = WM_MOUSEWHEEL;
              delta = -WHEEL_DELTA / linesPerWheelDelta;
              delta *= wheelEvent->mLineOrPageDeltaY;
              break;
            }
            case nsIDOMWheelEvent::DOM_DELTA_PIXEL:
            default:
              // We don't support WM_GESTURE with this path.
              MOZ_ASSERT(!pluginEvent.event);
              break;
          }
        } else if (wheelEvent->mLineOrPageDeltaX) {
          switch (wheelEvent->mDeltaMode) {
            case nsIDOMWheelEvent::DOM_DELTA_PAGE:
              pluginEvent.event = WM_MOUSEHWHEEL;
              delta = -WHEEL_DELTA * wheelEvent->mLineOrPageDeltaX;
              break;
            case nsIDOMWheelEvent::DOM_DELTA_LINE: {
              pluginEvent.event = WM_MOUSEHWHEEL;
              UINT charsPerWheelDelta = 0;
              // FYI: SPI_GETWHEELSCROLLCHARS is available on Vista or later.
              if (::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0,
                                         &charsPerWheelDelta, 0)) {
                // Use system default scroll amount, 3, when
                // SPI_GETWHEELSCROLLCHARS isn't available.
                charsPerWheelDelta = 3;
              }
              if (!charsPerWheelDelta) {
                break;
              }
              delta = WHEEL_DELTA / charsPerWheelDelta;
              delta *= wheelEvent->mLineOrPageDeltaX;
              break;
            }
            case nsIDOMWheelEvent::DOM_DELTA_PIXEL:
            default:
              // We don't support WM_GESTURE with this path.
              MOZ_ASSERT(!pluginEvent.event);
              break;
          }
        }

        if (!pluginEvent.event) {
          break;
        }

        initWParamWithCurrentState = false;
        int32_t modifiers =
          (wheelEvent->IsControl() ?             MK_CONTROL  : 0) |
          (wheelEvent->IsShift() ?               MK_SHIFT    : 0) |
          (wheelEvent->IsLeftButtonPressed() ?   MK_LBUTTON  : 0) |
          (wheelEvent->IsMiddleButtonPressed() ? MK_MBUTTON  : 0) |
          (wheelEvent->IsRightButtonPressed() ?  MK_RBUTTON  : 0) |
          (wheelEvent->Is4thButtonPressed() ?    MK_XBUTTON1 : 0) |
          (wheelEvent->Is5thButtonPressed() ?    MK_XBUTTON2 : 0);
        pluginEvent.wParam = MAKEWPARAM(modifiers, delta);
        pPluginEvent = &pluginEvent;
        break;
      }
      // don't synthesize anything for eMouseDoubleClick, since that
      // is a synthetic event generated on mouse-up, and Windows WM_*DBLCLK
      // messages are sent on mouse-down
      default:
        break;
      }
      if (pluginEvent.event && initWParamWithCurrentState) {
        pPluginEvent = &pluginEvent;
        pluginEvent.wParam =
          (::GetKeyState(VK_CONTROL) ? MK_CONTROL : 0) |
          (::GetKeyState(VK_SHIFT) ? MK_SHIFT : 0) |
          (::GetKeyState(VK_LBUTTON) ? MK_LBUTTON : 0) |
          (::GetKeyState(VK_MBUTTON) ? MK_MBUTTON : 0) |
          (::GetKeyState(VK_RBUTTON) ? MK_RBUTTON : 0) |
          (::GetKeyState(VK_XBUTTON1) ? MK_XBUTTON1 : 0) |
          (::GetKeyState(VK_XBUTTON2) ? MK_XBUTTON2 : 0);
      }
    }
    if (pPluginEvent) {
      // Make event coordinates relative to our enclosing widget,
      // not the widget they were received on.
      // See use of NPEvent in widget/windows/nsWindow.cpp
      // for why this assert should be safe
      NS_ASSERTION(anEvent.mMessage == eMouseDown ||
                   anEvent.mMessage == eMouseUp ||
                   anEvent.mMessage == eMouseDoubleClick ||
                   anEvent.mMessage == eMouseAuxClick ||
                   anEvent.mMessage == eMouseOver ||
                   anEvent.mMessage == eMouseOut ||
                   anEvent.mMessage == eMouseMove ||
                   anEvent.mMessage == eWheel,
                   "Incorrect event type for coordinate translation");
      nsPoint pt =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mPluginFrame) -
        mPluginFrame->GetContentRectRelativeToSelf().TopLeft();
      nsPresContext* presContext = mPluginFrame->PresContext();
      nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x),
                      presContext->AppUnitsToDevPixels(pt.y));
      nsIntPoint widgetPtPx = ptPx + mPluginFrame->GetWindowOriginInPixels(true);
      const_cast<NPEvent*>(pPluginEvent)->lParam = MAKELPARAM(widgetPtPx.x, widgetPtPx.y);
    }
  }
  else if (!pPluginEvent) {
    switch (anEvent.mMessage) {
      case eFocus:
        pluginEvent.event = WM_SETFOCUS;
        pluginEvent.wParam = 0;
        pluginEvent.lParam = 0;
        pPluginEvent = &pluginEvent;
        break;
      case eBlur:
        pluginEvent.event = WM_KILLFOCUS;
        pluginEvent.wParam = 0;
        pluginEvent.lParam = 0;
        pPluginEvent = &pluginEvent;
        break;
      default:
        break;
    }
  }

  if (pPluginEvent && !pPluginEvent->event) {
    // Don't send null events to plugins.
    NS_WARNING("nsPluginFrame ProcessEvent: trying to send null event to plugin.");
    return rv;
  }

  if (pPluginEvent) {
    int16_t response = kNPEventNotHandled;
    mInstance->HandleEvent(const_cast<NPEvent*>(pPluginEvent),
                           &response,
                           NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
    if (response == kNPEventHandled)
      rv = nsEventStatus_eConsumeNoDefault;
  }
#endif

#ifdef MOZ_X11
  // this code supports windowless plugins
  nsIWidget* widget = anEvent.mWidget;
  XEvent pluginEvent = XEvent();
  pluginEvent.type = 0;

  switch(anEvent.mClass) {
    case eMouseEventClass:
      {
        switch (anEvent.mMessage) {
          case eMouseClick:
          case eMouseDoubleClick:
          case eMouseAuxClick:
            // Button up/down events sent instead.
            return rv;
          default:
            break;
          }

        // Get reference point relative to plugin origin.
        const nsPresContext* presContext = mPluginFrame->PresContext();
        nsPoint appPoint =
          nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mPluginFrame) -
          mPluginFrame->GetContentRectRelativeToSelf().TopLeft();
        nsIntPoint pluginPoint(presContext->AppUnitsToDevPixels(appPoint.x),
                               presContext->AppUnitsToDevPixels(appPoint.y));
        const WidgetMouseEvent& mouseEvent = *anEvent.AsMouseEvent();
        // Get reference point relative to screen:
        LayoutDeviceIntPoint rootPoint(-1, -1);
        if (widget) {
          rootPoint = anEvent.mRefPoint + widget->WidgetToScreenOffset();
        }
#ifdef MOZ_WIDGET_GTK
        Window root = GDK_ROOT_WINDOW();
#else
        Window root = X11None; // Could XQueryTree, but this is not important.
#endif

        switch (anEvent.mMessage) {
          case eMouseOver:
          case eMouseOut:
            {
              XCrossingEvent& event = pluginEvent.xcrossing;
              event.type = anEvent.mMessage == eMouseOver ?
                EnterNotify : LeaveNotify;
              event.root = root;
              event.time = anEvent.mTime;
              event.x = pluginPoint.x;
              event.y = pluginPoint.y;
              event.x_root = rootPoint.x;
              event.y_root = rootPoint.y;
              event.state = XInputEventState(mouseEvent);
              // information lost
              event.subwindow = X11None;
              event.mode = -1;
              event.detail = NotifyDetailNone;
              event.same_screen = True;
              event.focus = mContentFocused;
            }
            break;
          case eMouseMove:
            {
              XMotionEvent& event = pluginEvent.xmotion;
              event.type = MotionNotify;
              event.root = root;
              event.time = anEvent.mTime;
              event.x = pluginPoint.x;
              event.y = pluginPoint.y;
              event.x_root = rootPoint.x;
              event.y_root = rootPoint.y;
              event.state = XInputEventState(mouseEvent);
              // information lost
              event.subwindow = X11None;
              event.is_hint = NotifyNormal;
              event.same_screen = True;
            }
            break;
          case eMouseDown:
          case eMouseUp:
            {
              XButtonEvent& event = pluginEvent.xbutton;
              event.type = anEvent.mMessage == eMouseDown ?
                ButtonPress : ButtonRelease;
              event.root = root;
              event.time = anEvent.mTime;
              event.x = pluginPoint.x;
              event.y = pluginPoint.y;
              event.x_root = rootPoint.x;
              event.y_root = rootPoint.y;
              event.state = XInputEventState(mouseEvent);
              switch (mouseEvent.button)
                {
                case WidgetMouseEvent::eMiddleButton:
                  event.button = 2;
                  break;
                case WidgetMouseEvent::eRightButton:
                  event.button = 3;
                  break;
                default: // WidgetMouseEvent::eLeftButton;
                  event.button = 1;
                  break;
                }
              // information lost:
              event.subwindow = X11None;
              event.same_screen = True;
            }
            break;
          default:
            break;
          }
      }
      break;

   //XXX case eMouseScrollEventClass: not received.

   case eKeyboardEventClass:
      if (anEvent.mPluginEvent)
        {
          XKeyEvent &event = pluginEvent.xkey;
#ifdef MOZ_WIDGET_GTK
          event.root = GDK_ROOT_WINDOW();
          event.time = anEvent.mTime;
          const GdkEventKey* gdkEvent =
            static_cast<const GdkEventKey*>(anEvent.mPluginEvent);
          event.keycode = gdkEvent->hardware_keycode;
          event.state = gdkEvent->state;
          switch (anEvent.mMessage)
            {
            case eKeyDown:
              // Handle eKeyDown for modifier key presses
              // For non-modifiers we get eKeyPress
              if (gdkEvent->is_modifier)
                event.type = XKeyPress;
              break;
            case eKeyPress:
              event.type = XKeyPress;
              break;
            case eKeyUp:
              event.type = KeyRelease;
              break;
            default:
              break;
            }
#endif

          // Information that could be obtained from pluginEvent but we may not
          // want to promise to provide:
          event.subwindow = X11None;
          event.x = 0;
          event.y = 0;
          event.x_root = -1;
          event.y_root = -1;
          event.same_screen = False;
        }
      else
        {
          // If we need to send synthesized key events, then
          // DOMKeyCodeToGdkKeyCode(keyEvent.keyCode) and
          // gdk_keymap_get_entries_for_keyval will be useful, but the
          // mappings will not be unique.
          NS_WARNING("Synthesized key event not sent to plugin");
        }
      break;

    default:
      switch (anEvent.mMessage) {
        case eFocus:
        case eBlur:
          {
            XFocusChangeEvent &event = pluginEvent.xfocus;
            event.type = anEvent.mMessage == eFocus ? FocusIn : FocusOut;
            // information lost:
            event.mode = -1;
            event.detail = NotifyDetailNone;
          }
          break;
        default:
          break;
      }
    }

  if (!pluginEvent.type) {
    return rv;
  }

  // Fill in (useless) generic event information.
  XAnyEvent& event = pluginEvent.xany;
  event.display = widget ?
    static_cast<Display*>(widget->GetNativeData(NS_NATIVE_DISPLAY)) : nullptr;
  event.window = X11None; // not a real window
  // information lost:
  event.serial = 0;
  event.send_event = False;

  int16_t response = kNPEventNotHandled;
  mInstance->HandleEvent(&pluginEvent, &response, NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
  if (response == kNPEventHandled)
    rv = nsEventStatus_eConsumeNoDefault;
#endif

#ifdef MOZ_WIDGET_ANDROID
  // this code supports windowless plugins
  {
    // The plugin needs focus to receive keyboard and touch events
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsCOMPtr<nsIDOMElement> elem = do_QueryReferent(mContent);
      fm->SetFocus(elem, 0);
    }
  }
  switch(anEvent.mClass) {
    case eMouseEventClass:
      {
        switch (anEvent.mMessage) {
          case eMouseClick:
          case eMouseDoubleClick:
          case eMouseAuxClick:
            // Button up/down events sent instead.
            return rv;
          default:
            break;
          }

        // Get reference point relative to plugin origin.
        const nsPresContext* presContext = mPluginFrame->PresContext();
        nsPoint appPoint =
          nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mPluginFrame) -
          mPluginFrame->GetContentRectRelativeToSelf().TopLeft();
        nsIntPoint pluginPoint(presContext->AppUnitsToDevPixels(appPoint.x),
                               presContext->AppUnitsToDevPixels(appPoint.y));

        switch (anEvent.mMessage) {
          case eMouseMove:
            {
              // are these going to be touch events?
              // pluginPoint.x;
              // pluginPoint.y;
            }
            break;
          case eMouseDown:
            {
              ANPEvent event;
              event.inSize = sizeof(ANPEvent);
              event.eventType = kMouse_ANPEventType;
              event.data.mouse.action = kDown_ANPMouseAction;
              event.data.mouse.x = pluginPoint.x;
              event.data.mouse.y = pluginPoint.y;
              mInstance->HandleEvent(&event, nullptr, NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
            }
            break;
          case eMouseUp:
            {
              ANPEvent event;
              event.inSize = sizeof(ANPEvent);
              event.eventType = kMouse_ANPEventType;
              event.data.mouse.action = kUp_ANPMouseAction;
              event.data.mouse.x = pluginPoint.x;
              event.data.mouse.y = pluginPoint.y;
              mInstance->HandleEvent(&event, nullptr, NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
            }
            break;
          default:
            break;
          }
      }
      break;

    case eKeyboardEventClass:
     {
       const WidgetKeyboardEvent& keyEvent = *anEvent.AsKeyboardEvent();
       LOG("Firing eKeyboardEventClass %d %d\n",
           keyEvent.mKeyCode, keyEvent.mCharCode);
       // pluginEvent is initialized by nsWindow::InitKeyEvent().
       const ANPEvent* pluginEvent = static_cast<const ANPEvent*>(keyEvent.mPluginEvent);
       if (pluginEvent) {
         MOZ_ASSERT(pluginEvent->inSize == sizeof(ANPEvent));
         MOZ_ASSERT(pluginEvent->eventType == kKey_ANPEventType);
         mInstance->HandleEvent(const_cast<ANPEvent*>(pluginEvent),
                                nullptr,
                                NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
       }
     }
     break;

    default:
      break;
    }
    rv = nsEventStatus_eConsumeNoDefault;
#endif

  return rv;
}

nsresult
nsPluginInstanceOwner::Destroy()
{
  SetFrame(nullptr);

#ifdef XP_MACOSX
  RemoveFromCARefreshTimer();
#endif

  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);

  // unregister context menu listener
  if (mCXMenuListener) {
    mCXMenuListener->Destroy(content);
    mCXMenuListener = nullptr;
  }

  content->RemoveEventListener(NS_LITERAL_STRING("focus"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("blur"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("mouseup"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("mousemove"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("click"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("dblclick"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("mouseover"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("mouseout"), this, false);
  content->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("keydown"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("keyup"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("drop"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("drag"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("dragenter"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("dragover"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("dragleave"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("dragexit"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("dragstart"), this, true);
  content->RemoveEventListener(NS_LITERAL_STRING("dragend"), this, true);
  content->RemoveSystemEventListener(NS_LITERAL_STRING("compositionstart"),
                                     this, true);
  content->RemoveSystemEventListener(NS_LITERAL_STRING("compositionend"),
                                     this, true);
  content->RemoveSystemEventListener(NS_LITERAL_STRING("text"), this, true);

#if MOZ_WIDGET_ANDROID
  RemovePluginView();
#endif

  if (mWidget) {
    if (mPluginWindow) {
      mPluginWindow->SetPluginWidget(nullptr);
    }

    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget) {
      pluginWidget->SetPluginInstanceOwner(nullptr);
    }
    mWidget->Destroy();
  }

  return NS_OK;
}

// Paints are handled differently, so we just simulate an update event.

#ifdef XP_MACOSX
void nsPluginInstanceOwner::Paint(const gfxRect& aDirtyRect, CGContextRef cgContext)
{
  if (!mInstance || !mPluginFrame)
    return;

  gfxRect dirtyRectCopy = aDirtyRect;
  double scaleFactor = 1.0;
  GetContentsScaleFactor(&scaleFactor);
  if (scaleFactor != 1.0) {
    ::CGContextScaleCTM(cgContext, scaleFactor, scaleFactor);
    // Convert aDirtyRect from device pixels to "display pixels"
    // for HiDPI modes
    dirtyRectCopy.ScaleRoundOut(1.0 / scaleFactor);
  }

  DoCocoaEventDrawRect(dirtyRectCopy, cgContext);
}

void nsPluginInstanceOwner::DoCocoaEventDrawRect(const gfxRect& aDrawRect, CGContextRef cgContext)
{
  if (!mInstance || !mPluginFrame)
    return;

  // The context given here is only valid during the HandleEvent call.
  NPCocoaEvent updateEvent;
  InitializeNPCocoaEvent(&updateEvent);
  updateEvent.type = NPCocoaEventDrawRect;
  updateEvent.data.draw.context = cgContext;
  updateEvent.data.draw.x = aDrawRect.X();
  updateEvent.data.draw.y = aDrawRect.Y();
  updateEvent.data.draw.width = aDrawRect.Width();
  updateEvent.data.draw.height = aDrawRect.Height();

  mInstance->HandleEvent(&updateEvent, nullptr);
}
#endif

#ifdef XP_WIN
void nsPluginInstanceOwner::Paint(const RECT& aDirty, HDC aDC)
{
  if (!mInstance || !mPluginFrame)
    return;

  NPEvent pluginEvent;
  pluginEvent.event = WM_PAINT;
  pluginEvent.wParam = WPARAM(aDC);
  pluginEvent.lParam = LPARAM(&aDirty);
  mInstance->HandleEvent(&pluginEvent, nullptr);
}
#endif

#ifdef MOZ_WIDGET_ANDROID

void nsPluginInstanceOwner::Paint(gfxContext* aContext,
                                  const gfxRect& aFrameRect,
                                  const gfxRect& aDirtyRect)
{
  if (!mInstance || !mPluginFrame || !mPluginDocumentActiveState || mFullScreen)
    return;

  int32_t model = mInstance->GetANPDrawingModel();

  if (model == kSurface_ANPDrawingModel) {
    if (!AddPluginView(GetPluginRect())) {
      Invalidate();
    }
    return;
  }

  if (model != kBitmap_ANPDrawingModel)
    return;

#ifdef ANP_BITMAP_DRAWING_MODEL
  static RefPtr<gfxImageSurface> pluginSurface;

  if (pluginSurface == nullptr ||
      aFrameRect.width  != pluginSurface->Width() ||
      aFrameRect.height != pluginSurface->Height()) {

    pluginSurface = new gfxImageSurface(gfx::IntSize(aFrameRect.width, aFrameRect.height),
                                        SurfaceFormat::A8R8G8B8_UINT32);
    if (!pluginSurface)
      return;
  }

  // Clears buffer.  I think this is needed.
  gfxUtils::ClearThebesSurface(pluginSurface);

  ANPEvent event;
  event.inSize = sizeof(ANPEvent);
  event.eventType = 4;
  event.data.draw.model = 1;

  event.data.draw.clip.top     = 0;
  event.data.draw.clip.left    = 0;
  event.data.draw.clip.bottom  = aFrameRect.width;
  event.data.draw.clip.right   = aFrameRect.height;

  event.data.draw.data.bitmap.format   = kRGBA_8888_ANPBitmapFormat;
  event.data.draw.data.bitmap.width    = aFrameRect.width;
  event.data.draw.data.bitmap.height   = aFrameRect.height;
  event.data.draw.data.bitmap.baseAddr = pluginSurface->Data();
  event.data.draw.data.bitmap.rowBytes = aFrameRect.width * 4;

  if (!mInstance)
    return;

  mInstance->HandleEvent(&event, nullptr);

  aContext->SetOp(gfx::CompositionOp::OP_SOURCE);
  aContext->SetSource(pluginSurface, gfxPoint(aFrameRect.x, aFrameRect.y));
  aContext->Clip(aFrameRect);
  aContext->Paint();
#endif
}
#endif

#if defined(MOZ_X11)
void nsPluginInstanceOwner::Paint(gfxContext* aContext,
                                  const gfxRect& aFrameRect,
                                  const gfxRect& aDirtyRect)
{
  if (!mInstance || !mPluginFrame)
    return;

  // to provide crisper and faster drawing.
  gfxRect pluginRect = aFrameRect;
  if (aContext->UserToDevicePixelSnapped(pluginRect)) {
    pluginRect = aContext->DeviceToUser(pluginRect);
  }

  // Round out the dirty rect to plugin pixels to ensure the plugin draws
  // enough pixels for interpolation to device pixels.
  gfxRect dirtyRect = aDirtyRect - pluginRect.TopLeft();
  dirtyRect.RoundOut();

  // Plugins can only draw an integer number of pixels.
  //
  // With translation-only transformation matrices, pluginRect is already
  // pixel-aligned.
  //
  // With more complex transformations, modifying the scales in the
  // transformation matrix could retain subpixel accuracy and let the plugin
  // draw a suitable number of pixels for interpolation to device pixels in
  // Renderer::Draw, but such cases are not common enough to warrant the
  // effort now.
  nsIntSize pluginSize(NS_lround(pluginRect.width),
                       NS_lround(pluginRect.height));

  // Determine what the plugin needs to draw.
  nsIntRect pluginDirtyRect(int32_t(dirtyRect.x),
                            int32_t(dirtyRect.y),
                            int32_t(dirtyRect.width),
                            int32_t(dirtyRect.height));
  if (!pluginDirtyRect.
      IntersectRect(nsIntRect(0, 0, pluginSize.width, pluginSize.height),
                    pluginDirtyRect))
    return;

  NPWindow* window;
  GetWindow(window);

  uint32_t rendererFlags = 0;
  if (!mFlash10Quirks) {
    rendererFlags |=
      Renderer::DRAW_SUPPORTS_CLIP_RECT |
      Renderer::DRAW_SUPPORTS_ALTERNATE_VISUAL;
  }

  bool transparent;
  mInstance->IsTransparent(&transparent);
  if (!transparent)
    rendererFlags |= Renderer::DRAW_IS_OPAQUE;

  // Renderer::Draw() draws a rectangle with top-left at the aContext origin.
  gfxContextAutoSaveRestore autoSR(aContext);
  aContext->SetMatrix(
    aContext->CurrentMatrix().Translate(pluginRect.TopLeft()));

  Renderer renderer(window, this, pluginSize, pluginDirtyRect);

  Display* dpy = mozilla::DefaultXDisplay();
  Screen* screen = DefaultScreenOfDisplay(dpy);
  Visual* visual = DefaultVisualOfScreen(screen);

  renderer.Draw(aContext, nsIntSize(window->width, window->height),
                rendererFlags, screen, visual);
}
nsresult
nsPluginInstanceOwner::Renderer::DrawWithXlib(cairo_surface_t* xsurface,
                                              nsIntPoint offset,
                                              nsIntRect *clipRects,
                                              uint32_t numClipRects)
{
  Screen *screen = cairo_xlib_surface_get_screen(xsurface);
  Colormap colormap;
  Visual* visual;
  if (!gfxXlibSurface::GetColormapAndVisual(xsurface, &colormap, &visual)) {
    NS_ERROR("Failed to get visual and colormap");
    return NS_ERROR_UNEXPECTED;
  }

  nsNPAPIPluginInstance *instance = mInstanceOwner->mInstance;
  if (!instance)
    return NS_ERROR_FAILURE;

  // See if the plugin must be notified of new window parameters.
  bool doupdatewindow = false;

  if (mWindow->x != offset.x || mWindow->y != offset.y) {
    mWindow->x = offset.x;
    mWindow->y = offset.y;
    doupdatewindow = true;
  }

  if (nsIntSize(mWindow->width, mWindow->height) != mPluginSize) {
    mWindow->width = mPluginSize.width;
    mWindow->height = mPluginSize.height;
    doupdatewindow = true;
  }

  // The clip rect is relative to drawable top-left.
  NS_ASSERTION(numClipRects <= 1, "We don't support multiple clip rectangles!");
  nsIntRect clipRect;
  if (numClipRects) {
    clipRect.x = clipRects[0].x;
    clipRect.y = clipRects[0].y;
    clipRect.width  = clipRects[0].width;
    clipRect.height = clipRects[0].height;
    // NPRect members are unsigned, but clip rectangles should be contained by
    // the surface.
    NS_ASSERTION(clipRect.x >= 0 && clipRect.y >= 0,
                 "Clip rectangle offsets are negative!");
  }
  else {
    clipRect.x = offset.x;
    clipRect.y = offset.y;
    clipRect.width  = mWindow->width;
    clipRect.height = mWindow->height;
    // Don't ask the plugin to draw outside the drawable.
    // This also ensures that the unsigned clip rectangle offsets won't be -ve.
    clipRect.IntersectRect(clipRect,
                           nsIntRect(0, 0,
                                     cairo_xlib_surface_get_width(xsurface),
                                     cairo_xlib_surface_get_height(xsurface)));
  }

  NPRect newClipRect;
  newClipRect.left = clipRect.x;
  newClipRect.top = clipRect.y;
  newClipRect.right = clipRect.XMost();
  newClipRect.bottom = clipRect.YMost();
  if (mWindow->clipRect.left    != newClipRect.left   ||
      mWindow->clipRect.top     != newClipRect.top    ||
      mWindow->clipRect.right   != newClipRect.right  ||
      mWindow->clipRect.bottom  != newClipRect.bottom) {
    mWindow->clipRect = newClipRect;
    doupdatewindow = true;
  }

  NPSetWindowCallbackStruct* ws_info =
    static_cast<NPSetWindowCallbackStruct*>(mWindow->ws_info);
#ifdef MOZ_X11
  if (ws_info->visual != visual || ws_info->colormap != colormap) {
    ws_info->visual = visual;
    ws_info->colormap = colormap;
    ws_info->depth = gfxXlibSurface::DepthOfVisual(screen, visual);
    doupdatewindow = true;
  }
#endif

  {
    if (doupdatewindow)
      instance->SetWindow(mWindow);
  }

  // Translate the dirty rect to drawable coordinates.
  nsIntRect dirtyRect = mDirtyRect + offset;
  if (mInstanceOwner->mFlash10Quirks) {
    // Work around a bug in Flash up to 10.1 d51 at least, where expose event
    // top left coordinates within the plugin-rect and not at the drawable
    // origin are misinterpreted.  (We can move the top left coordinate
    // provided it is within the clipRect.)
    dirtyRect.SetRect(offset.x, offset.y,
                      mDirtyRect.XMost(), mDirtyRect.YMost());
  }
  // Intersect the dirty rect with the clip rect to ensure that it lies within
  // the drawable.
  if (!dirtyRect.IntersectRect(dirtyRect, clipRect))
    return NS_OK;

  {
    XEvent pluginEvent = XEvent();
    XGraphicsExposeEvent& exposeEvent = pluginEvent.xgraphicsexpose;
    // set the drawing info
    exposeEvent.type = GraphicsExpose;
    exposeEvent.display = DisplayOfScreen(screen);
    exposeEvent.drawable = cairo_xlib_surface_get_drawable(xsurface);
    exposeEvent.x = dirtyRect.x;
    exposeEvent.y = dirtyRect.y;
    exposeEvent.width  = dirtyRect.width;
    exposeEvent.height = dirtyRect.height;
    exposeEvent.count = 0;
    // information not set:
    exposeEvent.serial = 0;
    exposeEvent.send_event = False;
    exposeEvent.major_code = 0;
    exposeEvent.minor_code = 0;

    instance->HandleEvent(&pluginEvent, nullptr);
  }
  return NS_OK;
}
#endif

nsresult nsPluginInstanceOwner::Init(nsIContent* aContent)
{
  mLastEventloopNestingLevel = GetEventloopNestingLevel();

  mContent = do_GetWeakReference(aContent);

  // Get a frame, don't reflow. If a reflow was necessary it should have been
  // done at a higher level than this (content).
  nsIFrame* frame = aContent->GetPrimaryFrame();
  nsIObjectFrame* iObjFrame = do_QueryFrame(frame);
  nsPluginFrame* objFrame =  static_cast<nsPluginFrame*>(iObjFrame);
  if (objFrame) {
    SetFrame(objFrame);
    // Some plugins require a specific sequence of shutdown and startup when
    // a page is reloaded. Shutdown happens usually when the last instance
    // is destroyed. Here we make sure the plugin instance in the old
    // document is destroyed before we try to create the new one.
    objFrame->PresContext()->EnsureVisible();
  } else {
    NS_NOTREACHED("Should not be initializing plugin without a frame");
    return NS_ERROR_FAILURE;
  }

  // register context menu listener
  mCXMenuListener = new nsPluginDOMContextMenuListener(aContent);

  aContent->AddEventListener(NS_LITERAL_STRING("focus"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("blur"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("mouseup"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("mousedown"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("mousemove"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("click"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("dblclick"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("mouseover"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("mouseout"), this, false,
                             false);
  aContent->AddEventListener(NS_LITERAL_STRING("keypress"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("keydown"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("keyup"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("drop"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("drag"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("dragenter"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("dragover"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("dragleave"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("dragexit"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("dragstart"), this, true);
  aContent->AddEventListener(NS_LITERAL_STRING("dragend"), this, true);
  aContent->AddSystemEventListener(NS_LITERAL_STRING("compositionstart"),
    this, true);
  aContent->AddSystemEventListener(NS_LITERAL_STRING("compositionend"), this,
    true);
  aContent->AddSystemEventListener(NS_LITERAL_STRING("text"), this, true);

  return NS_OK;
}

void* nsPluginInstanceOwner::GetPluginPort()
{
  void* result = nullptr;
  if (mWidget) {
#ifdef XP_WIN
    if (!mPluginWindow || mPluginWindow->type == NPWindowTypeWindow)
#endif
      result = mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT); // HWND/gdk window
  }

  return result;
}

void nsPluginInstanceOwner::ReleasePluginPort(void * pluginPort)
{
}

NS_IMETHODIMP nsPluginInstanceOwner::CreateWidget(void)
{
  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  nsresult rv = NS_ERROR_FAILURE;

  // Can't call this twice!
  if (mWidget) {
    NS_WARNING("Trying to create a plugin widget twice!");
    return NS_ERROR_FAILURE;
  }

  bool windowless = false;
  mInstance->IsWindowless(&windowless);
  if (!windowless) {
    // Try to get a parent widget, on some platforms widget creation will fail without
    // a parent.
    nsCOMPtr<nsIWidget> parentWidget;
    nsIDocument *doc = nullptr;
    nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
    if (content) {
      doc = content->OwnerDoc();
      parentWidget = nsContentUtils::WidgetForDocument(doc);
#ifndef XP_MACOSX
      // If we're running in the content process, we need a remote widget created in chrome.
      if (XRE_IsContentProcess()) {
        if (nsCOMPtr<nsPIDOMWindowOuter> window = doc->GetWindow()) {
          if (nsCOMPtr<nsPIDOMWindowOuter> topWindow = window->GetTop()) {
            dom::TabChild* tc = dom::TabChild::GetFrom(topWindow);
            if (tc) {
              // This returns a PluginWidgetProxy which remotes a number of calls.
              rv = tc->CreatePluginWidget(parentWidget.get(), getter_AddRefs(mWidget));
              if (NS_FAILED(rv)) {
                return rv;
              }
            }
          }
        }
      }
#endif // XP_MACOSX
    }

#ifndef XP_MACOSX
    // A failure here is terminal since we can't fall back on the non-e10s code
    // path below.
    if (!mWidget && XRE_IsContentProcess()) {
      return NS_ERROR_UNEXPECTED;
    }
#endif // XP_MACOSX

    if (!mWidget) {
      // native (single process)
      mWidget = do_CreateInstance(kWidgetCID, &rv);
      nsWidgetInitData initData;
      initData.mWindowType = eWindowType_plugin;
      initData.mUnicode = false;
      initData.clipChildren = true;
      initData.clipSiblings = true;
      rv = mWidget->Create(parentWidget.get(), nullptr,
                           LayoutDeviceIntRect(0, 0, 0, 0), &initData);
      if (NS_FAILED(rv)) {
        mWidget->Destroy();
        mWidget = nullptr;
        return rv;
      }
    }

    mWidget->EnableDragDrop(true);
    mWidget->Show(false);
    mWidget->Enable(false);
  }

  if (mPluginFrame) {
    // nullptr widget is fine, will result in windowless setup.
    mPluginFrame->PrepForDrawing(mWidget);
  }

  if (windowless) {
    mPluginWindow->type = NPWindowTypeDrawable;

    // this needs to be a HDC according to the spec, but I do
    // not see the right way to release it so let's postpone
    // passing HDC till paint event when it is really
    // needed. Change spec?
    mPluginWindow->window = nullptr;
#ifdef MOZ_X11
    // Fill in the display field.
    NPSetWindowCallbackStruct* ws_info =
    static_cast<NPSetWindowCallbackStruct*>(mPluginWindow->ws_info);
    ws_info->display = DefaultXDisplay();

    nsAutoCString description;
    GetPluginDescription(description);
    NS_NAMED_LITERAL_CSTRING(flash10Head, "Shockwave Flash 10.");
    mFlash10Quirks = StringBeginsWith(description, flash10Head);
#endif
  } else if (mWidget) {
    // mPluginWindow->type is used in |GetPluginPort| so it must
    // be initialized first
    mPluginWindow->type = NPWindowTypeWindow;
    mPluginWindow->window = GetPluginPort();
    // tell the plugin window about the widget
    mPluginWindow->SetPluginWidget(mWidget);

    // tell the widget about the current plugin instance owner.
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget) {
      pluginWidget->SetPluginInstanceOwner(this);
    }
  }

#ifdef XP_MACOSX
  if (GetDrawingModel() == NPDrawingModelCoreAnimation) {
    AddToCARefreshTimer();
  }
#endif

  mWidgetCreationComplete = true;

  return NS_OK;
}

// Mac specific code to fix up the port location and clipping region
#ifdef XP_MACOSX

void nsPluginInstanceOwner::FixUpPluginWindow(int32_t inPaintState)
{
  if (!mPluginWindow || !mInstance || !mPluginFrame) {
    return;
  }

  SetPluginPort();

  LayoutDeviceIntSize widgetClip = mPluginFrame->GetWidgetlessClipRect().Size();

  mPluginWindow->x = 0;
  mPluginWindow->y = 0;

  NPRect oldClipRect = mPluginWindow->clipRect;

  // fix up the clipping region
  mPluginWindow->clipRect.top  = 0;
  mPluginWindow->clipRect.left = 0;

  if (inPaintState == ePluginPaintDisable) {
    mPluginWindow->clipRect.bottom = mPluginWindow->clipRect.top;
    mPluginWindow->clipRect.right  = mPluginWindow->clipRect.left;
  }
  else if (inPaintState == ePluginPaintEnable)
  {
    mPluginWindow->clipRect.bottom = mPluginWindow->clipRect.top + widgetClip.height;
    mPluginWindow->clipRect.right  = mPluginWindow->clipRect.left + widgetClip.width;
  }

  // if the clip rect changed, call SetWindow()
  // (RealPlayer needs this to draw correctly)
  if (mPluginWindow->clipRect.left    != oldClipRect.left   ||
      mPluginWindow->clipRect.top     != oldClipRect.top    ||
      mPluginWindow->clipRect.right   != oldClipRect.right  ||
      mPluginWindow->clipRect.bottom  != oldClipRect.bottom)
  {
    if (UseAsyncRendering()) {
      mInstance->AsyncSetWindow(mPluginWindow);
    }
    else {
      mPluginWindow->CallSetWindow(mInstance);
    }
  }

  // After the first NPP_SetWindow call we need to send an initial
  // top-level window focus event.
  if (!mSentInitialTopLevelWindowEvent) {
    // Set this before calling ProcessEvent to avoid endless recursion.
    mSentInitialTopLevelWindowEvent = true;

    bool isActive = WindowIsActive();
    SendWindowFocusChanged(isActive);
    mLastWindowIsActive = isActive;
  }
}

void
nsPluginInstanceOwner::WindowFocusMayHaveChanged()
{
  if (!mSentInitialTopLevelWindowEvent) {
    return;
  }

  bool isActive = WindowIsActive();
  if (isActive != mLastWindowIsActive) {
    SendWindowFocusChanged(isActive);
    mLastWindowIsActive = isActive;
  }
}

bool
nsPluginInstanceOwner::WindowIsActive()
{
  if (!mPluginFrame) {
    return false;
  }

  EventStates docState = mPluginFrame->GetContent()->OwnerDoc()->GetDocumentState();
  return !docState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
}

void
nsPluginInstanceOwner::SendWindowFocusChanged(bool aIsActive)
{
  if (!mInstance) {
    return;
  }

  NPCocoaEvent cocoaEvent;
  InitializeNPCocoaEvent(&cocoaEvent);
  cocoaEvent.type = NPCocoaEventWindowFocusChanged;
  cocoaEvent.data.focus.hasFocus = aIsActive;
  mInstance->HandleEvent(&cocoaEvent,
                         nullptr,
                         NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
}

void
nsPluginInstanceOwner::HidePluginWindow()
{
  if (!mPluginWindow || !mInstance) {
    return;
  }

  mPluginWindow->clipRect.bottom = mPluginWindow->clipRect.top;
  mPluginWindow->clipRect.right  = mPluginWindow->clipRect.left;
  mWidgetVisible = false;
  if (UseAsyncRendering()) {
    mInstance->AsyncSetWindow(mPluginWindow);
  } else {
    mInstance->SetWindow(mPluginWindow);
  }
}

#else // XP_MACOSX

void nsPluginInstanceOwner::UpdateWindowPositionAndClipRect(bool aSetWindow)
{
  if (!mPluginWindow)
    return;

  // For windowless plugins a non-empty clip rectangle will be
  // passed to the plugin during paint, an additional update
  // of the the clip rectangle here is not required
  if (aSetWindow && !mWidget && mPluginWindowVisible && !UseAsyncRendering())
    return;

  const NPWindow oldWindow = *mPluginWindow;

  bool windowless = (mPluginWindow->type == NPWindowTypeDrawable);
  nsIntPoint origin = mPluginFrame->GetWindowOriginInPixels(windowless);

  mPluginWindow->x = origin.x;
  mPluginWindow->y = origin.y;

  mPluginWindow->clipRect.left = 0;
  mPluginWindow->clipRect.top = 0;

  if (mPluginWindowVisible && mPluginDocumentActiveState) {
    mPluginWindow->clipRect.right = mPluginWindow->width;
    mPluginWindow->clipRect.bottom = mPluginWindow->height;
  } else {
    mPluginWindow->clipRect.right = 0;
    mPluginWindow->clipRect.bottom = 0;
  }

  if (!aSetWindow)
    return;

  if (mPluginWindow->x               != oldWindow.x               ||
      mPluginWindow->y               != oldWindow.y               ||
      mPluginWindow->clipRect.left   != oldWindow.clipRect.left   ||
      mPluginWindow->clipRect.top    != oldWindow.clipRect.top    ||
      mPluginWindow->clipRect.right  != oldWindow.clipRect.right  ||
      mPluginWindow->clipRect.bottom != oldWindow.clipRect.bottom) {
    CallSetWindow();
  }
}

void
nsPluginInstanceOwner::UpdateWindowVisibility(bool aVisible)
{
  mPluginWindowVisible = aVisible;
  UpdateWindowPositionAndClipRect(true);
}
#endif // XP_MACOSX

void
nsPluginInstanceOwner::ResolutionMayHaveChanged()
{
#if defined(XP_MACOSX) || defined(XP_WIN)
  double scaleFactor = 1.0;
  GetContentsScaleFactor(&scaleFactor);
  if (scaleFactor != mLastScaleFactor) {
    ContentsScaleFactorChanged(scaleFactor);
    mLastScaleFactor = scaleFactor;
  }
#endif
  float zoomFactor = 1.0;
  GetCSSZoomFactor(&zoomFactor);
  if (zoomFactor != mLastCSSZoomFactor) {
    if (mInstance) {
      mInstance->CSSZoomFactorChanged(zoomFactor);
    }
    mLastCSSZoomFactor = zoomFactor;
  }

}

void
nsPluginInstanceOwner::UpdateDocumentActiveState(bool aIsActive)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);

  mPluginDocumentActiveState = aIsActive;
#ifndef XP_MACOSX
  UpdateWindowPositionAndClipRect(true);

#ifdef MOZ_WIDGET_ANDROID
  if (mInstance) {
    if (!mPluginDocumentActiveState) {
      RemovePluginView();
    }

    mInstance->NotifyOnScreen(mPluginDocumentActiveState);

    // This is, perhaps, incorrect. It is supposed to be sent
    // when "the webview has paused or resumed". The side effect
    // is that Flash video players pause or resume (if they were
    // playing before) based on the value here. I personally think
    // we want that on Android when switching to another tab, so
    // that's why we call it here.
    mInstance->NotifyForeground(mPluginDocumentActiveState);
  }
#endif // #ifdef MOZ_WIDGET_ANDROID

  // We don't have a connection to PluginWidgetParent in the chrome
  // process when dealing with tab visibility changes, so this needs
  // to be forwarded over after the active state is updated. If we
  // don't hide plugin widgets in hidden tabs, the native child window
  // in chrome will remain visible after a tab switch.
  if (mWidget && XRE_IsContentProcess()) {
    mWidget->Show(aIsActive);
    mWidget->Enable(aIsActive);
  }
#endif // #ifndef XP_MACOSX
}

NS_IMETHODIMP
nsPluginInstanceOwner::CallSetWindow()
{
  if (!mWidgetCreationComplete) {
    // No widget yet, we can't run this code
    return NS_OK;
  }
  if (mPluginFrame) {
    mPluginFrame->CallSetWindow(false);
  } else if (mInstance) {
    if (UseAsyncRendering()) {
      mInstance->AsyncSetWindow(mPluginWindow);
    } else {
      mInstance->SetWindow(mPluginWindow);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPluginInstanceOwner::GetContentsScaleFactor(double *result)
{
  NS_ENSURE_ARG_POINTER(result);
  double scaleFactor = 1.0;
  // On Mac, device pixels need to be translated to (and from) "display pixels"
  // for plugins. On other platforms, plugin coordinates are always in device
  // pixels.
#if defined(XP_MACOSX) || defined(XP_WIN)
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  nsIPresShell* presShell = nsContentUtils::FindPresShellForDocument(content->OwnerDoc());
  if (presShell) {
    scaleFactor = double(nsPresContext::AppUnitsPerCSSPixel())/
      presShell->GetPresContext()->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom();
  }
#endif
  *result = scaleFactor;
  return NS_OK;
}

void
nsPluginInstanceOwner::GetCSSZoomFactor(float *result)
{
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  nsIPresShell* presShell = nsContentUtils::FindPresShellForDocument(content->OwnerDoc());
  if (presShell) {
    *result = presShell->GetPresContext()->DeviceContext()->GetFullZoom();
  } else {
    *result = 1.0;
  }
}

void nsPluginInstanceOwner::SetFrame(nsPluginFrame *aFrame)
{
  // Don't do anything if the frame situation hasn't changed.
  if (mPluginFrame == aFrame) {
    return;
  }

  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);

  // If we already have a frame that is changing or going away...
  if (mPluginFrame) {
    if (content && content->OwnerDoc() && content->OwnerDoc()->GetWindow()) {
      nsCOMPtr<EventTarget> windowRoot = content->OwnerDoc()->GetWindow()->GetTopWindowRoot();
      if (windowRoot) {
        windowRoot->RemoveEventListener(NS_LITERAL_STRING("activate"),
                                              this, false);
        windowRoot->RemoveEventListener(NS_LITERAL_STRING("deactivate"),
                                              this, false);
        windowRoot->RemoveEventListener(NS_LITERAL_STRING("MozPerformDelayedBlur"),
                                              this, false);
      }
    }

    // Make sure the old frame isn't holding a reference to us.
    mPluginFrame->SetInstanceOwner(nullptr);
  }

  // Swap in the new frame (or no frame)
  mPluginFrame = aFrame;

  // Set up a new frame
  if (mPluginFrame) {
    mPluginFrame->SetInstanceOwner(this);
    // Can only call PrepForDrawing on an object frame once. Don't do it here unless
    // widget creation is complete. Doesn't matter if we actually have a widget.
    if (mWidgetCreationComplete) {
      mPluginFrame->PrepForDrawing(mWidget);
    }
    mPluginFrame->FixupWindow(mPluginFrame->GetContentRectRelativeToSelf().Size());
    mPluginFrame->InvalidateFrame();

    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    const nsIContent* content = aFrame->GetContent();
    if (fm && content) {
      mContentFocused = (content == fm->GetFocusedContent());
    }

    // Register for widget-focus events on the window root.
    if (content && content->OwnerDoc() && content->OwnerDoc()->GetWindow()) {
      nsCOMPtr<EventTarget> windowRoot = content->OwnerDoc()->GetWindow()->GetTopWindowRoot();
      if (windowRoot) {
        windowRoot->AddEventListener(NS_LITERAL_STRING("activate"),
                                           this, false, false);
        windowRoot->AddEventListener(NS_LITERAL_STRING("deactivate"),
                                           this, false, false);
        windowRoot->AddEventListener(NS_LITERAL_STRING("MozPerformDelayedBlur"),
                                           this, false, false);
      }
    }
  }
}

nsPluginFrame* nsPluginInstanceOwner::GetFrame()
{
  return mPluginFrame;
}

NS_IMETHODIMP nsPluginInstanceOwner::PrivateModeChanged(bool aEnabled)
{
  return mInstance ? mInstance->PrivateModeStateChanged(aEnabled) : NS_OK;
}

already_AddRefed<nsIURI> nsPluginInstanceOwner::GetBaseURI() const
{
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (!content) {
    return nullptr;
  }
  return content->GetBaseURI();
}

// static
void
nsPluginInstanceOwner::GeneratePluginEvent(
  const WidgetCompositionEvent* aSrcCompositionEvent,
  WidgetCompositionEvent* aDistCompositionEvent)
{
#ifdef XP_WIN
  NPEvent newEvent;
  switch (aDistCompositionEvent->mMessage) {
    case eCompositionChange: {
      newEvent.event = WM_IME_COMPOSITION;
      newEvent.wParam = 0;
      if (aSrcCompositionEvent &&
          (aSrcCompositionEvent->mMessage == eCompositionCommit ||
           aSrcCompositionEvent->mMessage == eCompositionCommitAsIs)) {
        newEvent.lParam = GCS_RESULTSTR;
      } else {
        newEvent.lParam = GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE;
      }
      TextRangeArray* ranges = aDistCompositionEvent->mRanges;
      if (ranges && ranges->HasCaret()) {
        newEvent.lParam |= GCS_CURSORPOS;
      }
      break;
    }

    case eCompositionStart:
      newEvent.event = WM_IME_STARTCOMPOSITION;
      newEvent.wParam = 0;
      newEvent.lParam = 0;
      break;

    case eCompositionEnd:
      newEvent.event = WM_IME_ENDCOMPOSITION;
      newEvent.wParam = 0;
      newEvent.lParam = 0;
      break;

    default:
      return;
  }
  aDistCompositionEvent->mPluginEvent.Copy(newEvent);
#endif
}

// nsPluginDOMContextMenuListener class implementation

nsPluginDOMContextMenuListener::nsPluginDOMContextMenuListener(nsIContent* aContent)
{
  aContent->AddEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
}

nsPluginDOMContextMenuListener::~nsPluginDOMContextMenuListener()
{
}

NS_IMPL_ISUPPORTS(nsPluginDOMContextMenuListener,
                  nsIDOMEventListener)

NS_IMETHODIMP
nsPluginDOMContextMenuListener::HandleEvent(nsIDOMEvent* aEvent)
{
  aEvent->PreventDefault(); // consume event

  return NS_OK;
}

void nsPluginDOMContextMenuListener::Destroy(nsIContent* aContent)
{
  // Unregister context menu listener
  aContent->RemoveEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
}
