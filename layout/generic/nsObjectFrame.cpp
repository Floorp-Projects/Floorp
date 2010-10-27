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

/* rendering objects for replaced elements implemented by a plugin */

#ifdef MOZ_WIDGET_QT
#include <QWidget>
#include <QKeyEvent>
#ifdef MOZ_X11
#include <QX11Info>
#endif
#endif

#ifdef MOZ_IPC
#include "mozilla/plugins/PluginMessageUtils.h"
#endif

#ifdef MOZ_X11
#include <cairo-xlib.h>
#include "gfxXlibSurface.h"
/* X headers suck */
enum { XKeyPress = KeyPress };
#ifdef KeyPress
#undef KeyPress
#endif
#endif

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMDragEvent.h"
#include "nsIPluginHost.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prmem.h"
#include "nsGkAtoms.h"
#include "nsIAppShell.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIPluginInstance.h"
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
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMContextMenuListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDocumentEncoder.h"
#include "nsXPIDLString.h"
#include "nsIDOMRange.h"
#include "nsIPluginWidget.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "npapi.h"
#include "nsTransform2D.h"
#include "nsIImageLoadingContent.h"
#include "nsIObjectLoadingContent.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
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

#include "nsThreadUtils.h"

#include "gfxContext.h"
#include "gfxPlatform.h"

#ifdef XP_WIN
#include "gfxWindowsNativeDrawing.h"
#include "gfxWindowsSurface.h"
#endif

#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "Layers.h"

// accessibility support
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1 /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include <errno.h>

#include "nsContentCID.h"
static NS_DEFINE_CID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef XP_MACOSX
#include "gfxQuartzNativeDrawing.h"
#include "nsPluginUtilsOSX.h"
#include "nsCoreAnimationSupport.h"
#endif

#ifdef MOZ_X11
#if (MOZ_PLATFORM_MAEMO == 5) && defined(MOZ_WIDGET_GTK2)
#define MOZ_COMPOSITED_PLUGINS 1
#define MOZ_USE_IMAGE_EXPOSE   1
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "gfxXlibNativeRenderer.h"
#endif
#endif

#ifdef MOZ_WIDGET_QT
#include "gfxQtNativeRenderer.h"
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
#endif

#ifdef CreateEvent // Thank you MS.
#undef CreateEvent
#endif

#ifdef PR_LOGGING 
static PRLogModuleInfo *nsObjectFrameLM = PR_NewLogModule("nsObjectFrame");
#endif /* PR_LOGGING */

#if defined(XP_MACOSX) && !defined(NP_NO_CARBON)
#define MAC_CARBON_PLUGINS
#endif

using namespace mozilla::layers;

// special class for handeling DOM context menu events because for
// some reason it starves other mouse events if implemented on the
// same class
class nsPluginDOMContextMenuListener : public nsIDOMContextMenuListener
{
public:
  nsPluginDOMContextMenuListener();
  virtual ~nsPluginDOMContextMenuListener();

  NS_DECL_ISUPPORTS

  NS_IMETHOD ContextMenu(nsIDOMEvent* aContextMenuEvent);
  
  nsresult Init(nsIContent* aContent);
  nsresult Destroy(nsIContent* aContent);
  
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent)
  {
    return NS_OK;
  }
  nsEventStatus ProcessEvent(const nsGUIEvent& anEvent)
  {
    return nsEventStatus_eConsumeNoDefault;
  }
};


class nsPluginInstanceOwner : public nsIPluginInstanceOwner,
                              public nsIPluginTagInfo,
                              public nsIDOMMouseListener,
                              public nsIDOMMouseMotionListener,
                              public nsIDOMKeyListener,
                              public nsIDOMFocusListener,
                              public nsIScrollPositionListener
{
public:
  nsPluginInstanceOwner();
  virtual ~nsPluginInstanceOwner();

  NS_DECL_ISUPPORTS

  //nsIPluginInstanceOwner interface
  NS_DECL_NSIPLUGININSTANCEOWNER

  NS_IMETHOD GetURL(const char *aURL, const char *aTarget,
                    nsIInputStream *aPostStream, 
                    void *aHeadersData, PRUint32 aHeadersDataLen);

  NS_IMETHOD ShowStatus(const PRUnichar *aStatusMsg);

  NPError    ShowNativeContextMenu(NPMenu* menu, void* event);

  NPBool     ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                          double *destX, double *destY, NPCoordinateSpace destSpace);

  //nsIPluginTagInfo interface
  NS_DECL_NSIPLUGINTAGINFO

  // nsIDOMMouseListener interfaces 
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);     

  // nsIDOMMouseMotionListener interfaces
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  // nsIDOMKeyListener interfaces
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);

  // nsIDOMFocusListener interfaces
  NS_IMETHOD Focus(nsIDOMEvent * aFocusEvent);
  NS_IMETHOD Blur(nsIDOMEvent * aFocusEvent);

  nsresult Destroy();  

  void PrepareToStop(PRBool aDelayedStop);
  
#ifdef XP_WIN
  void Paint(const RECT& aDirty, HDC aDC);
#elif defined(XP_MACOSX)
  void Paint(const gfxRect& aDirtyRect, CGContextRef cgContext);  
  void RenderCoreAnimation(CGContextRef aCGContext, int aWidth, int aHeight);
#elif defined(MOZ_X11)
  void Paint(gfxContext* aContext,
             const gfxRect& aFrameRect,
             const gfxRect& aDirtyRect);
#elif defined(XP_OS2)
  void Paint(const nsRect& aDirtyRect, HPS aHPS);
#endif

#ifdef MAC_CARBON_PLUGINS
  void CancelTimer();
  void StartTimer(PRBool isVisible);
#endif
  void SendIdleEvent();

  // nsIScrollPositionListener interface
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY);
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY);

  //locals

  nsresult Init(nsPresContext* aPresContext, nsObjectFrame* aFrame,
                nsIContent* aContent);

  void* GetPluginPortFromWidget();
  void ReleasePluginPort(void* pluginPort);

  void SetPluginHost(nsIPluginHost* aHost);

  nsEventStatus ProcessEvent(const nsGUIEvent & anEvent);

#ifdef XP_MACOSX
  NPDrawingModel GetDrawingModel();
  NPEventModel GetEventModel();
  static void CARefresh(nsITimer *aTimer, void *aClosure);
  static void AddToCARefreshTimer(nsPluginInstanceOwner *aPluginInstance);
  static void RemoveFromCARefreshTimer(nsPluginInstanceOwner *aPluginInstance);
  void SetupCARefresh();
  void* FixUpPluginWindow(PRInt32 inPaintState);
  // Set a flag that (if true) indicates the plugin port info has changed and
  // SetWindow() needs to be called.
  void SetPluginPortChanged(PRBool aState) { mPluginPortChanged = aState; }
  // Return a pointer to the internal nsPluginPort structure that's used to
  // store a copy of plugin port info and to detect when it's been changed.
  void* GetPluginPortCopy();
  // Set plugin port info in the plugin (in the 'window' member of the
  // NPWindow structure passed to the plugin by SetWindow()) and set a
  // flag (mPluginPortChanged) to indicate whether or not this info has
  // changed, and SetWindow() needs to be called again.
  void* SetPluginPortAndDetectChange();
  // Flag when we've set up a Thebes (and CoreGraphics) context in
  // nsObjectFrame::PaintPlugin().  We need to know this in
  // FixUpPluginWindow() (i.e. we need to know when FixUpPluginWindow() has
  // been called from nsObjectFrame::PaintPlugin() when we're using the
  // CoreGraphics drawing model).
  void BeginCGPaint();
  void EndCGPaint();
#endif

  void SetOwner(nsObjectFrame *aOwner)
  {
    mObjectFrame = aOwner;
  }

  PRUint32 GetLastEventloopNestingLevel() const {
    return mLastEventloopNestingLevel; 
  }

  static PRUint32 GetEventloopNestingLevel();
      
  void ConsiderNewEventloopNestingLevel() {
    PRUint32 currentLevel = GetEventloopNestingLevel();

    if (currentLevel < mLastEventloopNestingLevel) {
      mLastEventloopNestingLevel = currentLevel;
    }
  }

  const char* GetPluginName()
  {
    if (mInstance && mPluginHost) {
      const char* name = NULL;
      if (NS_SUCCEEDED(mPluginHost->GetPluginName(mInstance, &name)) && name)
        return name;
    }
    return "";
  }

#ifdef MOZ_X11
  void GetPluginDescription(nsACString& aDescription)
  {
    aDescription.Truncate();
    if (mInstance && mPluginHost) {
      nsCOMPtr<nsIPluginTag> pluginTag;
      mPluginHost->GetPluginTagForInstance(mInstance,
                                           getter_AddRefs(pluginTag));
      if (pluginTag) {
        pluginTag->GetDescription(aDescription);
      }
    }
  }
#endif

  PRBool SendNativeEvents()
  {
#ifdef XP_WIN
    return mPluginWindow->type == NPWindowTypeDrawable &&
           MatchPluginName("Shockwave Flash");
#elif defined(MOZ_X11) || defined(XP_MACOSX)
    return PR_TRUE;
#else
    return PR_FALSE;
#endif
  }

  PRBool MatchPluginName(const char *aPluginName)
  {
    return strncmp(GetPluginName(), aPluginName, strlen(aPluginName)) == 0;
  }

#ifdef MOZ_USE_IMAGE_EXPOSE
  nsresult SetAbsoluteScreenPosition(nsIDOMElement* element,
                                     nsIDOMClientRect* position,
                                     nsIDOMClientRect* clip);
#endif

  void NotifyPaintWaiter(nsDisplayListBuilder* aBuilder,
                         LayerManager* aManager);
  // Return true if we set image with valid surface
  PRBool SetCurrentImage(ImageContainer* aContainer);

  PRBool UseLayers()
  {
    PRBool useAsyncRendering;
    return (mInstance &&
            NS_SUCCEEDED(mInstance->UseAsyncPainting(&useAsyncRendering)) &&
            useAsyncRendering &&
            (!mPluginWindow ||
             mPluginWindow->type == NPWindowTypeDrawable));
  }

private:
  // return FALSE if LayerSurface dirty (newly created and don't have valid plugin content yet)
  PRBool IsUpToDate()
  {
    nsRefPtr<gfxASurface> readyToUse;
    return NS_SUCCEEDED(mInstance->GetSurface(getter_AddRefs(readyToUse))) &&
           readyToUse && readyToUse->GetSize() == gfxIntSize(mPluginWindow->width,
                                                             mPluginWindow->height);
  }

  void FixUpURLS(const nsString &name, nsAString &value);

  nsPluginNativeWindow       *mPluginWindow;
  nsCOMPtr<nsIPluginInstance> mInstance;
  nsObjectFrame              *mObjectFrame; // owns nsPluginInstanceOwner
  nsCOMPtr<nsIContent>        mContent;
  nsCString                   mDocumentBase;
  char                       *mTagText;
  nsCOMPtr<nsIWidget>         mWidget;
  nsCOMPtr<nsIPluginHost>     mPluginHost;

#ifdef XP_MACOSX
  NP_CGContext                              mCGPluginPortCopy;
#ifndef NP_NO_QUICKDRAW
  NP_Port                                   mQDPluginPortCopy;
#endif
  PRInt32                                   mInCGPaintLevel;
  nsIOSurface                              *mIOSurface;
  nsCARenderer                              mCARenderer;
  static nsCOMPtr<nsITimer>                *sCATimer;
  static nsTArray<nsPluginInstanceOwner*>  *sCARefreshListeners;
  PRBool                                    mSentInitialTopLevelWindowEvent;
#endif

  // Initially, the event loop nesting level we were created on, it's updated
  // if we detect the appshell is on a lower level as long as we're not stopped.
  // We delay DoStopPlugin() until the appshell reaches this level or lower.
  PRUint32                    mLastEventloopNestingLevel;
  PRPackedBool                mContentFocused;
  PRPackedBool                mWidgetVisible;    // used on Mac to store our widget's visible state
#ifdef XP_MACOSX
  PRPackedBool                mPluginPortChanged;
#endif
#ifdef MOZ_X11
  // Used with windowless plugins only, initialized in CreateWidget().
  PRPackedBool                mFlash10Quirks;
#endif

  // If true, destroy the widget on destruction. Used when plugin stop
  // is being delayed to a safer point in time.
  PRPackedBool                mDestroyWidget;
  PRUint16          mNumCachedAttrs;
  PRUint16          mNumCachedParams;
  char              **mCachedAttrParamNames;
  char              **mCachedAttrParamValues;

#ifdef MOZ_COMPOSITED_PLUGINS
  nsIntPoint        mLastPoint;
#endif

#ifdef XP_MACOSX
  NPEventModel mEventModel;
#endif

  // pointer to wrapper for nsIDOMContextMenuListener
  nsRefPtr<nsPluginDOMContextMenuListener> mCXMenuListener;

  nsresult DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent);
  nsresult DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent);
  nsresult DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent);

  nsresult EnsureCachedAttrParamArrays();

#ifdef MOZ_COMPOSITED_PLUGINS
  nsEventStatus ProcessEventX11Composited(const nsGUIEvent & anEvent);
#endif

#ifdef MOZ_X11
  class Renderer
#if defined(MOZ_WIDGET_GTK2)
    : public gfxXlibNativeRenderer
#elif defined(MOZ_WIDGET_QT)
    : public gfxQtNativeRenderer
#endif
  {
  public:
    Renderer(NPWindow* aWindow, nsPluginInstanceOwner* aInstanceOwner,
             const nsIntSize& aPluginSize, const nsIntRect& aDirtyRect)
      : mWindow(aWindow), mInstanceOwner(aInstanceOwner),
        mPluginSize(aPluginSize), mDirtyRect(aDirtyRect)
    {}
    virtual nsresult DrawWithXlib(gfxXlibSurface* surface, nsIntPoint offset, 
            nsIntRect* clipRects, PRUint32 numClipRects);
  private:
    NPWindow* mWindow;
    nsPluginInstanceOwner* mInstanceOwner;
    const nsIntSize& mPluginSize;
    const nsIntRect& mDirtyRect;
  };
#endif

#ifdef MOZ_USE_IMAGE_EXPOSE

  // On hildon, we attempt to use NPImageExpose which allows us faster
  // painting.

  // used to keep track of how big our buffer is.
  nsIntSize mPluginSize;

  // The absolute position on the screen to draw to.
  gfxRect mAbsolutePosition;

  // The clip region that we should draw into.
  gfxRect mAbsolutePositionClip;

  GC mXlibSurfGC;
  Window mBlitWindow;
  XImage *mSharedXImage;
  XShmSegmentInfo mSharedSegmentInfo;

  PRBool SetupXShm();
  void ReleaseXShm();
  void NativeImageDraw(NPRect* invalidRect = nsnull);
  PRBool UpdateVisibility(PRBool aVisible);

#endif

  nsRefPtr<gfxASurface> mLayerSurface;
  PRPackedBool          mWaitingForPaint;
};

  // Mac specific code to fix up port position and clip
#ifdef XP_MACOSX

  enum { ePluginPaintEnable, ePluginPaintDisable };

#endif // XP_MACOSX

nsObjectFrame::nsObjectFrame(nsStyleContext* aContext)
  : nsObjectFrameSuper(aContext)
  , mReflowCallbackPosted(PR_FALSE)
{
  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("Created new nsObjectFrame %p\n", this));
}

nsObjectFrame::~nsObjectFrame()
{
  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsObjectFrame %p deleted\n", this));
}

NS_QUERYFRAME_HEAD(nsObjectFrame)
  NS_QUERYFRAME_ENTRY(nsIObjectFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsObjectFrameSuper)

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsObjectFrame::CreateAccessible()
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");
  return accService ?
    accService->CreateHTMLObjectFrameAccessible(this, mContent,
                                                PresContext()->PresShell()) :
    nsnull;
}

#ifdef XP_WIN
NS_IMETHODIMP nsObjectFrame::GetPluginPort(HWND *aPort)
{
  *aPort = (HWND) mInstanceOwner->GetPluginPortFromWidget();
  return NS_OK;
}
#endif
#endif


static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);

NS_IMETHODIMP 
nsObjectFrame::Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow)
{
  NS_PRECONDITION(aContent, "How did that happen?");
  mPreventInstantiation =
    (aContent->GetCurrentDoc()->GetDisplayDocument() != nsnull);

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("Initializing nsObjectFrame %p for content %p\n", this, aContent));

  nsresult rv = nsObjectFrameSuper::Init(aContent, aParent, aPrevInFlow);

  return rv;
}

void
nsObjectFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!mPreventInstantiation ||
               (mContent && mContent->GetCurrentDoc()->GetDisplayDocument()),
               "about to crash due to bug 136927");

  // we need to finish with the plugin before native window is destroyed
  // doing this in the destructor is too late.
  StopPluginInternal(PR_TRUE);

  // StopPluginInternal might have disowned the widget; if it has,
  // mWidget will be null.
  if (mWidget) {
    mInnerView->DetachWidgetEventHandler(mWidget);
    mWidget->Destroy();
  }

  nsObjectFrameSuper::DestroyFrom(aDestructRoot);
}

/* virtual */ void
nsObjectFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  if (HasView()) {
    nsIView* view = GetView();
    nsIViewManager* vm = view->GetViewManager();
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
nsObjectFrame::CreateWidget(nscoord aWidth,
                            nscoord aHeight,
                            PRBool  aViewOnly)
{
  nsIView* view = GetView();
  NS_ASSERTION(view, "Object frames must have views");  
  if (!view) {
    return NS_OK;       //XXX why OK? MMP
  }

  nsIViewManager* viewMan = view->GetViewManager();
  // mark the view as hidden since we don't know the (x,y) until Paint
  // XXX is the above comment correct?
  viewMan->SetViewVisibility(view, nsViewVisibility_kHide);

  nsCOMPtr<nsIDeviceContext> dx;
  viewMan->GetDeviceContext(*getter_AddRefs(dx));

  //this is ugly. it was ripped off from didreflow(). MMP
  // Position and size view relative to its parent, not relative to our
  // parent frame (our parent frame may not have a view).
  
  nsIView* parentWithView;
  nsPoint origin;
  nsRect r(0, 0, mRect.width, mRect.height);

  GetOffsetFromView(origin, &parentWithView);
  viewMan->ResizeView(view, r);
  viewMan->MoveViewTo(view, origin.x, origin.y);

  nsRootPresContext* rpc = PresContext()->GetRootPresContext();
  if (!rpc) {
    return NS_ERROR_FAILURE;
  }

  if (!aViewOnly && !mWidget) {
    mInnerView = viewMan->CreateView(GetContentRect() - GetPosition(), view);
    if (!mInnerView) {
      NS_ERROR("Could not create inner view");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    viewMan->InsertChild(view, mInnerView, nsnull, PR_TRUE);

    nsresult rv;
    mWidget = do_CreateInstance(kWidgetCID, &rv);
    if (NS_FAILED(rv))
      return rv;

    // XXX this breaks plugins in popups ... do we care?
    nsIWidget* parentWidget =
      rpc->PresShell()->FrameManager()->GetRootFrame()->GetNearestWidget();

    nsWidgetInitData initData;
    initData.mWindowType = eWindowType_plugin;
    initData.mUnicode = PR_FALSE;
    initData.clipChildren = PR_TRUE;
    initData.clipSiblings = PR_TRUE;
    // We want mWidget to be able to deliver events to us, especially on
    // Mac where events to the plugin are routed through Gecko. So we
    // allow the view to attach its event handler to mWidget even though
    // mWidget isn't the view's designated widget.
    EVENT_CALLBACK eventHandler = mInnerView->AttachWidgetEventHandler(mWidget);
    mWidget->Create(parentWidget, nsnull, nsIntRect(0,0,0,0),
                    eventHandler, dx, nsnull, nsnull, &initData);

    mWidget->EnableDragDrop(PR_TRUE);

    // If this frame has an ancestor with a widget which is not
    // the root prescontext's widget, then this plugin should not be
    // displayed, so don't show the widget. If we show the widget, the
    // plugin may appear in the main window. In Web content this would
    // only happen with a plugin in a XUL popup.
    if (parentWidget == GetNearestWidget()) {
      mWidget->Show(PR_TRUE);
#ifdef XP_MACOSX
      // On Mac, we need to invalidate ourselves since even windowed
      // plugins are painted through Thebes and we need to ensure
      // the Thebes layer containing the plugin is updated.
      Invalidate(GetContentRect() - GetPosition());
#endif
    }
  }

  if (mWidget) {
    rpc->RegisterPluginForGeometryUpdates(this);
    rpc->RequestUpdatePluginGeometry(this);

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

#ifdef XP_MACOSX
    // Now that we have a widget we want to set the event model before
    // any events are processed.
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (!pluginWidget)
      return NS_ERROR_FAILURE;
    pluginWidget->SetPluginEventModel(mInstanceOwner->GetEventModel());
    pluginWidget->SetPluginDrawingModel(mInstanceOwner->GetDrawingModel());

    if (mInstanceOwner->GetDrawingModel() == NPDrawingModelCoreAnimation) {
      mInstanceOwner->SetupCARefresh();
    }
#endif
  }

  if (!IsHidden()) {
    viewMan->SetViewVisibility(view, nsViewVisibility_kShow);
  }

  return NS_OK;
}

#define EMBED_DEF_WIDTH 240
#define EMBED_DEF_HEIGHT 200

/* virtual */ nscoord
nsObjectFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = 0;

  if (!IsHidden(PR_FALSE)) {
    nsIAtom *atom = mContent->Tag();
    if (atom == nsGkAtoms::applet || atom == nsGkAtoms::embed) {
      result = nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_WIDTH);
    }
  }

  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsObjectFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
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

  if (IsHidden(PR_FALSE)) {
    return;
  }
  
  aMetrics.width = aReflowState.ComputedWidth();
  aMetrics.height = aReflowState.ComputedHeight();

  // for EMBED and APPLET, default to 240x200 for compatibility
  nsIAtom *atom = mContent->Tag();
  if (atom == nsGkAtoms::applet || atom == nsGkAtoms::embed) {
    if (aMetrics.width == NS_UNCONSTRAINEDSIZE) {
      aMetrics.width = NS_MIN(NS_MAX(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_WIDTH),
                                     aReflowState.mComputedMinWidth),
                              aReflowState.mComputedMaxWidth);
    }
    if (aMetrics.height == NS_UNCONSTRAINEDSIZE) {
      aMetrics.height = NS_MIN(NS_MAX(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_HEIGHT),
                                      aReflowState.mComputedMinHeight),
                               aReflowState.mComputedMaxHeight);
    }

#if defined (MOZ_WIDGET_GTK2)
    // We need to make sure that the size of the object frame does not
    // exceed the maximum size of X coordinates.  See bug #225357 for
    // more information.  In theory Gtk2 can handle large coordinates,
    // but underlying plugins can't.
    aMetrics.height = NS_MIN(aPresContext->DevPixelsToAppUnits(PR_INT16_MAX), aMetrics.height);
    aMetrics.width = NS_MIN(aPresContext->DevPixelsToAppUnits(PR_INT16_MAX), aMetrics.width);
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
    nsIViewManager* vm = mInnerView->GetViewManager();
    vm->MoveViewTo(mInnerView, r.x, r.y);
    vm->ResizeView(mInnerView, nsRect(nsPoint(0, 0), r.Size()), PR_TRUE);
  }

  FixupWindow(r.Size());
  if (!mReflowCallbackPosted) {
    mReflowCallbackPosted = PR_TRUE;
    aPresContext->PresShell()->PostReflowCallback(this);
  }

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
}

///////////// nsIReflowCallback ///////////////

PRBool
nsObjectFrame::ReflowFinished()
{
  mReflowCallbackPosted = PR_FALSE;
  CallSetWindow();
  return PR_TRUE;
}

void
nsObjectFrame::ReflowCallbackCanceled()
{
  mReflowCallbackPosted = PR_FALSE;
}

nsresult
nsObjectFrame::InstantiatePlugin(nsIPluginHost* aPluginHost, 
                                 const char* aMimeType,
                                 nsIURI* aURI)
{
  NS_ASSERTION(mPreventInstantiation,
               "Instantiation should be prevented here!");

  // If you add early return(s), be sure to balance this call to
  // appShell->SuspendNative() with additional call(s) to
  // appShell->ReturnNative().
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->SuspendNative();
  }

  NS_ASSERTION(mContent, "We should have a content node.");

  nsIDocument* doc = mContent->GetOwnerDoc();
  nsCOMPtr<nsIPluginDocument> pDoc (do_QueryInterface(doc));
  PRBool fullPageMode = PR_FALSE;
  if (pDoc) {
    pDoc->GetWillHandleInstantiation(&fullPageMode);
  }

  nsresult rv;
  if (fullPageMode) {  /* full-page mode */
    nsCOMPtr<nsIStreamListener> stream;
    rv = aPluginHost->InstantiateFullPagePlugin(aMimeType, aURI, mInstanceOwner, getter_AddRefs(stream));
    if (NS_SUCCEEDED(rv))
      pDoc->SetStreamListener(stream);
  } else {   /* embedded mode */
    rv = aPluginHost->InstantiateEmbeddedPlugin(aMimeType, aURI,
                                                mInstanceOwner);
  }

  // Note that |this| may very well be destroyed already!

  if (appShell) {
    appShell->ResumeNative();
  }

  return rv;
}

void
nsObjectFrame::FixupWindow(const nsSize& aSize)
{
  nsPresContext* presContext = PresContext();

  if (!mInstanceOwner)
    return;

  NPWindow *window;
  mInstanceOwner->GetWindow(window);

  NS_ENSURE_TRUE(window, /**/);

#ifdef XP_MACOSX
  mInstanceOwner->FixUpPluginWindow(ePluginPaintDisable);
#endif

  PRBool windowless = (window->type == NPWindowTypeDrawable);

  nsIntPoint origin = GetWindowOriginInPixels(windowless);

  window->x = origin.x;
  window->y = origin.y;

  window->width = presContext->AppUnitsToDevPixels(aSize.width);
  window->height = presContext->AppUnitsToDevPixels(aSize.height);

  // on the Mac we need to set the clipRect to { 0, 0, 0, 0 } for now. This will keep
  // us from drawing on screen until the widget is properly positioned, which will not
  // happen until we have finished the reflow process.
  window->clipRect.top = 0;
  window->clipRect.left = 0;
#ifdef XP_MACOSX
  window->clipRect.bottom = 0;
  window->clipRect.right = 0;
#else
  window->clipRect.bottom = presContext->AppUnitsToDevPixels(aSize.height);
  window->clipRect.right = presContext->AppUnitsToDevPixels(aSize.width);
#endif
  NotifyPluginReflowObservers();
}

void
nsObjectFrame::CallSetWindow()
{
  NPWindow *win = nsnull;
 
  nsresult rv;
  nsCOMPtr<nsIPluginInstance> pi; 
  if (!mInstanceOwner ||
      NS_FAILED(rv = mInstanceOwner->GetInstance(*getter_AddRefs(pi))) ||
      !pi ||
      NS_FAILED(rv = mInstanceOwner->GetWindow(win)) || 
      !win)
    return;

  nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;
#ifdef XP_MACOSX
  mInstanceOwner->FixUpPluginWindow(ePluginPaintDisable);
#endif

  if (IsHidden())
    return;

  PRBool windowless = (window->type == NPWindowTypeDrawable);

  // refresh the plugin port as well
  window->window = mInstanceOwner->GetPluginPortFromWidget();

  // Adjust plugin dimensions according to pixel snap results
  // and reduce amount of SetWindow calls
  nsPresContext* presContext = PresContext();
  nsRootPresContext* rootPC = presContext->GetRootPresContext();
  if (!rootPC)
    return;
  PRInt32 appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  nsIFrame* rootFrame = rootPC->PresShell()->FrameManager()->GetRootFrame();
  nsRect bounds = GetContentRect() + GetParent()->GetOffsetToCrossDoc(rootFrame);
  nsIntRect intBounds = bounds.ToNearestPixels(appUnitsPerDevPixel);
  window->x = intBounds.x;
  window->y = intBounds.y;
  window->width = intBounds.width;
  window->height = intBounds.height;

  // this will call pi->SetWindow and take care of window subclassing
  // if needed, see bug 132759.
  if (mInstanceOwner->UseLayers()) {
    pi->AsyncSetWindow(window);
  }
  else {
    window->CallSetWindow(pi);
  }

  mInstanceOwner->ReleasePluginPort(window->window);
}

PRBool
nsObjectFrame::IsFocusable(PRInt32 *aTabIndex, PRBool aWithMouse)
{
  if (aTabIndex)
    *aTabIndex = -1;
  return nsObjectFrameSuper::IsFocusable(aTabIndex, aWithMouse);
}

PRBool
nsObjectFrame::IsHidden(PRBool aCheckVisibilityStyle) const
{
  if (aCheckVisibilityStyle) {
    if (!GetStyleVisibility()->IsVisibleOrCollapsed())
      return PR_TRUE;    
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
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsIntPoint nsObjectFrame::GetWindowOriginInPixels(PRBool aWindowless)
{
  nsIView * parentWithView;
  nsPoint origin(0,0);

  GetOffsetFromView(origin, &parentWithView);

  // if it's windowless, let's make sure we have our origin set right
  // it may need to be corrected, like after scrolling
  if (aWindowless && parentWithView) {
    nsPoint offsetToWidget;
    parentWithView->GetNearestWidget(&offsetToWidget);
    origin += offsetToWidget;
  }
  // It's OK to use GetUsedBorderAndPadding here (and below) since
  // GetSkipSides always returns 0; we don't split nsObjectFrames
  origin += GetUsedBorderAndPadding().TopLeft();

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
  if (aStatus == NS_FRAME_REFLOW_FINISHED &&
      (GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    nsCOMPtr<nsIObjectLoadingContent> objContent(do_QueryInterface(mContent));
    NS_ASSERTION(objContent, "Why not an object loading content?");
    objContent->HasNewFrame(this);
  }

  nsresult rv = nsObjectFrameSuper::DidReflow(aPresContext, aReflowState, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (aStatus != NS_FRAME_REFLOW_FINISHED) 
    return rv;

  if (HasView()) {
    nsIView* view = GetView();
    nsIViewManager* vm = view->GetViewManager();
    if (vm)
      vm->SetViewVisibility(view, IsHidden() ? nsViewVisibility_kHide : nsViewVisibility_kShow);
  }

  return rv;
}

/* static */ void
nsObjectFrame::PaintPrintPlugin(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                                const nsRect& aDirtyRect, nsPoint aPt)
{
  nsPoint pt = aPt + aFrame->GetUsedBorderAndPadding().TopLeft();
  nsIRenderingContext::AutoPushTranslation translate(aCtx, pt.x, pt.y);
  // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
  static_cast<nsObjectFrame*>(aFrame)->PrintPlugin(*aCtx, aDirtyRect);
}

nsRect
nsDisplayPlugin::GetBounds(nsDisplayListBuilder* aBuilder)
{
  nsRect r = mFrame->GetContentRect() - mFrame->GetPosition() +
    ToReferenceFrame();
  nsObjectFrame* f = static_cast<nsObjectFrame*>(mFrame);
  if (mozilla::LAYER_ACTIVE == f->GetLayerState(aBuilder, nsnull)) {
    gfxIntSize size = f->GetImageContainer()->GetCurrentSize();
    PRInt32 appUnitsPerDevPixel = f->PresContext()->AppUnitsPerDevPixel();
    r -= nsPoint((r.width - size.width * appUnitsPerDevPixel) / 2,
                 (r.height - size.height * appUnitsPerDevPixel) / 2);
  }
  return r;
}

void
nsDisplayPlugin::Paint(nsDisplayListBuilder* aBuilder,
                       nsIRenderingContext* aCtx)
{
  nsObjectFrame* f = static_cast<nsObjectFrame*>(mFrame);
  f->PaintPlugin(aBuilder, *aCtx, mVisibleRect, GetBounds(aBuilder));
}

PRBool
nsDisplayPlugin::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion)
{
  mVisibleRegion.And(*aVisibleRegion, GetBounds(aBuilder));  
  return nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion);
}

PRBool
nsDisplayPlugin::IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aForceTransparentSurface)
{
  if (aForceTransparentSurface) {
    *aForceTransparentSurface = PR_FALSE;
  }
  nsObjectFrame* f = static_cast<nsObjectFrame*>(mFrame);
  return f->IsOpaque();
}

void
nsDisplayPlugin::GetWidgetConfiguration(nsDisplayListBuilder* aBuilder,
                                        nsTArray<nsIWidget::Configuration>* aConfigurations)
{
  nsObjectFrame* f = static_cast<nsObjectFrame*>(mFrame);
  nsPoint pluginOrigin = mFrame->GetUsedBorderAndPadding().TopLeft() +
    ToReferenceFrame();
  f->ComputeWidgetGeometry(mVisibleRegion, pluginOrigin, aConfigurations);
}

void
nsObjectFrame::ComputeWidgetGeometry(const nsRegion& aRegion,
                                     const nsPoint& aPluginOrigin,
                                     nsTArray<nsIWidget::Configuration>* aConfigurations)
{
  if (!mWidget)
    return;

  nsPresContext* presContext = PresContext();
  nsRootPresContext* rootPC = presContext->GetRootPresContext();
  if (!rootPC)
    return;

  nsIWidget::Configuration* configuration = aConfigurations->AppendElement();
  if (!configuration)
    return;
  configuration->mChild = mWidget;

  PRInt32 appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  nsIFrame* rootFrame = rootPC->PresShell()->FrameManager()->GetRootFrame();
  nsRect bounds = GetContentRect() + GetParent()->GetOffsetToCrossDoc(rootFrame);
  configuration->mBounds = bounds.ToNearestPixels(appUnitsPerDevPixel);

  // This should produce basically the same rectangle (but not relative
  // to the root frame). We only call this here for the side-effect of
  // setting mViewToWidgetOffset on the view.
  mInnerView->CalcWidgetBounds(eWindowType_plugin);

  nsRegionRectIterator iter(aRegion);
  nsIntPoint pluginOrigin = aPluginOrigin.ToNearestPixels(appUnitsPerDevPixel);
  for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
    // Snap *r to pixels while it's relative to the painted widget, to
    // improve consistency with rectangle and image drawing
    nsIntRect pixRect =
      r->ToNearestPixels(appUnitsPerDevPixel) - pluginOrigin;
    if (!pixRect.IsEmpty()) {
      configuration->mClipRegion.AppendElement(pixRect);
    }
  }
}

nsresult
nsObjectFrame::SetAbsoluteScreenPosition(nsIDOMElement* element,
                                         nsIDOMClientRect* position,
                                         nsIDOMClientRect* clip)
{
#ifdef MOZ_USE_IMAGE_EXPOSE
  if (!mInstanceOwner)
    return NS_ERROR_NOT_AVAILABLE;
  return mInstanceOwner->SetAbsoluteScreenPosition(element, position, clip);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult
nsObjectFrame::PluginEventNotifier::Run() {
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  obsSvc->NotifyObservers(nsnull, "plugin-changed-event", mEventType.get());
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
    mInstanceOwner->FixUpPluginWindow(ePluginPaintEnable);
  }
#endif
}

PRBool
nsObjectFrame::IsOpaque() const
{
#if defined(XP_MACOSX)
  return PR_FALSE;
#else
  if (!mInstanceOwner)
    return PR_FALSE;

  NPWindow *window;
  mInstanceOwner->GetWindow(window);
  if (window->type != NPWindowTypeDrawable)
    return PR_TRUE;

  nsresult rv;
  nsCOMPtr<nsIPluginInstance> pi;
  rv = mInstanceOwner->GetInstance(*getter_AddRefs(pi));
  if (NS_FAILED(rv) || !pi)
    return PR_FALSE;

  PRBool transparent = PR_FALSE;
  pi->IsTransparent(&transparent);
  return !transparent;
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

  // determine if we are printing
  if (type == nsPresContext::eContext_Print) {
    rv = replacedContent.AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(aBuilder, this, PaintPrintPlugin, "PrintPlugin",
                         nsDisplayItem::TYPE_PRINT_PLUGIN));
  } else {
    rv = replacedContent.AppendNewToTop(new (aBuilder)
        nsDisplayPlugin(aBuilder, this));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  WrapReplacedContentForBorderRadius(aBuilder, &replacedContent, aLists);

  return NS_OK;
}

void
nsObjectFrame::PrintPlugin(nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect)
{
  nsCOMPtr<nsIObjectLoadingContent> obj(do_QueryInterface(mContent));
  if (!obj)
    return;

  nsIFrame* frame = nsnull;
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
  nsCOMPtr<nsIPluginInstance> pi;
  if (NS_FAILED(objectFrame->GetPluginInstance(*getter_AddRefs(pi))) || !pi)
    return;

  // now we need to setup the correct location for printing
  NPWindow window;
  window.window = nsnull;

  // prepare embedded mode printing struct
  NPPrint npprint;
  npprint.mode = NP_EMBED;

  // we need to find out if we are windowless or not
  PRBool windowless = PR_FALSE;
  pi->IsWindowless(&windowless);
  window.type = windowless ? NPWindowTypeDrawable : NPWindowTypeWindow;

  window.clipRect.bottom = 0; window.clipRect.top = 0;
  window.clipRect.left = 0; window.clipRect.right = 0;
  
// platform specific printing code
#ifdef MAC_CARBON_PLUGINS
  nsSize contentSize = GetContentRect().Size();
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

  Rect gwBounds;
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
  nsresult rv = pi->Print(&npprint);

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
#elif defined(XP_UNIX)

  /* XXX this just flat-out doesn't work in a thebes world --
   * RenderEPS is a no-op.  So don't bother to do any work here.
   */

#elif defined(XP_OS2)
  void *hps = aRenderingContext.GetNativeGraphicData(nsIRenderingContext::NATIVE_OS2_PS);
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
  nsSize contentSize = GetContentRect().Size();
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
  nsDidReflowStatus status = NS_FRAME_REFLOW_FINISHED; // should we use a special status?
  frame->DidReflow(presContext,
                   nsnull, status);  // DidReflow will take care of it
}

ImageContainer*
nsObjectFrame::GetImageContainer()
{
  if (mImageContainer)
    return mImageContainer;

  nsRefPtr<LayerManager> manager =
    nsContentUtils::LayerManagerForDocument(mContent->GetOwnerDoc());
  if (!manager)
    return nsnull;

  mImageContainer = manager->CreateImageContainer();
  return mImageContainer;
}

void
nsPluginInstanceOwner::NotifyPaintWaiter(nsDisplayListBuilder* aBuilder,
                                         LayerManager* aManager)
{
  // This is notification for reftests about async plugin paint start
  if (!mWaitingForPaint && !IsUpToDate() && aBuilder->ShouldSyncDecodeImages()) {
    nsContentUtils::DispatchTrustedEvent(mContent->GetOwnerDoc(), mContent,
                                         NS_LITERAL_STRING("MozPaintWait"),
                                         PR_TRUE, PR_TRUE);
    mWaitingForPaint = PR_TRUE;
  }
}

PRBool
nsPluginInstanceOwner::SetCurrentImage(ImageContainer* aContainer)
{
  mInstance->GetSurface(getter_AddRefs(mLayerSurface));
  if (!mLayerSurface)
    return PR_FALSE;

  Image::Format format = Image::CAIRO_SURFACE;
  nsRefPtr<Image> image;
  image = aContainer->CreateImage(&format, 1);
  if (!image)
    return PR_FALSE;

  NS_ASSERTION(image->GetFormat() == Image::CAIRO_SURFACE, "Wrong format?");
  CairoImage* pluginImage = static_cast<CairoImage*>(image.get());
  CairoImage::Data cairoData;
  cairoData.mSurface = mLayerSurface.get();
  cairoData.mSize = mLayerSurface->GetSize();
  pluginImage->SetData(cairoData);
  aContainer->SetCurrentImage(image);

  return PR_TRUE;
}

mozilla::LayerState
nsObjectFrame::GetLayerState(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager)
{
  if (!mInstanceOwner || !mInstanceOwner->UseLayers())
    return mozilla::LAYER_NONE;

  return mozilla::LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsObjectFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                          LayerManager* aManager,
                          nsDisplayItem* aItem)
{
  if (!mInstanceOwner)
    return nsnull;

  NPWindow* window = nsnull;
  mInstanceOwner->GetWindow(window);
  if (!window)
    return nsnull;

  if (window->width <= 0 || window->height <= 0)
    return nsnull;

  nsRect area = GetContentRect() + aBuilder->ToReferenceFrame(GetParent());
  gfxRect r = nsLayoutUtils::RectToGfxRect(area, PresContext()->AppUnitsPerDevPixel());
  // to provide crisper and faster drawing.
  r.Round();
  nsRefPtr<Layer> layer =
    (aBuilder->LayerBuilder()->GetLeafLayerFor(aBuilder, aManager, aItem));

  if (!layer) {
    mInstanceOwner->NotifyPaintWaiter(aBuilder, aManager);
    // Initialize ImageLayer
    layer = aManager->CreateImageLayer();
  }

  nsCOMPtr<nsIPluginInstance> pi;
  mInstanceOwner->GetInstance(*getter_AddRefs(pi));
  // Give plugin info about layer paint
  if (pi) {
    if (NS_FAILED(pi->NotifyPainted())) {
      return nsnull;
    }
  }

  if (!layer)
    return nsnull;

  NS_ASSERTION(layer->GetType() == Layer::TYPE_IMAGE, "ObjectFrame works only with ImageLayer");
  // Create image
  nsRefPtr<ImageContainer> container = GetImageContainer();
  if (!container)
    return nsnull;

  if (!mInstanceOwner->SetCurrentImage(container)) {
    return nsnull;
  }

  ImageLayer* imglayer = static_cast<ImageLayer*>(layer.get());
  imglayer->SetContainer(container);
  imglayer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(this));

  layer->SetContentFlags(IsOpaque() ? Layer::CONTENT_OPAQUE : 0);

  // Set a transform on the layer to draw the plugin in the right place
  gfxMatrix transform;
  // Center plugin if layer size != frame rect
  r.pos.x += (r.Width() - container->GetCurrentSize().width) / 2;
  r.pos.y += (r.Height() - container->GetCurrentSize().height) / 2;
  transform.Translate(r.pos);

  layer->SetTransform(gfx3DMatrix::From2D(transform));
  nsRefPtr<Layer> result = layer.forget();
  return result.forget();
}

void
nsObjectFrame::PaintPlugin(nsDisplayListBuilder* aBuilder,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect, const nsRect& aPluginRect)
{
  // Screen painting code
#if defined(XP_MACOSX)
  // delegate all painting to the plugin instance.
  if (mInstanceOwner) {
    if (mInstanceOwner->GetDrawingModel() == NPDrawingModelCoreGraphics ||
        mInstanceOwner->GetDrawingModel() == NPDrawingModelCoreAnimation ||
        mInstanceOwner->GetDrawingModel() == 
                                  NPDrawingModelInvalidatingCoreAnimation) {
      PRInt32 appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();
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

      nsCOMPtr<nsIPluginInstance> inst;
      GetPluginInstance(*getter_AddRefs(inst));
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
#ifndef NP_NO_CARBON
      if (mInstanceOwner->GetEventModel() == NPEventModelCarbon &&
          !mInstanceOwner->SetPluginPortAndDetectChange()) {
        NS_WARNING("null plugin port during PaintPlugin");
        nativeDrawing.EndNativeDrawing();
        return;
      }

      // In the Carbon event model...
      // If gfxQuartzNativeDrawing hands out a CGContext different from the
      // one set by SetPluginPortAndDetectChange(), we need to pass it to the
      // plugin via SetWindow().  This will happen in nsPluginInstanceOwner::
      // FixUpPluginWindow(), called from nsPluginInstanceOwner::Paint().
      // (If SetPluginPortAndDetectChange() made any changes itself, this has
      // already been detected in that method, and will likewise result in a
      // call to SetWindow() from FixUpPluginWindow().)
      NP_CGContext* windowContext = static_cast<NP_CGContext*>(window->window);
      if (mInstanceOwner->GetEventModel() == NPEventModelCarbon &&
          windowContext->context != cgContext) {
        windowContext->context = cgContext;
        cgPluginPortCopy->context = cgContext;
        mInstanceOwner->SetPluginPortChanged(PR_TRUE);
      }
#endif

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
      nsIRenderingContext::AutoPushTranslation
        translate(&aRenderingContext, aPluginRect.x, aPluginRect.y);

      // this rect is used only in the CoreGraphics drawing model
      gfxRect tmpRect(0, 0, 0, 0);
      mInstanceOwner->Paint(tmpRect, NULL);
    }
  }
#elif defined(MOZ_X11)
  if (mInstanceOwner) {
    NPWindow *window;
    mInstanceOwner->GetWindow(window);
#ifdef MOZ_COMPOSITED_PLUGINS
    {
#else
    if (window->type == NPWindowTypeDrawable) {
#endif
      gfxRect frameGfxRect =
        PresContext()->AppUnitsToGfxUnits(aPluginRect);
      gfxRect dirtyGfxRect =
        PresContext()->AppUnitsToGfxUnits(aDirtyRect);
      gfxContext* ctx = aRenderingContext.ThebesContext();

      mInstanceOwner->Paint(ctx, frameGfxRect, dirtyGfxRect);
    }
  }
#elif defined(XP_WIN)
  nsCOMPtr<nsIPluginInstance> inst;
  GetPluginInstance(*getter_AddRefs(inst));
  if (inst) {
    gfxRect frameGfxRect =
      PresContext()->AppUnitsToGfxUnits(aPluginRect);
    gfxRect dirtyGfxRect =
      PresContext()->AppUnitsToGfxUnits(aDirtyRect);
    gfxContext *ctx = aRenderingContext.ThebesContext();
    gfxMatrix currentMatrix = ctx->CurrentMatrix();

    if (ctx->UserToDevicePixelSnapped(frameGfxRect, PR_FALSE)) {
      dirtyGfxRect = ctx->UserToDevice(dirtyGfxRect);
      ctx->IdentityMatrix();
    }
    dirtyGfxRect.RoundOut();

    // Look if it's windowless
    NPWindow *window;
    mInstanceOwner->GetWindow(window);

    if (window->type == NPWindowTypeDrawable) {
      // check if we need to call SetWindow with updated parameters
      PRBool doupdatewindow = PR_FALSE;
      // the offset of the DC
      nsPoint origin;

      gfxWindowsNativeDrawing nativeDraw(ctx, frameGfxRect);
#ifdef MOZ_IPC
      if (nativeDraw.IsDoublePass()) {
        // OOP plugin specific: let the shim know before we paint if we are doing a
        // double pass render. If this plugin isn't oop, the register window message
        // will be ignored.
        NPEvent pluginEvent;
        pluginEvent.event = mozilla::plugins::DoublePassRenderingEvent();
        pluginEvent.wParam = 0;
        pluginEvent.lParam = 0;
        if (pluginEvent.event)
          inst->HandleEvent(&pluginEvent, nsnull);
      }
#endif
      do {
        HDC hdc = nativeDraw.BeginNativeDrawing();
        if (!hdc)
          return;

        RECT dest;
        nativeDraw.TransformToNativeRect(frameGfxRect, dest);
        RECT dirty;
        nativeDraw.TransformToNativeRect(dirtyGfxRect, dirty);

        // XXX how can we be sure that window->window doesn't point to
        // a dead DC and hdc has been reallocated at the same address?
        if (reinterpret_cast<HDC>(window->window) != hdc ||
            window->x != dest.left || window->y != dest.top) {
          window->window = hdc;
          window->x = dest.left;
          window->y = dest.top;

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

          nsIntPoint origin = GetWindowOriginInPixels(PR_TRUE);
          nsIntRect winlessRect = nsIntRect(origin, nsIntSize(window->width, window->height));
          // XXX I don't think we can be certain that the location wrt to
          // the window only changes when the location wrt to the drawable
          // changes, but the hdc probably changes on every paint so
          // doupdatewindow is rarely false, and there is not likely to be
          // a problem.
          if (mWindowlessRect != winlessRect) {
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
            inst->HandleEvent(&pluginEvent, nsnull);
          }

          inst->SetWindow(window);
        }
        mInstanceOwner->Paint(dirty, hdc);
        nativeDraw.EndNativeDrawing();
      } while (nativeDraw.ShouldRenderAgain());
      nativeDraw.PaintToContext();
    } else if (!aBuilder->IsPaintingToWindow()) {
      // Get PrintWindow dynamically since it's not present on Win2K,
      // which we still support
      typedef BOOL (WINAPI * PrintWindowPtr)
          (HWND hwnd, HDC hdcBlt, UINT nFlags);
      PrintWindowPtr printProc = nsnull;
      HMODULE module = ::GetModuleHandleW(L"user32.dll");
      if (module) {
        printProc = reinterpret_cast<PrintWindowPtr>
          (::GetProcAddress(module, "PrintWindow"));
      }
      // Disable this for Sun Java, it makes it go into a 100% cpu burn loop.
      if (printProc && !mInstanceOwner->MatchPluginName("Java(TM) Platform")) {
        HWND hwnd = reinterpret_cast<HWND>(window->window);
        RECT rc;
        GetWindowRect(hwnd, &rc);
        nsRefPtr<gfxWindowsSurface> surface =
          new gfxWindowsSurface(gfxIntSize(rc.right - rc.left, rc.bottom - rc.top));

        if (surface && printProc) {
          printProc(hwnd, surface->GetDC(), 0);
        
          ctx->Translate(frameGfxRect.pos);
          ctx->SetSource(surface);
          gfxRect r = frameGfxRect.Intersect(dirtyGfxRect) - frameGfxRect.pos;
          ctx->NewPath();
          ctx->Rectangle(r);
          ctx->Fill();
        }
      }
    }

    ctx->SetMatrix(currentMatrix);
  }
#elif defined(XP_OS2)
  nsCOMPtr<nsIPluginInstance> inst;
  GetPluginInstance(*getter_AddRefs(inst));
  if (inst) {
    // Look if it's windowless
    NPWindow *window;
    mInstanceOwner->GetWindow(window);

    if (window->type == NPWindowTypeDrawable) {
      // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
      nsIRenderingContext::AutoPushTranslation
        translate(&aRenderingContext, aPluginRect.x, aPluginRect.y);

      // check if we need to call SetWindow with updated parameters
      PRBool doupdatewindow = PR_FALSE;
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

      origin.x = NSToIntRound(float(ctxMatrix.GetTranslation().x));
      origin.y = NSToIntRound(float(ctxMatrix.GetTranslation().y));

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
      HPS hps = (HPS)aRenderingContext.GetNativeGraphicData(nsIRenderingContext::NATIVE_OS2_PS);
      if (reinterpret_cast<HPS>(window->window) != hps) {
        window->window = reinterpret_cast<void*>(hps);
        doupdatewindow = PR_TRUE;
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
        doupdatewindow = PR_TRUE;
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

  if (mInstanceOwner->SendNativeEvents() &&
      (NS_IS_PLUGIN_EVENT(anEvent) || NS_IS_NON_RETARGETED_PLUGIN_EVENT(anEvent))) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    return rv;
  }

#ifdef XP_WIN
  rv = nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
  return rv;
#endif

#ifdef XP_MACOSX
  // we want to process some native mouse events in the cocoa event model
  if ((anEvent->message == NS_MOUSE_ENTER || anEvent->message == NS_MOUSE_SCROLL) &&
      mInstanceOwner->GetEventModel() == NPEventModelCocoa) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    return rv;
  }
#endif

  if (anEvent->message == NS_DESTROY) {
#ifdef MAC_CARBON_PLUGINS
    mInstanceOwner->CancelTimer();
#endif
    return rv;
  }

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
nsObjectFrame::GetPluginInstance(nsIPluginInstance*& aPluginInstance)
{
  aPluginInstance = nsnull;

  if (!mInstanceOwner)
    return NS_OK;
  
  return mInstanceOwner->GetInstance(aPluginInstance);
}

nsresult
nsObjectFrame::PrepareInstanceOwner()
{
  nsWeakFrame weakFrame(this);

  // First, have to stop any possibly running plugins.
  StopPluginInternal(PR_FALSE);

  if (!weakFrame.IsAlive()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(!mInstanceOwner, "Must not have an instance owner here");

  mInstanceOwner = new nsPluginInstanceOwner();

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("Created new instance owner %p for frame %p\n", mInstanceOwner.get(),
          this));

  if (!mInstanceOwner)
    return NS_ERROR_OUT_OF_MEMORY;

  // Note, |this| may very well be gone after this call.
  return mInstanceOwner->Init(PresContext(), this, GetContent());
}

nsresult
nsObjectFrame::Instantiate(nsIChannel* aChannel, nsIStreamListener** aStreamListener)
{
  if (mPreventInstantiation) {
    return NS_OK;
  }
  
  // Note: If PrepareInstanceOwner() returns an error, |this| may very
  // well be deleted already.
  nsresult rv = PrepareInstanceOwner();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPluginHost> pluginHost(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  mInstanceOwner->SetPluginHost(pluginHost);

  // This must be done before instantiating the plugin
  FixupWindow(GetContentRect().Size());

  // Ensure we redraw when a plugin is instantiated
  Invalidate(GetContentRect() - GetPosition());

  nsWeakFrame weakFrame(this);

  NS_ASSERTION(!mPreventInstantiation, "Say what?");
  mPreventInstantiation = PR_TRUE;
  rv = pluginHost->InstantiatePluginForChannel(aChannel, mInstanceOwner, aStreamListener);

  if (!weakFrame.IsAlive()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mPreventInstantiation,
               "Instantiation should still be prevented!");
  mPreventInstantiation = PR_FALSE;

#ifdef ACCESSIBILITY
  if (PresContext()->PresShell()->IsAccessibilityActive()) {
    nsCOMPtr<nsIAccessibilityService> accService =
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService) {
      accService->RecreateAccessible(PresContext()->PresShell(), mContent);
    }
  }
#endif

  return rv;
}

nsresult
nsObjectFrame::Instantiate(const char* aMimeType, nsIURI* aURI)
{
  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsObjectFrame::Instantiate(%s) called on frame %p\n", aMimeType,
          this));

  if (mPreventInstantiation) {
    return NS_OK;
  }

  // XXXbz can aMimeType ever actually be null here?  If not, either
  // the callers are wrong (and passing "" instead of null) or we can
  // remove the codepaths dealing with null aMimeType in
  // InstantiateEmbeddedPlugin.
  NS_ASSERTION(aMimeType || aURI, "Need a type or a URI!");

  // Note: If PrepareInstanceOwner() returns an error, |this| may very
  // well be deleted already.
  nsresult rv = PrepareInstanceOwner();
  NS_ENSURE_SUCCESS(rv, rv);

  nsWeakFrame weakFrame(this);

  // This must be done before instantiating the plugin
  FixupWindow(GetContentRect().Size());

  // Ensure we redraw when a plugin is instantiated
  Invalidate(GetContentRect() - GetPosition());

  // get the nsIPluginHost service
  nsCOMPtr<nsIPluginHost> pluginHost(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  mInstanceOwner->SetPluginHost(pluginHost);

  NS_ASSERTION(!mPreventInstantiation, "Say what?");
  mPreventInstantiation = PR_TRUE;

  rv = InstantiatePlugin(pluginHost, aMimeType, aURI);

  if (!weakFrame.IsAlive()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // finish up
  if (NS_SUCCEEDED(rv)) {
    TryNotifyContentObjectWrapper();

    if (!weakFrame.IsAlive()) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    CallSetWindow();
  }

  NS_ASSERTION(mPreventInstantiation,
               "Instantiation should still be prevented!");

#ifdef ACCESSIBILITY
  if (PresContext()->PresShell()->IsAccessibilityActive()) {
    nsCOMPtr<nsIAccessibilityService> accService =
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService) {
      accService->RecreateAccessible(PresContext()->PresShell(), mContent);
    }
  }
#endif

  mPreventInstantiation = PR_FALSE;

  return rv;
}

void
nsObjectFrame::TryNotifyContentObjectWrapper()
{
  nsCOMPtr<nsIPluginInstance> inst;
  mInstanceOwner->GetInstance(*getter_AddRefs(inst));
  if (inst) {
    // The plugin may have set up new interfaces; we need to mess with our JS
    // wrapper.  Note that we DO NOT want to call this if there is no plugin
    // instance!  That would just reenter Instantiate(), trying to create
    // said plugin instance.
    NotifyContentObjectWrapper();
  }
}

class nsStopPluginRunnable : public nsRunnable, public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsStopPluginRunnable(nsPluginInstanceOwner *aInstanceOwner)
    : mInstanceOwner(aInstanceOwner)
  {
    NS_ASSERTION(aInstanceOwner, "need an owner");
  }

  // nsRunnable
  NS_IMETHOD Run();

  // nsITimerCallback
  NS_IMETHOD Notify(nsITimer *timer);

private:  
  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<nsPluginInstanceOwner> mInstanceOwner;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsStopPluginRunnable, nsRunnable, nsITimerCallback)

static const char*
GetMIMEType(nsIPluginInstance *aPluginInstance)
{
  if (aPluginInstance) {
    const char* mime = nsnull;
    if (NS_SUCCEEDED(aPluginInstance->GetMIMEType(&mime)) && mime)
      return mime;
  }
  return "";
}

static PRBool
DoDelayedStop(nsPluginInstanceOwner *aInstanceOwner, PRBool aDelayedStop)
{
#if (MOZ_PLATFORM_MAEMO==5)
  // Don't delay stop on Maemo/Hildon (bug 530739).
  if (aDelayedStop && aInstanceOwner->MatchPluginName("Shockwave Flash"))
    return PR_FALSE;
#endif

  // Don't delay stopping QuickTime (bug 425157), Flip4Mac (bug 426524),
  // XStandard (bug 430219), CMISS Zinc (bug 429604).
  if (aDelayedStop
#if !(defined XP_WIN || defined MOZ_X11)
      && !aInstanceOwner->MatchPluginName("QuickTime")
      && !aInstanceOwner->MatchPluginName("Flip4Mac")
      && !aInstanceOwner->MatchPluginName("XStandard plugin")
      && !aInstanceOwner->MatchPluginName("CMISS Zinc Plugin")
#endif
      ) {
    nsCOMPtr<nsIRunnable> evt = new nsStopPluginRunnable(aInstanceOwner);
    NS_DispatchToCurrentThread(evt);
    return PR_TRUE;
  }
  return PR_FALSE;
}

static void
DoStopPlugin(nsPluginInstanceOwner *aInstanceOwner, PRBool aDelayedStop)
{
  nsCOMPtr<nsIPluginInstance> inst;
  aInstanceOwner->GetInstance(*getter_AddRefs(inst));
  if (inst) {
    NPWindow *win;
    aInstanceOwner->GetWindow(win);
    nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;
    nsCOMPtr<nsIPluginInstance> nullinst;

    if (window) 
      window->CallSetWindow(nullinst);
    else 
      inst->SetWindow(nsnull);
    
    if (DoDelayedStop(aInstanceOwner, aDelayedStop))
      return;
    
    inst->Stop();

    nsCOMPtr<nsIPluginHost> pluginHost = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
    if (pluginHost)
      pluginHost->StopPluginInstance(inst);

    // the frame is going away along with its widget so tell the
    // window to forget its widget too
    if (window)
      window->SetPluginWidget(nsnull);
  }

  aInstanceOwner->Destroy();
}

NS_IMETHODIMP
nsStopPluginRunnable::Notify(nsITimer *aTimer)
{
  return Run();
}

NS_IMETHODIMP
nsStopPluginRunnable::Run()
{
  // InitWithCallback calls Release before AddRef so we need to hold a
  // strong ref on 'this' since we fall through to this scope if it fails.
  nsCOMPtr<nsITimerCallback> kungFuDeathGrip = this;
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    PRUint32 currentLevel = 0;
    appShell->GetEventloopNestingLevel(&currentLevel);
    if (currentLevel > mInstanceOwner->GetLastEventloopNestingLevel()) {
      if (!mTimer)
        mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (mTimer) {
        // Fire 100ms timer to try to tear down this plugin as quickly as
        // possible once the nesting level comes back down.
        nsresult rv = mTimer->InitWithCallback(this, 100, nsITimer::TYPE_ONE_SHOT);
        if (NS_SUCCEEDED(rv)) {
          return rv;
        }
      }
      NS_ERROR("Failed to setup a timer to stop the plugin later (at a safe "
               "time). Stopping the plugin now, this might crash.");
    }
  }

  mTimer = nsnull;

  DoStopPlugin(mInstanceOwner, PR_FALSE);

  return NS_OK;
}

void
nsObjectFrame::StopPlugin()
{
  PRBool delayedStop = PR_FALSE;
#ifdef XP_WIN
  nsCOMPtr<nsIPluginInstance> inst;
  if (mInstanceOwner)
    mInstanceOwner->GetInstance(*getter_AddRefs(inst));
  if (inst) {
    // Delayed stop for Real plugin only; see bug 420886, 426852.
    const char* pluginType = ::GetMIMEType(inst);
    delayedStop = strcmp(pluginType, "audio/x-pn-realaudio-plugin") == 0;
  }
#endif
  StopPluginInternal(delayedStop);
}

void
nsObjectFrame::StopPluginInternal(PRBool aDelayedStop)
{
  if (!mInstanceOwner) {
    return;
  }

  if (mWidget) {
    nsRootPresContext* rootPC = PresContext()->GetRootPresContext();
    if (rootPC) {
      rootPC->UnregisterPluginForGeometryUpdates(this);

      // Make sure the plugin is hidden in case an update of plugin geometry
      // hasn't happened since this plugin became hidden.
      nsIWidget* parent = mWidget->GetParent();
      if (parent) {
        nsTArray<nsIWidget::Configuration> configurations;
        GetEmptyClipConfiguration(&configurations);
        parent->ConfigureChildren(configurations);
        DidSetWidgetGeometry();
      }
    }
    else {
      NS_ASSERTION(PresContext()->PresShell()->IsFrozen(),
                   "unable to unregister the plugin frame");
    }
  }

  // Transfer the reference to the instance owner onto the stack so
  // that if we do end up re-entering this code, or if we unwind back
  // here witha deleted frame (this), we can still continue to stop
  // the plugin. Note that due to that, the ordering of the code in
  // this function is extremely important.

  nsRefPtr<nsPluginInstanceOwner> owner;
  owner.swap(mInstanceOwner);

  // Make sure that our windowless rect has been zeroed out, so if we
  // get reinstantiated we'll send the right messages to the plug-in.
  mWindowlessRect.Empty();

  PRBool oldVal = mPreventInstantiation;
  mPreventInstantiation = PR_TRUE;

  nsWeakFrame weakFrame(this);

#if defined(XP_WIN) || defined(MOZ_X11)
  if (aDelayedStop && mWidget) {
    // If we're asked to do a delayed stop it means we're stopping the
    // plugin because we're destroying the frame. In that case, disown
    // the widget.
    mInnerView->DetachWidgetEventHandler(mWidget);
    mWidget = nsnull;
  }
#endif

  // From this point on, |this| could have been deleted, so don't
  // touch it!
  owner->PrepareToStop(aDelayedStop);

  DoStopPlugin(owner, aDelayedStop);

  // If |this| is still alive, reset mPreventInstantiation.
  if (weakFrame.IsAlive()) {
    NS_ASSERTION(mPreventInstantiation,
                 "Instantiation should still be prevented!");

    mPreventInstantiation = oldVal;
  }

  // Break relationship between frame and plugin instance owner
  owner->SetOwner(nsnull);
}

void
nsObjectFrame::NotifyContentObjectWrapper()
{
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();
  if (!doc)
    return;

  nsIScriptGlobalObject *sgo = doc->GetScriptGlobalObject();
  if (!sgo)
    return;

  nsIScriptContext *scx = sgo->GetContext();
  if (!scx)
    return;

  JSContext *cx = (JSContext *)scx->GetNativeContext();

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfNativeObject(cx, sgo->GetGlobalJSObject(), mContent,
                                   NS_GET_IID(nsISupports),
                                   getter_AddRefs(wrapper));

  if (!wrapper) {
    // Nothing to do here if there's no wrapper for mContent. The proto
    // chain will be fixed appropriately when the wrapper is created.
    return;
  }

  JSObject *obj = nsnull;
  nsresult rv = wrapper->GetJSObject(&obj);
  if (NS_FAILED(rv))
    return;

  nsHTMLPluginObjElementSH::SetupProtoChain(wrapper, cx, obj);
}

// static
nsIObjectFrame *
nsObjectFrame::GetNextObjectFrame(nsPresContext* aPresContext, nsIFrame* aRoot)
{
  nsIFrame* child = aRoot->GetFirstChild(nsnull);

  while (child) {
    nsIObjectFrame* outFrame = do_QueryFrame(child);
    if (outFrame) {
      nsCOMPtr<nsIPluginInstance> pi;
      outFrame->GetPluginInstance(*getter_AddRefs(pi));  // make sure we have a REAL plugin
      if (pi)
        return outFrame;
    }

    outFrame = GetNextObjectFrame(aPresContext, child);
    if (outFrame)
      return outFrame;
    child = child->GetNextSibling();
  }

  return nsnull;
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
  nsRootPresContext* rootPC = objectFrame->PresContext()->GetRootPresContext();
  NS_ASSERTION(rootPC, "unable to unregister the plugin frame");
  rootPC->UnregisterPluginForGeometryUpdates(objectFrame);
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
  nsIWidget* widget = objectFrame->GetWidget();
  if (widget) {
    // Reparent the widget.
    nsIWidget* parent =
      rootPC->PresShell()->GetRootFrame()->GetNearestWidget();
    widget->SetParent(parent);
    objectFrame->CallSetWindow();

    // Register for geometry updates and make a request.
    rootPC->RegisterPluginForGeometryUpdates(objectFrame);
    rootPC->RequestUpdatePluginGeometry(objectFrame);
  }
}

nsIFrame*
NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsObjectFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsObjectFrame)


// nsPluginDOMContextMenuListener class implementation

nsPluginDOMContextMenuListener::nsPluginDOMContextMenuListener()
{
}

nsPluginDOMContextMenuListener::~nsPluginDOMContextMenuListener()
{
}

NS_IMPL_ISUPPORTS2(nsPluginDOMContextMenuListener,
                   nsIDOMContextMenuListener,
                   nsIDOMEventListener)

NS_IMETHODIMP
nsPluginDOMContextMenuListener::ContextMenu(nsIDOMEvent* aContextMenuEvent)
{
  aContextMenuEvent->PreventDefault(); // consume event

  return NS_OK;
}

nsresult nsPluginDOMContextMenuListener::Init(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMEventTarget> receiver(do_QueryInterface(aContent));
  if (receiver) {
    receiver->AddEventListener(NS_LITERAL_STRING("contextmenu"), this, PR_TRUE);
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

nsresult nsPluginDOMContextMenuListener::Destroy(nsIContent* aContent)
{
  // Unregister context menu listener
  nsCOMPtr<nsIDOMEventTarget> receiver(do_QueryInterface(aContent));
  if (receiver) {
    receiver->RemoveEventListener(NS_LITERAL_STRING("contextmenu"), this, PR_TRUE);
  }

  return NS_OK;
}

//plugin instance owner

nsPluginInstanceOwner::nsPluginInstanceOwner()
{
  // create nsPluginNativeWindow object, it is derived from NPWindow
  // struct and allows to manipulate native window procedure
  nsCOMPtr<nsIPluginHost> ph = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  if (ph)
    ph->NewPluginNativeWindow(&mPluginWindow);
  else
    mPluginWindow = nsnull;

  mObjectFrame = nsnull;
  mTagText = nsnull;
#ifdef XP_MACOSX
  memset(&mCGPluginPortCopy, 0, sizeof(NP_CGContext));
#ifndef NP_NO_QUICKDRAW
  memset(&mQDPluginPortCopy, 0, sizeof(NP_Port));
#endif
  mInCGPaintLevel = 0;
  mSentInitialTopLevelWindowEvent = PR_FALSE;
  mIOSurface = nsnull;
  mPluginPortChanged = PR_FALSE;
#endif
  mContentFocused = PR_FALSE;
  mWidgetVisible = PR_TRUE;
  mNumCachedAttrs = 0;
  mNumCachedParams = 0;
  mCachedAttrParamNames = nsnull;
  mCachedAttrParamValues = nsnull;
  mDestroyWidget = PR_FALSE;

#ifdef MOZ_COMPOSITED_PLUGINS
  mLastPoint = nsIntPoint(0,0);
#endif

#ifdef MOZ_USE_IMAGE_EXPOSE
  mPluginSize = nsIntSize(0,0);
  mXlibSurfGC = None;
  mBlitWindow = nsnull;
  mSharedXImage = nsnull;
  mSharedSegmentInfo.shmaddr = nsnull;
#endif

#ifdef XP_MACOSX
#ifndef NP_NO_QUICKDRAW
  mEventModel = NPEventModelCarbon;
#else
  mEventModel = NPEventModelCocoa;
#endif
#endif

  mWaitingForPaint = PR_FALSE;

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsPluginInstanceOwner %p created\n", this));
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  PRInt32 cnt;

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsPluginInstanceOwner %p deleted\n", this));

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

  // clean up plugin native window object
  nsCOMPtr<nsIPluginHost> ph = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  if (ph) {
    ph->DeletePluginNativeWindow(mPluginWindow);
    mPluginWindow = nsnull;
  }

  if (mInstance) {
    mInstance->InvalidateOwner();
  }

#ifdef MOZ_USE_IMAGE_EXPOSE
  ReleaseXShm();
#endif
}

/*
 * nsISupports Implementation
 */

NS_IMPL_ADDREF(nsPluginInstanceOwner)
NS_IMPL_RELEASE(nsPluginInstanceOwner)

NS_INTERFACE_MAP_BEGIN(nsPluginInstanceOwner)
  NS_INTERFACE_MAP_ENTRY(nsIPluginInstanceOwner)
  NS_INTERFACE_MAP_ENTRY(nsIPluginTagInfo)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPluginInstanceOwner)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsPluginInstanceOwner::SetInstance(nsIPluginInstance *aInstance)
{
  NS_ASSERTION(!mInstance || !aInstance, "mInstance should only be set once!");

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

NS_IMETHODIMP nsPluginInstanceOwner::GetInstance(nsIPluginInstance *&aInstance)
{
  NS_IF_ADDREF(aInstance = mInstance);

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetURL(const char *aURL,
                                            const char *aTarget,
                                            nsIInputStream *aPostStream,
                                            void *aHeadersData,
                                            PRUint32 aHeadersDataLen)
{
  NS_ENSURE_TRUE(mObjectFrame, NS_ERROR_NULL_POINTER);

  if (mContent->IsEditable()) {
    return NS_OK;
  }

  // the container of the pres context will give us the link handler
  nsCOMPtr<nsISupports> container = mObjectFrame->PresContext()->GetContainer();
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
    nsContentUtils::GetIntPref("privacy.popups.disable_from_plugins");
  nsAutoPopupStatePusher popupStatePusher((PopupControlState)blockPopups);

  rv = lh->OnLinkClick(mContent, uri, unitarget.get(), 
                       aPostStream, headersDataStream);

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
  NS_IF_ADDREF(*aDocument = mContent->GetOwnerDoc());
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRect(NPRect *invalidRect)
{
  if (!mObjectFrame || !invalidRect || !mWidgetVisible)
    return NS_ERROR_FAILURE;


  // Each time an asynchronously-drawing plugin sends a new surface to display,
  // InvalidateRect is called. We notify reftests that painting is up to
  // date and update our ImageContainer with the new surface.
  nsRefPtr<ImageContainer> container = mObjectFrame->GetImageContainer();
  if (container) {
    SetCurrentImage(container);
  }

  if (mWaitingForPaint && IsUpToDate()) {
    nsContentUtils::DispatchTrustedEvent(mContent->GetOwnerDoc(), mContent,
                                         NS_LITERAL_STRING("MozPaintWaitFinished"),
                                         PR_TRUE, PR_TRUE);
    mWaitingForPaint = false;
  }

#ifdef MOZ_USE_IMAGE_EXPOSE
  PRBool simpleImageRender = PR_FALSE;
  mInstance->GetValueFromPlugin(NPPVpluginWindowlessLocalBool,
                                &simpleImageRender);
  if (simpleImageRender) {  
    NativeImageDraw(invalidRect);
    return NS_OK;
  }
#endif

#ifndef XP_MACOSX
  // Windowed plugins should not be calling NPN_InvalidateRect, but
  // Silverlight does and expects it to "work"
  if (mWidget) {
    mWidget->Invalidate(nsIntRect(invalidRect->left, invalidRect->top,
                                  invalidRect->right - invalidRect->left,
                                  invalidRect->bottom - invalidRect->top),
                        PR_FALSE);
    return NS_OK;
  }
#endif

  nsPresContext* presContext = mObjectFrame->PresContext();
  nsRect rect(presContext->DevPixelsToAppUnits(invalidRect->left),
              presContext->DevPixelsToAppUnits(invalidRect->top),
              presContext->DevPixelsToAppUnits(invalidRect->right - invalidRect->left),
              presContext->DevPixelsToAppUnits(invalidRect->bottom - invalidRect->top));
  mObjectFrame->Invalidate(rect + mObjectFrame->GetUsedBorderAndPadding().TopLeft());
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRegion(NPRegion invalidRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginInstanceOwner::ForceRedraw()
{
  NS_ENSURE_TRUE(mObjectFrame, NS_ERROR_NULL_POINTER);
  nsIView* view = mObjectFrame->GetView();
  if (view) {
    return view->GetViewManager()->Composite();
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
#elif defined(MOZ_WIDGET_GTK2)
  // X11 window managers want the toplevel window for WM_TRANSIENT_FOR.
  nsIWidget* win = mObjectFrame->GetNearestWidget();
  if (!win)
    return NS_ERROR_FAILURE;
  GdkWindow* gdkWindow = static_cast<GdkWindow*>(win->GetNativeData(NS_NATIVE_WINDOW));
  if (!gdkWindow)
    return NS_ERROR_FAILURE;
  gdkWindow = gdk_window_get_toplevel(gdkWindow);
#ifdef MOZ_X11
  *static_cast<Window*>(value) = GDK_WINDOW_XID(gdkWindow);
#endif
  return NS_OK;
#elif defined(MOZ_WIDGET_QT)
  // X11 window managers want the toplevel window for WM_TRANSIENT_FOR.
  nsIWidget* win = mObjectFrame->GetNearestWidget();
  if (!win)
    return NS_ERROR_FAILURE;
  QWidget* widget = static_cast<QWidget*>(win->GetNativeData(NS_NATIVE_WINDOW));
  if (!widget)
    return NS_ERROR_FAILURE;
#ifdef MOZ_X11
  *static_cast<Window*>(value) = widget->handle();
  return NS_OK;
#endif
  return NS_ERROR_FAILURE;
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
    return PR_FALSE;

  return NS_NPAPI_ConvertPointCocoa(mWidget->GetNativeData(NS_NATIVE_WIDGET),
                                    sourceX, sourceY, sourceSpace, destX, destY, destSpace);
#else
  // we should implement this for all platforms
  return PR_FALSE;
#endif
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

    nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID,&rv));
    if (NS_FAILED(rv))
      return rv;

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

    nsIDocument* doc = mContent->GetOwnerDoc();
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
    {"x-gbk",           "GBK"},
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
    {"windows-936",     "MS936"},
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

NS_IMETHODIMP nsPluginInstanceOwner::GetUniqueID(PRUint32 *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = NS_PTR_TO_INT32(mObjectFrame);
  return NS_OK;
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
  NS_ENSURE_TRUE(mObjectFrame, NS_ERROR_NULL_POINTER);

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

  // We're done with DOM method calls now. Make sure we still have a frame.
  NS_ENSURE_TRUE(mObjectFrame, NS_ERROR_OUT_OF_MEMORY);

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
  nsAdoptingCString wmodeType = nsContentUtils::GetCharPref("plugins.force.wmode");
  if (!wmodeType.IsEmpty()) {
    mNumCachedAttrs++;
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

  // Potentially add WMODE attribute.
  if (!wmodeType.IsEmpty()) {
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(NS_LITERAL_STRING("wmode"));
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(NS_ConvertUTF8toUTF16(wmodeType));
    nextAttrParamIndex++;
  }

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
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(value);
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
  mCachedAttrParamValues[nextAttrParamIndex] = nsnull;
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
    name.Trim(" \n\r\t\b", PR_TRUE, PR_TRUE, PR_FALSE);
    value.Trim(" \n\r\t\b", PR_TRUE, PR_TRUE, PR_FALSE);
    mCachedAttrParamNames [nextAttrParamIndex] = ToNewUTF8String(name);
    mCachedAttrParamValues[nextAttrParamIndex] = ToNewUTF8String(value);
    nextAttrParamIndex++;
  }

  return NS_OK;
}


// Here's where we forward events to plugins.

#ifdef XP_MACOSX

#ifndef NP_NO_CARBON
static void InitializeEventRecord(EventRecord* event, Point* aMousePosition)
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

void nsPluginInstanceOwner::AddToCARefreshTimer(nsPluginInstanceOwner *aPluginInstance) {
  if (!sCARefreshListeners) {
    sCARefreshListeners = new nsTArray<nsPluginInstanceOwner*>();
    if (!sCARefreshListeners) {
      return;
    }
  }

  NS_ASSERTION(!sCARefreshListeners->Contains(aPluginInstance), 
      "pluginInstanceOwner already registered as a listener");
  sCARefreshListeners->AppendElement(aPluginInstance);

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

void nsPluginInstanceOwner::RemoveFromCARefreshTimer(nsPluginInstanceOwner *aPluginInstance) {
  if (!sCARefreshListeners || sCARefreshListeners->Contains(aPluginInstance) == false) {
    return;
  }

  sCARefreshListeners->RemoveElement(aPluginInstance);

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

void nsPluginInstanceOwner::SetupCARefresh()
{
  const char* pluginType = GetMIMEType(mInstance);
  if (strcmp(pluginType, "application/x-shockwave-flash") != 0) {
    // We don't need a timer since Flash is invoking InvalidateRect for us.
    AddToCARefreshTimer(this);
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
    if (mIOSurface) {
      delete mIOSurface;
    }

    // If the renderer is backed by an IOSurface, resize it as required.
    mIOSurface = nsIOSurface::CreateIOSurface(aWidth, aHeight);
    if (mIOSurface) {
      nsIOSurface *attachSurface = nsIOSurface::LookupSurface(
                                      mIOSurface->GetIOSurfaceID());
      if (attachSurface) {
        mCARenderer.AttachIOSurface(attachSurface);
      } else {
        NS_ERROR("IOSurface attachment failed");
        delete attachSurface;
        delete mIOSurface;
        mIOSurface = NULL;
      }
    }
  }

  if (mCARenderer.isInit() == false) {
    void *caLayer = NULL;
    mInstance->GetValueFromPlugin(NPPVpluginCoreAnimationLayer, &caLayer);
    if (!caLayer) {
      return;
    }

    mCARenderer.SetupRenderer(caLayer, aWidth, aHeight);

    // Setting up the CALayer requires resetting the painting otherwise we
    // get garbage for the first few frames.
    FixUpPluginWindow(ePluginPaintDisable);
    FixUpPluginWindow(ePluginPaintEnable);
  }

  CGImageRef caImage = NULL;
  nsresult rt = mCARenderer.Render(aWidth, aHeight, &caImage);
  if (rt == NS_OK && mIOSurface) {
    nsCARenderer::DrawSurfaceToCGContext(aCGContext, mIOSurface, CreateSystemColorSpace(),
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
      mPluginPortChanged = PR_TRUE;
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
        mPluginPortChanged = PR_TRUE;
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

/*=============== nsIDOMFocusListener ======================*/
nsresult nsPluginInstanceOwner::Focus(nsIDOMEvent * aFocusEvent)
{
  mContentFocused = PR_TRUE;
  return DispatchFocusToPlugin(aFocusEvent);
}

nsresult nsPluginInstanceOwner::Blur(nsIDOMEvent * aFocusEvent)
{
  mContentFocused = PR_FALSE;
  return DispatchFocusToPlugin(aFocusEvent);
}

nsresult nsPluginInstanceOwner::DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent)
{
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
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, focusEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, privateEvent null");   
  
  return NS_OK;
}    


/*=============== nsIKeyListener ======================*/
nsresult nsPluginInstanceOwner::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return DispatchKeyToPlugin(aKeyEvent);
}

nsresult nsPluginInstanceOwner::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return DispatchKeyToPlugin(aKeyEvent);
}

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
      const nsGUIEvent *guiEvent = (nsGUIEvent*)theEvent;
      const EventRecord *ev = (EventRecord*)(guiEvent->pluginEvent); 
      if (guiEvent &&
          guiEvent->message == NS_KEY_PRESS &&
          ev &&
          ev->what == keyDown)
        return aKeyEvent->PreventDefault(); // consume event
    }

    // Nasty hack to avoid recursive event dispatching with Java. Java can
    // dispatch key events to a TSM handler, which comes back and calls 
    // [ChildView insertText:] on the cocoa widget, which sends a key
    // event back down.
    static PRBool sInKeyDispatch = PR_FALSE;

    if (sInKeyDispatch)
      return aKeyEvent->PreventDefault(); // consume event

    sInKeyDispatch = PR_TRUE;
    nsresult rv =  DispatchKeyToPlugin(aKeyEvent);
    sInKeyDispatch = PR_FALSE;
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
#if !defined(XP_MACOSX) && !defined(MOZ_COMPOSITED_PLUGINS)
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow))
    return aKeyEvent->PreventDefault(); // consume event
  // continue only for cases without child window
#endif

  if (mInstance) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aKeyEvent));
    if (privateEvent) {
      nsKeyEvent *keyEvent = (nsKeyEvent *) privateEvent->GetInternalNSEvent();
      if (keyEvent) {
        nsEventStatus rv = ProcessEvent(*keyEvent);
        if (nsEventStatus_eConsumeNoDefault == rv) {
          aKeyEvent->PreventDefault();
          aKeyEvent->StopPropagation();
        }
      }
      else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, keyEvent null");   
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, privateEvent null");   
  }

  return NS_OK;
}    

/*=============== nsIDOMMouseMotionListener ======================*/

nsresult
nsPluginInstanceOwner::MouseMove(nsIDOMEvent* aMouseEvent)
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
    nsMouseEvent* mouseEvent = (nsMouseEvent *) privateEvent->GetInternalNSEvent();
    if (mouseEvent) {
      nsEventStatus rv = ProcessEvent(*mouseEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        return aMouseEvent->PreventDefault(); // consume event
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseMove failed, mouseEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseMove failed, privateEvent null");   
  
  return NS_OK;
}

/*=============== nsIDOMMouseListener ======================*/

nsresult
nsPluginInstanceOwner::MouseDown(nsIDOMEvent* aMouseEvent)
{
#if !defined(XP_MACOSX) && !defined(MOZ_COMPOSITED_PLUGINS)
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
    nsMouseEvent* mouseEvent = (nsMouseEvent *) privateEvent->GetInternalNSEvent();
    if (mouseEvent) {
      nsEventStatus rv = ProcessEvent(*mouseEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        return aMouseEvent->PreventDefault(); // consume event
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseDown failed, mouseEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseDown failed, privateEvent null");   
  
  return NS_OK;
}

nsresult
nsPluginInstanceOwner::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult nsPluginInstanceOwner::DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent)
{
#if !defined(XP_MACOSX) && !defined(MOZ_COMPOSITED_PLUGINS)
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow))
    return aMouseEvent->PreventDefault(); // consume event
  // continue only for cases without child window
#endif
  // don't send mouse events if we are hidden
  if (!mWidgetVisible)
    return NS_OK;

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
  if (privateEvent) {
    nsMouseEvent* mouseEvent = (nsMouseEvent *) privateEvent->GetInternalNSEvent();
    if (mouseEvent) {
      nsEventStatus rv = ProcessEvent(*mouseEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aMouseEvent->PreventDefault();
        aMouseEvent->StopPropagation();
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchMouseToPlugin failed, mouseEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchMouseToPlugin failed, privateEvent null");   
  
  return NS_OK;
}

nsresult
nsPluginInstanceOwner::HandleEvent(nsIDOMEvent* aEvent)
{
  if (mInstance) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aEvent));
    nsCOMPtr<nsIDOMDragEvent> dragEvent(do_QueryInterface(aEvent));
    if (privateEvent && dragEvent) {
      nsEvent* ievent = privateEvent->GetInternalNSEvent();
      if (ievent && NS_IS_TRUSTED_EVENT(ievent) &&
          (ievent->message == NS_DRAGDROP_ENTER || ievent->message == NS_DRAGDROP_OVER)) {
        // set the allowed effect to none here. The plugin should set it if necessary
        nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
        dragEvent->GetDataTransfer(getter_AddRefs(dataTransfer));
        if (dataTransfer)
          dataTransfer->SetEffectAllowed(NS_LITERAL_STRING("none"));
      }

      // Let the plugin handle drag events.
      aEvent->PreventDefault();
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

#ifdef MOZ_COMPOSITED_PLUGINS
static void find_dest_id(XID top, XID *root, XID *dest, int target_x, int target_y)
{
  XID target_id = top;
  XID parent;
  XID *children;
  unsigned int nchildren;

  Display *display = DefaultXDisplay();

  while (1) {
loop:
    //printf("searching %x\n", target_id);
    if (!XQueryTree(display, target_id, root, &parent, &children, &nchildren) ||
        !nchildren)
      break;
    for (unsigned int i=0; i<nchildren; i++) {
      Window root;
      int x, y;
      unsigned int width, height;
      unsigned int border_width, depth;
      XGetGeometry(display, children[i], &root, &x, &y,
          &width, &height, &border_width,
          &depth);
      //printf("target: %d %d\n", target_x, target_y);
      //printf("geom: %dx%x @ %dx%d\n", width, height, x, y);
      // XXX: we may need to be more careful here, i.e. if
      // this condition matches more than one child
      if (target_x >= x && target_y >= y &&
          target_x <= x + int(width) &&
          target_y <= y + int(height)) {
        target_id = children[i];
        // printf("found new target: %x\n", target_id);
        XFree(children);
        goto loop;
      }
    }
    XFree(children);
    /* no children contain the target */
    break;
  }
  *dest = target_id;
}
#endif

#ifdef MOZ_COMPOSITED_PLUGINS
nsEventStatus nsPluginInstanceOwner::ProcessEventX11Composited(const nsGUIEvent& anEvent)
{
  //printf("nsGUIEvent.message: %d\n", anEvent.message);
  nsEventStatus rv = nsEventStatus_eIgnore;
  if (!mInstance || !mObjectFrame)   // if mInstance is null, we shouldn't be here
    return rv;

  // this code supports windowless plugins
  nsIWidget* widget = anEvent.widget;
  XEvent pluginEvent;
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
          mObjectFrame->GetUsedBorderAndPadding().TopLeft();
        nsIntPoint pluginPoint(presContext->AppUnitsToDevPixels(appPoint.x),
                               presContext->AppUnitsToDevPixels(appPoint.y));
        mLastPoint = pluginPoint;
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
        Window root = None;
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
              XEvent be;
              be.xmotion = pluginEvent.xmotion;
              //printf("xmotion: %d %d\n", be.xmotion.x, be.xmotion.y);
              XID w = (XID)mPluginWindow->window;
              be.xmotion.window = w;
              XSendEvent (be.xmotion.display, w,
                  FALSE, ButtonMotionMask, &be);

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
              XEvent be;
              be.xbutton =  event;
              XID target;
              XID root;
              int wx, wy;
              unsigned int width, height, border_width, depth;

              //printf("xbutton: %d %d %d\n", anEvent.message, be.xbutton.x, be.xbutton.y);
              XID w = (XID)mPluginWindow->window;
              XGetGeometry(DefaultXDisplay(), w, &root, &wx, &wy, &width, &height, &border_width, &depth);
              find_dest_id(w, &root, &target, pluginPoint.x + wx, pluginPoint.y + wy);
              be.xbutton.window = target;
              XSendEvent (DefaultXDisplay(), target,
                  FALSE, event.type == ButtonPress ? ButtonPressMask : ButtonReleaseMask, &be);

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
          XEvent be;
          be.xkey =  event;
          XID target;
          XID root;
          int wx, wy;
          unsigned int width, height, border_width, depth;

          //printf("xkey: %d %d %d\n", anEvent.message, be.xkey.keycode, be.xkey.state);
          XID w = (XID)mPluginWindow->window;
          XGetGeometry(DefaultXDisplay(), w, &root, &wx, &wy, &width, &height, &border_width, &depth);
          find_dest_id(w, &root, &target, mLastPoint.x + wx, mLastPoint.y + wy);
          be.xkey.window = target;
          XSendEvent (DefaultXDisplay(), target,
              FALSE, event.type == XKeyPress ? KeyPressMask : KeyReleaseMask, &be);


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
    PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
           ("Unhandled event message %d with struct type %d\n",
            anEvent.message, anEvent.eventStructType));
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

#if 0
  /* we've sent the event via XSendEvent so don't send it directly to the plugin */
  PRInt16 response = kNPEventNotHandled;
  mInstance->HandleEvent(&pluginEvent, &response);
  if (response == kNPEventHandled)
    rv = nsEventStatus_eConsumeNoDefault;
#endif

  return rv;
}
#endif

nsEventStatus nsPluginInstanceOwner::ProcessEvent(const nsGUIEvent& anEvent)
{
  // printf("nsGUIEvent.message: %d\n", anEvent.message);

#ifdef MOZ_COMPOSITED_PLUGINS
  if (mPluginWindow && (mPluginWindow->type != NPWindowTypeDrawable))
    return ProcessEventX11Composited(anEvent);
#endif

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
  mObjectFrame->GetUsedBorderAndPadding().TopLeft();
  nsPresContext* presContext = mObjectFrame->PresContext();
  nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x),
                  presContext->AppUnitsToDevPixels(pt.y));
#ifndef NP_NO_CARBON
  nsIntPoint geckoScreenCoords = mWidget->WidgetToScreenOffset();
  Point carbonPt = { ptPx.y + geckoScreenCoords.y, ptPx.x + geckoScreenCoords.x };
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
        nsCOMPtr<nsFrameSelection> frameselection = mObjectFrame->GetFrameSelection();
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
      // See use of NPEvent in widget/src/windows/nsWindow.cpp
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
        mObjectFrame->GetUsedBorderAndPadding().TopLeft();
      nsPresContext* presContext = mObjectFrame->PresContext();
      nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x),
                      presContext->AppUnitsToDevPixels(pt.y));
      nsIntPoint widgetPtPx = ptPx + mObjectFrame->GetWindowOriginInPixels(PR_TRUE);
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
          mObjectFrame->GetUsedBorderAndPadding().TopLeft();
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
    PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
           ("Unhandled event message %d with struct type %d\n",
            anEvent.message, anEvent.eventStructType));
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

  return rv;
}

nsresult
nsPluginInstanceOwner::Destroy()
{
#ifdef MAC_CARBON_PLUGINS
  // stop the timer explicitly to reduce reference count.
  CancelTimer();
#endif
#ifdef XP_MACOSX
  RemoveFromCARefreshTimer(this);
  if (mIOSurface)
    delete mIOSurface;
#endif

  // unregister context menu listener
  if (mCXMenuListener) {
    mCXMenuListener->Destroy(mContent);
    mCXMenuListener = nsnull;
  }

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mContent));
  if (target) {

    nsCOMPtr<nsIDOMEventListener> listener;
    QueryInterface(NS_GET_IID(nsIDOMEventListener), getter_AddRefs(listener));

    // Unregister focus event listener
    mContent->RemoveEventListenerByIID(listener, NS_GET_IID(nsIDOMFocusListener));

    // Unregister mouse event listener
    mContent->RemoveEventListenerByIID(listener, NS_GET_IID(nsIDOMMouseListener));

    // now for the mouse motion listener
    mContent->RemoveEventListenerByIID(listener, NS_GET_IID(nsIDOMMouseMotionListener));

    // Unregister key event listener;
    target->RemoveEventListener(NS_LITERAL_STRING("keypress"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("keydown"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("keyup"), listener, PR_TRUE);

    // Unregister drag event listener;
    target->RemoveEventListener(NS_LITERAL_STRING("drop"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("dragdrop"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("drag"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("dragenter"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("dragover"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("dragexit"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("dragleave"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("dragstart"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("draggesture"), listener, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("dragend"), listener, PR_TRUE);
  }

  if (mWidget) {
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget)
      pluginWidget->SetPluginInstanceOwner(nsnull);

    if (mDestroyWidget)
      mWidget->Destroy();
  }

  return NS_OK;
}

/*
 * Prepare to stop 
 */
void
nsPluginInstanceOwner::PrepareToStop(PRBool aDelayedStop)
{
  // Drop image reference because the child may destroy the surface after we return.
  if (mLayerSurface) {
     nsRefPtr<ImageContainer> container = mObjectFrame->GetImageContainer();
     container->SetCurrentImage(nsnull);
     mLayerSurface = nsnull;
  }

#if defined(XP_WIN) || defined(MOZ_X11)
  if (aDelayedStop && mWidget) {
    // To delay stopping a plugin we need to reparent the plugin
    // so that we can safely tear down the
    // plugin after its frame (and view) is gone.

    // Also hide and disable the widget to avoid it from appearing in
    // odd places after reparenting it, but before it gets destroyed.
    mWidget->Show(PR_FALSE);
    mWidget->Enable(PR_FALSE);

    // Reparent the plugins native window. This relies on the widget
    // and plugin et al not holding any other references to its
    // parent.
    mWidget->SetParent(nsnull);

    mDestroyWidget = PR_TRUE;
  }
#endif

  // Unregister scroll position listeners
  for (nsIFrame* f = mObjectFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    nsIScrollableFrame* sf = do_QueryFrame(f);
    if (sf) {
      sf->RemoveScrollPositionListener(this);
    }
  }
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
      // The context given here is only valid during the HandleEvent call.
      NPCocoaEvent updateEvent;
      InitializeNPCocoaEvent(&updateEvent);
      updateEvent.type = NPCocoaEventDrawRect;
      updateEvent.data.draw.context = cgContext;
      updateEvent.data.draw.x = aDirtyRect.X();
      updateEvent.data.draw.y = aDirtyRect.Y();
      updateEvent.data.draw.width = aDirtyRect.Width();
      updateEvent.data.draw.height = aDirtyRect.Height();

      mInstance->HandleEvent(&updateEvent, nsnull);
    }
    pluginWidget->EndDrawPlugin();
  }
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

#if defined(MOZ_X11)
void nsPluginInstanceOwner::Paint(gfxContext* aContext,
                                  const gfxRect& aFrameRect,
                                  const gfxRect& aDirtyRect)
{
  if (!mInstance || !mObjectFrame)
    return;

#ifdef MOZ_USE_IMAGE_EXPOSE
  // through to be able to paint the context passed in.  This allows
  // us to handle plugins that do not self invalidate (slowly, but
  // accurately), and it allows us to reduce flicker.
  PRBool simpleImageRender = PR_FALSE;
  mInstance->GetValueFromPlugin(NPPVpluginWindowlessLocalBool,
                                &simpleImageRender);
  if (simpleImageRender) {
    gfxMatrix matrix = aContext->CurrentMatrix();
    if (!matrix.HasNonAxisAlignedTransform())
      NativeImageDraw();
    return;
  } 
#endif

  // to provide crisper and faster drawing.
  gfxRect pluginRect = aFrameRect;
  if (aContext->UserToDevicePixelSnapped(pluginRect)) {
    pluginRect = aContext->DeviceToUser(pluginRect);
  }

  // Round out the dirty rect to plugin pixels to ensure the plugin draws
  // enough pixels for interpolation to device pixels.
  gfxRect dirtyRect = aDirtyRect + -pluginRect.pos;
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
  nsIntSize pluginSize(NS_lround(pluginRect.size.width),
                       NS_lround(pluginRect.size.height));

  // Determine what the plugin needs to draw.
  nsIntRect pluginDirtyRect(PRInt32(dirtyRect.pos.x),
                            PRInt32(dirtyRect.pos.y),
                            PRInt32(dirtyRect.size.width),
                            PRInt32(dirtyRect.size.height));
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

  PRBool transparent;
  mInstance->IsTransparent(&transparent);
  if (!transparent)
    rendererFlags |= Renderer::DRAW_IS_OPAQUE;

  // Renderer::Draw() draws a rectangle with top-left at the aContext origin.
  gfxContextAutoSaveRestore autoSR(aContext);
  aContext->Translate(pluginRect.pos);

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

#ifdef MOZ_USE_IMAGE_EXPOSE

static GdkWindow* GetClosestWindow(nsIDOMElement *element)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame)
    return nsnull;

  nsIWidget* win = frame->GetNearestWidget();
  if (!win)
    return nsnull;

  GdkWindow* w = static_cast<GdkWindow*>(win->GetNativeData(NS_NATIVE_WINDOW));
  return w;
}

void
nsPluginInstanceOwner::ReleaseXShm()
{
  if (mXlibSurfGC) {
    XFreeGC(gdk_x11_get_default_xdisplay(), mXlibSurfGC);
    mXlibSurfGC = None;
  }
 
 if (mSharedSegmentInfo.shmaddr) {
    XShmDetach(gdk_x11_get_default_xdisplay(), &mSharedSegmentInfo);
    shmdt(mSharedSegmentInfo.shmaddr);
    mSharedSegmentInfo.shmaddr = nsnull;
  }

  if (mSharedXImage) {
    XDestroyImage(mSharedXImage);
    mSharedXImage = nsnull;
  }
}

PRBool
nsPluginInstanceOwner::SetupXShm()
{
  if (!mBlitWindow)
    return PR_FALSE;

  ReleaseXShm();

  mXlibSurfGC = XCreateGC(gdk_x11_get_default_xdisplay(),
                          mBlitWindow,
                          0,
                          0);
  if (!mXlibSurfGC)
    return PR_FALSE;

  // we use 16 as the default depth because that is the value of the
  // screen, but not the default X default depth.
  XVisualInfo vinfo;
  int foundVisual = XMatchVisualInfo(gdk_x11_get_default_xdisplay(),
                                     gdk_x11_get_default_screen(),
                                     16,
                                     TrueColor,
                                     &vinfo);
  if (!foundVisual) 
    return PR_FALSE;

  memset(&mSharedSegmentInfo, 0, sizeof(XShmSegmentInfo));
  mSharedXImage = XShmCreateImage(gdk_x11_get_default_xdisplay(),
                                  vinfo.visual,
                                  16,
                                  ZPixmap,
                                  0,
                                  &mSharedSegmentInfo,
                                  mPluginSize.width,
                                  mPluginSize.height);
  if (!mSharedXImage)
    return PR_FALSE;

  NS_ASSERTION(mSharedXImage->height, "do not call shmget with zero");
  mSharedSegmentInfo.shmid = shmget(IPC_PRIVATE,
                                    mSharedXImage->bytes_per_line * mSharedXImage->height,
                                    IPC_CREAT | 0600);
  if (mSharedSegmentInfo.shmid == -1) {
    XDestroyImage(mSharedXImage);
    mSharedXImage = nsnull;
    return PR_FALSE;
  }

  mSharedXImage->data = static_cast<char*>(shmat(mSharedSegmentInfo.shmid, 0, 0));
  if (mSharedXImage->data == (char*) -1) {
    shmctl(mSharedSegmentInfo.shmid, IPC_RMID, 0);
    XDestroyImage(mSharedXImage);
    mSharedXImage = nsnull;
    return PR_FALSE;
  }
    
  mSharedSegmentInfo.shmaddr = mSharedXImage->data;
  mSharedSegmentInfo.readOnly = False;

  Status s = XShmAttach(gdk_x11_get_default_xdisplay(), &mSharedSegmentInfo);
  XSync(gdk_x11_get_default_xdisplay(), False);
  shmctl(mSharedSegmentInfo.shmid, IPC_RMID, 0);
  if (!s) {
    // attach failed, call shmdt and null shmaddr before calling
    // ReleaseXShm().
    shmdt(mSharedSegmentInfo.shmaddr);
    mSharedSegmentInfo.shmaddr = nsnull;
    ReleaseXShm();
    return PR_FALSE;
  }

  return PR_TRUE;
}


// NativeImageDraw
//
// This method supports the NPImageExpose API which is specific to the
// HILDON platform.  Basically what it allows us to do is to pass a
// memory buffer into a plugin (namely flash), and have flash draw
// directly into the buffer.
//
// It may be faster if the rest of the system used offscreen image
// surfaces, but right now offscreen surfaces are using X
// surfaces. And because of this, we need to create a new image
// surface and copy that to the passed gfx context.
//
// This is not ideal and it should not be faster than what a
// windowless plugin can do.  However, in A/B testing of flash on the
// N900, this approach is considerably faster.
//
// Hopefully this API can die off in favor of a more robust plugin API.

void
nsPluginInstanceOwner::NativeImageDraw(NPRect* invalidRect)
{
  // if we haven't been positioned yet, ignore
  if (!mBlitWindow)
    return;

  // if the clip rect is zero, we have nothing to do.
  if (NSToIntCeil(mAbsolutePositionClip.Width()) == 0 ||
      NSToIntCeil(mAbsolutePositionClip.Height()) == 0)
    return;
  
  // The flash plugin on Maemo n900 requires the width/height to be
  // even.
  PRInt32 absPosWidth  = NSToIntCeil(mAbsolutePosition.Width()) / 2 * 2;
  PRInt32 absPosHeight = NSToIntCeil(mAbsolutePosition.Height()) / 2 * 2;

  // if the plugin is hidden, nothing to draw.
  if (absPosHeight == 0 || absPosWidth == 0)
    return;

  // Making X or DOM method calls can cause our frame to go
  // away, which might kill us...
  nsCOMPtr<nsIPluginInstanceOwner> kungFuDeathGrip(this);

  PRBool sizeChanged = (mPluginSize.width != absPosWidth ||
                        mPluginSize.height != absPosHeight);

  if (!mSharedXImage || sizeChanged) {
    mPluginSize = nsIntSize(absPosWidth, absPosHeight);

    if (NS_FAILED(SetupXShm()))
      return;
  }  
  
  NPWindow* window;
  GetWindow(window);
  NS_ASSERTION(window, "Window can not be null");

  // setup window such that it knows about the size and clip.  This
  // is to work around a flash clipping bug when using the Image
  // Expose API.
  if (!invalidRect && sizeChanged) {
    NPRect newClipRect;
    newClipRect.left = 0;
    newClipRect.top = 0;
    newClipRect.right = window->width;
    newClipRect.bottom = window->height;
    
    window->clipRect = newClipRect; 
    window->x = 0;
    window->y = 0;
      
    NPSetWindowCallbackStruct* ws_info =
      static_cast<NPSetWindowCallbackStruct*>(window->ws_info);
    ws_info->visual = 0;
    ws_info->colormap = 0;
    ws_info->depth = 16;
    mInstance->SetWindow(window);
  }

  NPEvent pluginEvent;
  NPImageExpose imageExpose;
  XGraphicsExposeEvent& exposeEvent = pluginEvent.xgraphicsexpose;

  // set the drawing info
  exposeEvent.type = GraphicsExpose;
  exposeEvent.display = 0;

  // Store imageExpose structure pointer as drawable member
  exposeEvent.drawable = (Drawable)&imageExpose;
  exposeEvent.count = 0;
  exposeEvent.serial = 0;
  exposeEvent.send_event = False;
  exposeEvent.major_code = 0;
  exposeEvent.minor_code = 0;

  exposeEvent.x = 0;
  exposeEvent.y = 0;
  exposeEvent.width  = window->width;
  exposeEvent.height = window->height;

  imageExpose.x = 0;
  imageExpose.y = 0;
  imageExpose.width  = window->width;
  imageExpose.height = window->height;

  imageExpose.depth = 16;

  imageExpose.translateX = 0;
  imageExpose.translateY = 0;

  if (window->width == 0)
    return;
  
  float scale = mAbsolutePosition.Width() / (float) window->width;
  
  imageExpose.scaleX = scale;
  imageExpose.scaleY = scale;

  imageExpose.stride          = mPluginSize.width * 2;
  imageExpose.data            = mSharedXImage->data;
  imageExpose.dataSize.width  = mPluginSize.width;
  imageExpose.dataSize.height = mPluginSize.height; 

  if (invalidRect)
    memset(mSharedXImage->data, 0, mPluginSize.width * mPluginSize.height * 2);

  PRInt16 response = kNPEventNotHandled;
  mInstance->HandleEvent(&pluginEvent, &response);
  if (response == kNPEventNotHandled)
    return;

  // Setup the clip rectangle
  XRectangle rect;
  rect.x = NSToIntFloor(mAbsolutePositionClip.X());
  rect.y = NSToIntFloor(mAbsolutePositionClip.Y());
  rect.width = NSToIntCeil(mAbsolutePositionClip.Width());
  rect.height = NSToIntCeil(mAbsolutePositionClip.Height());
  
  PRInt32 absPosX = NSToIntFloor(mAbsolutePosition.X());
  PRInt32 absPosY = NSToIntFloor(mAbsolutePosition.Y());
  
  XSetClipRectangles(gdk_x11_get_default_xdisplay(),
                     mXlibSurfGC,
                     absPosX,
                     absPosY, 
                     &rect, 1,
                     Unsorted);

  XShmPutImage(gdk_x11_get_default_xdisplay(),
               mBlitWindow,
               mXlibSurfGC,
               mSharedXImage,
               0,
               0,
               absPosX,
               absPosY,
               mPluginSize.width,
               mPluginSize.height,
               PR_FALSE);
  
  XSetClipRectangles(gdk_x11_get_default_xdisplay(), mXlibSurfGC, 0, 0, nsnull, 0, Unsorted);  

  XFlush(gdk_x11_get_default_xdisplay());
  return;
}
#endif

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

  nsIPluginInstance *instance = mInstanceOwner->mInstance;
  if (!instance)
    return NS_ERROR_FAILURE;

  // See if the plugin must be notified of new window parameters.
  PRBool doupdatewindow = PR_FALSE;

  if (mWindow->x != offset.x || mWindow->y != offset.y) {
    mWindow->x = offset.x;
    mWindow->y = offset.y;
    doupdatewindow = PR_TRUE;
  }

  if (nsIntSize(mWindow->width, mWindow->height) != mPluginSize) {
    mWindow->width = mPluginSize.width;
    mWindow->height = mPluginSize.height;
    doupdatewindow = PR_TRUE;
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
    doupdatewindow = PR_TRUE;
  }

  NPSetWindowCallbackStruct* ws_info = 
    static_cast<NPSetWindowCallbackStruct*>(mWindow->ws_info);
#ifdef MOZ_X11
  if (ws_info->visual != visual || ws_info->colormap != colormap) {
    ws_info->visual = visual;
    ws_info->colormap = colormap;
    ws_info->depth = gfxXlibSurface::DepthOfVisual(screen, visual);
    doupdatewindow = PR_TRUE;
  }
#endif

#ifdef MOZ_COMPOSITED_PLUGINS
  if (mWindow->type == NPWindowTypeDrawable)
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

#ifdef MOZ_COMPOSITED_PLUGINS
  if (mWindow->type == NPWindowTypeDrawable) {
#endif
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
#ifdef MOZ_COMPOSITED_PLUGINS
  }
  else {
    /* XXX: this is very nasty. We need a better way of getting at mPlugWindow */
    GtkWidget *plug = (GtkWidget*)(((nsPluginNativeWindow*)mWindow)->mPlugWindow);
    //GtkWidget *plug = (GtkWidget*)(((nsPluginNativeWindowGtk2*)mWindow)->mSocketWidget);

    /* Cairo has bugs with IncludeInferiors when using paint
     * so we use XCopyArea directly instead. */
    XGCValues gcv;
    gcv.subwindow_mode = IncludeInferiors;
    gcv.graphics_exposures = False;
    Drawable drawable = xsurface->XDrawable();
    GC gc = XCreateGC(DefaultXDisplay(), drawable, GCGraphicsExposures | GCSubwindowMode, &gcv);
    /* The source and destination appear to always line up, so src and dest
     * coords should be the same */
    XCopyArea(DefaultXDisplay(), gdk_x11_drawable_get_xid(plug->window),
              drawable,
              gc,
              mDirtyRect.x,
              mDirtyRect.y,
              mDirtyRect.width,
              mDirtyRect.height,
              mDirtyRect.x,
              mDirtyRect.y);
    XFreeGC(DefaultXDisplay(), gc);
  }
#endif
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
void nsPluginInstanceOwner::StartTimer(PRBool isVisible)
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

nsresult nsPluginInstanceOwner::Init(nsPresContext* aPresContext,
                                     nsObjectFrame* aFrame,
                                     nsIContent*    aContent)
{
  mLastEventloopNestingLevel = GetEventloopNestingLevel();

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsPluginInstanceOwner::Init() called on %p for frame %p\n", this,
          aFrame));

  mObjectFrame = aFrame;
  mContent = aContent;

  nsWeakFrame weakFrame(aFrame);

  // Some plugins require a specific sequence of shutdown and startup when
  // a page is reloaded. Shutdown happens usually when the last instance
  // is destroyed. Here we make sure the plugin instance in the old
  // document is destroyed before we try to create the new one.
  aPresContext->EnsureVisible();

  if (!weakFrame.IsAlive()) {
    PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
           ("nsPluginInstanceOwner::Init's EnsureVisible() call destroyed "
            "instance owner %p\n", this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  // register context menu listener
  mCXMenuListener = new nsPluginDOMContextMenuListener();
  if (mCXMenuListener) {    
    mCXMenuListener->Init(aContent);
  }

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mContent));
  if (target) {

    nsCOMPtr<nsIDOMEventListener> listener;
    QueryInterface(NS_GET_IID(nsIDOMEventListener), getter_AddRefs(listener));

    // Register focus listener
    mContent->AddEventListenerByIID(listener, NS_GET_IID(nsIDOMFocusListener));

    // Register mouse listener
    mContent->AddEventListenerByIID(listener, NS_GET_IID(nsIDOMMouseListener));

    // now do the mouse motion listener
    mContent->AddEventListenerByIID(listener, NS_GET_IID(nsIDOMMouseMotionListener));

    // Register key listener
    target->AddEventListener(NS_LITERAL_STRING("keypress"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("keydown"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("keyup"), listener, PR_TRUE);

    // Register drag listener
    target->AddEventListener(NS_LITERAL_STRING("drop"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("dragdrop"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("drag"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("dragenter"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("dragover"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("dragleave"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("dragexit"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("dragstart"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("draggesture"), listener, PR_TRUE);
    target->AddEventListener(NS_LITERAL_STRING("dragend"), listener, PR_TRUE);
  }
  
  // Register scroll position listeners
  // We need to register a scroll position listener on every scrollable
  // frame up to the top
  for (nsIFrame* f = mObjectFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    nsIScrollableFrame* sf = do_QueryFrame(f);
    if (sf) {
      sf->AddScrollPositionListener(this);
    }
  }

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

  nsIView   *view;
  nsresult  rv = NS_ERROR_FAILURE;

  if (mObjectFrame) {
    // Create view if necessary

    view = mObjectFrame->GetView();

    if (!view || !mWidget) {
      PRBool windowless = PR_FALSE;
      mInstance->IsWindowless(&windowless);

      // always create widgets in Twips, not pixels
      nsPresContext* context = mObjectFrame->PresContext();
      rv = mObjectFrame->CreateWidget(context->DevPixelsToAppUnits(mPluginWindow->width),
                                      context->DevPixelsToAppUnits(mPluginWindow->height),
                                      windowless);
      if (NS_OK == rv) {
        mWidget = mObjectFrame->GetWidget();

        if (PR_TRUE == windowless) {
          mPluginWindow->type = NPWindowTypeDrawable;

          // this needs to be a HDC according to the spec, but I do
          // not see the right way to release it so let's postpone
          // passing HDC till paint event when it is really
          // needed. Change spec?
          mPluginWindow->window = nsnull;
#ifdef MOZ_X11
          // Fill in the display field.
          nsIWidget* win = mObjectFrame->GetNearestWidget();
          NPSetWindowCallbackStruct* ws_info = 
            static_cast<NPSetWindowCallbackStruct*>(mPluginWindow->ws_info);
          if (win) {
            ws_info->display =
              static_cast<Display*>(win->GetNativeData(NS_NATIVE_DISPLAY));
          }
          else {
            ws_info->display = DefaultXDisplay();
          }

          nsCAutoString description;
          GetPluginDescription(description);
          NS_NAMED_LITERAL_CSTRING(flash10Head, "Shockwave Flash 10.");
          mFlash10Quirks = StringBeginsWith(description, flash10Head);
#endif
        } else if (mWidget) {
          nsIWidget* parent = mWidget->GetParent();
          NS_ASSERTION(parent, "Plugin windows must not be toplevel");
          // Set the plugin window to have an empty cliprect. The cliprect
          // will be reset when nsRootPresContext::UpdatePluginGeometry
          // runs later. The plugin window does need to have the correct
          // size here.
          nsAutoTArray<nsIWidget::Configuration,1> configuration;
          if (configuration.AppendElement()) {
            configuration[0].mChild = mWidget;
            configuration[0].mBounds =
              nsIntRect(0, 0, mPluginWindow->width, mPluginWindow->height);
            parent->ConfigureChildren(configuration);
          }

          // mPluginWindow->type is used in |GetPluginPort| so it must
          // be initialized first
          mPluginWindow->type = NPWindowTypeWindow;
          mPluginWindow->window = GetPluginPortFromWidget();

#ifdef MAC_CARBON_PLUGINS
          // start the idle timer.
          StartTimer(PR_TRUE);
#endif

          // tell the plugin window about the widget
          mPluginWindow->SetPluginWidget(mWidget);

          // tell the widget about the current plugin instance owner.
          nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
          if (pluginWidget)
            pluginWidget->SetPluginInstanceOwner(this);
        }
      }
    }
  }

  return rv;
}

void nsPluginInstanceOwner::SetPluginHost(nsIPluginHost* aHost)
{
  mPluginHost = aHost;
}

#ifdef MOZ_USE_IMAGE_EXPOSE
PRBool nsPluginInstanceOwner::UpdateVisibility(PRBool aVisible)
{
  // NOTE: Death grip must be held by caller.
  if (!mInstance)
    return PR_TRUE;

  NPEvent pluginEvent;
  XVisibilityEvent& visibilityEvent = pluginEvent.xvisibility;
  visibilityEvent.type = VisibilityNotify;
  visibilityEvent.display = 0;
  visibilityEvent.state = aVisible ? VisibilityUnobscured : VisibilityFullyObscured;
  mInstance->HandleEvent(&pluginEvent, nsnull);

  mWidgetVisible = PR_TRUE;
  return PR_TRUE;
}
#endif

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
  PRBool widgetVisible;
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
    mInstance->SetWindow(mPluginWindow);
    mPluginPortChanged = PR_FALSE;
#ifdef MAC_CARBON_PLUGINS
    // if the clipRect is of size 0, make the null timer fire less often
    CancelTimer();
    if (mPluginWindow->clipRect.left == mPluginWindow->clipRect.right ||
        mPluginWindow->clipRect.top == mPluginWindow->clipRect.bottom) {
      StartTimer(PR_FALSE);
    }
    else {
      StartTimer(PR_TRUE);
    }
#endif
  } else if (mPluginPortChanged) {
    mInstance->SetWindow(mPluginWindow);
    mPluginPortChanged = PR_FALSE;
  }

  // After the first NPP_SetWindow call we need to send an initial
  // top-level window focus event.
  if (eventModel == NPEventModelCocoa && !mSentInitialTopLevelWindowEvent) {
    // Set this before calling ProcessEvent to avoid endless recursion.
    mSentInitialTopLevelWindowEvent = PR_TRUE;

    nsGUIEvent pluginEvent(PR_TRUE, NS_NON_RETARGETED_PLUGIN_EVENT, nsnull);
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

#endif // XP_MACOSX

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

#ifdef MOZ_USE_IMAGE_EXPOSE
nsresult
nsPluginInstanceOwner::SetAbsoluteScreenPosition(nsIDOMElement* element,
                                                 nsIDOMClientRect* position,
                                                 nsIDOMClientRect* clip)
{
  if (!element || !position || !clip)
    return NS_ERROR_FAILURE;
  
  // Making X or DOM method calls can cause our frame to go
  // away, which might kill us...
  nsCOMPtr<nsIPluginInstanceOwner> kungFuDeathGrip(this);

  if (!mBlitWindow) {
    mBlitWindow = GDK_WINDOW_XWINDOW(GetClosestWindow(element));
    if (!mBlitWindow)
      return NS_ERROR_FAILURE;
  }

  float left, top, width, height;
  position->GetLeft(&left);
  position->GetTop(&top);
  position->GetWidth(&width);
  position->GetHeight(&height);

  mAbsolutePosition = gfxRect(left, top, width, height);
  
  clip->GetLeft(&left);
  clip->GetTop(&top);
  clip->GetWidth(&width);
  clip->GetHeight(&height);

  mAbsolutePositionClip = gfxRect(left, top, width, height);

  UpdateVisibility(!(width == 0 && height == 0));

  if (!mInstance)
    return NS_OK;

  PRBool simpleImageRender = PR_FALSE;
  mInstance->GetValueFromPlugin(NPPVpluginWindowlessLocalBool,
                                &simpleImageRender);
  if (simpleImageRender)
    NativeImageDraw();
  return NS_OK;
}
#endif

