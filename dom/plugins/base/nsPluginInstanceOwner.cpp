/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_QT
#include <QWidget>
#include <QKeyEvent>
#if defined(MOZ_X11)
#if defined(Q_WS_X11)
#include <QX11Info>
#else
#include "gfxQtPlatform.h"
#endif
#endif
#undef slots
#endif

#ifdef MOZ_X11
#include <cairo-xlib.h>
#include "gfxXlibSurface.h"
/* X headers suck */
enum { XKeyPress = KeyPress };
#include "mozilla/X11Util.h"
using mozilla::DefaultXDisplay;
#endif

#include "nsPluginInstanceOwner.h"
#include "nsIRunnable.h"
#include "nsContentUtils.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsDisplayList.h"
#include "ImageLayers.h"
#include "SharedTextureImage.h"
#include "nsObjectFrame.h"
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
#include "nsAttrName.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIDOMDragEvent.h"
#include "nsIScrollableFrame.h"
#include "nsIDocShell.h"
#include "ImageContainer.h"
#include "nsIDOMHTMLCollection.h"
#include "GLContext.h"
#include "nsIContentInlines.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"

#include "nsContentCID.h"
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef XP_WIN
#include <wtypes.h>
#include <winuser.h>
#endif

#ifdef XP_MACOSX
#include <Carbon/Carbon.h>
#include "nsPluginUtilsOSX.h"
#endif

#if (MOZ_WIDGET_GTK == 2)
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "ANPBase.h"
#include "AndroidBridge.h"
#include "nsWindow.h"

static nsPluginInstanceOwner* sFullScreenInstance = nullptr;

using namespace mozilla::dom;

#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#endif

using namespace mozilla;
using namespace mozilla::layers;

// special class for handeling DOM context menu events because for
// some reason it starves other mouse events if implemented on the
// same class
class nsPluginDOMContextMenuListener : public nsIDOMEventListener
{
public:
  nsPluginDOMContextMenuListener(nsIContent* aContent);
  virtual ~nsPluginDOMContextMenuListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void Destroy(nsIContent* aContent);

  nsEventStatus ProcessEvent(const WidgetGUIEvent& anEvent)
  {
    return nsEventStatus_eConsumeNoDefault;
  }
};

class AsyncPaintWaitEvent : public nsRunnable
{
public:
  AsyncPaintWaitEvent(nsIContent* aContent, bool aFinished) :
    mContent(aContent), mFinished(aFinished)
  {
  }

  NS_IMETHOD Run()
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
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(mContent, false);
    // Run this event as soon as it's safe to do so, since listeners need to
    // receive it immediately
    mWaitingForPaint = nsContentUtils::AddScriptRunner(event);
  }
}

already_AddRefed<ImageContainer>
nsPluginInstanceOwner::GetImageContainer()
{
  if (!mInstance)
    return nullptr;

  nsRefPtr<ImageContainer> container;

#if MOZ_WIDGET_ANDROID
  // Right now we only draw with Gecko layers on Honeycomb and higher. See Paint()
  // for what we do on other versions.
  if (AndroidBridge::Bridge()->GetAPIVersion() < 11)
    return NULL;
  
  container = LayerManager::CreateImageContainer();

  ImageFormat format = ImageFormat::SHARED_TEXTURE;
  nsRefPtr<Image> img = container->CreateImage(&format, 1);

  SharedTextureImage::Data data;
  data.mHandle = mInstance->CreateSharedHandle();
  data.mShareType = mozilla::gl::SameProcess;
  data.mInverted = mInstance->Inverted();

  LayoutDeviceRect r = GetPluginRect();
  data.mSize = gfxIntSize(r.width, r.height);

  SharedTextureImage* pluginImage = static_cast<SharedTextureImage*>(img.get());
  pluginImage->SetData(data);

  container->SetCurrentImageInTransaction(img);

  float xResolution = mObjectFrame->PresContext()->GetRootPresContext()->PresShell()->GetXResolution();
  float yResolution = mObjectFrame->PresContext()->GetRootPresContext()->PresShell()->GetYResolution();
  ScreenSize screenSize = (r * LayoutDeviceToScreenScale(xResolution, yResolution)).Size();
  mInstance->NotifySize(nsIntSize(screenSize.width, screenSize.height));

  return container.forget();
#endif

  mInstance->GetImageContainer(getter_AddRefs(container));
  return container.forget();
}

void
nsPluginInstanceOwner::SetBackgroundUnknown()
{
  if (mInstance) {
    mInstance->SetBackgroundUnknown();
  }
}

already_AddRefed<gfxContext>
nsPluginInstanceOwner::BeginUpdateBackground(const nsIntRect& aRect)
{
  nsIntRect rect = aRect;
  nsRefPtr<gfxContext> ctx;
  if (mInstance &&
      NS_SUCCEEDED(mInstance->BeginUpdateBackground(&rect, getter_AddRefs(ctx)))) {
    return ctx.forget();
  }
  return nullptr;
}

void
nsPluginInstanceOwner::EndUpdateBackground(gfxContext* aContext,
                                           const nsIntRect& aRect)
{
  nsIntRect rect = aRect;
  if (mInstance) {
    mInstance->EndUpdateBackground(aContext, &rect);
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
{
  // create nsPluginNativeWindow object, it is derived from NPWindow
  // struct and allows to manipulate native window procedure
  nsCOMPtr<nsIPluginHost> pluginHostCOM = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  mPluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());  
  if (mPluginHost)
    mPluginHost->NewPluginNativeWindow(&mPluginWindow);
  else
    mPluginWindow = nullptr;

  mObjectFrame = nullptr;
  mContent = nullptr;
  mWidgetCreationComplete = false;
#ifdef XP_MACOSX
  memset(&mCGPluginPortCopy, 0, sizeof(NP_CGContext));
  mInCGPaintLevel = 0;
  mSentInitialTopLevelWindowEvent = false;
  mColorProfile = nullptr;
  mPluginPortChanged = false;
#endif
  mContentFocused = false;
  mWidgetVisible = true;
  mPluginWindowVisible = false;
  mPluginDocumentActiveState = true;
  mNumCachedAttrs = 0;
  mNumCachedParams = 0;
  mCachedAttrParamNames = nullptr;
  mCachedAttrParamValues = nullptr;
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
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  int32_t cnt;

  if (mWaitingForPaint) {
    // We don't care when the event is dispatched as long as it's "soon",
    // since whoever needs it will be waiting for it.
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(mContent, true);
    NS_DispatchToMainThread(event);
  }

  mObjectFrame = nullptr;

  for (cnt = 0; cnt < (mNumCachedAttrs + 1 + mNumCachedParams); cnt++) {
    if (mCachedAttrParamNames && mCachedAttrParamNames[cnt]) {
      NS_Free(mCachedAttrParamNames[cnt]);
      mCachedAttrParamNames[cnt] = nullptr;
    }

    if (mCachedAttrParamValues && mCachedAttrParamValues[cnt]) {
      NS_Free(mCachedAttrParamValues[cnt]);
      mCachedAttrParamValues[cnt] = nullptr;
    }
  }

  if (mCachedAttrParamNames) {
    NS_Free(mCachedAttrParamNames);
    mCachedAttrParamNames = nullptr;
  }

  if (mCachedAttrParamValues) {
    NS_Free(mCachedAttrParamValues);
    mCachedAttrParamValues = nullptr;
  }

  PLUG_DeletePluginNativeWindow(mPluginWindow);
  mPluginWindow = nullptr;

#ifdef MOZ_WIDGET_ANDROID
  RemovePluginView();
#endif

  if (mInstance) {
    mInstance->SetOwner(nullptr);
  }
}

NS_IMPL_ISUPPORTS5(nsPluginInstanceOwner,
                   nsIPluginInstanceOwner,
                   nsIPluginTagInfo,
                   nsIDOMEventListener,
                   nsIPrivacyTransitionObserver,
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
    nsCOMPtr<nsPIDOMWindow> domWindow = doc->GetWindow();
    if (domWindow) {
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

NS_IMETHODIMP nsPluginInstanceOwner::GetAttributes(uint16_t& n,
                                                   const char*const*& names,
                                                   const char*const*& values)
{
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  n = mNumCachedAttrs;
  names  = (const char **)mCachedAttrParamNames;
  values = (const char **)mCachedAttrParamValues;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetAttribute(const char* name, const char* *result)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(result);
  
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  *result = nullptr;

  for (int i = 0; i < mNumCachedAttrs; i++) {
    if (0 == PL_strcasecmp(mCachedAttrParamNames[i], name)) {
      *result = mCachedAttrParamValues[i];
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDOMElement(nsIDOMElement* *result)
{
  return CallQueryInterface(mContent, result);
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
                                            uint32_t aHeadersDataLen)
{
  NS_ENSURE_TRUE(mContent, NS_ERROR_NULL_POINTER);

  if (mContent->IsEditable()) {
    return NS_OK;
  }

  nsIDocument *doc = mContent->GetCurrentDoc();
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
  nsCOMPtr<nsISupports> container = presContext->GetContainer();
  NS_ENSURE_TRUE(container,NS_ERROR_FAILURE);
  nsCOMPtr<nsILinkHandler> lh = do_QueryInterface(container);
  NS_ENSURE_TRUE(lh, NS_ERROR_FAILURE);

  nsAutoString  unitarget;
  unitarget.AssignASCII(aTarget); // XXX could this be nonascii?

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

  rv = lh->OnLinkClick(mContent, uri, unitarget.get(), NullString(),
                       aPostStream, headersDataStream, true);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::ShowStatus(const char *aStatusMsg)
{
  nsresult  rv = NS_ERROR_FAILURE;
  
  rv = this->ShowStatus(NS_ConvertUTF8toUTF16(aStatusMsg).get());
  
  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::ShowStatus(const PRUnichar *aStatusMsg)
{
  nsresult  rv = NS_ERROR_FAILURE;

  if (!mObjectFrame) {
    return rv;
  }
  nsCOMPtr<nsISupports> cont = mObjectFrame->PresContext()->GetContainer();
  if (!cont) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(cont, &rv));
  if (NS_FAILED(rv) || !docShellItem) {
    return rv;
  }

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  rv = docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (NS_FAILED(rv) || !treeOwner) {
    return rv;
  }

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner, &rv));
  if (NS_FAILED(rv) || !browserChrome) {
    return rv;
  }
  rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, 
                                aStatusMsg);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDocument(nsIDocument* *aDocument)
{
  if (!aDocument)
    return NS_ERROR_NULL_POINTER;

  // XXX sXBL/XBL2 issue: current doc or owner doc?
  // But keep in mind bug 322414 comment 33
  NS_IF_ADDREF(*aDocument = mContent->OwnerDoc());
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRect(NPRect *invalidRect)
{
  // If our object frame has gone away, we won't be able to determine
  // up-to-date-ness, so just fire off the event.
  if (mWaitingForPaint && (!mObjectFrame || IsUpToDate())) {
    // We don't care when the event is dispatched as long as it's "soon",
    // since whoever needs it will be waiting for it.
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(mContent, true);
    NS_DispatchToMainThread(event);
    mWaitingForPaint = false;
  }

  if (!mObjectFrame || !invalidRect || !mWidgetVisible)
    return NS_ERROR_FAILURE;

#if defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID)
  // Each time an asynchronously-drawing plugin sends a new surface to display,
  // the image in the ImageContainer is updated and InvalidateRect is called.
  // There are different side effects for (sync) Android plugins.
  nsRefPtr<ImageContainer> container;
  mInstance->GetImageContainer(getter_AddRefs(container));
#endif

#ifndef XP_MACOSX
  // Windowed plugins should not be calling NPN_InvalidateRect, but
  // Silverlight does and expects it to "work"
  if (mWidget) {
    mWidget->Invalidate(nsIntRect(invalidRect->left, invalidRect->top,
                                  invalidRect->right - invalidRect->left,
                                  invalidRect->bottom - invalidRect->top));
    return NS_OK;
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
  mObjectFrame->InvalidateLayer(nsDisplayItem::TYPE_PLUGIN, &rect);
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRegion(NPRegion invalidRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP
nsPluginInstanceOwner::RedrawPlugin()
{
  if (mObjectFrame) {
    mObjectFrame->InvalidateLayer(nsDisplayItem::TYPE_PLUGIN);
  }
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetNetscapeWindow(void *value)
{
  if (!mObjectFrame) {
    NS_WARNING("plugin owner has no owner in getting doc's window handle");
    return NS_ERROR_FAILURE;
  }
  
#if defined(XP_WIN) || defined(XP_OS2)
  void** pvalue = (void**)value;
  nsViewManager* vm = mObjectFrame->PresContext()->GetPresShell()->GetViewManager();
  if (!vm)
    return NS_ERROR_FAILURE;
#if defined(XP_WIN)
  // This property is provided to allow a "windowless" plugin to determine the window it is drawing
  // in, so it can translate mouse coordinates it receives directly from the operating system
  // to coordinates relative to itself.
  
  // The original code (outside this #if) returns the document's window, which is OK if the window the "windowless" plugin
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
  if (mPluginWindow && mPluginWindow->type == NPWindowTypeDrawable) {
    // it turns out that flash also uses this window for determining focus, and is currently
    // unable to show a caret correctly if we return the enclosing window. Therefore for
    // now we only return the enclosing window when there is an actual offset which
    // would otherwise cause coordinates to be offset incorrectly. (i.e.
    // if the enclosing window if offset from the document window)
    //
    // fixing both the caret and ability to interact issues for a windowless control in a non document aligned windw
    // does not seem to be possible without a change to the flash plugin
    
    nsIWidget* win = mObjectFrame->GetNearestWidget();
    if (win) {
      nsView *view = nsView::GetViewFor(win);
      NS_ASSERTION(view, "No view for widget");
      nsPoint offset = view->GetOffsetTo(nullptr);
      
      if (offset.x || offset.y) {
        // in the case the two windows are offset from eachother, we do go ahead and return the correct enclosing window
        // so that mouse co-ordinates are not messed up.
        *pvalue = (void*)win->GetNativeData(NS_NATIVE_WINDOW);
        if (*pvalue)
          return NS_OK;
      }
    }
  }
#endif
  // simply return the topmost document window
  nsCOMPtr<nsIWidget> widget;
  vm->GetRootWidget(getter_AddRefs(widget));
  if (widget) {
    *pvalue = (void*)widget->GetNativeData(NS_NATIVE_WINDOW);
  } else {
    NS_ASSERTION(widget, "couldn't get doc's widget in getting doc's window handle");
  }

  return NS_OK;
#elif (defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_QT)) && defined(MOZ_X11)
  // X11 window managers want the toplevel window for WM_TRANSIENT_FOR.
  nsIWidget* win = mObjectFrame->GetNearestWidget();
  if (!win)
    return NS_ERROR_FAILURE;
  *static_cast<Window*>(value) = (long unsigned int)win->GetNativeData(NS_NATIVE_SHAREABLE_WINDOW);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
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

NPError nsPluginInstanceOwner::ShowNativeContextMenu(NPMenu* menu, void* event)
{
  if (!menu || !event)
    return NPERR_GENERIC_ERROR;

#ifdef XP_MACOSX
  if (GetEventModel() != NPEventModelCocoa)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  return NS_NPAPI_ShowCocoaContextMenu(static_cast<void*>(menu), mWidget,
                                       static_cast<NPCocoaEvent*>(event));
#else
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
#endif
}

NPBool nsPluginInstanceOwner::ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                           double *destX, double *destY, NPCoordinateSpace destSpace)
{
#ifdef XP_MACOSX
  if (!mWidget)
    return false;

  return NS_NPAPI_ConvertPointCocoa(mWidget->GetNativeData(NS_NATIVE_WIDGET),
                                    sourceX, sourceY, sourceSpace, destX, destY, destSpace);
#else
  // we should implement this for all platforms
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

  nsIAtom *atom = mContent->Tag();

  if (atom == nsGkAtoms::applet)
    *result = nsPluginTagType_Applet;
  else if (atom == nsGkAtoms::embed)
    *result = nsPluginTagType_Embed;
  else if (atom == nsGkAtoms::object)
    *result = nsPluginTagType_Object;

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetParameters(uint16_t& n, const char*const*& names, const char*const*& values)
{
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  n = mNumCachedParams;
  if (n) {
    names  = (const char **)(mCachedAttrParamNames + mNumCachedAttrs + 1);
    values = (const char **)(mCachedAttrParamValues + mNumCachedAttrs + 1);
  } else
    names = values = nullptr;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetParameter(const char* name, const char* *result)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(result);
  
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  *result = nullptr;

  for (int i = mNumCachedAttrs + 1; i < (mNumCachedParams + 1 + mNumCachedAttrs); i++) {
    if (0 == PL_strcasecmp(mCachedAttrParamNames[i], name)) {
      *result = mCachedAttrParamValues[i];
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDocumentBase(const char* *result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsresult rv = NS_OK;
  if (mDocumentBase.IsEmpty()) {
    if (!mObjectFrame) {
      *result = nullptr;
      return NS_ERROR_FAILURE;
    }

    nsIDocument* doc = mContent->OwnerDoc();
    NS_ASSERTION(doc, "Must have an owner doc");
    rv = doc->GetDocBaseURI()->GetSpec(mDocumentBase);
  }
  if (NS_SUCCEEDED(rv))
    *result = ToNewCString(mDocumentBase);
  return rv;
}

static nsDataHashtable<nsDepCharHashKey, const char *> * gCharsetMap;
typedef struct {
    char mozName[16];
    char javaName[12];
} moz2javaCharset;

/* XXX If you add any strings longer than
 *  {"x-mac-cyrillic",  "MacCyrillic"},
 *  {"x-mac-ukrainian", "MacUkraine"},
 * to the following array then you MUST update the
 * sizes of the arrays in the moz2javaCharset struct
 */

static const moz2javaCharset charsets[] = 
{
    {"windows-1252",    "Cp1252"},
    {"IBM850",          "Cp850"},
    {"IBM852",          "Cp852"},
    {"IBM855",          "Cp855"},
    {"IBM857",          "Cp857"},
    {"IBM828",          "Cp862"},
    {"IBM866",          "Cp866"},
    {"windows-1250",    "Cp1250"},
    {"windows-1251",    "Cp1251"},
    {"windows-1253",    "Cp1253"},
    {"windows-1254",    "Cp1254"},
    {"windows-1255",    "Cp1255"},
    {"windows-1256",    "Cp1256"},
    {"windows-1257",    "Cp1257"},
    {"windows-1258",    "Cp1258"},
    {"EUC-JP",          "EUC_JP"},
    {"EUC-KR",          "MS949"},
    {"x-euc-tw",        "EUC_TW"},
    {"gb18030",         "GB18030"},
    {"gbk",             "GBK"},
    {"ISO-2022-JP",     "ISO2022JP"},
    {"ISO-2022-KR",     "ISO2022KR"},
    {"ISO-8859-2",      "ISO8859_2"},
    {"ISO-8859-3",      "ISO8859_3"},
    {"ISO-8859-4",      "ISO8859_4"},
    {"ISO-8859-5",      "ISO8859_5"},
    {"ISO-8859-6",      "ISO8859_6"},
    {"ISO-8859-7",      "ISO8859_7"},
    {"ISO-8859-8",      "ISO8859_8"},
    {"ISO-8859-9",      "ISO8859_9"},
    {"ISO-8859-13",     "ISO8859_13"},
    {"x-johab",         "Johab"},
    {"KOI8-R",          "KOI8_R"},
    {"TIS-620",         "MS874"},
    {"x-mac-arabic",    "MacArabic"},
    {"x-mac-croatian",  "MacCroatia"},
    {"x-mac-cyrillic",  "MacCyrillic"},
    {"x-mac-greek",     "MacGreek"},
    {"x-mac-hebrew",    "MacHebrew"},
    {"x-mac-icelandic", "MacIceland"},
    {"macintosh",       "MacRoman"},
    {"x-mac-romanian",  "MacRomania"},
    {"x-mac-ukrainian", "MacUkraine"},
    {"Shift_JIS",       "SJIS"},
    {"TIS-620",         "TIS620"}
};

NS_IMETHODIMP nsPluginInstanceOwner::GetDocumentEncoding(const char* *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nullptr;

  nsresult rv;
  // XXX sXBL/XBL2 issue: current doc or owner doc?
  nsCOMPtr<nsIDocument> doc;
  rv = GetDocument(getter_AddRefs(doc));
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get document");
  if (NS_FAILED(rv))
    return rv;

  const nsCString &charset = doc->GetDocumentCharacterSet();

  if (charset.IsEmpty())
    return NS_OK;

  // common charsets and those not requiring conversion first
  if (charset.EqualsLiteral("us-ascii")) {
    *result = PL_strdup("US_ASCII");
  } else if (charset.EqualsLiteral("ISO-8859-1") ||
      !nsCRT::strncmp(charset.get(), "UTF", 3)) {
    *result = ToNewCString(charset);
  } else {
    if (!gCharsetMap) {
      const int NUM_CHARSETS = sizeof(charsets) / sizeof(moz2javaCharset);
      gCharsetMap = new nsDataHashtable<nsDepCharHashKey, const char*>(NUM_CHARSETS);
      if (!gCharsetMap)
        return NS_ERROR_OUT_OF_MEMORY;
      for (uint16_t i = 0; i < NUM_CHARSETS; i++) {
        gCharsetMap->Put(charsets[i].mozName, charsets[i].javaName);
      }
    }
    // if found mapping, return it; otherwise return original charset
    const char *mapping;
    *result = gCharsetMap->Get(charset.get(), &mapping) ? PL_strdup(mapping) :
                                                          ToNewCString(charset);
  }

  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetAlignment(const char* *result)
{
  return GetAttribute("ALIGN", result);
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetWidth(uint32_t *result)
{
  NS_ENSURE_ARG_POINTER(result);

  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  *result = mPluginWindow->width;

  return NS_OK;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetHeight(uint32_t *result)
{
  NS_ENSURE_ARG_POINTER(result);

  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  *result = mPluginWindow->height;

  return NS_OK;
}

  
NS_IMETHODIMP nsPluginInstanceOwner::GetBorderVertSpace(uint32_t *result)
{
  nsresult    rv;
  const char  *vspace;

  rv = GetAttribute("VSPACE", &vspace);

  if (NS_OK == rv) {
    if (*result != 0)
      *result = (uint32_t)atol(vspace);
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetBorderHorizSpace(uint32_t *result)
{
  nsresult    rv;
  const char  *hspace;

  rv = GetAttribute("HSPACE", &hspace);

  if (NS_OK == rv) {
    if (*result != 0)
      *result = (uint32_t)atol(hspace);
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}

// Cache the attributes and/or parameters of our tag into a single set
// of arrays to be compatible with Netscape 4.x. The attributes go first,
// followed by a PARAM/null and then any PARAM tags. Also, hold the
// cached array around for the duration of the life of the instance
// because Netscape 4.x did. See bug 111008.
nsresult nsPluginInstanceOwner::EnsureCachedAttrParamArrays()
{
  if (mCachedAttrParamValues)
    return NS_OK;

  NS_PRECONDITION(((mNumCachedAttrs + mNumCachedParams) == 0) &&
                    !mCachedAttrParamNames,
                  "re-cache of attrs/params not implemented! use the DOM "
                    "node directy instead");

  // Convert to a 16-bit count. Subtract 2 in case we add an extra
  // "src" or "wmode" entry below.
  uint32_t cattrs = mContent->GetAttrCount();
  if (cattrs < 0x0000FFFD) {
    mNumCachedAttrs = static_cast<uint16_t>(cattrs);
  } else {
    mNumCachedAttrs = 0xFFFD;
  }

  // Check if we are java for special codebase handling
  const char* mime = nullptr;
  bool isJava = NS_SUCCEEDED(mInstance->GetMIMEType(&mime)) && mime &&
                nsPluginHost::IsJavaMIMEType(mime);

  // now, we need to find all the PARAM tags that are children of us
  // however, be careful not to include any PARAMs that don't have us
  // as a direct parent. For nested object (or applet) tags, be sure
  // to only round up the param tags that coorespond with THIS
  // instance. And also, weed out any bogus tags that may get in the
  // way, see bug 39609. Then, with any param tag that meet our
  // qualification, temporarly cache them in an nsCOMArray until
  // we can figure out what size to make our fixed char* array.
  nsCOMArray<nsIDOMElement> ourParams;

  // Get all dependent PARAM tags, even if they are not direct children.
  nsCOMPtr<nsIDOMElement> mydomElement = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(mydomElement, NS_ERROR_NO_INTERFACE);

  // Making DOM method calls can cause our frame to go away.
  nsCOMPtr<nsIPluginInstanceOwner> kungFuDeathGrip(this);

  nsCOMPtr<nsIDOMHTMLCollection> allParams;
  NS_NAMED_LITERAL_STRING(xhtml_ns, "http://www.w3.org/1999/xhtml");
  mydomElement->GetElementsByTagNameNS(xhtml_ns, NS_LITERAL_STRING("param"),
                                       getter_AddRefs(allParams));
  if (allParams) {
    uint32_t numAllParams; 
    allParams->GetLength(&numAllParams);
    for (uint32_t i = 0; i < numAllParams; i++) {
      nsCOMPtr<nsIDOMNode> pnode;
      allParams->Item(i, getter_AddRefs(pnode));
      nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(pnode);
      if (domelement) {
        // Ignore params without a name attribute.
        nsAutoString name;
        domelement->GetAttribute(NS_LITERAL_STRING("name"), name);
        if (!name.IsEmpty()) {
          // Find the first object or applet parent.
          nsCOMPtr<nsIDOMNode> parent;
          nsCOMPtr<nsIDOMHTMLObjectElement> domobject;
          nsCOMPtr<nsIDOMHTMLAppletElement> domapplet;
          pnode->GetParentNode(getter_AddRefs(parent));
          while (!(domobject || domapplet) && parent) {
            domobject = do_QueryInterface(parent);
            domapplet = do_QueryInterface(parent);
            nsCOMPtr<nsIDOMNode> temp;
            parent->GetParentNode(getter_AddRefs(temp));
            parent = temp;
          }
          if (domapplet || domobject) {
            if (domapplet) {
              parent = do_QueryInterface(domapplet);
            }
            else {
              parent = do_QueryInterface(domobject);
            }
            nsCOMPtr<nsIDOMNode> mydomNode = do_QueryInterface(mydomElement);
            if (parent == mydomNode) {
              ourParams.AppendObject(domelement);
            }
          }
        }
      }
    }
  }

  // Convert to a 16-bit count.
  uint32_t cparams = ourParams.Count();
  if (cparams < 0x0000FFFF) {
    mNumCachedParams = static_cast<uint16_t>(cparams);
  } else {
    mNumCachedParams = 0xFFFF;
  }

  uint16_t numRealAttrs = mNumCachedAttrs;

  // Some plugins were never written to understand the "data" attribute of the OBJECT tag.
  // Real and WMP will not play unless they find a "src" attribute, see bug 152334.
  // Nav 4.x would simply replace the "data" with "src". Because some plugins correctly
  // look for "data", lets instead copy the "data" attribute and add another entry
  // to the bottom of the array if there isn't already a "src" specified.
  nsAutoString data;
  if (mContent->Tag() == nsGkAtoms::object &&
      !mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::src) &&
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, data) &&
      !data.IsEmpty()) {
    mNumCachedAttrs++;
  }

  // "plugins.force.wmode" preference is forcing wmode type for plugins
  // possible values - "opaque", "transparent", "windowed"
  nsAdoptingCString wmodeType = Preferences::GetCString("plugins.force.wmode");
  if (!wmodeType.IsEmpty()) {
    mNumCachedAttrs++;
  }

  // (Bug 738396) java has quirks in its codebase parsing, pass the
  // absolute codebase URI as content sees it.
  bool addCodebase = false;
  nsAutoCString codebaseStr;
  if (isJava) {
    nsCOMPtr<nsIObjectLoadingContent> objlc = do_QueryInterface(mContent);
    NS_ENSURE_TRUE(objlc, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsIURI> codebaseURI;
    nsresult rv = objlc->GetBaseURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_SUCCESS(rv, rv);
    codebaseURI->GetSpec(codebaseStr);
    if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::codebase)) {
      mNumCachedAttrs++;
      addCodebase = true;
    }
  }

  mCachedAttrParamNames  = (char**)NS_Alloc(sizeof(char*) * (mNumCachedAttrs + 1 + mNumCachedParams));
  NS_ENSURE_TRUE(mCachedAttrParamNames,  NS_ERROR_OUT_OF_MEMORY);
  mCachedAttrParamValues = (char**)NS_Alloc(sizeof(char*) * (mNumCachedAttrs + 1 + mNumCachedParams));
  NS_ENSURE_TRUE(mCachedAttrParamValues, NS_ERROR_OUT_OF_MEMORY);

  // Some plugins (eg Flash, see bug 234675.) are actually sensitive to the
  // attribute order.  So we want to make sure we give the plugin the
  // attributes in the order they came in in the source, to be compatible with
  // other browsers.  Now in HTML, the storage order is the reverse of the
  // source order, while in XML and XHTML it's the same as the source order
  // (see the AddAttributes functions in the HTML and XML content sinks).
  int32_t start, end, increment;
  if (mContent->IsHTML() &&
      mContent->IsInHTMLDocument()) {
    // HTML.  Walk attributes in reverse order.
    start = numRealAttrs - 1;
    end = -1;
    increment = -1;
  } else {
    // XHTML or XML.  Walk attributes in forward order.
    start = 0;
    end = numRealAttrs;
    increment = 1;
  }

  // Set to the next slot to fill in name and value cache arrays.
  uint32_t nextAttrParamIndex = 0;

  // Whether or not we force the wmode below while traversing
  // the name/value pairs.
  bool wmodeSet = false;

  // Add attribute name/value pairs.
  for (int32_t index = start; index != end; index += increment) {
    const nsAttrName* attrName = mContent->GetAttrNameAt(index);
    nsIAtom* atom = attrName->LocalName();
    nsAutoString value;
    mContent->GetAttr(attrName->NamespaceID(), atom, value);
    nsAutoString name;
    atom->ToString(name);

    FixUpURLS(name, value);

    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(name);
    if (!wmodeType.IsEmpty() &&
        0 == PL_strcasecmp(mCachedAttrParamNames[nextAttrParamIndex], "wmode")) {
      mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_ConvertUTF8toUTF16(wmodeType));

      if (!wmodeSet) {
        // We allocated space to add a wmode attr, but we don't need it now.
        mNumCachedAttrs--;
        wmodeSet = true;
      }
    } else if (isJava && 0 == PL_strcasecmp(mCachedAttrParamNames[nextAttrParamIndex], "codebase")) {
      mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_ConvertUTF8toUTF16(codebaseStr));
    } else {
      mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(value);
    }
    nextAttrParamIndex++;
  }

  // Potentially add CODEBASE attribute
  if (addCodebase) {
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(NS_LITERAL_STRING("codebase"));
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_ConvertUTF8toUTF16(codebaseStr));
    nextAttrParamIndex++;
  }

  // Potentially add WMODE attribute.
  if (!wmodeType.IsEmpty() && !wmodeSet) {
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(NS_LITERAL_STRING("wmode"));
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_ConvertUTF8toUTF16(wmodeType));
    nextAttrParamIndex++;
  }

  // Potentially add SRC attribute.
  if (!data.IsEmpty()) {
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(NS_LITERAL_STRING("SRC"));
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(data);
    nextAttrParamIndex++;
  }

  // Add PARAM and null separator.
  mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(NS_LITERAL_STRING("PARAM"));
#ifdef MOZ_WIDGET_ANDROID
  // Flash expects an empty string on android
  mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_LITERAL_STRING(""));
#else
  mCachedAttrParamValues[nextAttrParamIndex] = nullptr;
#endif
  nextAttrParamIndex++;

  // Add PARAM name/value pairs.

  // We may decrement mNumCachedParams below
  uint16_t totalParams = mNumCachedParams;
  for (uint16_t i = 0; i < totalParams; i++) {
    nsIDOMElement* param = ourParams.ObjectAt(i);
    if (!param) {
      continue;
    }

    nsAutoString name;
    nsAutoString value;
    param->GetAttribute(NS_LITERAL_STRING("name"), name); // check for empty done above
    param->GetAttribute(NS_LITERAL_STRING("value"), value);

    FixUpURLS(name, value);

    /*
     * According to the HTML 4.01 spec, at
     * http://www.w3.org/TR/html4/types.html#type-cdata
     * ''User agents may ignore leading and trailing
     * white space in CDATA attribute values (e.g., "
     * myval " may be interpreted as "myval"). Authors
     * should not declare attribute values with
     * leading or trailing white space.''
     * However, do not trim consecutive spaces as in bug 122119
     */
    name.Trim(" \n\r\t\b", true, true, false);
    value.Trim(" \n\r\t\b", true, true, false);
    if (isJava && name.EqualsIgnoreCase("codebase")) {
      // We inserted normalized codebase above, don't include other versions in
      // params
      mNumCachedParams--;
      continue;
    }
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(name);
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(value);
    nextAttrParamIndex++;
  }

  return NS_OK;
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

nsresult nsPluginInstanceOwner::ContentsScaleFactorChanged(double aContentsScaleFactor)
{
  if (!mInstance) {
    return NS_ERROR_NULL_POINTER;
  }
  return mInstance->ContentsScaleFactorChanged(aContentsScaleFactor);
}

NPEventModel nsPluginInstanceOwner::GetEventModel()
{
  return mEventModel;
}

#define DEFAULT_REFRESH_RATE 20 // 50 FPS

nsCOMPtr<nsITimer>                *nsPluginInstanceOwner::sCATimer = NULL;
nsTArray<nsPluginInstanceOwner*>  *nsPluginInstanceOwner::sCARefreshListeners = NULL;

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
  if (NS_SUCCEEDED(mInstance->GetMIMEType(&mime)) && mime) {
    if (strcmp(mime, "application/x-shockwave-flash") == 0) {
      return;
    }
  }

  if (!sCARefreshListeners) {
    sCARefreshListeners = new nsTArray<nsPluginInstanceOwner*>();
    if (!sCARefreshListeners) {
      return;
    }
  }

  if (sCARefreshListeners->Contains(this)) {
    return;
  }

  sCARefreshListeners->AppendElement(this);

  if (!sCATimer) {
    sCATimer = new nsCOMPtr<nsITimer>();
    if (!sCATimer) {
      return;
    }
  }

  if (sCARefreshListeners->Length() == 1) {
    *sCATimer = do_CreateInstance("@mozilla.org/timer;1");
    (*sCATimer)->InitWithFuncCallback(CARefresh, NULL, 
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
      sCATimer = NULL;
    }
    delete sCARefreshListeners;
    sCARefreshListeners = NULL;
  }
}

void nsPluginInstanceOwner::RenderCoreAnimation(CGContextRef aCGContext,
                                                int aWidth, int aHeight)
{
  if (aWidth == 0 || aHeight == 0)
    return;

  if (!mCARenderer) {
    mCARenderer = new nsCARenderer();
  }

  // aWidth and aHeight are in "display pixels".  In non-HiDPI modes
  // "display pixels" are device pixels.  But in HiDPI modes each
  // display pixel corresponds to more than one device pixel.
  double scaleFactor = 1.0;
  GetContentsScaleFactor(&scaleFactor);

  if (!mIOSurface ||
      (mIOSurface->GetWidth() != (size_t)aWidth ||
       mIOSurface->GetHeight() != (size_t)aHeight ||
       mIOSurface->GetContentsScaleFactor() != scaleFactor)) {
    mIOSurface = nullptr;

    // If the renderer is backed by an IOSurface, resize it as required.
    mIOSurface = MacIOSurface::CreateIOSurface(aWidth, aHeight, scaleFactor);
    if (mIOSurface) {
      RefPtr<MacIOSurface> attachSurface = MacIOSurface::LookupSurface(
                                              mIOSurface->GetIOSurfaceID(),
                                              scaleFactor);
      if (attachSurface) {
        mCARenderer->AttachIOSurface(attachSurface);
      } else {
        NS_ERROR("IOSurface attachment failed");
        mIOSurface = nullptr;
      }
    }
  }

  if (!mColorProfile) {
    mColorProfile = CreateSystemColorSpace();
  }

  if (mCARenderer->isInit() == false) {
    void *caLayer = NULL;
    nsresult rv = mInstance->GetValueFromPlugin(NPPVpluginCoreAnimationLayer, &caLayer);
    if (NS_FAILED(rv) || !caLayer) {
      return;
    }

    // We don't run Flash in-process so we can unconditionally disallow
    // the offliner renderer.
    mCARenderer->SetupRenderer(caLayer, aWidth, aHeight, scaleFactor,
                               DISALLOW_OFFLINE_RENDERER);

    // Setting up the CALayer requires resetting the painting otherwise we
    // get garbage for the first few frames.
    FixUpPluginWindow(ePluginPaintDisable);
    FixUpPluginWindow(ePluginPaintEnable);
  }

  CGImageRef caImage = NULL;
  nsresult rt = mCARenderer->Render(aWidth, aHeight, scaleFactor, &caImage);
  if (rt == NS_OK && mIOSurface && mColorProfile) {
    nsCARenderer::DrawSurfaceToCGContext(aCGContext, mIOSurface, mColorProfile,
                                         0, 0, aWidth, aHeight);
  } else if (rt == NS_OK && caImage != NULL) {
    // Significant speed up by resetting the scaling
    ::CGContextSetInterpolationQuality(aCGContext, kCGInterpolationNone );
    ::CGContextTranslateCTM(aCGContext, 0, (double) aHeight * scaleFactor);
    ::CGContextScaleCTM(aCGContext, scaleFactor, -scaleFactor);

    ::CGContextDrawImage(aCGContext, CGRectMake(0,0,aWidth,aHeight), caImage);
  } else {
    NS_NOTREACHED("nsCARenderer::Render failure");
  }
}

void* nsPluginInstanceOwner::GetPluginPortCopy()
{
  if (GetDrawingModel() == NPDrawingModelCoreGraphics || 
      GetDrawingModel() == NPDrawingModelCoreAnimation ||
      GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation)
    return &mCGPluginPortCopy;
  return nullptr;
}
  
// Currently (on OS X in Cocoa widgets) any changes made as a result of
// calling GetPluginPortFromWidget() are immediately reflected in the NPWindow
// structure that has been passed to the plugin via SetWindow().  This is
// because calls to nsChildView::GetNativeData(NS_NATIVE_PLUGIN_PORT_CG)
// always return a pointer to the same internal (private) object, but may
// make changes inside that object.  All calls to GetPluginPortFromWidget() made while
// the plugin is active (i.e. excluding those made at our initialization)
// need to take this into account.  The easiest way to do so is to replace
// them with calls to SetPluginPortAndDetectChange().  This method keeps track
// of when calls to GetPluginPortFromWidget() result in changes, and sets a flag to make
// sure SetWindow() gets called the next time through FixUpPluginWindow(), so
// that the plugin is notified of these changes.
void* nsPluginInstanceOwner::SetPluginPortAndDetectChange()
{
  if (!mPluginWindow)
    return nullptr;
  void* pluginPort = GetPluginPortFromWidget();
  if (!pluginPort)
    return nullptr;
  mPluginWindow->window = pluginPort;

  return mPluginWindow->window;
}

void nsPluginInstanceOwner::BeginCGPaint()
{
  ++mInCGPaintLevel;
}

void nsPluginInstanceOwner::EndCGPaint()
{
  --mInCGPaintLevel;
  NS_ASSERTION(mInCGPaintLevel >= 0, "Mismatched call to nsPluginInstanceOwner::EndCGPaint()!");
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
  nsRect displayPort;
  while (f) {
    if (f->GetContent() && nsLayoutUtils::GetDisplayPort(f->GetContent(), &displayPort))
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
        offset += docOffset.ConvertAppUnits(currAPD, apd);
        docOffset.x = docOffset.y = 0;
      }
      currAPD = newAPD;
      docOffset += newOffset;
    }
  }

  offset += docOffset.ConvertAppUnits(currAPD, apd);

  return offset;
}

LayoutDeviceRect nsPluginInstanceOwner::GetPluginRect()
{
  // Get the offset of the content relative to the page
  nsRect bounds = mObjectFrame->GetContentRectRelativeToSelf() + GetOffsetRootContent(mObjectFrame);
  LayoutDeviceIntRect rect = LayoutDeviceIntRect::FromAppUnitsToNearest(bounds, mObjectFrame->PresContext()->AppUnitsPerDevPixel());
  return LayoutDeviceRect(rect);
}

bool nsPluginInstanceOwner::AddPluginView(const LayoutDeviceRect& aRect /* = LayoutDeviceRect(0, 0, 0, 0) */)
{
  if (!mJavaView) {
    mJavaView = mInstance->GetJavaSurface();
  
    if (!mJavaView)
      return false;

    mJavaView = (void*)AndroidBridge::GetJNIEnv()->NewGlobalRef((jobject)mJavaView);
  }

  if (AndroidBridge::Bridge())
    AndroidBridge::Bridge()->AddPluginView((jobject)mJavaView, aRect, mFullScreen);

  if (mFullScreen)
    sFullScreenInstance = this;

  return true;
}

void nsPluginInstanceOwner::RemovePluginView()
{
  if (!mInstance || !mJavaView)
    return;

  if (AndroidBridge::Bridge())
    AndroidBridge::Bridge()->RemovePluginView((jobject)mJavaView, mFullScreen);

  AndroidBridge::GetJNIEnv()->DeleteGlobalRef((jobject)mJavaView);
  mJavaView = nullptr;

  if (mFullScreen)
    sFullScreenInstance = nullptr;
}

void nsPluginInstanceOwner::GetVideos(nsTArray<nsNPAPIPluginInstance::VideoInfo*>& aVideos)
{
  if (!mInstance)
    return;

  mInstance->GetVideos(aVideos);
}

already_AddRefed<ImageContainer> nsPluginInstanceOwner::GetImageContainerForVideo(nsNPAPIPluginInstance::VideoInfo* aVideoInfo)
{
  nsRefPtr<ImageContainer> container = LayerManager::CreateImageContainer();

  ImageFormat format = ImageFormat::SHARED_TEXTURE;
  nsRefPtr<Image> img = container->CreateImage(&format, 1);

  SharedTextureImage::Data data;

  data.mShareType = gl::SameProcess;
  data.mHandle = mInstance->GLContext()->CreateSharedHandle(data.mShareType,
                                                            aVideoInfo->mSurfaceTexture,
                                                            gl::SurfaceTexture);

  // The logic below for Honeycomb is just a guess, but seems to work. We don't have a separate
  // inverted flag for video.
  data.mInverted = AndroidBridge::Bridge()->IsHoneycomb() ? true : mInstance->Inverted();
  data.mSize = gfxIntSize(aVideoInfo->mDimensions.width, aVideoInfo->mDimensions.height);

  SharedTextureImage* pluginImage = static_cast<SharedTextureImage*>(img.get());
  pluginImage->SetData(data);
  container->SetCurrentImageInTransaction(img);

  return container.forget();
}

void nsPluginInstanceOwner::Invalidate() {
  NPRect rect;
  rect.left = rect.top = 0;
  rect.right = mPluginWindow->width;
  rect.bottom = mPluginWindow->height;
  InvalidateRect(&rect);
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
  JNIEnv* env = AndroidBridge::GetJNIEnv();

  if (env && sFullScreenInstance && sFullScreenInstance->mInstance &&
      env->IsSameObject(view, (jobject)sFullScreenInstance->mInstance->GetJavaSurface())) {
    sFullScreenInstance->ExitFullScreen();
  } 
}

#endif

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

  WidgetEvent* theEvent = aFocusEvent->GetInternalNSEvent();
  if (theEvent) {
    // we only care about the message in ProcessEvent
    WidgetGUIEvent focusEvent(theEvent->mFlags.mIsTrusted, theEvent->message,
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
      aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
    if (keyEvent && keyEvent->eventStructType == NS_KEY_EVENT) {
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
  if (mObjectFrame && mPluginWindow &&
      mPluginWindow->type == NPWindowTypeDrawable) {
    
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(mContent);
      fm->SetFocus(elem, 0);
    }
  }

  WidgetMouseEvent* mouseEvent =
    aMouseEvent->GetInternalNSEvent()->AsMouseEvent();
  if (mouseEvent && mouseEvent->eventStructType == NS_MOUSE_EVENT) {
    mLastMouseDownButtonType = mouseEvent->button;
    nsEventStatus rv = ProcessEvent(*mouseEvent);
    if (nsEventStatus_eConsumeNoDefault == rv) {
      return aMouseEvent->PreventDefault(); // consume event
    }
  }
  
  return NS_OK;
}

nsresult nsPluginInstanceOwner::DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent)
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
    aMouseEvent->GetInternalNSEvent()->AsMouseEvent();
  if (mouseEvent && mouseEvent->eventStructType == NS_MOUSE_EVENT) {
    nsEventStatus rv = ProcessEvent(*mouseEvent);
    if (nsEventStatus_eConsumeNoDefault == rv) {
      aMouseEvent->PreventDefault();
      aMouseEvent->StopPropagation();
    }
    if (mouseEvent->message == NS_MOUSE_BUTTON_UP) {
      mLastMouseDownButtonType = -1;
    }
  }
  return NS_OK;
}

nsresult
nsPluginInstanceOwner::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(mInstance, "Should have a valid plugin instance or not receive events.");

  nsAutoString eventType;
  aEvent->GetType(eventType);
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
    // Don't send a mouse-up event to the plugin if its button type doesn't
    // match that of the preceding mouse-down event (if any).  This kind of
    // mismatch can happen if the previous mouse-down event was sent to a DOM
    // element above the plugin, the mouse is still above the plugin, and the
    // mouse-down event caused the element to disappear.  See bug 627649 and
    // bug 909678.
    WidgetMouseEvent* mouseEvent = aEvent->GetInternalNSEvent()->AsMouseEvent();
    if (mouseEvent &&
        static_cast<int>(mouseEvent->button) != mLastMouseDownButtonType) {
      aEvent->PreventDefault();
      return NS_OK;
    }
    return DispatchMouseToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("mousemove") ||
      eventType.EqualsLiteral("click") ||
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

  nsCOMPtr<nsIDOMDragEvent> dragEvent(do_QueryInterface(aEvent));
  if (dragEvent && mInstance) {
    WidgetEvent* ievent = aEvent->GetInternalNSEvent();
    if ((ievent && ievent->mFlags.mIsTrusted) &&
         ievent->message != NS_DRAGDROP_ENTER && ievent->message != NS_DRAGDROP_OVER) {
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

nsEventStatus nsPluginInstanceOwner::ProcessEvent(const WidgetGUIEvent& anEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (!mInstance || !mObjectFrame)   // if mInstance is null, we shouldn't be here
    return nsEventStatus_eIgnore;

#ifdef XP_MACOSX
  if (!mWidget)
    return nsEventStatus_eIgnore;

  // we never care about synthesized mouse enter
  if (anEvent.message == NS_MOUSE_ENTER_SYNTH)
    return nsEventStatus_eIgnore;

  nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
  if (!pluginWidget || NS_FAILED(pluginWidget->StartDrawPlugin()))
    return nsEventStatus_eIgnore;

  NPEventModel eventModel = GetEventModel();

  // If we have to synthesize an event we'll use one of these.
  NPCocoaEvent synthCocoaEvent;
  void* event = anEvent.pluginEvent;
  nsPoint pt =
  nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mObjectFrame) -
  mObjectFrame->GetContentRectRelativeToSelf().TopLeft();
  nsPresContext* presContext = mObjectFrame->PresContext();
  // Plugin event coordinates need to be translated from device pixels
  // into "display pixels" in HiDPI modes.
  double scaleFactor = 1.0;
  GetContentsScaleFactor(&scaleFactor);
  size_t intScaleFactor = ceil(scaleFactor);
  nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x) / intScaleFactor,
                  presContext->AppUnitsToDevPixels(pt.y) / intScaleFactor);

  if (!event) {
    InitializeNPCocoaEvent(&synthCocoaEvent);
    switch (anEvent.message) {
      case NS_MOUSE_MOVE:
      {
        // Ignore mouse-moved events that happen as part of a dragging
        // operation that started over another frame.  See bug 525078.
        nsRefPtr<nsFrameSelection> frameselection = mObjectFrame->GetFrameSelection();
        if (!frameselection->GetMouseDownState() ||
          (nsIPresShell::GetCapturingContent() == mObjectFrame->GetContent())) {
          synthCocoaEvent.type = NPCocoaEventMouseMoved;
          synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
          synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
          event = &synthCocoaEvent;
        }
      }
        break;
      case NS_MOUSE_BUTTON_DOWN:
        synthCocoaEvent.type = NPCocoaEventMouseDown;
        synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
        synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
        event = &synthCocoaEvent;
        break;
      case NS_MOUSE_BUTTON_UP:
        // If we're in a dragging operation that started over another frame,
        // convert it into a mouse-entered event (in the Cocoa Event Model).
        // See bug 525078.
        if (anEvent.AsMouseEvent()->button == WidgetMouseEvent::eLeftButton &&
            (nsIPresShell::GetCapturingContent() != mObjectFrame->GetContent())) {
          synthCocoaEvent.type = NPCocoaEventMouseEntered;
          synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
          synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
          event = &synthCocoaEvent;
        } else {
          synthCocoaEvent.type = NPCocoaEventMouseUp;
          synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
          synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
          event = &synthCocoaEvent;
        }
        break;
      default:
        break;
    }

    // If we still don't have an event, bail.
    if (!event) {
      pluginWidget->EndDrawPlugin();
      return nsEventStatus_eIgnore;
    }
  }

  int16_t response = kNPEventNotHandled;
  void* window = FixUpPluginWindow(ePluginPaintEnable);
  if (window || (eventModel == NPEventModelCocoa)) {
    mInstance->HandleEvent(event, &response, NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
  }

  if (eventModel == NPEventModelCocoa && response == kNPEventStartIME) {
    pluginWidget->StartComplexTextInputForCurrentEvent();
  }

  if ((response == kNPEventHandled || response == kNPEventStartIME) &&
      !(anEvent.message == NS_MOUSE_BUTTON_DOWN &&
        anEvent.AsMouseEvent()->button == WidgetMouseEvent::eLeftButton &&
        !mContentFocused)) {
    rv = nsEventStatus_eConsumeNoDefault;
  }

  pluginWidget->EndDrawPlugin();
#endif

#ifdef XP_WIN
  // this code supports windowless plugins
  NPEvent *pPluginEvent = (NPEvent*)anEvent.pluginEvent;
  // we can get synthetic events from the nsEventStateManager... these
  // have no pluginEvent
  NPEvent pluginEvent;
  if (anEvent.eventStructType == NS_MOUSE_EVENT) {
    if (!pPluginEvent) {
      // XXX Should extend this list to synthesize events for more event
      // types
      pluginEvent.event = 0;
      const WidgetMouseEvent* mouseEvent = anEvent.AsMouseEvent();
      switch (anEvent.message) {
      case NS_MOUSE_MOVE:
        pluginEvent.event = WM_MOUSEMOVE;
        break;
      case NS_MOUSE_BUTTON_DOWN: {
        static const int downMsgs[] =
          { WM_LBUTTONDOWN, WM_MBUTTONDOWN, WM_RBUTTONDOWN };
        static const int dblClickMsgs[] =
          { WM_LBUTTONDBLCLK, WM_MBUTTONDBLCLK, WM_RBUTTONDBLCLK };
        if (mouseEvent->clickCount == 2) {
          pluginEvent.event = dblClickMsgs[mouseEvent->button];
        } else {
          pluginEvent.event = downMsgs[mouseEvent->button];
        }
        break;
      }
      case NS_MOUSE_BUTTON_UP: {
        static const int upMsgs[] =
          { WM_LBUTTONUP, WM_MBUTTONUP, WM_RBUTTONUP };
        pluginEvent.event = upMsgs[mouseEvent->button];
        break;
      }
      // don't synthesize anything for NS_MOUSE_DOUBLECLICK, since that
      // is a synthetic event generated on mouse-up, and Windows WM_*DBLCLK
      // messages are sent on mouse-down
      default:
        break;
      }
      if (pluginEvent.event) {
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
      NS_ASSERTION(anEvent.message == NS_MOUSE_BUTTON_DOWN ||
                   anEvent.message == NS_MOUSE_BUTTON_UP ||
                   anEvent.message == NS_MOUSE_DOUBLECLICK ||
                   anEvent.message == NS_MOUSE_ENTER_SYNTH ||
                   anEvent.message == NS_MOUSE_EXIT_SYNTH ||
                   anEvent.message == NS_MOUSE_MOVE,
                   "Incorrect event type for coordinate translation");
      nsPoint pt =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mObjectFrame) -
        mObjectFrame->GetContentRectRelativeToSelf().TopLeft();
      nsPresContext* presContext = mObjectFrame->PresContext();
      nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x),
                      presContext->AppUnitsToDevPixels(pt.y));
      nsIntPoint widgetPtPx = ptPx + mObjectFrame->GetWindowOriginInPixels(true);
      pPluginEvent->lParam = MAKELPARAM(widgetPtPx.x, widgetPtPx.y);
    }
  }
  else if (!pPluginEvent) {
    switch (anEvent.message) {
      case NS_FOCUS_CONTENT:
        pluginEvent.event = WM_SETFOCUS;
        pluginEvent.wParam = 0;
        pluginEvent.lParam = 0;
        pPluginEvent = &pluginEvent;
        break;
      case NS_BLUR_CONTENT:
        pluginEvent.event = WM_KILLFOCUS;
        pluginEvent.wParam = 0;
        pluginEvent.lParam = 0;
        pPluginEvent = &pluginEvent;
        break;
    }
  }

  if (pPluginEvent && !pPluginEvent->event) {
    // Don't send null events to plugins.
    NS_WARNING("nsObjectFrame ProcessEvent: trying to send null event to plugin.");
    return rv;
  }

  if (pPluginEvent) {
    int16_t response = kNPEventNotHandled;
    mInstance->HandleEvent(pPluginEvent, &response, NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
    if (response == kNPEventHandled)
      rv = nsEventStatus_eConsumeNoDefault;
  }
#endif

#ifdef MOZ_X11
  // this code supports windowless plugins
  nsIWidget* widget = anEvent.widget;
  XEvent pluginEvent = XEvent();
  pluginEvent.type = 0;

  switch(anEvent.eventStructType)
    {
    case NS_MOUSE_EVENT:
      {
        switch (anEvent.message)
          {
          case NS_MOUSE_CLICK:
          case NS_MOUSE_DOUBLECLICK:
            // Button up/down events sent instead.
            return rv;
          }

        // Get reference point relative to plugin origin.
        const nsPresContext* presContext = mObjectFrame->PresContext();
        nsPoint appPoint =
          nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mObjectFrame) -
          mObjectFrame->GetContentRectRelativeToSelf().TopLeft();
        nsIntPoint pluginPoint(presContext->AppUnitsToDevPixels(appPoint.x),
                               presContext->AppUnitsToDevPixels(appPoint.y));
        const WidgetMouseEvent& mouseEvent = *anEvent.AsMouseEvent();
        // Get reference point relative to screen:
        LayoutDeviceIntPoint rootPoint(-1, -1);
        if (widget)
          rootPoint = anEvent.refPoint +
            LayoutDeviceIntPoint::FromUntyped(widget->WidgetToScreenOffset());
#ifdef MOZ_WIDGET_GTK
        Window root = GDK_ROOT_WINDOW();
#elif defined(MOZ_WIDGET_QT)
        Window root = RootWindowOfScreen(DefaultScreenOfDisplay(mozilla::DefaultXDisplay()));
#else
        Window root = None; // Could XQueryTree, but this is not important.
#endif

        switch (anEvent.message)
          {
          case NS_MOUSE_ENTER_SYNTH:
          case NS_MOUSE_EXIT_SYNTH:
            {
              XCrossingEvent& event = pluginEvent.xcrossing;
              event.type = anEvent.message == NS_MOUSE_ENTER_SYNTH ?
                EnterNotify : LeaveNotify;
              event.root = root;
              event.time = anEvent.time;
              event.x = pluginPoint.x;
              event.y = pluginPoint.y;
              event.x_root = rootPoint.x;
              event.y_root = rootPoint.y;
              event.state = XInputEventState(mouseEvent);
              // information lost
              event.subwindow = None;
              event.mode = -1;
              event.detail = NotifyDetailNone;
              event.same_screen = True;
              event.focus = mContentFocused;
            }
            break;
          case NS_MOUSE_MOVE:
            {
              XMotionEvent& event = pluginEvent.xmotion;
              event.type = MotionNotify;
              event.root = root;
              event.time = anEvent.time;
              event.x = pluginPoint.x;
              event.y = pluginPoint.y;
              event.x_root = rootPoint.x;
              event.y_root = rootPoint.y;
              event.state = XInputEventState(mouseEvent);
              // information lost
              event.subwindow = None;
              event.is_hint = NotifyNormal;
              event.same_screen = True;
            }
            break;
          case NS_MOUSE_BUTTON_DOWN:
          case NS_MOUSE_BUTTON_UP:
            {
              XButtonEvent& event = pluginEvent.xbutton;
              event.type = anEvent.message == NS_MOUSE_BUTTON_DOWN ?
                ButtonPress : ButtonRelease;
              event.root = root;
              event.time = anEvent.time;
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
              event.subwindow = None;
              event.same_screen = True;
            }
            break;
          }
      }
      break;

   //XXX case NS_MOUSE_SCROLL_EVENT: not received.
 
   case NS_KEY_EVENT:
      if (anEvent.pluginEvent)
        {
          XKeyEvent &event = pluginEvent.xkey;
#ifdef MOZ_WIDGET_GTK
          event.root = GDK_ROOT_WINDOW();
          event.time = anEvent.time;
          const GdkEventKey* gdkEvent =
            static_cast<const GdkEventKey*>(anEvent.pluginEvent);
          event.keycode = gdkEvent->hardware_keycode;
          event.state = gdkEvent->state;
          switch (anEvent.message)
            {
            case NS_KEY_DOWN:
              // Handle NS_KEY_DOWN for modifier key presses
              // For non-modifiers we get NS_KEY_PRESS
              if (gdkEvent->is_modifier)
                event.type = XKeyPress;
              break;
            case NS_KEY_PRESS:
              event.type = XKeyPress;
              break;
            case NS_KEY_UP:
              event.type = KeyRelease;
              break;
            }
#endif

#ifdef MOZ_WIDGET_QT
          const WidgetKeyboardEvent& keyEvent = *anEvent.AsKeyboardEvent();

          memset( &event, 0, sizeof(event) );
          event.time = anEvent.time;

          QWidget* qWidget = static_cast<QWidget*>(widget->GetNativeData(NS_NATIVE_WINDOW));

          if (qWidget)
#if defined(Q_WS_X11)
            event.root = qWidget->x11Info().appRootWindow();
#else
            event.root = RootWindowOfScreen(DefaultScreenOfDisplay(gfxQtPlatform::GetXDisplay(qWidget)));
#endif

          // deduce keycode from the information in the attached QKeyEvent
          const QKeyEvent* qtEvent = static_cast<const QKeyEvent*>(anEvent.pluginEvent);
          if (qtEvent) {

            if (qtEvent->nativeModifiers())
              event.state = qtEvent->nativeModifiers();
            else // fallback
              event.state = XInputEventState(keyEvent);

            if (qtEvent->nativeScanCode())
              event.keycode = qtEvent->nativeScanCode();
            else // fallback
              event.keycode = XKeysymToKeycode( (widget ? static_cast<Display*>(widget->GetNativeData(NS_NATIVE_DISPLAY)) : nullptr), qtEvent->key());
          }

          switch (anEvent.message)
            {
            case NS_KEY_DOWN:
              event.type = XKeyPress;
              break;
            case NS_KEY_UP:
              event.type = KeyRelease;
              break;
           }
#endif
          // Information that could be obtained from pluginEvent but we may not
          // want to promise to provide:
          event.subwindow = None;
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
      switch (anEvent.message)
        {
        case NS_FOCUS_CONTENT:
        case NS_BLUR_CONTENT:
          {
            XFocusChangeEvent &event = pluginEvent.xfocus;
            event.type =
              anEvent.message == NS_FOCUS_CONTENT ? FocusIn : FocusOut;
            // information lost:
            event.mode = -1;
            event.detail = NotifyDetailNone;
          }
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
  event.window = None; // not a real window
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
      nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(mContent);
      fm->SetFocus(elem, 0);
    }
  }
  switch(anEvent.eventStructType)
    {
    case NS_MOUSE_EVENT:
      {
        switch (anEvent.message)
          {
          case NS_MOUSE_CLICK:
          case NS_MOUSE_DOUBLECLICK:
            // Button up/down events sent instead.
            return rv;
          }

        // Get reference point relative to plugin origin.
        const nsPresContext* presContext = mObjectFrame->PresContext();
        nsPoint appPoint =
          nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mObjectFrame) -
          mObjectFrame->GetContentRectRelativeToSelf().TopLeft();
        nsIntPoint pluginPoint(presContext->AppUnitsToDevPixels(appPoint.x),
                               presContext->AppUnitsToDevPixels(appPoint.y));

        switch (anEvent.message)
          {
          case NS_MOUSE_MOVE:
            {
              // are these going to be touch events?
              // pluginPoint.x;
              // pluginPoint.y;
            }
            break;
          case NS_MOUSE_BUTTON_DOWN:
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
          case NS_MOUSE_BUTTON_UP:
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
          }
      }
      break;

    case NS_KEY_EVENT:
     {
       const WidgetKeyboardEvent& keyEvent = *anEvent.AsKeyboardEvent();
       LOG("Firing NS_KEY_EVENT %d %d\n", keyEvent.keyCode, keyEvent.charCode);
       // pluginEvent is initialized by nsWindow::InitKeyEvent().
       ANPEvent* pluginEvent = reinterpret_cast<ANPEvent*>(keyEvent.pluginEvent);
       if (pluginEvent) {
         MOZ_ASSERT(pluginEvent->inSize == sizeof(ANPEvent));
         MOZ_ASSERT(pluginEvent->eventType == kKey_ANPEventType);
         mInstance->HandleEvent(pluginEvent, nullptr, NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
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
  if (mColorProfile)
    ::CGColorSpaceRelease(mColorProfile);
#endif

  // unregister context menu listener
  if (mCXMenuListener) {
    mCXMenuListener->Destroy(mContent);
    mCXMenuListener = nullptr;
  }

  mContent->RemoveEventListener(NS_LITERAL_STRING("focus"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("blur"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mouseup"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mousemove"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("click"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dblclick"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mouseover"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("mouseout"), this, false);
  mContent->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("keydown"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("keyup"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("drop"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dragdrop"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("drag"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dragenter"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dragover"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dragleave"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dragexit"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dragstart"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("draggesture"), this, true);
  mContent->RemoveEventListener(NS_LITERAL_STRING("dragend"), this, true);

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
  if (!mInstance || !mObjectFrame)
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

  nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
  if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
    DoCocoaEventDrawRect(dirtyRectCopy, cgContext);
    pluginWidget->EndDrawPlugin();
  }
}

void nsPluginInstanceOwner::DoCocoaEventDrawRect(const gfxRect& aDrawRect, CGContextRef cgContext)
{
  if (!mInstance || !mObjectFrame)
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
  if (!mInstance || !mObjectFrame)
    return;

  NPEvent pluginEvent;
  pluginEvent.event = WM_PAINT;
  pluginEvent.wParam = WPARAM(aDC);
  pluginEvent.lParam = LPARAM(&aDirty);
  mInstance->HandleEvent(&pluginEvent, nullptr);
}
#endif

#ifdef XP_OS2
void nsPluginInstanceOwner::Paint(const nsRect& aDirtyRect, HPS aHPS)
{
  if (!mInstance || !mObjectFrame)
    return;

  NPWindow *window;
  GetWindow(window);
  nsIntRect relDirtyRect = aDirtyRect.ToOutsidePixels(mObjectFrame->PresContext()->AppUnitsPerDevPixel());

  // we got dirty rectangle in relative window coordinates, but we
  // need it in absolute units and in the (left, top, right, bottom) form
  RECTL rectl;
  rectl.xLeft   = relDirtyRect.x + window->x;
  rectl.yBottom = relDirtyRect.y + window->y;
  rectl.xRight  = rectl.xLeft + relDirtyRect.width;
  rectl.yTop    = rectl.yBottom + relDirtyRect.height;

  NPEvent pluginEvent;
  pluginEvent.event = WM_PAINT;
  pluginEvent.wParam = (uint32_t)aHPS;
  pluginEvent.lParam = (uint32_t)&rectl;
  mInstance->HandleEvent(&pluginEvent, nullptr);
}
#endif

#ifdef MOZ_WIDGET_ANDROID

void nsPluginInstanceOwner::Paint(gfxContext* aContext,
                                  const gfxRect& aFrameRect,
                                  const gfxRect& aDirtyRect)
{
  if (!mInstance || !mObjectFrame || !mPluginDocumentActiveState || mFullScreen)
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
  static nsRefPtr<gfxImageSurface> pluginSurface;

  if (pluginSurface == nullptr ||
      aFrameRect.width  != pluginSurface->Width() ||
      aFrameRect.height != pluginSurface->Height()) {

    pluginSurface = new gfxImageSurface(gfxIntSize(aFrameRect.width, aFrameRect.height), 
                                        gfxImageFormatARGB32);
    if (!pluginSurface)
      return;
  }

  // Clears buffer.  I think this is needed.
  nsRefPtr<gfxContext> ctx = new gfxContext(pluginSurface);
  ctx->SetOperator(gfxContext::OPERATOR_CLEAR);
  ctx->Paint();
  
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

  aContext->SetOperator(gfxContext::OPERATOR_SOURCE);
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
  if (!mInstance || !mObjectFrame)
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
  aContext->Translate(pluginRect.TopLeft());

  Renderer renderer(window, this, pluginSize, pluginDirtyRect);

  Display* dpy = mozilla::DefaultXDisplay();
  Screen* screen = DefaultScreenOfDisplay(dpy);
  Visual* visual = DefaultVisualOfScreen(screen);

  renderer.Draw(aContext, nsIntSize(window->width, window->height),
                rendererFlags, screen, visual, nullptr);
}
nsresult
nsPluginInstanceOwner::Renderer::DrawWithXlib(gfxXlibSurface* xsurface, 
                                              nsIntPoint offset,
                                              nsIntRect *clipRects, 
                                              uint32_t numClipRects)
{
  Screen *screen = cairo_xlib_surface_get_screen(xsurface->CairoSurface());
  Colormap colormap;
  Visual* visual;
  if (!xsurface->GetColormapAndVisual(&colormap, &visual)) {
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
    gfxIntSize surfaceSize = xsurface->GetSize();
    clipRect.IntersectRect(clipRect,
                           nsIntRect(0, 0,
                                     surfaceSize.width, surfaceSize.height));
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
    exposeEvent.drawable = xsurface->XDrawable();
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

  mContent = aContent;

  // Get a frame, don't reflow. If a reflow was necessary it should have been
  // done at a higher level than this (content).
  nsIFrame* frame = aContent->GetPrimaryFrame();
  nsIObjectFrame* iObjFrame = do_QueryFrame(frame);
  nsObjectFrame* objFrame =  static_cast<nsObjectFrame*>(iObjFrame);
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

  mContent->AddEventListener(NS_LITERAL_STRING("focus"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("blur"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("mouseup"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("mousedown"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("mousemove"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("click"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("dblclick"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("mouseover"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("mouseout"), this, false,
                             false);
  mContent->AddEventListener(NS_LITERAL_STRING("keypress"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("keydown"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("keyup"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("drop"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("dragdrop"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("drag"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("dragenter"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("dragover"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("dragleave"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("dragexit"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("dragstart"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("draggesture"), this, true);
  mContent->AddEventListener(NS_LITERAL_STRING("dragend"), this, true);

  return NS_OK; 
}

void* nsPluginInstanceOwner::GetPluginPortFromWidget()
{
//!!! Port must be released for windowless plugins on Windows, because it is HDC !!!

  void* result = NULL;
  if (mWidget) {
#ifdef XP_WIN
    if (mPluginWindow && (mPluginWindow->type == NPWindowTypeDrawable))
      result = mWidget->GetNativeData(NS_NATIVE_GRAPHIC);
    else
#endif
#ifdef XP_MACOSX
    if (GetDrawingModel() == NPDrawingModelCoreGraphics || 
        GetDrawingModel() == NPDrawingModelCoreAnimation ||
        GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation)
      result = mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT_CG);
    else
#endif
      result = mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
  }
  return result;
}

void nsPluginInstanceOwner::ReleasePluginPort(void * pluginPort)
{
#ifdef XP_WIN
  if (mWidget && mPluginWindow &&
      mPluginWindow->type == NPWindowTypeDrawable) {
    mWidget->FreeNativeData((HDC)pluginPort, NS_NATIVE_GRAPHIC);
  }
#endif
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
  if (!windowless && !nsIWidget::UsePuppetWidgets()) {
    // Try to get a parent widget, on some platforms widget creation will fail without
    // a parent.
    nsCOMPtr<nsIWidget> parentWidget;
    nsIDocument *doc = nullptr;
    if (mContent) {
      doc = mContent->OwnerDoc();
      parentWidget = nsContentUtils::WidgetForDocument(doc);
    }

    mWidget = do_CreateInstance(kWidgetCID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsWidgetInitData initData;
    initData.mWindowType = eWindowType_plugin;
    initData.mUnicode = false;
    initData.clipChildren = true;
    initData.clipSiblings = true;
    rv = mWidget->Create(parentWidget.get(), nullptr, nsIntRect(0,0,0,0),
                         nullptr, &initData);
    if (NS_FAILED(rv)) {
      mWidget->Destroy();
      mWidget = nullptr;
      return rv;
    }

    mWidget->EnableDragDrop(true);
    mWidget->Show(false);
    mWidget->Enable(false);

#ifdef XP_MACOSX
    // Now that we have a widget we want to set the event model before
    // any events are processed.
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (!pluginWidget) {
      return NS_ERROR_FAILURE;
    }
    pluginWidget->SetPluginEventModel(GetEventModel());
    pluginWidget->SetPluginDrawingModel(GetDrawingModel());

    if (GetDrawingModel() == NPDrawingModelCoreAnimation) {
      AddToCARefreshTimer();
    }
#endif
  }

  if (mObjectFrame) {
    // NULL widget is fine, will result in windowless setup.
    mObjectFrame->PrepForDrawing(mWidget);
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
    mPluginWindow->window = GetPluginPortFromWidget();
    // tell the plugin window about the widget
    mPluginWindow->SetPluginWidget(mWidget);
    
    // tell the widget about the current plugin instance owner.
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget) {
      pluginWidget->SetPluginInstanceOwner(this);
    }
  }

  mWidgetCreationComplete = true;

  return NS_OK;
}

// Mac specific code to fix up the port location and clipping region
#ifdef XP_MACOSX

void* nsPluginInstanceOwner::FixUpPluginWindow(int32_t inPaintState)
{
  if (!mWidget || !mPluginWindow || !mInstance || !mObjectFrame)
    return nullptr;

  nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
  if (!pluginWidget)
    return nullptr;

  // If we've already set up a CGContext in nsObjectFrame::PaintPlugin(), we
  // don't want calls to SetPluginPortAndDetectChange() to step on our work.
  if (mInCGPaintLevel < 1) {
    SetPluginPortAndDetectChange();
  }

  // We'll need the top-level Cocoa window for the Cocoa event model.
  nsIWidget* widget = mObjectFrame->GetNearestWidget();
  if (!widget)
    return nullptr;
  void *cocoaTopLevelWindow = widget->GetNativeData(NS_NATIVE_WINDOW);
  if (!cocoaTopLevelWindow)
    return nullptr;

  nsIntPoint pluginOrigin;
  nsIntRect widgetClip;
  bool widgetVisible;
  pluginWidget->GetPluginClipRect(widgetClip, pluginOrigin, widgetVisible);
  mWidgetVisible = widgetVisible;

  // printf("GetPluginClipRect returning visible %d\n", widgetVisible);

  // This would be a lot easier if we could use obj-c here,
  // but we can't. Since we have only nsIWidget and we can't
  // use its native widget (an obj-c object) we have to go
  // from the widget's screen coordinates to its window coords
  // instead of straight to window coords.
  nsIntPoint geckoScreenCoords = mWidget->WidgetToScreenOffset();

  nsRect windowRect;
  NS_NPAPI_CocoaWindowFrame(cocoaTopLevelWindow, windowRect);

  double scaleFactor = 1.0;
  GetContentsScaleFactor(&scaleFactor);
  int intScaleFactor = ceil(scaleFactor);

  // Convert geckoScreenCoords from device pixels to "display pixels"
  // for HiDPI modes.
  mPluginWindow->x = geckoScreenCoords.x/intScaleFactor - windowRect.x;
  mPluginWindow->y = geckoScreenCoords.y/intScaleFactor - windowRect.y;

  NPRect oldClipRect = mPluginWindow->clipRect;
  
  // fix up the clipping region
  mPluginWindow->clipRect.top    = widgetClip.y;
  mPluginWindow->clipRect.left   = widgetClip.x;

  if (!mWidgetVisible || inPaintState == ePluginPaintDisable) {
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
      mPluginWindow->clipRect.bottom  != oldClipRect.bottom ||
      mPluginPortChanged)
  {
    if (UseAsyncRendering()) {
      mInstance->AsyncSetWindow(mPluginWindow);
    }
    else {
      mPluginWindow->CallSetWindow(mInstance);
    }
    mPluginPortChanged = false;
  }

  // After the first NPP_SetWindow call we need to send an initial
  // top-level window focus event.
  if (!mSentInitialTopLevelWindowEvent) {
    // Set this before calling ProcessEvent to avoid endless recursion.
    mSentInitialTopLevelWindowEvent = true;

    WidgetPluginEvent pluginEvent(true, NS_PLUGIN_FOCUS_EVENT, nullptr);
    NPCocoaEvent cocoaEvent;
    InitializeNPCocoaEvent(&cocoaEvent);
    cocoaEvent.type = NPCocoaEventWindowFocusChanged;
    cocoaEvent.data.focus.hasFocus = NS_NPAPI_CocoaWindowIsMain(cocoaTopLevelWindow);
    pluginEvent.pluginEvent = &cocoaEvent;
    ProcessEvent(pluginEvent);
  }

  return nullptr;
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
  nsIntPoint origin = mObjectFrame->GetWindowOriginInPixels(windowless);

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

void
nsPluginInstanceOwner::UpdateDocumentActiveState(bool aIsActive)
{
  mPluginDocumentActiveState = aIsActive;
  UpdateWindowPositionAndClipRect(true);

#ifdef MOZ_WIDGET_ANDROID
  if (mInstance) {
    if (!mPluginDocumentActiveState)
      RemovePluginView();

    mInstance->NotifyOnScreen(mPluginDocumentActiveState);

    // This is, perhaps, incorrect. It is supposed to be sent
    // when "the webview has paused or resumed". The side effect
    // is that Flash video players pause or resume (if they were
    // playing before) based on the value here. I personally think
    // we want that on Android when switching to another tab, so
    // that's why we call it here.
    mInstance->NotifyForeground(mPluginDocumentActiveState);
  }
#endif
}
#endif // XP_MACOSX

NS_IMETHODIMP
nsPluginInstanceOwner::CallSetWindow()
{
  if (mObjectFrame) {
    mObjectFrame->CallSetWindow(false);
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
#if defined(XP_MACOSX)
  nsIPresShell* presShell = nsContentUtils::FindPresShellForDocument(mContent->OwnerDoc());
  if (presShell) {
    scaleFactor = double(nsPresContext::AppUnitsPerCSSPixel())/
      presShell->GetPresContext()->DeviceContext()->UnscaledAppUnitsPerDevPixel();
  }
#endif
  *result = scaleFactor;
  return NS_OK;
}

void nsPluginInstanceOwner::SetFrame(nsObjectFrame *aFrame)
{
  // Don't do anything if the frame situation hasn't changed.
  if (mObjectFrame == aFrame) {
    return;
  }

  // If we already have a frame that is changing or going away...
  if (mObjectFrame) {
    // Make sure the old frame isn't holding a reference to us.
    mObjectFrame->SetInstanceOwner(nullptr);
  }

  // Swap in the new frame (or no frame)
  mObjectFrame = aFrame;

  // Set up a new frame
  if (mObjectFrame) {
    mObjectFrame->SetInstanceOwner(this);
    // Can only call PrepForDrawing on an object frame once. Don't do it here unless
    // widget creation is complete. Doesn't matter if we actually have a widget.
    if (mWidgetCreationComplete) {
      mObjectFrame->PrepForDrawing(mWidget);
    }
    mObjectFrame->FixupWindow(mObjectFrame->GetContentRectRelativeToSelf().Size());
    mObjectFrame->InvalidateFrame();

    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    const nsIContent* content = aFrame->GetContent();
    if (fm && content) {
      mContentFocused = (content == fm->GetFocusedContent());
    }
  }
}

nsObjectFrame* nsPluginInstanceOwner::GetFrame()
{
  return mObjectFrame;
}

// Little helper function to resolve relative URL in
// |value| for certain inputs of |name|
void nsPluginInstanceOwner::FixUpURLS(const nsString &name, nsAString &value)
{
  if (name.LowerCaseEqualsLiteral("pluginspage")) {
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    nsAutoString newURL;
    NS_MakeAbsoluteURI(newURL, value, baseURI);
    if (!newURL.IsEmpty())
      value = newURL;
  }
}

NS_IMETHODIMP nsPluginInstanceOwner::PrivateModeChanged(bool aEnabled)
{
  return mInstance ? mInstance->PrivateModeStateChanged(aEnabled) : NS_OK;
}

already_AddRefed<nsIURI> nsPluginInstanceOwner::GetBaseURI() const
{
  if (!mContent) {
    return nullptr;
  }
  return mContent->GetBaseURI();
}

// nsPluginDOMContextMenuListener class implementation

nsPluginDOMContextMenuListener::nsPluginDOMContextMenuListener(nsIContent* aContent)
{
  aContent->AddEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
}

nsPluginDOMContextMenuListener::~nsPluginDOMContextMenuListener()
{
}

NS_IMPL_ISUPPORTS1(nsPluginDOMContextMenuListener,
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
