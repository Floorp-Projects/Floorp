/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Jacek Piskozub <piskozub@iopan.gda.pl>
 *   Leon Sha <leon.sha@sun.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Josh Aas <josh@mozilla.com>
 *   Mats Palmgren <matspal@gmail.com>
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

#ifdef MOZ_WIDGET_QT
#include <QWidget>
#include <QKeyEvent>
#ifdef MOZ_X11
#include <QX11Info>
#endif
#undef slots
#endif

#ifdef MOZ_X11
#include <cairo-xlib.h>
#include "gfxXlibSurface.h"
/* X headers suck */
enum { XKeyPress = KeyPress };
#ifdef KeyPress
#undef KeyPress
#endif
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
#include "nsIDOMEventTarget.h"
#include "nsObjectFrame.h"
#include "nsIPluginDocument.h"
#include "nsIStringStream.h"
#include "nsNetUtil.h"
#include "mozilla/Preferences.h"
#include "nsILinkHandler.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebBrowserChrome.h"
#include "nsLayoutUtils.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIPluginWidget.h"
#include "nsIViewManager.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIAppShell.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsAttrName.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIDOMDragEvent.h"
#include "nsIScrollableFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsIObjectLoadingContent.h"

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

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "gfxXlibNativeRenderer.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "ANPBase.h"
#include "android_npapi.h"
#include "AndroidBridge.h"
#include "AndroidMediaLayer.h"
using namespace mozilla::dom;

#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#endif

using namespace mozilla;

// special class for handeling DOM context menu events because for
// some reason it starves other mouse events if implemented on the
// same class
class nsPluginDOMContextMenuListener : public nsIDOMEventListener
{
public:
  nsPluginDOMContextMenuListener();
  virtual ~nsPluginDOMContextMenuListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  nsresult Init(nsIContent* aContent);
  nsresult Destroy(nsIContent* aContent);
  
  nsEventStatus ProcessEvent(const nsGUIEvent& anEvent)
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

#ifdef XP_MACOSX
static void DrawPlugin(ImageContainer* aContainer, void* aPluginInstanceOwner)
{
  nsObjectFrame* frame = static_cast<nsPluginInstanceOwner*>(aPluginInstanceOwner)->GetFrame();
  if (frame) {
    frame->UpdateImageLayer(gfxRect(0,0,0,0));
  }
}

static void OnDestroyImage(void* aPluginInstanceOwner)
{
  nsPluginInstanceOwner* owner = static_cast<nsPluginInstanceOwner*>(aPluginInstanceOwner);
  NS_IF_RELEASE(owner);
}
#endif // XP_MACOSX

already_AddRefed<ImageContainer>
nsPluginInstanceOwner::GetImageContainer()
{
  if (mInstance) {
    nsRefPtr<ImageContainer> container;
    // Every call to nsIPluginInstance::GetImage() creates
    // a new image.  See nsIPluginInstance.idl.
    mInstance->GetImageContainer(getter_AddRefs(container));
    if (container) {
#ifdef XP_MACOSX
      AutoLockImage autoLock(container);
      Image* image = autoLock.GetImage();
      if (image && image->GetFormat() == Image::MAC_IO_SURFACE && mObjectFrame) {
        MacIOSurfaceImage *oglImage = static_cast<MacIOSurfaceImage*>(image);
        NS_ADDREF_THIS();
        oglImage->SetUpdateCallback(&DrawPlugin, this);
        oglImage->SetDestroyCallback(&OnDestroyImage);
      }
#endif
      return container.forget();
    }
  }
  return nsnull;
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
  return nsnull;
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

  bool useAsyncRendering;
  bool result = (mInstance &&
          NS_SUCCEEDED(mInstance->UseAsyncPainting(&useAsyncRendering)) &&
          useAsyncRendering
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
    mPluginWindow = nsnull;

  mObjectFrame = nsnull;
  mContent = nsnull;
  mTagText = nsnull;
  mWidgetCreationComplete = false;
#ifdef XP_MACOSX
  memset(&mCGPluginPortCopy, 0, sizeof(NP_CGContext));
#ifndef NP_NO_QUICKDRAW
  memset(&mQDPluginPortCopy, 0, sizeof(NP_Port));
#endif
  mInCGPaintLevel = 0;
  mSentInitialTopLevelWindowEvent = false;
  mColorProfile = nsnull;
  mPluginPortChanged = false;
#endif
  mContentFocused = false;
  mWidgetVisible = true;
  mPluginWindowVisible = false;
  mPluginDocumentActiveState = true;
  mNumCachedAttrs = 0;
  mNumCachedParams = 0;
  mCachedAttrParamNames = nsnull;
  mCachedAttrParamValues = nsnull;

#ifdef XP_MACOSX
#ifndef NP_NO_QUICKDRAW
  mEventModel = NPEventModelCarbon;
#else
  mEventModel = NPEventModelCocoa;
#endif
  mUseAsyncRendering = false;
#endif

  mWaitingForPaint = false;

#ifdef MOZ_WIDGET_ANDROID
  mInverted = false;
  mLayer = nsnull;
#endif
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  PRInt32 cnt;

  if (mWaitingForPaint) {
    // We don't care when the event is dispatched as long as it's "soon",
    // since whoever needs it will be waiting for it.
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(mContent, true);
    NS_DispatchToMainThread(event);
  }

#ifdef MAC_CARBON_PLUGINS
  CancelTimer();
#endif

  mObjectFrame = nsnull;

  for (cnt = 0; cnt < (mNumCachedAttrs + 1 + mNumCachedParams); cnt++) {
    if (mCachedAttrParamNames && mCachedAttrParamNames[cnt]) {
      NS_Free(mCachedAttrParamNames[cnt]);
      mCachedAttrParamNames[cnt] = nsnull;
    }

    if (mCachedAttrParamValues && mCachedAttrParamValues[cnt]) {
      NS_Free(mCachedAttrParamValues[cnt]);
      mCachedAttrParamValues[cnt] = nsnull;
    }
  }

  if (mCachedAttrParamNames) {
    NS_Free(mCachedAttrParamNames);
    mCachedAttrParamNames = nsnull;
  }

  if (mCachedAttrParamValues) {
    NS_Free(mCachedAttrParamValues);
    mCachedAttrParamValues = nsnull;
  }

  if (mTagText) {
    NS_Free(mTagText);
    mTagText = nsnull;
  }

  PLUG_DeletePluginNativeWindow(mPluginWindow);
  mPluginWindow = nsnull;

#ifdef MOZ_WIDGET_ANDROID
  RemovePluginView();
#endif

  if (mInstance) {
    mInstance->InvalidateOwner();
  }
}

NS_IMPL_ISUPPORTS3(nsPluginInstanceOwner,
                   nsIPluginInstanceOwner,
                   nsIPluginTagInfo,
                   nsIDOMEventListener)

nsresult
nsPluginInstanceOwner::SetInstance(nsNPAPIPluginInstance *aInstance)
{
  NS_ASSERTION(!mInstance || !aInstance, "mInstance should only be set or unset!");

  // If we're going to null out mInstance after use, be sure to call
  // mInstance->InvalidateOwner() here, since it now won't be called
  // from our destructor.  This fixes bug 613376.
  if (mInstance && !aInstance)
    mInstance->InvalidateOwner();

  mInstance = aInstance;

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetWindow(NPWindow *&aWindow)
{
  NS_ASSERTION(mPluginWindow, "the plugin window object being returned is null");
  aWindow = mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMode(PRInt32 *aMode)
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

NS_IMETHODIMP nsPluginInstanceOwner::GetAttributes(PRUint16& n,
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

  *result = nsnull;

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
                                            PRUint32 aHeadersDataLen)
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

  nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();

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

  PRInt32 blockPopups =
    Preferences::GetInt("privacy.popups.disable_from_plugins");
  nsAutoPopupStatePusher popupStatePusher((PopupControlState)blockPopups);

  rv = lh->OnLinkClick(mContent, uri, unitarget.get(), 
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

  // Each time an asynchronously-drawing plugin sends a new surface to display,
  // InvalidateRect is called. We notify reftests that painting is up to
  // date and update our ImageContainer with the new surface.
  nsRefPtr<ImageContainer> container;
  mInstance->GetImageContainer(getter_AddRefs(container));
  gfxIntSize oldSize(0, 0);

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

  nsPresContext* presContext = mObjectFrame->PresContext();
  nsRect rect(presContext->DevPixelsToAppUnits(invalidRect->left),
              presContext->DevPixelsToAppUnits(invalidRect->top),
              presContext->DevPixelsToAppUnits(invalidRect->right - invalidRect->left),
              presContext->DevPixelsToAppUnits(invalidRect->bottom - invalidRect->top));
  if (container) {
    gfxIntSize newSize = container->GetCurrentSize();
    if (newSize != oldSize) {
      // The image size has changed - invalidate the old area too, bug 635405.
      nsRect oldRect = nsRect(0, 0,
                              presContext->DevPixelsToAppUnits(oldSize.width),
                              presContext->DevPixelsToAppUnits(oldSize.height));
      rect.UnionRect(rect, oldRect);
    }
  }
  rect.MoveBy(mObjectFrame->GetContentRectRelativeToSelf().TopLeft());
  mObjectFrame->InvalidateLayer(rect, nsDisplayItem::TYPE_PLUGIN);
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
    mObjectFrame->InvalidateLayer(mObjectFrame->GetContentRectRelativeToSelf(), nsDisplayItem::TYPE_PLUGIN);
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
  nsIViewManager* vm = mObjectFrame->PresContext()->GetPresShell()->GetViewManager();
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
      nsIView *view = nsIView::GetViewFor(win);
      NS_ASSERTION(view, "No view for widget");
      nsPoint offset = view->GetOffsetTo(nsnull);
      
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
  nsresult rv = vm->GetRootWidget(getter_AddRefs(widget));            
  if (widget) {
    *pvalue = (void*)widget->GetNativeData(NS_NATIVE_WINDOW);
  } else {
    NS_ASSERTION(widget, "couldn't get doc's widget in getting doc's window handle");
  }

  return rv;
#elif defined(MOZ_WIDGET_GTK2) || defined(MOZ_WIDGET_QT)
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

NS_IMETHODIMP nsPluginInstanceOwner::SetEventModel(PRInt32 eventModel)
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

NS_IMETHODIMP nsPluginInstanceOwner::GetTagText(const char* *result)
{
  NS_ENSURE_ARG_POINTER(result);
  if (nsnull == mTagText) {
    nsresult rv;
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent, &rv));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDocument> document;
    rv = GetDocument(getter_AddRefs(document));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(document);
    NS_ASSERTION(domDoc, "Need a document");

    nsCOMPtr<nsIDocumentEncoder> docEncoder(do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html", &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = docEncoder->Init(domDoc, NS_LITERAL_STRING("text/html"), nsIDocumentEncoder::OutputEncodeBasicEntities);
    if (NS_FAILED(rv))
      return rv;

    nsRefPtr<nsRange> range = new nsRange();
    rv = range->SelectNode(node);
    if (NS_FAILED(rv))
      return rv;

    docEncoder->SetRange(range);
    nsString elementHTML;
    rv = docEncoder->EncodeToString(elementHTML);
    if (NS_FAILED(rv))
      return rv;

    mTagText = ToNewUTF8String(elementHTML);
    if (!mTagText)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  *result = mTagText;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetParameters(PRUint16& n, const char*const*& names, const char*const*& values)
{
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  n = mNumCachedParams;
  if (n) {
    names  = (const char **)(mCachedAttrParamNames + mNumCachedAttrs + 1);
    values = (const char **)(mCachedAttrParamValues + mNumCachedAttrs + 1);
  } else
    names = values = nsnull;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetParameter(const char* name, const char* *result)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(result);
  
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  *result = nsnull;

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
      *result = nsnull;
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
    {"IBM864",          "Cp864"},
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
    {"EUC-KR",          "EUC_KR"},
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
    {"x-windows-949",   "MS949"},
    {"x-mac-arabic",    "MacArabic"},
    {"x-mac-croatian",  "MacCroatia"},
    {"x-mac-cyrillic",  "MacCyrillic"},
    {"x-mac-greek",     "MacGreek"},
    {"x-mac-hebrew",    "MacHebrew"},
    {"x-mac-icelandic", "MacIceland"},
    {"x-mac-roman",     "MacRoman"},
    {"x-mac-romanian",  "MacRomania"},
    {"x-mac-ukrainian", "MacUkraine"},
    {"Shift_JIS",       "SJIS"},
    {"TIS-620",         "TIS620"}
};

NS_IMETHODIMP nsPluginInstanceOwner::GetDocumentEncoding(const char* *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nsnull;

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
      !nsCRT::strncmp(PromiseFlatCString(charset).get(), "UTF", 3)) {
    *result = ToNewCString(charset);
  } else {
    if (!gCharsetMap) {
      const int NUM_CHARSETS = sizeof(charsets) / sizeof(moz2javaCharset);
      gCharsetMap = new nsDataHashtable<nsDepCharHashKey, const char*>();
      if (!gCharsetMap || !gCharsetMap->Init(NUM_CHARSETS))
        return NS_ERROR_OUT_OF_MEMORY;

      for (PRUint16 i = 0; i < NUM_CHARSETS; i++) {
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
  
NS_IMETHODIMP nsPluginInstanceOwner::GetWidth(PRUint32 *result)
{
  NS_ENSURE_ARG_POINTER(result);

  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  *result = mPluginWindow->width;

  return NS_OK;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetHeight(PRUint32 *result)
{
  NS_ENSURE_ARG_POINTER(result);

  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  *result = mPluginWindow->height;

  return NS_OK;
}

  
NS_IMETHODIMP nsPluginInstanceOwner::GetBorderVertSpace(PRUint32 *result)
{
  nsresult    rv;
  const char  *vspace;

  rv = GetAttribute("VSPACE", &vspace);

  if (NS_OK == rv) {
    if (*result != 0)
      *result = (PRUint32)atol(vspace);
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetBorderHorizSpace(PRUint32 *result)
{
  nsresult    rv;
  const char  *hspace;

  rv = GetAttribute("HSPACE", &hspace);

  if (NS_OK == rv) {
    if (*result != 0)
      *result = (PRUint32)atol(hspace);
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
  PRUint32 cattrs = mContent->GetAttrCount();
  if (cattrs < 0x0000FFFD) {
    mNumCachedAttrs = static_cast<PRUint16>(cattrs);
  } else {
    mNumCachedAttrs = 0xFFFD;
  }

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

  nsCOMPtr<nsIDOMNodeList> allParams;
  NS_NAMED_LITERAL_STRING(xhtml_ns, "http://www.w3.org/1999/xhtml");
  mydomElement->GetElementsByTagNameNS(xhtml_ns, NS_LITERAL_STRING("param"),
                                       getter_AddRefs(allParams));
  if (allParams) {
    PRUint32 numAllParams; 
    allParams->GetLength(&numAllParams);
    for (PRUint32 i = 0; i < numAllParams; i++) {
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
              parent = domapplet;
            }
            else {
              parent = domobject;
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
  PRUint32 cparams = ourParams.Count();
  if (cparams < 0x0000FFFF) {
    mNumCachedParams = static_cast<PRUint16>(cparams);
  } else {
    mNumCachedParams = 0xFFFF;
  }

  PRUint16 numRealAttrs = mNumCachedAttrs;

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

  // Some plugins (java, bug 406541) don't canonicalize the 'codebase' attribute
  // in a standard way - codebase="" results in / (domain root), but
  // codebase="blah" results in ./blah; codebase="file:" results in "file:///".
  // We canonicalize codebase here to ensure the codebase we run security checks
  // against is the same codebase java uses.
  // Note that GetObjectBaseURI mimics some of java's quirks for maximal
  // compatibility.
  const char* mime = nsnull;
  bool addCodebase = false;
  nsCAutoString codebaseSpec;
  if (mInstance && NS_SUCCEEDED(mInstance->GetMIMEType(&mime)) && mime &&
      strcmp(mime, "application/x-java-vm") == 0) {
    addCodebase = true;
    nsCOMPtr<nsIObjectLoadingContent> objlContent = do_QueryInterface(mContent);
    nsCOMPtr<nsIURI> codebaseURI;
    objlContent->GetObjectBaseURI(nsCString(mime), getter_AddRefs(codebaseURI));
    nsresult rv = codebaseURI->GetSpec(codebaseSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make space if codebase isn't already set
    if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::codebase)) {
      mNumCachedAttrs++;
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
  PRInt32 start, end, increment;
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
  PRUint32 nextAttrParamIndex = 0;

  // Whether or not we force the wmode below while traversing
  // the name/value pairs.
  bool wmodeSet = false;

  // Add attribute name/value pairs.
  for (PRInt32 index = start; index != end; index += increment) {
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
    } else if (addCodebase && 0 == PL_strcasecmp(mCachedAttrParamNames[nextAttrParamIndex], "codebase")) {
      mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_ConvertUTF8toUTF16(codebaseSpec));
      addCodebase = false;
    } else {
      mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(value);
    }
    nextAttrParamIndex++;
  }

  // Pontentially add CODEBASE attribute
  if (addCodebase) {
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(NS_LITERAL_STRING("codebase"));
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_ConvertUTF8toUTF16(codebaseSpec));
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
  mCachedAttrParamValues[nextAttrParamIndex] = nsnull;
#endif
  nextAttrParamIndex++;

  // Add PARAM name/value pairs.
  for (PRUint16 i = 0; i < mNumCachedParams; i++) {
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
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(name);
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(value);
    nextAttrParamIndex++;
  }

  return NS_OK;
}

#ifdef XP_MACOSX

#ifndef NP_NO_CARBON
static void InitializeEventRecord(EventRecord* event, ::Point* aMousePosition)
{
  memset(event, 0, sizeof(EventRecord));
  if (aMousePosition) {
    event->where = *aMousePosition;
  } else {
    ::GetGlobalMouse(&event->where);
  }
  event->when = ::TickCount();
  event->modifiers = ::GetCurrentKeyModifiers();
}
#endif

static void InitializeNPCocoaEvent(NPCocoaEvent* event)
{
  memset(event, 0, sizeof(NPCocoaEvent));
}

NPDrawingModel nsPluginInstanceOwner::GetDrawingModel()
{
#ifndef NP_NO_QUICKDRAW
  NPDrawingModel drawingModel = NPDrawingModelQuickDraw;
#else
  NPDrawingModel drawingModel = NPDrawingModelCoreGraphics;
#endif

  if (!mInstance)
    return drawingModel;

  mInstance->GetDrawingModel((PRInt32*)&drawingModel);
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
  const char* mime = nsnull;
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

  if (!mIOSurface ||
      (mIOSurface->GetWidth() != (size_t)aWidth ||
       mIOSurface->GetHeight() != (size_t)aHeight)) {
    mIOSurface = nsnull;

    // If the renderer is backed by an IOSurface, resize it as required.
    mIOSurface = nsIOSurface::CreateIOSurface(aWidth, aHeight);
    if (mIOSurface) {
      nsRefPtr<nsIOSurface> attachSurface = nsIOSurface::LookupSurface(
                                              mIOSurface->GetIOSurfaceID());
      if (attachSurface) {
        mCARenderer.AttachIOSurface(attachSurface);
      } else {
        NS_ERROR("IOSurface attachment failed");
        mIOSurface = nsnull;
      }
    }
  }

  if (!mColorProfile) {
    mColorProfile = CreateSystemColorSpace();
  }

  if (mCARenderer.isInit() == false) {
    void *caLayer = NULL;
    nsresult rv = mInstance->GetValueFromPlugin(NPPVpluginCoreAnimationLayer, &caLayer);
    if (NS_FAILED(rv) || !caLayer) {
      return;
    }

    // We don't run Flash in-process so we can unconditionally disallow
    // the offliner renderer.
    mCARenderer.SetupRenderer(caLayer, aWidth, aHeight, DISALLOW_OFFLINE_RENDERER);

    // Setting up the CALayer requires resetting the painting otherwise we
    // get garbage for the first few frames.
    FixUpPluginWindow(ePluginPaintDisable);
    FixUpPluginWindow(ePluginPaintEnable);
  }

  CGImageRef caImage = NULL;
  nsresult rt = mCARenderer.Render(aWidth, aHeight, &caImage);
  if (rt == NS_OK && mIOSurface && mColorProfile) {
    nsCARenderer::DrawSurfaceToCGContext(aCGContext, mIOSurface, mColorProfile,
                                         0, 0, aWidth, aHeight);
  } else if (rt == NS_OK && caImage != NULL) {
    // Significant speed up by resetting the scaling
    ::CGContextSetInterpolationQuality(aCGContext, kCGInterpolationNone );
    ::CGContextTranslateCTM(aCGContext, 0, aHeight);
    ::CGContextScaleCTM(aCGContext, 1.0, -1.0);

    ::CGContextDrawImage(aCGContext, CGRectMake(0,0,aWidth,aHeight), caImage);
  } else {
    NS_NOTREACHED("nsCARenderer::Render failure");
  }
}

void* nsPluginInstanceOwner::GetPluginPortCopy()
{
#ifndef NP_NO_QUICKDRAW
  if (GetDrawingModel() == NPDrawingModelQuickDraw)
    return &mQDPluginPortCopy;
#endif
  if (GetDrawingModel() == NPDrawingModelCoreGraphics || 
      GetDrawingModel() == NPDrawingModelCoreAnimation ||
      GetDrawingModel() == NPDrawingModelInvalidatingCoreAnimation)
    return &mCGPluginPortCopy;
  return nsnull;
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
    return nsnull;
  void* pluginPort = GetPluginPortFromWidget();
  if (!pluginPort)
    return nsnull;
  mPluginWindow->window = pluginPort;

#ifndef NP_NO_QUICKDRAW
  NPDrawingModel drawingModel = GetDrawingModel();
  if (drawingModel == NPDrawingModelQuickDraw) {
    NP_Port* windowQDPort = static_cast<NP_Port*>(mPluginWindow->window);
    if (windowQDPort->port != mQDPluginPortCopy.port) {
      mQDPluginPortCopy.port = windowQDPort->port;
      mPluginPortChanged = true;
    }
  } else if (drawingModel == NPDrawingModelCoreGraphics || 
             drawingModel == NPDrawingModelCoreAnimation ||
             drawingModel == NPDrawingModelInvalidatingCoreAnimation)
#endif
  {
#ifndef NP_NO_CARBON
    if (GetEventModel() == NPEventModelCarbon) {
      NP_CGContext* windowCGPort = static_cast<NP_CGContext*>(mPluginWindow->window);
      if ((windowCGPort->context != mCGPluginPortCopy.context) ||
          (windowCGPort->window != mCGPluginPortCopy.window)) {
        mCGPluginPortCopy.context = windowCGPort->context;
        mCGPluginPortCopy.window = windowCGPort->window;
        mPluginPortChanged = true;
      }
    }
#endif
  }

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
PRUint32
nsPluginInstanceOwner::GetEventloopNestingLevel()
{
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  PRUint32 currentLevel = 0;
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

void nsPluginInstanceOwner::ScrollPositionWillChange(nscoord aX, nscoord aY)
{
#ifdef MAC_CARBON_PLUGINS
  if (GetEventModel() != NPEventModelCarbon)
    return;

  CancelTimer();

  if (mInstance) {
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
      EventRecord scrollEvent;
      InitializeEventRecord(&scrollEvent, nsnull);
      scrollEvent.what = NPEventType_ScrollingBeginsEvent;

      void* window = FixUpPluginWindow(ePluginPaintDisable);
      if (window) {
        mInstance->HandleEvent(&scrollEvent, nsnull);
      }
      pluginWidget->EndDrawPlugin();
    }
  }
#endif
}

void nsPluginInstanceOwner::ScrollPositionDidChange(nscoord aX, nscoord aY)
{
#ifdef MAC_CARBON_PLUGINS
  if (GetEventModel() != NPEventModelCarbon)
    return;

  if (mInstance) {
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
      EventRecord scrollEvent;
      InitializeEventRecord(&scrollEvent, nsnull);
      scrollEvent.what = NPEventType_ScrollingEndsEvent;

      void* window = FixUpPluginWindow(ePluginPaintEnable);
      if (window) {
        mInstance->HandleEvent(&scrollEvent, nsnull);
      }
      pluginWidget->EndDrawPlugin();
    }
  }
#endif
}

#ifdef MOZ_WIDGET_ANDROID

void nsPluginInstanceOwner::SendSize(int width, int height)
{
  if (!mInstance)
    return;

  PRInt32 model = mInstance->GetANPDrawingModel();

  if (model != kOpenGL_ANPDrawingModel)
    return;

  ANPEvent event;
  event.inSize = sizeof(ANPEvent);
  event.eventType = kDraw_ANPEventType;
  event.data.draw.model = kOpenGL_ANPDrawingModel;
  event.data.draw.data.surfaceSize.width = width;
  event.data.draw.data.surfaceSize.height = height;

  mInstance->HandleEvent(&event, nsnull);
}

bool nsPluginInstanceOwner::AddPluginView(const gfxRect& aRect)
{
  void* javaSurface = mInstance->GetJavaSurface();
  if (!javaSurface) {
    mInstance->RequestJavaSurface();
    return false;
  }

  JNIEnv* env = GetJNIForThread();
  if (!env)
    return false;

  AndroidBridge::AutoLocalJNIFrame frame(env, 1);

  jclass cls = env->FindClass("org/mozilla/gecko/GeckoAppShell");

#ifdef MOZ_JAVA_COMPOSITOR
  nsAutoString metadata;
  nsCOMPtr<nsIAndroidDrawMetadataProvider> metadataProvider =
      AndroidBridge::Bridge()->GetDrawMetadataProvider();
  metadataProvider->GetDrawMetadata(metadata);

  jstring jMetadata = env->NewString(nsPromiseFlatString(metadata).get(), metadata.Length());

  jmethodID method = env->GetStaticMethodID(cls,
                                            "addPluginView",
                                            "(Landroid/view/View;IIIILjava/lang/String;)V");

  env->CallStaticVoidMethod(cls,
                            method,
                            javaSurface,
                            (int)aRect.x,
                            (int)aRect.y,
                            (int)aRect.width,
                            (int)aRect.height,
                            jMetadata);
#else
  jmethodID method = env->GetStaticMethodID(cls,
                                            "addPluginView",
                                            "(Landroid/view/View;DDDD)V");

  env->CallStaticVoidMethod(cls,
                            method,
                            javaSurface,
                            aRect.x,
                            aRect.y,
                            aRect.width,
                            aRect.height);
#endif

  return true;
}

void nsPluginInstanceOwner::RemovePluginView()
{
  if (!mInstance || !mObjectFrame)
    return;

  void* surface = mInstance->GetJavaSurface();
  if (!surface)
    return;

  JNIEnv* env = GetJNIForThread();
  if (!env)
    return;

  AndroidBridge::AutoLocalJNIFrame frame(env, 1);

  jclass cls = env->FindClass("org/mozilla/gecko/GeckoAppShell");
  jmethodID method = env->GetStaticMethodID(cls,
                                            "removePluginView",
                                            "(Landroid/view/View;)V");
  env->CallStaticVoidMethod(cls, method, surface);
}

void nsPluginInstanceOwner::Invalidate() {
  NPRect rect;
  rect.left = rect.top = 0;
  rect.right = mPluginWindow->width;
  rect.bottom = mPluginWindow->height;
  InvalidateRect(&rect);
}

#endif

nsresult nsPluginInstanceOwner::DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent)
{
#ifdef MOZ_WIDGET_ANDROID
  {
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
    mInstance->HandleEvent(&event, nsnull);
  }
#endif

#ifndef XP_MACOSX
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow)) {
    // continue only for cases without child window
    return aFocusEvent->PreventDefault(); // consume event
  }
#endif

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aFocusEvent));
  if (privateEvent) {
    nsEvent * theEvent = privateEvent->GetInternalNSEvent();
    if (theEvent) {
      // we only care about the message in ProcessEvent
      nsGUIEvent focusEvent(NS_IS_TRUSTED_EVENT(theEvent), theEvent->message,
                            nsnull);
      nsEventStatus rv = ProcessEvent(focusEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aFocusEvent->PreventDefault();
        aFocusEvent->StopPropagation();
      }
    }
    else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, focusEvent null");   
  }
  else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, privateEvent null");   
  
  return NS_OK;
}    

#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
nsresult nsPluginInstanceOwner::Text(nsIDOMEvent* aTextEvent)
{
  if (mInstance) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aTextEvent));
    if (privateEvent) {
      nsEvent *event = privateEvent->GetInternalNSEvent();
      if (event && event->eventStructType == NS_TEXT_EVENT) {
        nsEventStatus rv = ProcessEvent(*static_cast<nsGUIEvent*>(event));
        if (nsEventStatus_eConsumeNoDefault == rv) {
          aTextEvent->PreventDefault();
          aTextEvent->StopPropagation();
        }
      }
      else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchTextToPlugin failed, textEvent null");
    }
    else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchTextToPlugin failed, privateEvent null");
  }

  return NS_OK;
}
#endif

nsresult nsPluginInstanceOwner::KeyPress(nsIDOMEvent* aKeyEvent)
{
#ifdef XP_MACOSX
#ifndef NP_NO_CARBON
  if (GetEventModel() == NPEventModelCarbon) {
    // KeyPress events are really synthesized keyDown events.
    // Here we check the native message of the event so that
    // we won't send the plugin two keyDown events.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aKeyEvent));
    if (privateEvent) {
      nsEvent *theEvent = privateEvent->GetInternalNSEvent();
      const EventRecord *ev;
      if (theEvent &&
          theEvent->message == NS_KEY_PRESS &&
          (ev = (EventRecord*)(((nsGUIEvent*)theEvent)->pluginEvent)) &&
          ev->what == keyDown)
        return aKeyEvent->PreventDefault(); // consume event
    }

    // Nasty hack to avoid recursive event dispatching with Java. Java can
    // dispatch key events to a TSM handler, which comes back and calls 
    // [ChildView insertText:] on the cocoa widget, which sends a key
    // event back down.
    static bool sInKeyDispatch = false;

    if (sInKeyDispatch)
      return aKeyEvent->PreventDefault(); // consume event

    sInKeyDispatch = true;
    nsresult rv =  DispatchKeyToPlugin(aKeyEvent);
    sInKeyDispatch = false;
    return rv;
  }
#endif

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
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aKeyEvent));
    if (privateEvent) {
      nsEvent *event = privateEvent->GetInternalNSEvent();
      if (event && event->eventStructType == NS_KEY_EVENT) {
        nsEventStatus rv = ProcessEvent(*static_cast<nsGUIEvent*>(event));
        if (nsEventStatus_eConsumeNoDefault == rv) {
          aKeyEvent->PreventDefault();
          aKeyEvent->StopPropagation();
        }
      }
      else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, keyEvent null");   
    }
    else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, privateEvent null");   
  }

  return NS_OK;
}    

nsresult
nsPluginInstanceOwner::MouseDown(nsIDOMEvent* aMouseEvent)
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

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
  if (privateEvent) {
    nsEvent* event = privateEvent->GetInternalNSEvent();
      if (event && event->eventStructType == NS_MOUSE_EVENT) {
        nsEventStatus rv = ProcessEvent(*static_cast<nsGUIEvent*>(event));
      if (nsEventStatus_eConsumeNoDefault == rv) {
        return aMouseEvent->PreventDefault(); // consume event
      }
    }
    else NS_ASSERTION(false, "nsPluginInstanceOwner::MouseDown failed, mouseEvent null");   
  }
  else NS_ASSERTION(false, "nsPluginInstanceOwner::MouseDown failed, privateEvent null");   
  
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

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
  if (privateEvent) {
    nsEvent* event = privateEvent->GetInternalNSEvent();
    if (event && event->eventStructType == NS_MOUSE_EVENT) {
      nsEventStatus rv = ProcessEvent(*static_cast<nsGUIEvent*>(event));
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aMouseEvent->PreventDefault();
        aMouseEvent->StopPropagation();
      }
    }
    else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchMouseToPlugin failed, mouseEvent null");   
  }
  else NS_ASSERTION(false, "nsPluginInstanceOwner::DispatchMouseToPlugin failed, privateEvent null");   
  
  return NS_OK;
}

nsresult
nsPluginInstanceOwner::HandleEvent(nsIDOMEvent* aEvent)
{
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
    return MouseDown(aEvent);
  }
  if (eventType.EqualsLiteral("mouseup")) {
    // Don't send a mouse-up event to the plugin if it isn't focused.  This can
    // happen if the previous mouse-down was sent to a DOM element above the
    // plugin, the mouse is still above the plugin, and the mouse-down event
    // caused the element to disappear.  See bug 627649.
    if (!mContentFocused) {
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
    return KeyPress(aEvent);
  }
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
  if (eventType.EqualsLiteral("text")) {
    return Text(aEvent);
  }
#endif

  nsCOMPtr<nsIDOMDragEvent> dragEvent(do_QueryInterface(aEvent));
  if (dragEvent && mInstance) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aEvent));
    if (privateEvent) {
      nsEvent* ievent = privateEvent->GetInternalNSEvent();
      if ((ievent && NS_IS_TRUSTED_EVENT(ievent)) &&
           ievent->message != NS_DRAGDROP_ENTER && ievent->message != NS_DRAGDROP_OVER) {
        aEvent->PreventDefault();
      }

      // Let the plugin handle drag events.
      aEvent->StopPropagation();
    }
  }
  return NS_OK;
}

#ifdef MOZ_X11
static unsigned int XInputEventState(const nsInputEvent& anEvent)
{
  unsigned int state = 0;
  if (anEvent.isShift) state |= ShiftMask;
  if (anEvent.isControl) state |= ControlMask;
  if (anEvent.isAlt) state |= Mod1Mask;
  if (anEvent.isMeta) state |= Mod4Mask;
  return state;
}
#endif

nsEventStatus nsPluginInstanceOwner::ProcessEvent(const nsGUIEvent& anEvent)
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
#ifndef NP_NO_CARBON
  EventRecord synthCarbonEvent;
#endif
  NPCocoaEvent synthCocoaEvent;
  void* event = anEvent.pluginEvent;
  nsPoint pt =
  nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mObjectFrame) -
  mObjectFrame->GetContentRectRelativeToSelf().TopLeft();
  nsPresContext* presContext = mObjectFrame->PresContext();
  nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x),
                  presContext->AppUnitsToDevPixels(pt.y));
#ifndef NP_NO_CARBON
  nsIntPoint geckoScreenCoords = mWidget->WidgetToScreenOffset();
  ::Point carbonPt = { static_cast<short>(ptPx.y + geckoScreenCoords.y),
                       static_cast<short>(ptPx.x + geckoScreenCoords.x) };
  if (eventModel == NPEventModelCarbon) {
    if (event && anEvent.eventStructType == NS_MOUSE_EVENT) {
      static_cast<EventRecord*>(event)->where = carbonPt;
    }
  }
#endif
  if (!event) {
#ifndef NP_NO_CARBON
    if (eventModel == NPEventModelCarbon) {
      InitializeEventRecord(&synthCarbonEvent, &carbonPt);
    } else
#endif
    {
      InitializeNPCocoaEvent(&synthCocoaEvent);
    }
    
    switch (anEvent.message) {
      case NS_FOCUS_CONTENT:
      case NS_BLUR_CONTENT:
#ifndef NP_NO_CARBON
        if (eventModel == NPEventModelCarbon) {
          synthCarbonEvent.what = (anEvent.message == NS_FOCUS_CONTENT) ?
          NPEventType_GetFocusEvent : NPEventType_LoseFocusEvent;
          event = &synthCarbonEvent;
        }
#endif
        break;
      case NS_MOUSE_MOVE:
      {
        // Ignore mouse-moved events that happen as part of a dragging
        // operation that started over another frame.  See bug 525078.
        nsRefPtr<nsFrameSelection> frameselection = mObjectFrame->GetFrameSelection();
        if (!frameselection->GetMouseDownState() ||
            (nsIPresShell::GetCapturingContent() == mObjectFrame->GetContent())) {
#ifndef NP_NO_CARBON
          if (eventModel == NPEventModelCarbon) {
            synthCarbonEvent.what = osEvt;
            event = &synthCarbonEvent;
          } else
#endif
          {
            synthCocoaEvent.type = NPCocoaEventMouseMoved;
            synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
            synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
            event = &synthCocoaEvent;
          }
        }
      }
        break;
      case NS_MOUSE_BUTTON_DOWN:
#ifndef NP_NO_CARBON
        if (eventModel == NPEventModelCarbon) {
          synthCarbonEvent.what = mouseDown;
          event = &synthCarbonEvent;
        } else
#endif
        {
          synthCocoaEvent.type = NPCocoaEventMouseDown;
          synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
          synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
          event = &synthCocoaEvent;
        }
        break;
      case NS_MOUSE_BUTTON_UP:
        // If we're in a dragging operation that started over another frame,
        // either ignore the mouse-up event (in the Carbon Event Model) or
        // convert it into a mouse-entered event (in the Cocoa Event Model).
        // See bug 525078.
        if ((static_cast<const nsMouseEvent&>(anEvent).button == nsMouseEvent::eLeftButton) &&
            (nsIPresShell::GetCapturingContent() != mObjectFrame->GetContent())) {
          if (eventModel == NPEventModelCocoa) {
            synthCocoaEvent.type = NPCocoaEventMouseEntered;
            synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
            synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
            event = &synthCocoaEvent;
          }
        } else {
#ifndef NP_NO_CARBON
          if (eventModel == NPEventModelCarbon) {
            synthCarbonEvent.what = mouseUp;
            event = &synthCarbonEvent;
          } else
#endif
          {
            synthCocoaEvent.type = NPCocoaEventMouseUp;
            synthCocoaEvent.data.mouse.pluginX = static_cast<double>(ptPx.x);
            synthCocoaEvent.data.mouse.pluginY = static_cast<double>(ptPx.y);
            event = &synthCocoaEvent;
          }
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

#ifndef NP_NO_CARBON
  // Work around an issue in the Flash plugin, which can cache a pointer
  // to a doomed TSM document (one that belongs to a NSTSMInputContext)
  // and try to activate it after it has been deleted. See bug 183313.
  if (eventModel == NPEventModelCarbon && anEvent.message == NS_FOCUS_CONTENT)
    ::DeactivateTSMDocument(::TSMGetActiveDocument());
#endif

  PRInt16 response = kNPEventNotHandled;
  void* window = FixUpPluginWindow(ePluginPaintEnable);
  if (window || (eventModel == NPEventModelCocoa)) {
    mInstance->HandleEvent(event, &response);
  }

  if (eventModel == NPEventModelCocoa && response == kNPEventStartIME) {
    pluginWidget->StartComplexTextInputForCurrentEvent();
  }

  if ((response == kNPEventHandled || response == kNPEventStartIME) &&
      !(anEvent.eventStructType == NS_MOUSE_EVENT &&
        anEvent.message == NS_MOUSE_BUTTON_DOWN &&
        static_cast<const nsMouseEvent&>(anEvent).button == nsMouseEvent::eLeftButton &&
        !mContentFocused))
    rv = nsEventStatus_eConsumeNoDefault;

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
      const nsMouseEvent* mouseEvent = static_cast<const nsMouseEvent*>(&anEvent);
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
    PRInt16 response = kNPEventNotHandled;
    mInstance->HandleEvent(pPluginEvent, &response);
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
        const nsMouseEvent& mouseEvent =
          static_cast<const nsMouseEvent&>(anEvent);
        // Get reference point relative to screen:
        nsIntPoint rootPoint(-1,-1);
        if (widget)
          rootPoint = anEvent.refPoint + widget->WidgetToScreenOffset();
#ifdef MOZ_WIDGET_GTK2
        Window root = GDK_ROOT_WINDOW();
#elif defined(MOZ_WIDGET_QT)
        Window root = QX11Info::appRootWindow();
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
                case nsMouseEvent::eMiddleButton:
                  event.button = 2;
                  break;
                case nsMouseEvent::eRightButton:
                  event.button = 3;
                  break;
                default: // nsMouseEvent::eLeftButton;
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
#ifdef MOZ_WIDGET_GTK2
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
          const nsKeyEvent& keyEvent = static_cast<const nsKeyEvent&>(anEvent);

          memset( &event, 0, sizeof(event) );
          event.time = anEvent.time;

          QWidget* qWidget = static_cast<QWidget*>(widget->GetNativeData(NS_NATIVE_WINDOW));
          if (qWidget)
            event.root = qWidget->x11Info().appRootWindow();

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
              event.keycode = XKeysymToKeycode( (widget ? static_cast<Display*>(widget->GetNativeData(NS_NATIVE_DISPLAY)) : nsnull), qtEvent->key());
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
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
          bool handled;
          if (NS_SUCCEEDED(mInstance->HandleGUIEvent(anEvent, &handled)) &&
              handled) {
            rv = nsEventStatus_eConsumeNoDefault;
          }
#else
          NS_WARNING("Synthesized key event not sent to plugin");
#endif
        }
      break;

#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
   case NS_TEXT_EVENT:
        {
          bool handled;
          if (NS_SUCCEEDED(mInstance->HandleGUIEvent(anEvent, &handled)) &&
              handled) {
            rv = nsEventStatus_eConsumeNoDefault;
          }
        }
      break;
#endif
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
    static_cast<Display*>(widget->GetNativeData(NS_NATIVE_DISPLAY)) : nsnull;
  event.window = None; // not a real window
  // information lost:
  event.serial = 0;
  event.send_event = False;

  PRInt16 response = kNPEventNotHandled;
  mInstance->HandleEvent(&pluginEvent, &response);
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
              mInstance->HandleEvent(&event, nsnull);
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
              mInstance->HandleEvent(&event, nsnull);
            }
            break;
          }
      }
      break;

    case NS_KEY_EVENT:
     {
       const nsKeyEvent& keyEvent = static_cast<const nsKeyEvent&>(anEvent);
       LOG("Firing NS_KEY_EVENT %d %d\n", keyEvent.keyCode, keyEvent.charCode);
       
       int modifiers = 0;
       if (keyEvent.isShift)
         modifiers |= kShift_ANPKeyModifier;
       if (keyEvent.isAlt)
         modifiers |= kAlt_ANPKeyModifier;

       ANPEvent event;
       event.inSize = sizeof(ANPEvent);
       event.eventType = kKey_ANPEventType;
       event.data.key.nativeCode = keyEvent.keyCode;
       event.data.key.virtualCode = keyEvent.charCode;
       event.data.key.modifiers = modifiers;
       event.data.key.repeatCount = 0;
       event.data.key.unichar = 0;
       switch (anEvent.message)
         {
         case NS_KEY_DOWN:
           event.data.key.action = kDown_ANPKeyAction;
           mInstance->HandleEvent(&event, nsnull);
           break;
           
         case NS_KEY_UP:
           event.data.key.action = kUp_ANPKeyAction;
           mInstance->HandleEvent(&event, nsnull);
           break;
         }
     }
    }
    rv = nsEventStatus_eConsumeNoDefault;
#endif
 
  return rv;
}

nsresult
nsPluginInstanceOwner::Destroy()
{
  if (mObjectFrame)
    mObjectFrame->SetInstanceOwner(nsnull);

#ifdef MAC_CARBON_PLUGINS
  // stop the timer explicitly to reduce reference count.
  CancelTimer();
#endif
#ifdef XP_MACOSX
  RemoveFromCARefreshTimer();
  if (mColorProfile)
    ::CGColorSpaceRelease(mColorProfile);  
#endif

  // unregister context menu listener
  if (mCXMenuListener) {
    mCXMenuListener->Destroy(mContent);
    mCXMenuListener = nsnull;
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
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
  mContent->RemoveEventListener(NS_LITERAL_STRING("text"), this, true);
#endif

#if MOZ_WIDGET_ANDROID
  RemovePluginView();

  if (mLayer)
    mLayer->SetVisible(false);

#endif

  if (mWidget) {
    if (mPluginWindow) {
      mPluginWindow->SetPluginWidget(nsnull);
    }

    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget) {
      pluginWidget->SetPluginInstanceOwner(nsnull);
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
 
  nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
  if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
#ifndef NP_NO_CARBON
    void* window = FixUpPluginWindow(ePluginPaintEnable);
    if (GetEventModel() == NPEventModelCarbon && window) {
      EventRecord updateEvent;
      InitializeEventRecord(&updateEvent, nsnull);
      updateEvent.what = updateEvt;
      updateEvent.message = UInt32(window);

      mInstance->HandleEvent(&updateEvent, nsnull);
    } else if (GetEventModel() == NPEventModelCocoa)
#endif
    {
      DoCocoaEventDrawRect(aDirtyRect, cgContext);
    }
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

  mInstance->HandleEvent(&updateEvent, nsnull);
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
  mInstance->HandleEvent(&pluginEvent, nsnull);
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
  pluginEvent.wParam = (uint32)aHPS;
  pluginEvent.lParam = (uint32)&rectl;
  mInstance->HandleEvent(&pluginEvent, nsnull);
}
#endif

#ifdef MOZ_WIDGET_ANDROID

void nsPluginInstanceOwner::Paint(gfxContext* aContext,
                                  const gfxRect& aFrameRect,
                                  const gfxRect& aDirtyRect)
{
  if (!mInstance || !mObjectFrame || !mPluginDocumentActiveState)
    return;

  PRInt32 model = mInstance->GetANPDrawingModel();

  float xResolution = mObjectFrame->PresContext()->GetRootPresContext()->PresShell()->GetXResolution();
  float yResolution = mObjectFrame->PresContext()->GetRootPresContext()->PresShell()->GetYResolution();

  gfxRect scaledFrameRect = aFrameRect;
  scaledFrameRect.Scale(xResolution, yResolution);

  if (model == kSurface_ANPDrawingModel) {
    if (!AddPluginView(scaledFrameRect)) {
      Invalidate();
    }
    return;
  }

  if (model == kOpenGL_ANPDrawingModel) {
    if (!mLayer)
      mLayer = new AndroidMediaLayer();

    mLayer->UpdatePosition(scaledFrameRect, xResolution);

    SendSize((int)scaledFrameRect.width, (int)scaledFrameRect.height);
    return;
  }

  if (model != kBitmap_ANPDrawingModel)
    return;

#ifdef ANP_BITMAP_DRAWING_MODEL
  static nsRefPtr<gfxImageSurface> pluginSurface;

  if (pluginSurface == nsnull ||
      aFrameRect.width  != pluginSurface->Width() ||
      aFrameRect.height != pluginSurface->Height()) {

    pluginSurface = new gfxImageSurface(gfxIntSize(aFrameRect.width, aFrameRect.height), 
                                        gfxImageSurface::ImageFormatARGB32);
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
    
  mInstance->HandleEvent(&event, nsnull);

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
  nsIntRect pluginDirtyRect(PRInt32(dirtyRect.x),
                            PRInt32(dirtyRect.y),
                            PRInt32(dirtyRect.width),
                            PRInt32(dirtyRect.height));
  if (!pluginDirtyRect.
      IntersectRect(nsIntRect(0, 0, pluginSize.width, pluginSize.height),
                    pluginDirtyRect))
    return;

  NPWindow* window;
  GetWindow(window);

  PRUint32 rendererFlags = 0;
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
#ifdef MOZ_WIDGET_GTK2
  // This is the visual used by the widgets, 24-bit if available.
  GdkVisual* gdkVisual = gdk_rgb_get_visual();
  Visual* visual = gdk_x11_visual_get_xvisual(gdkVisual);
  Screen* screen =
    gdk_x11_screen_get_xscreen(gdk_visual_get_screen(gdkVisual));
#endif
#ifdef MOZ_WIDGET_QT
  Display* dpy = QX11Info().display();
  Screen* screen = ScreenOfDisplay(dpy, QX11Info().screen());
  Visual* visual = static_cast<Visual*>(QX11Info().visual());
#endif
  renderer.Draw(aContext, nsIntSize(window->width, window->height),
                rendererFlags, screen, visual, nsnull);
}
nsresult
nsPluginInstanceOwner::Renderer::DrawWithXlib(gfxXlibSurface* xsurface, 
                                              nsIntPoint offset,
                                              nsIntRect *clipRects, 
                                              PRUint32 numClipRects)
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

    instance->HandleEvent(&pluginEvent, nsnull);
  }
  return NS_OK;
}
#endif

void nsPluginInstanceOwner::SendIdleEvent()
{
#ifdef MAC_CARBON_PLUGINS
  // validate the plugin clipping information by syncing the plugin window info to
  // reflect the current widget location. This makes sure that everything is updated
  // correctly in the event of scrolling in the window.
  if (mInstance) {
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
      void* window = FixUpPluginWindow(ePluginPaintEnable);
      if (window) {
        EventRecord idleEvent;
        InitializeEventRecord(&idleEvent, nsnull);
        idleEvent.what = nullEvent;

        // give a bogus 'where' field of our null event when hidden, so Flash
        // won't respond to mouse moves in other tabs, see bug 120875
        if (!mWidgetVisible)
          idleEvent.where.h = idleEvent.where.v = 20000;

        mInstance->HandleEvent(&idleEvent, nsnull);
      }

      pluginWidget->EndDrawPlugin();
    }
  }
#endif
}

#ifdef MAC_CARBON_PLUGINS
void nsPluginInstanceOwner::StartTimer(bool isVisible)
{
  if (GetEventModel() != NPEventModelCarbon)
    return;

  mPluginHost->AddIdleTimeTarget(this, isVisible);
}

void nsPluginInstanceOwner::CancelTimer()
{
  mPluginHost->RemoveIdleTimeTarget(this);
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
    return NS_ERROR_FAILURE;
  }

  // register context menu listener
  mCXMenuListener = new nsPluginDOMContextMenuListener();
  if (mCXMenuListener) {    
    mCXMenuListener->Init(aContent);
  }

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
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
  mContent->AddEventListener(NS_LITERAL_STRING("text"), this, true);
#endif

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
    nsIDocument *doc = nsnull;
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
    rv = mWidget->Create(parentWidget.get(), nsnull, nsIntRect(0,0,0,0),
                         nsnull, nsnull, &initData);
    if (NS_FAILED(rv)) {
      mWidget->Destroy();
      mWidget = nsnull;
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
    mPluginWindow->window = nsnull;
#ifdef MOZ_X11
    // Fill in the display field.
    NPSetWindowCallbackStruct* ws_info = 
    static_cast<NPSetWindowCallbackStruct*>(mPluginWindow->ws_info);
    ws_info->display = DefaultXDisplay();
    
    nsCAutoString description;
    GetPluginDescription(description);
    NS_NAMED_LITERAL_CSTRING(flash10Head, "Shockwave Flash 10.");
    mFlash10Quirks = StringBeginsWith(description, flash10Head);
#endif
  } else if (mWidget) {
    // mPluginWindow->type is used in |GetPluginPort| so it must
    // be initialized first
    mPluginWindow->type = NPWindowTypeWindow;
    mPluginWindow->window = GetPluginPortFromWidget();
#ifdef MAC_CARBON_PLUGINS
    // start the idle timer.
    StartTimer(true);
#endif
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

void* nsPluginInstanceOwner::FixUpPluginWindow(PRInt32 inPaintState)
{
  if (!mWidget || !mPluginWindow || !mInstance || !mObjectFrame)
    return nsnull;

  NPDrawingModel drawingModel = GetDrawingModel();
  NPEventModel eventModel = GetEventModel();

  nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
  if (!pluginWidget)
    return nsnull;

  // If we've already set up a CGContext in nsObjectFrame::PaintPlugin(), we
  // don't want calls to SetPluginPortAndDetectChange() to step on our work.
  void* pluginPort = nsnull;
  if (mInCGPaintLevel > 0) {
    pluginPort = mPluginWindow->window;
  } else {
    pluginPort = SetPluginPortAndDetectChange();
  }

#ifdef MAC_CARBON_PLUGINS
  if (eventModel == NPEventModelCarbon && !pluginPort)
    return nsnull;
#endif

  // We'll need the top-level Cocoa window for the Cocoa event model.
  void* cocoaTopLevelWindow = nsnull;
  if (eventModel == NPEventModelCocoa) {
    nsIWidget* widget = mObjectFrame->GetNearestWidget();
    if (!widget)
      return nsnull;
    cocoaTopLevelWindow = widget->GetNativeData(NS_NATIVE_WINDOW);
    if (!cocoaTopLevelWindow)
      return nsnull;
  }

  nsIntPoint pluginOrigin;
  nsIntRect widgetClip;
  bool widgetVisible;
  pluginWidget->GetPluginClipRect(widgetClip, pluginOrigin, widgetVisible);
  mWidgetVisible = widgetVisible;

  // printf("GetPluginClipRect returning visible %d\n", widgetVisible);

#ifndef NP_NO_QUICKDRAW
  // set the port coordinates
  if (drawingModel == NPDrawingModelQuickDraw) {
    mPluginWindow->x = -static_cast<NP_Port*>(pluginPort)->portx;
    mPluginWindow->y = -static_cast<NP_Port*>(pluginPort)->porty;
  }
  else if (drawingModel == NPDrawingModelCoreGraphics || 
           drawingModel == NPDrawingModelCoreAnimation ||
           drawingModel == NPDrawingModelInvalidatingCoreAnimation)
#endif
  {
    // This would be a lot easier if we could use obj-c here,
    // but we can't. Since we have only nsIWidget and we can't
    // use its native widget (an obj-c object) we have to go
    // from the widget's screen coordinates to its window coords
    // instead of straight to window coords.
    nsIntPoint geckoScreenCoords = mWidget->WidgetToScreenOffset();

    nsRect windowRect;
#ifndef NP_NO_CARBON
    if (eventModel == NPEventModelCarbon)
      NS_NPAPI_CarbonWindowFrame(static_cast<WindowRef>(static_cast<NP_CGContext*>(pluginPort)->window), windowRect);
    else
#endif
    {
      NS_NPAPI_CocoaWindowFrame(cocoaTopLevelWindow, windowRect);
    }

    mPluginWindow->x = geckoScreenCoords.x - windowRect.x;
    mPluginWindow->y = geckoScreenCoords.y - windowRect.y;
  }

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
      mPluginWindow->clipRect.bottom  != oldClipRect.bottom)
  {
    if (UseAsyncRendering()) {
      mInstance->AsyncSetWindow(mPluginWindow);
    }
    else {
      mPluginWindow->CallSetWindow(mInstance);
    }
    mPluginPortChanged = false;
#ifdef MAC_CARBON_PLUGINS
    // if the clipRect is of size 0, make the null timer fire less often
    CancelTimer();
    if (mPluginWindow->clipRect.left == mPluginWindow->clipRect.right ||
        mPluginWindow->clipRect.top == mPluginWindow->clipRect.bottom) {
      StartTimer(false);
    }
    else {
      StartTimer(true);
    }
#endif
  } else if (mPluginPortChanged) {
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
  if (eventModel == NPEventModelCocoa && !mSentInitialTopLevelWindowEvent) {
    // Set this before calling ProcessEvent to avoid endless recursion.
    mSentInitialTopLevelWindowEvent = true;

    nsPluginEvent pluginEvent(true, NS_PLUGIN_FOCUS_EVENT, nsnull);
    NPCocoaEvent cocoaEvent;
    InitializeNPCocoaEvent(&cocoaEvent);
    cocoaEvent.type = NPCocoaEventWindowFocusChanged;
    cocoaEvent.data.focus.hasFocus = NS_NPAPI_CocoaWindowIsMain(cocoaTopLevelWindow);
    pluginEvent.pluginEvent = &cocoaEvent;
    ProcessEvent(pluginEvent);
  }

#ifndef NP_NO_QUICKDRAW
  if (drawingModel == NPDrawingModelQuickDraw)
    return ::GetWindowFromPort(static_cast<NP_Port*>(pluginPort)->port);
#endif

#ifdef MAC_CARBON_PLUGINS
  if (drawingModel == NPDrawingModelCoreGraphics && eventModel == NPEventModelCarbon)
    return static_cast<NP_CGContext*>(pluginPort)->window;
#endif

  return nsnull;
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
    if (mLayer)
      mLayer->SetVisible(mPluginDocumentActiveState);

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

void nsPluginInstanceOwner::SetFrame(nsObjectFrame *aFrame)
{
  // Don't do anything if the frame situation hasn't changed.
  if (mObjectFrame == aFrame) {
    return;
  }

  // If we already have a frame that is changing or going away...
  if (mObjectFrame) {
    // We have an old frame.
    // Drop image reference because the child may destroy the surface after we return.
    nsRefPtr<ImageContainer> container = mObjectFrame->GetImageContainer();
    if (container) {
#ifdef XP_MACOSX
      AutoLockImage autoLock(container);
      Image *image = autoLock.GetImage();
      if (image && (image->GetFormat() == Image::MAC_IO_SURFACE) && mObjectFrame) {
        // Undo what we did to the current image in SetCurrentImage().
        MacIOSurfaceImage *oglImage = static_cast<MacIOSurfaceImage*>(image);
        oglImage->SetUpdateCallback(nsnull, nsnull);
        oglImage->SetDestroyCallback(nsnull);
        // If we have a current image here, its destructor hasn't yet been
        // called, so OnDestroyImage() can't yet have been called.  So we need
        // to do ourselves what OnDestroyImage() would have done.
        NS_RELEASE_THIS();
      }
      // Important! Unlock here otherwise SetCurrentImage will deadlock with
      // our lock if we have a RemoteImage.
      autoLock.Unlock();
#endif
      container->SetCurrentImage(nsnull);
    }

    // Scroll position listening is only required for Carbon event model plugins on Mac OS X.
#if defined(XP_MACOSX) && !defined(NP_NO_QUICKDRAW)
    // Our frame is changing or going away, unregister for a scroll position listening.
    // It's OK to unregister when we didn't register, so don't be strict about unregistering.
    // Better to unregister when we didn't have to than to not unregister when we should.
    for (nsIFrame* f = mObjectFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
      nsIScrollableFrame* sf = do_QueryFrame(f);
      if (sf) {
        sf->RemoveScrollPositionListener(this);
      }
    }
#endif

    // Make sure the old frame isn't holding a reference to us.
    mObjectFrame->SetInstanceOwner(nsnull);
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
    mObjectFrame->Invalidate(mObjectFrame->GetContentRectRelativeToSelf());

    // Scroll position listening is only required for Carbon event model plugins on Mac OS X.
#if defined(XP_MACOSX) && !defined(NP_NO_QUICKDRAW)
    // We need to register as a scroll position listener on every scrollable frame up to the top.
    if (GetEventModel() == NPEventModelCarbon) {
      for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
        nsIScrollableFrame* sf = do_QueryFrame(f);
        if (sf) {
          sf->AddScrollPositionListener(this);
        }
      }
    }
#endif
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
  if (name.LowerCaseEqualsLiteral("pluginurl") ||
      name.LowerCaseEqualsLiteral("pluginspage")) {        
    nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
    nsAutoString newURL;
    NS_MakeAbsoluteURI(newURL, value, baseURI);
    if (!newURL.IsEmpty())
      value = newURL;
  }
}

// nsPluginDOMContextMenuListener class implementation

nsPluginDOMContextMenuListener::nsPluginDOMContextMenuListener()
{
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

nsresult nsPluginDOMContextMenuListener::Init(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMEventTarget> receiver(do_QueryInterface(aContent));
  if (receiver) {
    receiver->AddEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
    return NS_OK;
  }
  
  return NS_ERROR_NO_INTERFACE;
}

nsresult nsPluginDOMContextMenuListener::Destroy(nsIContent* aContent)
{
  // Unregister context menu listener
  nsCOMPtr<nsIDOMEventTarget> receiver(do_QueryInterface(aContent));
  if (receiver) {
    receiver->RemoveEventListener(NS_LITERAL_STRING("contextmenu"), this, true);
  }
  
  return NS_OK;
}
