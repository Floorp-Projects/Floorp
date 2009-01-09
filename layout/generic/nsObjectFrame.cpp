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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMDragEvent.h"
#ifdef MOZ_X11
#ifdef MOZ_WIDGET_QT
#include <QWidget>
#include <QX11Info>
#endif
#endif
#include "nsIPluginHost.h"
#include "nsplugin.h"
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
#include "plstr.h"
#include "nsILinkHandler.h"
#ifdef OJI
#include "nsIJVMPluginTagInfo.h"
#endif
#include "nsIEventListener.h"
#include "nsIScrollableView.h"
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
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsAttrName.h"
#include "nsDataHashtable.h"
#include "nsDOMClassInfo.h"

// headers for plugin scriptability
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsIClassInfo.h"

#include "nsObjectFrame.h"
#include "nsIObjectFrame.h"
#include "nsPluginNativeWindow.h"
#include "nsPIPluginHost.h"
#include "nsIPluginDocument.h"

#include "nsThreadUtils.h"

#include "gfxContext.h"

#ifdef XP_WIN
#include "gfxWindowsNativeDrawing.h"
#include "gfxWindowsSurface.h"
#endif

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
#endif

#ifdef MOZ_X11
#include <X11/Xlib.h>
/* X headers suck */
enum { XKeyPress = KeyPress };
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif
#endif

#ifdef MOZ_WIDGET_GTK2
#include "gfxGdkNativeRenderer.h"
#endif

#ifdef MOZ_WIDGET_QT
#include "gfxQtNativeRenderer.h"
#endif

#ifdef XP_WIN
#include <wtypes.h>
#include <winuser.h>
#endif

#ifdef CreateEvent // Thank you MS.
#undef CreateEvent
#endif

#ifdef PR_LOGGING 
static PRLogModuleInfo *nsObjectFrameLM = PR_NewLogModule("nsObjectFrame");
#endif /* PR_LOGGING */

// 1020 / 60
#define NORMAL_PLUGIN_DELAY 17

// low enough to avoid audio skipping/delays
#define HIDDEN_PLUGIN_DELAY 100

// special class for handeling DOM context menu events because for
// some reason it starves other mouse events if implemented on the
// same class
class nsPluginDOMContextMenuListener : public nsIDOMContextMenuListener,
                                       public nsIEventListener
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
                              public nsIPluginTagInfo2,
#ifdef OJI
                              public nsIJVMPluginTagInfo,
#endif
                              public nsIEventListener,
                              public nsITimerCallback,
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

  NS_IMETHOD SetInstance(nsIPluginInstance *aInstance);

  NS_IMETHOD GetInstance(nsIPluginInstance *&aInstance);

  NS_IMETHOD GetWindow(nsPluginWindow *&aWindow);

  NS_IMETHOD GetMode(nsPluginMode *aMode);

  NS_IMETHOD CreateWidget(void);

  NS_IMETHOD GetURL(const char *aURL, const char *aTarget, void *aPostData, 
                    PRUint32 aPostDataLen, void *aHeadersData, 
                    PRUint32 aHeadersDataLen, PRBool isFile = PR_FALSE);

  NS_IMETHOD ShowStatus(const char *aStatusMsg);

  NS_IMETHOD ShowStatus(const PRUnichar *aStatusMsg);
  
  NS_IMETHOD GetDocument(nsIDocument* *aDocument);

  NS_IMETHOD InvalidateRect(nsPluginRect *invalidRect);

  NS_IMETHOD InvalidateRegion(nsPluginRegion invalidRegion);

  NS_IMETHOD ForceRedraw();

  NS_IMETHOD GetValue(nsPluginInstancePeerVariable variable, void *value);

  //nsIPluginTagInfo interface

  NS_IMETHOD GetAttributes(PRUint16& n, const char*const*& names,
                           const char*const*& values);

  NS_IMETHOD GetAttribute(const char* name, const char* *result);

  //nsIPluginTagInfo2 interface

  NS_IMETHOD GetTagType(nsPluginTagType *result);

  NS_IMETHOD GetTagText(const char* *result);

  NS_IMETHOD GetParameters(PRUint16& n, const char*const*& names, const char*const*& values);

  NS_IMETHOD GetParameter(const char* name, const char* *result);
  
  NS_IMETHOD GetDocumentBase(const char* *result);
  
  NS_IMETHOD GetDocumentEncoding(const char* *result);
  
  NS_IMETHOD GetAlignment(const char* *result);
  
  NS_IMETHOD GetWidth(PRUint32 *result);
  
  NS_IMETHOD GetHeight(PRUint32 *result);

  NS_IMETHOD GetBorderVertSpace(PRUint32 *result);
  
  NS_IMETHOD GetBorderHorizSpace(PRUint32 *result);

  NS_IMETHOD GetUniqueID(PRUint32 *result);

  NS_IMETHOD GetDOMElement(nsIDOMElement* *result);

#ifdef OJI
  //nsIJVMPluginTagInfo interface

  NS_IMETHOD GetCode(const char* *result);

  NS_IMETHOD GetCodeBase(const char* *result);

  NS_IMETHOD GetArchive(const char* *result);

  NS_IMETHOD GetName(const char* *result);

  NS_IMETHOD GetMayScript(PRBool *result);

#endif /* OJI */

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

  // nsIDOMFocuListener interfaces
  NS_IMETHOD Focus(nsIDOMEvent * aFocusEvent);
  NS_IMETHOD Blur(nsIDOMEvent * aFocusEvent);

  nsresult Destroy();  

  void PrepareToStop(PRBool aDelayedStop);

  // nsIEventListener interface
  nsEventStatus ProcessEvent(const nsGUIEvent & anEvent);
  
#ifdef XP_WIN
  void Paint(const RECT& aDirty, HDC aDC);
#elif defined(XP_MACOSX)
  void Paint(const nsRect& aDirtyRect);  
#elif defined(MOZ_X11) || defined(MOZ_DFB)
  void Paint(gfxContext* aContext,
             const gfxRect& aFrameRect,
             const gfxRect& aDirtyRect);
#elif defined(XP_OS2)
  void Paint(const nsRect& aDirtyRect, HPS aHPS);
#endif

  // nsITimerCallback interface
  NS_DECL_NSITIMERCALLBACK
  
  void CancelTimer();
  void StartTimer(unsigned int aDelay);

  // nsIScrollPositionListener interface
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);
  virtual void ViewPositionDidChange(nsIScrollableView* aScrollable) {}
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);

  //locals

  nsresult Init(nsPresContext* aPresContext, nsObjectFrame* aFrame,
                nsIContent* aContent);

  nsPluginPort* GetPluginPort();
  void ReleasePluginPort(nsPluginPort * pluginPort);

  void SetPluginHost(nsIPluginHost* aHost);

#ifdef XP_MACOSX
  NPDrawingModel GetDrawingModel();
  WindowRef FixUpPluginWindow(PRInt32 inPaintState);
  void GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord* origEvent, EventRecord& aMacEvent);
  // Set a flag that (if true) indicates the plugin port info has changed and
  // SetWindow() needs to be called.
  void SetPluginPortChanged(PRBool aState) { mPluginPortChanged = aState; }
  // Return a pointer to the internal nsPluginPort structure that's used to
  // store a copy of plugin port info and to detect when it's been changed.
  nsPluginPort* GetPluginPortCopy() { return &mPluginPortCopy; }
  // Set plugin port info in the plugin (in the 'window' member of the
  // nsPluginWindow structure passed to the plugin by SetWindow()) and set a
  // flag (mPluginPortChanged) to indicate whether or not this info has
  // changed, and SetWindow() needs to be called again.
  nsPluginPort* SetPluginPortAndDetectChange();
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
    mOwner = aOwner;
  }

  PRUint32 GetLastEventloopNestingLevel() const {
    return mLastEventloopNestingLevel; 
  }

  void ConsiderNewEventloopNestingLevel() {
    nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
    if (appShell) {
      PRUint32 currentLevel = 0;
      appShell->GetEventloopNestingLevel(&currentLevel);
#ifdef XP_MACOSX
      // Cocoa widget code doesn't process UI events through the normal appshell
      // event loop, so it needs an additional count here.
      currentLevel++;
#else
      // No idea how this happens... but Linux doesn't consistently process UI
      // events through the appshell event loop. If we get a 0 here on any
      // platform we increment the level just in case so that we make sure we
      // always tear the plugin down eventually.
      if (!currentLevel) {
        currentLevel++;
      }
#endif
      if (currentLevel < mLastEventloopNestingLevel) {
        mLastEventloopNestingLevel = currentLevel;
      }
    }
  }

  const char* GetPluginName()
  {
    if (mInstance && mPluginHost) {
      nsCOMPtr<nsPIPluginHost> piPluginHost = do_QueryInterface(mPluginHost);
      const char* name = NULL;
      if (NS_SUCCEEDED(piPluginHost->GetPluginName(mInstance, &name)) &&
          name)
        return name;
    }
    return "";
  }

  PRBool SendNativeEvents()
  {
#ifdef XP_WIN
    return MatchPluginName("Shockwave Flash");
#else
    return PR_FALSE;
#endif
  }

  PRBool MatchPluginName(const char *aPluginName)
  {
    return strncmp(GetPluginName(), aPluginName, strlen(aPluginName)) == 0;
  }

private:
  void FixUpURLS(const nsString &name, nsAString &value);

  nsPluginNativeWindow       *mPluginWindow;
  nsCOMPtr<nsIPluginInstance> mInstance;
  nsObjectFrame              *mOwner;
  nsCOMPtr<nsIContent>        mContent;
  nsCString                   mDocumentBase;
  char                       *mTagText;
  nsCOMPtr<nsIWidget>         mWidget;
  nsCOMPtr<nsITimer>          mPluginTimer;
  nsCOMPtr<nsIPluginHost>     mPluginHost;

#ifdef XP_MACOSX
  nsPluginPort                mPluginPortCopy;
  PRInt32                     mInCGPaintLevel;
#endif

  // Initially, the event loop nesting level we were created on, it's updated
  // if we detect the appshell is on a lower level as long as we're not stopped.
  // We delay DoStopPlugin() until the appshell reaches this level or lower.
  PRUint32                    mLastEventloopNestingLevel;
  PRPackedBool                mContentFocused;
  PRPackedBool                mWidgetVisible;    // used on Mac to store our widget's visible state
  PRPackedBool                mPluginPortChanged;

  // If true, destroy the widget on destruction. Used when plugin stop
  // is being delayed to a safer point in time.
  PRPackedBool                mDestroyWidget;
  PRPackedBool                mTimerCanceled;
  PRUint16          mNumCachedAttrs;
  PRUint16          mNumCachedParams;
  char              **mCachedAttrParamNames;
  char              **mCachedAttrParamValues;

  // pointer to wrapper for nsIDOMContextMenuListener
  nsRefPtr<nsPluginDOMContextMenuListener> mCXMenuListener;

  nsresult DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent);
  nsresult DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent);
  nsresult DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent);

  nsresult EnsureCachedAttrParamArrays();

#if defined(MOZ_WIDGET_GTK2)
  class Renderer : public gfxGdkNativeRenderer {
  public:
    Renderer(nsPluginWindow* aWindow, nsIPluginInstance* aInstance,
             const nsIntSize& aPluginSize, const nsIntRect& aDirtyRect)
      : mWindow(aWindow), mInstance(aInstance),
        mPluginSize(aPluginSize), mDirtyRect(aDirtyRect)
    {}
    virtual nsresult NativeDraw(GdkDrawable * drawable, short offsetX, 
            short offsetY, GdkRectangle * clipRects, PRUint32 numClipRects);
  private:
    nsPluginWindow* mWindow;
    nsIPluginInstance* mInstance;
    const nsIntSize& mPluginSize;
    const nsIntRect& mDirtyRect;
  };
#elif defined(MOZ_WIDGET_QT)
  class Renderer : public gfxQtNativeRenderer {
  public:
    Renderer(nsPluginWindow* aWindow, nsIPluginInstance* aInstance,
             const nsIntSize& aPluginSize, const nsIntRect& aDirtyRect)
      : mWindow(aWindow), mInstance(aInstance),
        mPluginSize(aPluginSize), mDirtyRect(aDirtyRect)
    {}
    virtual nsresult NativeDraw(QWidget * drawable, short offsetX, 
            short offsetY, QRect * clipRects, PRUint32 numClipRects);
  private:
    nsPluginWindow* mWindow;
    nsIPluginInstance* mInstance;
    const nsIntSize& mPluginSize;
    const nsIntRect& mDirtyRect;
  };
#endif

};

#if defined(XP_WIN) || (defined(DO_DIRTY_INTERSECT) && defined(XP_MACOSX)) || defined(XP_OS2)
static void ConvertAppUnitsToPixels(const nsPresContext& aPresContext, const nsRect& aTwipsRect, nsIntRect& aPixelRect);
#endif

  // Mac specific code to fix up port position and clip during paint
#ifdef XP_MACOSX

#ifdef DO_DIRTY_INTERSECT
  // convert relative coordinates to absolute
  static void ConvertRelativeToWindowAbsolute(nsIFrame* aFrame, nsPoint& aRel, nsPoint& aAbs, nsIWidget *&aContainerWidget);
#endif

  enum { ePluginPaintIgnore, ePluginPaintEnable, ePluginPaintDisable };

#endif // XP_MACOSX

nsObjectFrame::nsObjectFrame(nsStyleContext* aContext)
  : nsObjectFrameSuper(aContext)
{
  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("Created new nsObjectFrame %p\n", this));
}

nsObjectFrame::~nsObjectFrame()
{
  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsObjectFrame %p deleted\n", this));
}

NS_IMETHODIMP
nsObjectFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(aInstancePtr, "null out param");

  if (aIID.Equals(NS_GET_IID(nsIObjectFrame))) {
    *aInstancePtr = static_cast<nsIObjectFrame*>(this);
    return NS_OK;
  }

  return nsObjectFrameSuper::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(nsrefcnt) nsObjectFrame::AddRef(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

NS_IMETHODIMP_(nsrefcnt) nsObjectFrame::Release(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsObjectFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLObjectFrameAccessible(this, aAccessible);
  }

  return NS_ERROR_FAILURE;
}

#ifdef XP_WIN
NS_IMETHODIMP nsObjectFrame::GetPluginPort(HWND *aPort)
{
  *aPort = (HWND) mInstanceOwner->GetPluginPort();
  return NS_OK;
}
#endif
#endif


static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

// #define DO_DIRTY_INTERSECT 1   // enable dirty rect intersection during paint

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

  return nsObjectFrameSuper::Init(aContent, aParent, aPrevInFlow);
}

void
nsObjectFrame::Destroy()
{
  NS_ASSERTION(!mPreventInstantiation ||
               mContent && mContent->GetCurrentDoc()->GetDisplayDocument(),
               "about to crash due to bug 136927");

  // we need to finish with the plugin before native window is destroyed
  // doing this in the destructor is too late.
  StopPluginInternal(PR_TRUE);
  
  nsObjectFrameSuper::Destroy();
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
nsObjectFrame::CreateWidgetForView(nsIView* aView)
{
  // Bug 179822: Create widget and allow non-unicode SubClass
  nsWidgetInitData initData;
  initData.mUnicode = PR_FALSE;
  return aView->CreateWidget(kWidgetCID, &initData);
}

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

  //this is ugly. it was ripped off from didreflow(). MMP
  // Position and size view relative to its parent, not relative to our
  // parent frame (our parent frame may not have a view).
  
  nsIView* parentWithView;
  nsPoint origin;
  nsRect r(0, 0, mRect.width, mRect.height);

  GetOffsetFromView(origin, &parentWithView);
  viewMan->ResizeView(view, r);
  viewMan->MoveViewTo(view, origin.x, origin.y);

  if (!aViewOnly && !view->HasWidget()) {
    nsresult rv = CreateWidgetForView(view);
    if (NS_FAILED(rv)) {
      return NS_OK;       //XXX why OK? MMP
    }
  }

  {
    // Here we set the background color for this widget because some plugins will use 
    // the child window background color when painting. If it's not set, it may default to gray
    // Sometimes, a frame doesn't have a background color or is transparent. In this
    // case, walk up the frame tree until we do find a frame with a background color
    for (nsIFrame* frame = this; frame; frame = frame->GetParent()) {
      const nsStyleBackground* background = frame->GetStyleBackground();
      if (!background->IsTransparent()) {  // make sure we got an actual color
        nsIWidget* win = view->GetWidget();
        if (win)
          win->SetBackgroundColor(background->mBackgroundColor);
        break;
      }
    }

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
      aMetrics.width = PR_MIN(PR_MAX(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_WIDTH),
                                     aReflowState.mComputedMinWidth),
                              aReflowState.mComputedMaxWidth);
    }
    if (aMetrics.height == NS_UNCONSTRAINEDSIZE) {
      aMetrics.height = PR_MIN(PR_MAX(nsPresContext::CSSPixelsToAppUnits(EMBED_DEF_HEIGHT),
                                      aReflowState.mComputedMinHeight),
                               aReflowState.mComputedMaxHeight);
    }

#if defined (MOZ_WIDGET_GTK2)
    // We need to make sure that the size of the object frame does not
    // exceed the maximum size of X coordinates.  See bug #225357 for
    // more information.  In theory Gtk2 can handle large coordinates,
    // but underlying plugins can't.
    aMetrics.height = PR_MIN(aPresContext->DevPixelsToAppUnits(PR_INT16_MAX), aMetrics.height);
    aMetrics.width = PR_MIN(aPresContext->DevPixelsToAppUnits(PR_INT16_MAX), aMetrics.width);
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
  aMetrics.mOverflowArea = nsRect(0, 0,
                                  aMetrics.width, aMetrics.height);

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

  FixupWindow(nsSize(aMetrics.width, aMetrics.height));

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
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
    rv = aPluginHost->InstantiateFullPagePlugin(aMimeType, aURI,
          /* resulting stream listener */       *getter_AddRefs(stream),
                                                mInstanceOwner);
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

  nsPluginWindow  *window;
  mInstanceOwner->GetWindow(window);

  NS_ENSURE_TRUE(window, /**/);

#ifdef XP_MACOSX
  mInstanceOwner->FixUpPluginWindow(ePluginPaintDisable);
#endif

  PRBool windowless = (window->type == nsPluginWindowType_Drawable);

  nsPoint origin = GetWindowOriginInPixels(windowless);

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
}

void
nsObjectFrame::CallSetWindow()
{
  nsPluginWindow *win = nsnull;
 
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

  PRBool windowless = (window->type == nsPluginWindowType_Drawable);

  nsPoint origin = GetWindowOriginInPixels(windowless);

  window->x = origin.x;
  window->y = origin.y;

  // refresh the plugin port as well
  window->window = mInstanceOwner->GetPluginPort();

  // this will call pi->SetWindow and take care of window subclassing
  // if needed, see bug 132759.
  window->CallSetWindow(pi);

  mInstanceOwner->ReleasePluginPort((nsPluginPort *)window->window);
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

nsPoint nsObjectFrame::GetWindowOriginInPixels(PRBool aWindowless)
{
  nsIView * parentWithView;
  nsPoint origin(0,0);

  GetOffsetFromView(origin, &parentWithView);

  // if it's windowless, let's make sure we have our origin set right
  // it may need to be corrected, like after scrolling
  if (aWindowless && parentWithView) {
    // XXX Should this be replaced by nsIView::GetNearestWidget?
    // The implementation below doesn't handle cases where the widget's origin
    // doesn't coincide with its view's origin.

    nsIViewManager* parentVM = parentWithView->GetViewManager();

    // Walk up all the views and add up their positions until we
    // reach the first view with a window (non-null widget). This will give us our
    // position relative to the containing window which is what we want to give the plugin
    nsIView* theView = parentWithView;
    while (theView && !theView->GetWidget()) {
      if (theView->GetViewManager() != parentVM)
        break;

      origin += theView->GetPosition();
      theView = theView->GetParent();
    }  
  }

  origin.x = PresContext()->AppUnitsToDevPixels(origin.x);
  origin.y = PresContext()->AppUnitsToDevPixels(origin.y);

  return origin;
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

  // WMP10 needs an additional SetWindow call here (bug 391261)
  CallSetWindow();

  return rv;
}

/* static */ void
nsObjectFrame::PaintPrintPlugin(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                                const nsRect& aDirtyRect, nsPoint aPt)
{
  // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
  nsIRenderingContext::AutoPushTranslation translate(aCtx, aPt.x, aPt.y);
  static_cast<nsObjectFrame*>(aFrame)->PrintPlugin(*aCtx, aDirtyRect);
}

/*static */ void
nsObjectFrame::PaintPlugin(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                           const nsRect& aDirtyRect, nsPoint aPt)
{
  static_cast<nsObjectFrame*>(aFrame)->PaintPlugin(*aCtx, aDirtyRect, aPt);
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
  
  // determine if we are printing
  if (type == nsPresContext::eContext_Print)
    return aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(this, PaintPrintPlugin, "PrintPlugin"));
  
  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(this, PaintPlugin, "Plugin"));
}

void
nsObjectFrame::PrintPlugin(nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect)
{
  // if we are printing, we need to get the correct nsIPluginInstance
  // for THIS content node in order to call ->Print() on the right plugin

  // first, we need to get the document
  nsIDocument* doc = mContent->GetCurrentDoc();
  if (!doc)
    return;

  // now we need to get the shell for the screen
  // XXX assuming that the shell at zero will always be the screen one
  nsIPresShell *shell = doc->GetPrimaryShell();
  if (!shell)
    return;

  // then the shell can give us the screen frame for this content node
  nsIFrame* frame = shell->GetPrimaryFrameFor(mContent);
  if (!frame)
    return;

  nsPresContext* presContext = PresContext();
  // make sure this is REALLY an nsIObjectFrame
  // we may need to go through the children to get it
  nsIObjectFrame* objectFrame = nsnull;
  CallQueryInterface(frame,&objectFrame);
  if (!objectFrame)
    objectFrame = GetNextObjectFrame(presContext,frame);
  if (!objectFrame)
    return;

  // finally we can get our plugin instance
  nsCOMPtr<nsIPluginInstance> pi;
  if (NS_FAILED(objectFrame->GetPluginInstance(*getter_AddRefs(pi))) || !pi)
    return;

  // now we need to setup the correct location for printing
  nsPluginWindow    window;
  window.window =   nsnull;

  // prepare embedded mode printing struct
  nsPluginPrint npprint;
  npprint.mode = nsPluginMode_Embedded;

  // we need to find out if we are windowless or not
  PRBool windowless = PR_FALSE;
  pi->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);
  window.type  =  windowless ? nsPluginWindowType_Drawable : nsPluginWindowType_Window;

  window.clipRect.bottom = 0; window.clipRect.top = 0;
  window.clipRect.left = 0; window.clipRect.right = 0;
  
// XXX platform specific printing code
#if defined(XP_UNIX) && !defined(XP_MACOSX)

  /* XXX this just flat-out doesn't work in a thebes world --
   * RenderEPS is a no-op.  So don't bother to do any work here.
   */
#if 0
    /* UNIX does things completely differently:
   * We call the plugin and it sends generated PostScript data into a
   * file handle we provide. If the plugin returns with success we embed
   * this PostScript code fragment into the PostScript job we send to the
   * printer.
   */

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG, ("nsObjectFrame::Paint() start for X11 platforms\n"));
         
  FILE *plugintmpfile = tmpfile();
  if (!plugintmpfile) {
    PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG, ("error: could not open tmp. file, errno=%d\n", errno));
    return;
  }
 
    /* Send off print info to plugin */
  NPPrintCallbackStruct npPrintInfo;
  npPrintInfo.type = NP_PRINT;
  npPrintInfo.fp   = plugintmpfile;
  npprint.print.embedPrint.platformPrint = (void *)&npPrintInfo;
  /* aDirtyRect contains the right information for ps print */
  window.x =   aDirtyRect.x;
  window.y =   aDirtyRect.y;
  window.width =   aDirtyRect.width;
  window.height =   aDirtyRect.height;
  npprint.print.embedPrint.window        = window;
  nsresult rv = pi->Print(&npprint);
  if (NS_FAILED(rv)) {
    PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG, ("error: plugin returned failure %lx\n", (long)rv));
    fclose(plugintmpfile);
    return;
  }

  /* Send data to printer */
  rv = aRenderingContext.RenderEPS(aDirtyRect, plugintmpfile);

  fclose(plugintmpfile);

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG, ("plugin printing done, return code is %lx\n", (long)rv));
#endif

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
  window.x = 0;
  window.y = 0;
  window.width = presContext->AppUnitsToDevPixels(mRect.width);
  window.height = presContext->AppUnitsToDevPixels(mRect.height);

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

#else

  // Get the offset of the DC
  nsTransform2D* rcTransform;
  aRenderingContext.GetCurrentTransform(rcTransform);
  nsPoint origin;
  rcTransform->GetTranslationCoord(&origin.x, &origin.y);

  // set it all up
  // XXX is windowless different?
  window.x = presContext->AppUnitsToDevPixels(origin.x);
  window.y = presContext->AppUnitsToDevPixels(origin.y);
  window.width = presContext->AppUnitsToDevPixels(mRect.width);
  window.height= presContext->AppUnitsToDevPixels(mRect.height);

  // we need the native printer device context to pass to plugin
  // NATIVE_WINDOWS_DC is a misnomer, it's whatever the native platform
  // thing is.
  void* dc;
  dc = aRenderingContext.GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);
  if (!dc)
    return; // no dc implemented so quit

  npprint.print.embedPrint.platformPrint = dc;
  npprint.print.embedPrint.window = window;
  // send off print info to plugin
  pi->Print(&npprint);
#endif

  // XXX Nav 4.x always sent a SetWindow call after print. Should we do the same?
  nsDidReflowStatus status = NS_FRAME_REFLOW_FINISHED; // should we use a special status?
  frame->DidReflow(presContext,
                   nsnull, status);  // DidReflow will take care of it
}

void
nsObjectFrame::PaintPlugin(nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect, const nsPoint& aFramePt)
{
  // Screen painting code
#if defined(XP_MACOSX)
  // delegate all painting to the plugin instance.
  if (mInstanceOwner) {
    // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
    nsIRenderingContext::AutoPushTranslation
      translate(&aRenderingContext, aFramePt.x, aFramePt.y);

    if (mInstanceOwner->GetDrawingModel() == NPDrawingModelCoreGraphics) {
      PRInt32 p2a = PresContext()->AppUnitsPerDevPixel();
      gfxRect nativeClipRect(aDirtyRect.x, aDirtyRect.y,
                             aDirtyRect.width, aDirtyRect.height);
      nativeClipRect.ScaleInverse(gfxFloat(p2a));
      gfxContext* ctx = aRenderingContext.ThebesContext();
      gfxQuartzNativeDrawing nativeDrawing(ctx, nativeClipRect);

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
      nsPluginWindow* window;
      mInstanceOwner->GetWindow(window);
      if (!window) {
        NS_WARNING("null plugin window during PaintPlugin");
        nativeDrawing.EndNativeDrawing();
        return;
      }
      nsPluginPort* pluginPortCopy = mInstanceOwner->GetPluginPortCopy();
      if (!pluginPortCopy) {
        NS_WARNING("null plugin port copy during PaintPlugin");
        nativeDrawing.EndNativeDrawing();
        return;
      }
      if (!mInstanceOwner->SetPluginPortAndDetectChange()) {
        NS_WARNING("null plugin port during PaintPlugin");
        nativeDrawing.EndNativeDrawing();
        return;
      }
      // If gfxQuartzNativeDrawing hands out a CGContext different from the
      // one set by SetPluginPortAndDetectChange(), we need to pass it to the
      // plugin via SetWindow().  This will happen in nsPluginInstanceOwner::
      // FixUpPluginWindow(), called from nsPluginInstanceOwner::Paint().
      // (If SetPluginPortAndDetectChange() made any changes itself, this has
      // already been detected in that method, and will likewise result in a
      // call to SetWindow() from FixUpPluginWindow().)
      if (window->window->cgPort.context != cgContext) {
        window->window->cgPort.context = cgContext;
        pluginPortCopy->cgPort.context = cgContext;
        mInstanceOwner->SetPluginPortChanged(PR_TRUE);
      }

      mInstanceOwner->BeginCGPaint();
      mInstanceOwner->Paint(aDirtyRect);
      mInstanceOwner->EndCGPaint();

      nativeDrawing.EndNativeDrawing();
    } else {
      mInstanceOwner->Paint(aDirtyRect);
    }
  }
#elif defined(MOZ_X11) || defined(MOZ_DFB)
  if (mInstanceOwner) {
    nsPluginWindow * window;
    mInstanceOwner->GetWindow(window);

    if (window->type == nsPluginWindowType_Drawable) {
      gfxRect frameGfxRect =
        PresContext()->AppUnitsToGfxUnits(nsRect(aFramePt, GetSize()));
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
      PresContext()->AppUnitsToGfxUnits(nsRect(aFramePt, GetSize()));
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
    nsPluginWindow * window;
    mInstanceOwner->GetWindow(window);

    if (window->type == nsPluginWindowType_Drawable) {
      // check if we need to call SetWindow with updated parameters
      PRBool doupdatewindow = PR_FALSE;
      // the offset of the DC
      nsPoint origin;
      
      gfxWindowsNativeDrawing nativeDraw(ctx, frameGfxRect);
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
          window->window = reinterpret_cast<nsPluginPort*>(hdc);
          window->x = dest.left;
          window->y = dest.top;

          // Windowless plugins on windows need a special event to update their location, see bug 135737
          // bug 271442: note, the rectangle we send is now purely the bounds of the plugin
          // relative to the window it is contained in, which is useful for the plugin to correctly translate mouse coordinates
          //
          // this does not mesh with the comments for bug 135737 which imply that the rectangle
          // must be clipped in some way to prevent the plugin attempting to paint over areas it shouldn't;
          //
          // since the two uses of the rectangle are mutually exclusive in some cases,
          // and since I don't see any incorrect painting (at least with Flash and ViewPoint - the originator of 135737),
          // it seems that windowless plugins are not relying on information here for clipping their drawing,
          // and we can safely use this message to tell the plugin exactly where it is in all cases.

          nsIntPoint origin = GetWindowOriginInPixels(PR_TRUE);
          nsRect winlessRect = nsRect(origin, nsSize(window->width, window->height));
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
            nsPluginEvent pluginEvent;
            pluginEvent.event = WM_WINDOWPOSCHANGED;
            pluginEvent.wParam = 0;
            pluginEvent.lParam = (uint32)&winpos;
            PRBool eventHandled = PR_FALSE;

            inst->HandleEvent(&pluginEvent, &eventHandled);
          }

          inst->SetWindow(window);        
        }

        mInstanceOwner->Paint(dirty, hdc);
        nativeDraw.EndNativeDrawing();
      } while (nativeDraw.ShouldRenderAgain());

      nativeDraw.PaintToContext();
    } else if (!(ctx->GetFlags() & gfxContext::FLAG_DESTINED_FOR_SCREEN)) {
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
      if (printProc) {
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
    nsPluginWindow * window;
    mInstanceOwner->GetWindow(window);

    if (window->type == nsPluginWindowType_Drawable) {
      // FIXME - Bug 385435: Doesn't aDirtyRect need translating too?
      nsIRenderingContext::AutoPushTranslation
        translate(&aRenderingContext, aFramePt.x, aFramePt.y);

      // check if we need to call SetWindow with updated parameters
      PRBool doupdatewindow = PR_FALSE;
      // the offset of the DC
      nsPoint origin;

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
        window->window = reinterpret_cast<nsPluginPort*>(hps);
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
  NS_ENSURE_ARG_POINTER(anEventStatus);
  nsresult rv = NS_OK;

  if (!mInstanceOwner)
    return NS_ERROR_NULL_POINTER;

  mInstanceOwner->ConsiderNewEventloopNestingLevel();

  if (anEvent->message == NS_PLUGIN_ACTIVATE) {
    nsIContent* content = GetContent();
    if (content) {
      content->SetFocus(aPresContext);
      return rv;
    }
  }

  if (mInstanceOwner->SendNativeEvents() && NS_IS_PLUGIN_EVENT(anEvent)) {
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    return rv;
  }

#ifdef XP_WIN
  rv = nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
  return rv;
#endif

  switch (anEvent->message) {
  case NS_DESTROY:
    mInstanceOwner->CancelTimer();
    break;
  case NS_GOTFOCUS:
  case NS_LOSTFOCUS:
    *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
    break;
    
  default:
    // instead of using an event listener, we can dispatch events to plugins directly.
    rv = nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
  }

  return rv;
}

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

  nsCOMPtr<nsIPluginHost> pluginHost(do_GetService(kCPluginManagerCID, &rv));
  if (NS_FAILED(rv))
    return rv;
  mInstanceOwner->SetPluginHost(pluginHost);

  // This must be done before instantiating the plugin
  FixupWindow(mRect.Size());

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

  NS_ASSERTION(aMimeType || aURI, "Need a type or a URI!");

  // Note: If PrepareInstanceOwner() returns an error, |this| may very
  // well be deleted already.
  nsresult rv = PrepareInstanceOwner();
  NS_ENSURE_SUCCESS(rv, rv);

  nsWeakFrame weakFrame(this);

  // This must be done before instantiating the plugin
  FixupWindow(mRect.Size());

  // get the nsIPluginHost service
  nsCOMPtr<nsIPluginHost> pluginHost(do_GetService(kCPluginManagerCID, &rv));
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

#ifdef XP_WIN
static const char*
GetMIMEType(nsIPluginInstance *aPluginInstance)
{
  nsCOMPtr<nsIPluginInstancePeer> peer;
  aPluginInstance->GetPeer(getter_AddRefs(peer));
  if (peer) {
    nsMIMEType mime = NULL;
    if (NS_SUCCEEDED(peer->GetMIMEType(&mime)) && mime)
      return mime;
  }
  return "";
}
#endif

static PRBool
DoDelayedStop(nsPluginInstanceOwner *aInstanceOwner, PRBool aDelayedStop)
{
  // Don't delay stopping QuickTime (bug 425157), Flip4Mac (bug 426524),
  // XStandard (bug 430219), CMISS Zinc (bug 429604). ARM Flash (454756)
  if (aDelayedStop
#ifndef XP_WIN
      && !aInstanceOwner->MatchPluginName("QuickTime")
      && !aInstanceOwner->MatchPluginName("Flip4Mac")
      && !aInstanceOwner->MatchPluginName("XStandard plugin")
      && !aInstanceOwner->MatchPluginName("CMISS Zinc Plugin")
#endif
#if defined(XP_UNIX) && defined(__arm__)
      && !aInstanceOwner->MatchPluginName("Shockwave Flash")
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
    nsPluginWindow *win;
    aInstanceOwner->GetWindow(win);
    nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;
    nsCOMPtr<nsIPluginInstance> nullinst;

    PRBool doCache = PR_TRUE;
    PRBool doCallSetWindowAfterDestroy = PR_FALSE;

    // first, determine if the plugin wants to be cached
    inst->GetValue(nsPluginInstanceVariable_DoCacheBool, (void *)&doCache);
    if (!doCache) {
      // then determine if the plugin wants Destroy to be called after
      // Set Window.  This is for bug 50547.
      inst->GetValue(nsPluginInstanceVariable_CallSetWindowAfterDestroyBool, 
                     (void *)&doCallSetWindowAfterDestroy);
      if (doCallSetWindowAfterDestroy) {
        // XXXjst: ns4xPluginInstance::Destroy() is a no-op, clean
        // this mess up when there are no other instance types.
        inst->Stop();
        inst->Destroy();

        if (window) 
          window->CallSetWindow(nullinst);
        else 
          inst->SetWindow(nsnull);
      }
      else {
        if (window) 
          window->CallSetWindow(nullinst);
        else 
          inst->SetWindow(nsnull);

        if (DoDelayedStop(aInstanceOwner, aDelayedStop))
          return;

        inst->Stop();
        inst->Destroy();
      }
    }
    else {
      if (window) 
        window->CallSetWindow(nullinst);
      else 
        inst->SetWindow(nsnull);

      if (DoDelayedStop(aInstanceOwner, aDelayedStop))
        return;

      inst->Stop();
    }

    nsCOMPtr<nsIPluginHost> pluginHost = do_GetService(kCPluginManagerCID);
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

#ifdef XP_WIN
  if (aDelayedStop) {
    // If we're asked to do a delayed stop it means we're stopping the
    // plugin because we're destroying the frame. In that case, tell
    // the view to disown the widget (i.e. leave it up to us to
    // destroy it).

    // Disown the view while we still know it's safe to do so.
    nsIView *view = GetView();
    if (view) {
      view->DisownWidget();
    }
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
    nsIObjectFrame* outFrame = nsnull;
    CallQueryInterface(child, &outFrame);
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

nsIFrame*
NS_NewObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsObjectFrame(aContext);
}


// nsPluginDOMContextMenuListener class implementation

nsPluginDOMContextMenuListener::nsPluginDOMContextMenuListener()
{
}

nsPluginDOMContextMenuListener::~nsPluginDOMContextMenuListener()
{
}

NS_IMPL_ISUPPORTS3(nsPluginDOMContextMenuListener,
                   nsIDOMContextMenuListener,
                   nsIDOMEventListener,
                   nsIEventListener)

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
  // create nsPluginNativeWindow object, it is derived from nsPluginWindow
  // struct and allows to manipulate native window procedure
  nsCOMPtr<nsIPluginHost> ph = do_GetService(kCPluginManagerCID);
  nsCOMPtr<nsPIPluginHost> pph(do_QueryInterface(ph));
  if (pph)
    pph->NewPluginNativeWindow(&mPluginWindow);
  else
    mPluginWindow = nsnull;

  mOwner = nsnull;
  mTagText = nsnull;
#ifdef XP_MACOSX
  memset(&mPluginPortCopy, 0, sizeof(nsPluginPort));
  mInCGPaintLevel = 0;
#endif
  mContentFocused = PR_FALSE;
  mWidgetVisible = PR_TRUE;
  mPluginPortChanged = PR_FALSE;
  mNumCachedAttrs = 0;
  mNumCachedParams = 0;
  mCachedAttrParamNames = nsnull;
  mCachedAttrParamValues = nsnull;
  mDestroyWidget = PR_FALSE;
  mTimerCanceled = PR_TRUE;

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsPluginInstanceOwner %p created\n", this));
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  PRInt32 cnt;

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsPluginInstanceOwner %p deleted\n", this));

  // shut off the timer.
  CancelTimer();

  mOwner = nsnull;

  for (cnt = 0; cnt < (mNumCachedAttrs + 1 + mNumCachedParams); cnt++) {
    if (mCachedAttrParamNames && mCachedAttrParamNames[cnt]) {
      PR_Free(mCachedAttrParamNames[cnt]);
      mCachedAttrParamNames[cnt] = nsnull;
    }

    if (mCachedAttrParamValues && mCachedAttrParamValues[cnt]) {
      PR_Free(mCachedAttrParamValues[cnt]);
      mCachedAttrParamValues[cnt] = nsnull;
    }
  }

  if (mCachedAttrParamNames) {
    PR_Free(mCachedAttrParamNames);
    mCachedAttrParamNames = nsnull;
  }

  if (mCachedAttrParamValues) {
    PR_Free(mCachedAttrParamValues);
    mCachedAttrParamValues = nsnull;
  }

  if (mTagText) {
    NS_Free(mTagText);
    mTagText = nsnull;
  }

  // clean up plugin native window object
  nsCOMPtr<nsIPluginHost> ph = do_GetService(kCPluginManagerCID);
  nsCOMPtr<nsPIPluginHost> pph(do_QueryInterface(ph));
  if (pph) {
    pph->DeletePluginNativeWindow(mPluginWindow);
    mPluginWindow = nsnull;
  }
}

/*
 * nsISupports Implementation
 */

NS_IMPL_ADDREF(nsPluginInstanceOwner)
NS_IMPL_RELEASE(nsPluginInstanceOwner)

NS_INTERFACE_MAP_BEGIN(nsPluginInstanceOwner)
  NS_INTERFACE_MAP_ENTRY(nsIPluginInstanceOwner)
  NS_INTERFACE_MAP_ENTRY(nsIPluginTagInfo)
  NS_INTERFACE_MAP_ENTRY(nsIPluginTagInfo2)
#ifdef OJI
  NS_INTERFACE_MAP_ENTRY(nsIJVMPluginTagInfo)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIEventListener)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPluginInstanceOwner)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsPluginInstanceOwner::SetInstance(nsIPluginInstance *aInstance)
{
  mInstance = aInstance;

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetWindow(nsPluginWindow *&aWindow)
{
  NS_ASSERTION(mPluginWindow, "the plugin window object being returned is null");
  aWindow = mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMode(nsPluginMode *aMode)
{
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = GetDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIPluginDocument> pDoc (do_QueryInterface(doc));

  if (pDoc) {
    *aMode = nsPluginMode_Full;
  } else {
    *aMode = nsPluginMode_Embedded;
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

NS_IMETHODIMP nsPluginInstanceOwner::GetURL(const char *aURL, const char *aTarget, void *aPostData, PRUint32 aPostDataLen, void *aHeadersData, 
                                            PRUint32 aHeadersDataLen, PRBool isFile)
{
  NS_ENSURE_TRUE(mOwner,NS_ERROR_NULL_POINTER);

  if (mContent->IsEditable()) {
    return NS_OK;
  }

  // the container of the pres context will give us the link handler
  nsCOMPtr<nsISupports> container = mOwner->PresContext()->GetContainer();
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

  nsCOMPtr<nsIInputStream> postDataStream;
  nsCOMPtr<nsIInputStream> headersDataStream;

  // deal with post data, either in a file or raw data, and any headers
  if (aPostData) {

    rv = NS_NewPluginPostDataStream(getter_AddRefs(postDataStream), (const char *)aPostData, aPostDataLen, isFile);

    NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin post data stream");
    if (NS_FAILED(rv))
      return rv;

    if (aHeadersData) {
      rv = NS_NewPluginPostDataStream(getter_AddRefs(headersDataStream), 
                                      (const char *) aHeadersData, 
                                      aHeadersDataLen,
                                      PR_FALSE,
                                      PR_TRUE);  // last arg says we are headers, no /r/n/r/n fixup!

      NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin header data stream");
      if (NS_FAILED(rv))
        return rv;
    }
  }

  PRInt32 blockPopups =
    nsContentUtils::GetIntPref("privacy.popups.disable_from_plugins");
  nsAutoPopupStatePusher popupStatePusher((PopupControlState)blockPopups);

  rv = lh->OnLinkClick(mContent, uri, unitarget.get(), 
                       postDataStream, headersDataStream);

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

  if (!mOwner) {
    return rv;
  }
  nsCOMPtr<nsISupports> cont = mOwner->PresContext()->GetContainer();
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

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRect(nsPluginRect *invalidRect)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mOwner && invalidRect && mWidgetVisible) {
    nsPresContext* presContext = mOwner->PresContext();
    nsRect rect(presContext->DevPixelsToAppUnits(invalidRect->left),
                presContext->DevPixelsToAppUnits(invalidRect->top),
                presContext->DevPixelsToAppUnits(invalidRect->right - invalidRect->left),
                presContext->DevPixelsToAppUnits(invalidRect->bottom - invalidRect->top));
    mOwner->Invalidate(rect);
  }

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRegion(nsPluginRegion invalidRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginInstanceOwner::ForceRedraw()
{
  NS_ENSURE_TRUE(mOwner,NS_ERROR_NULL_POINTER);
  nsIView* view = mOwner->GetView();
  if (view) {
    return view->GetViewManager()->Composite();
  }

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetValue(nsPluginInstancePeerVariable variable, void *value)
{
  nsresult rv = NS_ERROR_FAILURE;

  switch(variable) {
    case nsPluginInstancePeerVariable_NetscapeWindow:
    {      
      if (mOwner) {
#if defined(XP_WIN) || defined(XP_OS2)
        void** pvalue = (void**)value;
        nsIViewManager* vm = mOwner->PresContext()->GetViewManager();
        if (vm) {
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
          if (mPluginWindow && mPluginWindow->type == nsPluginWindowType_Drawable) {
              // it turns out that flash also uses this window for determining focus, and is currently
              // unable to show a caret correctly if we return the enclosing window. Therefore for
              // now we only return the enclosing window when there is an actual offset which
              // would otherwise cause coordinates to be offset incorrectly. (i.e.
              // if the enclosing window if offset from the document window)
              //
              // fixing both the caret and ability to interact issues for a windowless control in a non document aligned windw
              // does not seem to be possible without a change to the flash plugin
    
              nsIWidget* win = mOwner->GetWindow();
              if (win) {
                nsIView *view = nsIView::GetViewFor(win);
                NS_ASSERTION(view, "No view for widget");
                nsIView *rootView = nsnull;
                vm->GetRootView(rootView);
                NS_ASSERTION(rootView, "No root view");
                nsPoint offset = view->GetOffsetTo(rootView);
      
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
          // simply return the document window
          nsCOMPtr<nsIWidget> widget;
          rv = vm->GetWidget(getter_AddRefs(widget));            
          if (widget) {
            *pvalue = (void*)widget->GetNativeData(NS_NATIVE_WINDOW);
          } else NS_ASSERTION(widget, "couldn't get doc's widget in getting doc's window handle");
        } else NS_ASSERTION(vm, "couldn't get view manager in getting doc's window handle");
#elif defined(MOZ_WIDGET_GTK2)
        // X11 window managers want the toplevel window for WM_TRANSIENT_FOR.
        nsIWidget* win = mOwner->GetWindow();
        if (!win)
          return rv;
        GdkWindow* gdkWindow =
          static_cast<GdkWindow*>(win->GetNativeData(NS_NATIVE_WINDOW));
        if (!gdkWindow)
          return rv;
        gdkWindow = gdk_window_get_toplevel(gdkWindow);
#ifdef MOZ_X11
        *static_cast<Window*>(value) = GDK_WINDOW_XID(gdkWindow);
#endif
        return NS_OK;
#endif
      } else NS_ASSERTION(mOwner, "plugin owner has no owner in getting doc's window handle");
      break;
    }
  }

  return rv;
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
    if (!mOwner) {
      *result = nsnull;
      return NS_ERROR_FAILURE;
    }

    nsIDocument* doc = mContent->GetOwnerDoc();
    NS_ASSERTION(doc, "Must have an owner doc");
    rv = doc->GetBaseURI()->GetSpec(mDocumentBase);
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
  *result = NS_PTR_TO_INT32(mOwner);
  return NS_OK;
}

#ifdef OJI
NS_IMETHODIMP nsPluginInstanceOwner::GetCode(const char* *result)
{
  nsresult rv;
  nsPluginTagType tagType;  
  NS_ENSURE_SUCCESS(rv = GetTagType(&tagType), rv);

  rv = NS_ERROR_FAILURE;
  if (nsPluginTagType_Object != tagType)
    rv = GetAttribute("CODE", result);
  if (NS_FAILED(rv))
    rv = GetParameter("CODE", result);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetCodeBase(const char* *result)
{
  nsresult rv;
  if (NS_FAILED(rv = GetAttribute("CODEBASE", result)))
    rv = GetParameter("CODEBASE", result);
  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetArchive(const char* *result)
{
  nsresult rv;
  if (NS_FAILED(rv = GetAttribute("ARCHIVE", result)))
    rv = GetParameter("ARCHIVE", result);
  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetName(const char* *result)
{
  nsresult rv;
  nsPluginTagType tagType;  
  NS_ENSURE_SUCCESS(rv = GetTagType(&tagType), rv);

  rv = NS_ERROR_FAILURE;
  if (nsPluginTagType_Object != tagType)
    rv = GetAttribute("NAME", result);
  if (NS_FAILED(rv))
    rv = GetParameter("NAME", result);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMayScript(PRBool *result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsPluginTagType tagType;  
  NS_ENSURE_SUCCESS(GetTagType(&tagType), NS_ERROR_FAILURE);

  const char* unused;
  if (nsPluginTagType_Object == tagType)
    *result = NS_SUCCEEDED(GetParameter("MAYSCRIPT", &unused)); 
  else
    *result = NS_SUCCEEDED(GetAttribute("MAYSCRIPT", &unused));

  return NS_OK;
}
#endif /* OJI */

// Cache the attributes and/or parameters of our tag into a single set
// of arrays to be compatible with 4.x. The attributes go first,
// followed by a PARAM/null and then any PARAM tags. Also, hold the
// cached array around for the duration of the life of the instance
// because 4.x did. See bug 111008.

nsresult nsPluginInstanceOwner::EnsureCachedAttrParamArrays()
{
  if (mCachedAttrParamValues)
    return NS_OK;

  NS_PRECONDITION(((mNumCachedAttrs + mNumCachedParams) == 0) &&
                  !mCachedAttrParamNames,
                  "re-cache of attrs/params not implemented! use the DOM "
                  "node directy instead");
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NULL_POINTER);

  // first, we need to find out how much we need to allocate for our
  // arrays count up attributes
  mNumCachedAttrs = 0;

  PRUint32 cattrs = mContent->GetAttrCount();

  if (cattrs < 0x0000FFFF) {
    // unsigned 32 bits to unsigned 16 bits conversion
    mNumCachedAttrs = static_cast<PRUint16>(cattrs);
  } else {
    mNumCachedAttrs = 0xFFFE;  // minus one in case we add an extra "src" entry below
  }

  // now, we need to find all the PARAM tags that are children of us
  // however, be carefull NOT to include any PARAMs that don't have us
  // as a direct parent. For nested object (or applet) tags, be sure
  // to only round up the param tags that coorespond with THIS
  // instance. And also, weed out any bogus tags that may get in the
  // way, see bug 39609. Then, with any param tag that meet our
  // qualification, temporarly cache them in an nsCOMArray until
  // we can figure out what size to make our fixed char* array.

  mNumCachedParams = 0;
  nsCOMArray<nsIDOMElement> ourParams;
 
  // use the DOM to get us ALL our dependent PARAM tags, even if not
  // ours
  nsCOMPtr<nsIDOMElement> mydomElement = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(mydomElement, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIDOMNodeList> allParams; 

  nsINodeInfo *ni = mContent->NodeInfo();

  // Making DOM method calls can cause our frame to go away, which
  // might kill us...
  nsCOMPtr<nsIPluginInstanceOwner> kungFuDeathGrip(this);
  
  if (ni->NamespaceEquals(kNameSpaceID_XHTML)) {
    // For XHTML elements we need to take the namespace URI into
    // account when looking for param elements.

    NS_NAMED_LITERAL_STRING(xhtml_ns, "http://www.w3.org/1999/xhtml");

    mydomElement->GetElementsByTagNameNS(xhtml_ns, NS_LITERAL_STRING("param"),
                                         getter_AddRefs(allParams));
  } else {
    // If content is not XHTML, it must be HTML, no need to worry
    // about namespaces then...

    mydomElement->GetElementsByTagName(NS_LITERAL_STRING("param"),
                                       getter_AddRefs(allParams));
  }    

  if (allParams) {
    PRUint32 numAllParams; 
    allParams->GetLength(&numAllParams);
    // loop through every so called dependent PARAM tag to check if it
    // "belongs" to us

    for (PRUint32 i = 0; i < numAllParams; i++) {
      nsCOMPtr<nsIDOMNode> pnode;
      allParams->Item(i, getter_AddRefs(pnode));

      nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(pnode);
      if (domelement) {
        // let's NOT count up param tags that don't have a name attribute
        nsAutoString name;
        domelement->GetAttribute(NS_LITERAL_STRING("name"), name);
        if (!name.IsEmpty()) {
          nsCOMPtr<nsIDOMNode> parent;
          nsCOMPtr<nsIDOMHTMLObjectElement> domobject;
          nsCOMPtr<nsIDOMHTMLAppletElement> domapplet;
          pnode->GetParentNode(getter_AddRefs(parent));
          // walk up the parents of this PARAM until we find an object
          // (or applet) tag

          while (!(domobject || domapplet) && parent) {
            domobject = do_QueryInterface(parent);
            domapplet = do_QueryInterface(parent);
            nsCOMPtr<nsIDOMNode> temp;
            parent->GetParentNode(getter_AddRefs(temp));
            parent = temp;
          }

          if (domapplet || domobject) {
            if (domapplet)
              parent = domapplet;
            else
              parent = domobject;

            // now check to see if this PARAM's parent is us. if so,
            // cache it for later

            nsCOMPtr<nsIDOMNode> mydomNode = do_QueryInterface(mydomElement);
            if (parent == mydomNode) {
              ourParams.AppendObject(domelement);
            }
          }
        }
      }
    }
  }

  // We're done with DOM method calls now; make sure we still have a frame.
  NS_ENSURE_TRUE(mOwner, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 cparams = ourParams.Count(); // unsigned 32 bits to unsigned 16 bits conversion
  if (cparams < 0x0000FFFF)
    mNumCachedParams = static_cast<PRUint16>(cparams);
  else 
    mNumCachedParams = 0xFFFF;

  // Some plugins were never written to understand the "data" attribute of the OBJECT tag.
  // Real and WMP will not play unless they find a "src" attribute, see bug 152334.
  // Nav 4.x would simply replace the "data" with "src". Because some plugins correctly
  // look for "data", lets instead copy the "data" attribute and add another entry
  // to the bottom of the array if there isn't already a "src" specified.
  PRInt16 numRealAttrs = mNumCachedAttrs;
  nsAutoString data;
  if (mContent->Tag() == nsGkAtoms::object
    && !mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::src)
    && mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, data)) {
      mNumCachedAttrs++;
  }

  // now lets make the arrays
  mCachedAttrParamNames  = (char **)PR_Calloc(sizeof(char *) * (mNumCachedAttrs + 1 + mNumCachedParams), 1);
  NS_ENSURE_TRUE(mCachedAttrParamNames,  NS_ERROR_OUT_OF_MEMORY);
  mCachedAttrParamValues = (char **)PR_Calloc(sizeof(char *) * (mNumCachedAttrs + 1 + mNumCachedParams), 1);
  NS_ENSURE_TRUE(mCachedAttrParamValues, NS_ERROR_OUT_OF_MEMORY);

  // let's fill in our attributes
  PRInt16 c = 0;

  // Some plugins (eg Flash, see bug 234675.) are actually sensitive to the
  // attribute order.  So we want to make sure we give the plugin the
  // attributes in the order they came in in the source, to be compatible with
  // other browsers.  Now in HTML, the storage order is the reverse of the
  // source order, while in XML and XHTML it's the same as the source order
  // (see the AddAttributes functions in the HTML and XML content sinks).
  PRInt16 start, end, increment;
  if (mContent->IsNodeOfType(nsINode::eHTML) &&
      mContent->NodeInfo()->NamespaceEquals(kNameSpaceID_None)) {
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
  for (PRInt16 index = start; index != end; index += increment) {
    const nsAttrName* attrName = mContent->GetAttrNameAt(index);
    nsIAtom* atom = attrName->LocalName();
    nsAutoString value;
    mContent->GetAttr(attrName->NamespaceID(), atom, value);
    nsAutoString name;
    atom->ToString(name);

    FixUpURLS(name, value);

    mCachedAttrParamNames [c] = ToNewUTF8String(name);
    mCachedAttrParamValues[c] = ToNewUTF8String(value);
    c++;
  }

  // if the conditions above were met, copy the "data" attribute to a "src" array entry
  if (data.Length()) {
    mCachedAttrParamNames [mNumCachedAttrs-1] = ToNewUTF8String(NS_LITERAL_STRING("SRC"));
    mCachedAttrParamValues[mNumCachedAttrs-1] = ToNewUTF8String(data);
  }

  // add our PARAM and null separator
  mCachedAttrParamNames [mNumCachedAttrs] = ToNewUTF8String(NS_LITERAL_STRING("PARAM"));
  mCachedAttrParamValues[mNumCachedAttrs] = nsnull;

  // now fill in the PARAM name/value pairs from the cached DOM nodes
  c = 0;
  for (PRInt16 idx = 0; idx < mNumCachedParams; idx++) {
    nsIDOMElement* param = ourParams.ObjectAt(idx);
    if (param) {
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
     mCachedAttrParamNames [mNumCachedAttrs + 1 + c] = ToNewUTF8String(name);
     mCachedAttrParamValues[mNumCachedAttrs + 1 + c] = ToNewUTF8String(value);
     c++;                                                      // rules!
    }
  }

  return NS_OK;
}


// Here's where we forward events to plugins.

#ifdef XP_MACOSX

static void InitializeEventRecord(EventRecord* event)
{
  memset(event, 0, sizeof(EventRecord));
  ::GetGlobalMouse(&event->where);
  event->when = ::TickCount();
  event->modifiers = ::GetCurrentEventKeyModifiers();
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

  mInstance->GetValue(nsPluginInstanceVariable_DrawingModel,
                      (void *)&drawingModel);

  return drawingModel;
}

void nsPluginInstanceOwner::GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord* origEvent, EventRecord& aMacEvent)
{
  nsPresContext* presContext = mOwner ? mOwner->PresContext() : nsnull;
  InitializeEventRecord(&aMacEvent);
  switch (anEvent.message) {
    case NS_FOCUS_EVENT_START:   // this is the same as NS_FOCUS_CONTENT
        aMacEvent.what = nsPluginEventType_GetFocusEvent;
        if (presContext) {
            nsIContent* content = mContent;
            if (content)
                content->SetFocus(presContext);
        }
        break;

    case NS_BLUR_CONTENT:
        aMacEvent.what = nsPluginEventType_LoseFocusEvent;
        if (presContext) {
            nsIContent* content = mContent;
            if (content)
                content->RemoveFocus(presContext);
        }
        break;

    case NS_MOUSE_MOVE:
    case NS_MOUSE_ENTER:
        if (origEvent)
          aMacEvent = *origEvent;
        aMacEvent.what = nsPluginEventType_AdjustCursorEvent;
        break;
  }
}

// Currently (on OS X in Cocoa widgets) any changes made as a result of
// calling GetPluginPort() are immediately reflected in the nsPluginWindow
// structure that has been passed to the plugin via SetWindow().  This is
// because calls to nsChildView::GetNativeData(NS_NATIVE_PLUGIN_PORT_CG)
// always return a pointer to the same internal (private) object, but may
// make changes inside that object.  All calls to GetPluginPort() made while
// the plugin is active (i.e. excluding those made at our initialization)
// need to take this into account.  The easiest way to do so is to replace
// them with calls to SetPluginPortAndDetectChange().  This method keeps track
// of when calls to GetPluginPort() result in changes, and sets a flag to make
// sure SetWindow() gets called the next time through FixUpPluginWindow(), so
// that the plugin is notified of these changes.
nsPluginPort* nsPluginInstanceOwner::SetPluginPortAndDetectChange()
{
  if (!mPluginWindow)
    return nsnull;
  nsPluginPort* pluginPort = GetPluginPort();
  if (!pluginPort)
    return nsnull;
  mPluginWindow->window = pluginPort;

  NPDrawingModel drawingModel = GetDrawingModel();

#ifndef NP_NO_QUICKDRAW
  if (drawingModel == NPDrawingModelQuickDraw) {
    if (mPluginWindow->window->qdPort.port != mPluginPortCopy.qdPort.port) {
      mPluginPortCopy.qdPort.port = mPluginWindow->window->qdPort.port;
      mPluginPortChanged = PR_TRUE;
    }
  } else if (drawingModel == NPDrawingModelCoreGraphics)
#endif
  {
    if ((mPluginWindow->window->cgPort.context != mPluginPortCopy.cgPort.context) ||
        (mPluginWindow->window->cgPort.window != mPluginPortCopy.cgPort.window)) {
      mPluginPortCopy.cgPort.context = mPluginWindow->window->cgPort.context;
      mPluginPortCopy.cgPort.window = mPluginWindow->window->cgPort.window;
      mPluginPortChanged = PR_TRUE;
    }
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
  NS_ASSERTION(mInCGPaintLevel >= 0, "Mismatched call to nsPluginInstanceOwner::EndCGPlugin()!");
}

#endif

nsresult nsPluginInstanceOwner::ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
#ifdef XP_MACOSX
    CancelTimer();

    if (mInstance) {
        nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
        if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
            EventRecord scrollEvent;
            InitializeEventRecord(&scrollEvent);
            scrollEvent.what = nsPluginEventType_ScrollingBeginsEvent;
    
            WindowRef window = FixUpPluginWindow(ePluginPaintDisable);
            if (window) {
              nsPluginEvent pluginEvent = { &scrollEvent, nsPluginPlatformWindowRef(window) };
              PRBool eventHandled = PR_FALSE;
              mInstance->HandleEvent(&pluginEvent, &eventHandled);
            }
            pluginWidget->EndDrawPlugin();
        }
    }
#endif
    return NS_OK;
}

nsresult nsPluginInstanceOwner::ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
#ifdef XP_MACOSX
    if (mInstance) {
      nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
      if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
        EventRecord scrollEvent;
        InitializeEventRecord(&scrollEvent);
        scrollEvent.what = nsPluginEventType_ScrollingEndsEvent;
  
        WindowRef window = FixUpPluginWindow(ePluginPaintEnable);
        if (window) {
          nsPluginEvent pluginEvent = { &scrollEvent, nsPluginPlatformWindowRef(window) };
          PRBool eventHandled = PR_FALSE;
          mInstance->HandleEvent(&pluginEvent, &eventHandled);
        }
        pluginWidget->EndDrawPlugin();
      }
    }
#endif

    StartTimer(NORMAL_PLUGIN_DELAY);
    return NS_OK;
}

/*=============== nsIFocusListener ======================*/
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
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type) {
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
#ifdef XP_MACOSX // send KeyPress events only on Mac

  // KeyPress events are really synthesized keyDown events.
  // Here we check the native message of the event so that
  // we won't send the plugin two keyDown events.
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aKeyEvent));
  if (privateEvent) {
    nsEvent *theEvent = privateEvent->GetInternalNSEvent();
    const nsGUIEvent *guiEvent = (nsGUIEvent*)theEvent;
    const EventRecord *ev = (EventRecord*)(guiEvent->nativeMsg); 
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
#else

  if (SendNativeEvents())
    return DispatchKeyToPlugin(aKeyEvent);

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
#ifndef XP_MACOSX
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
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
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
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
#if !defined(XP_MACOSX)
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
    return aMouseEvent->PreventDefault(); // consume event
  // continue only for cases without child window
#endif

  // if the plugin is windowless, we need to set focus ourselves
  // otherwise, we might not get key events
  if (mOwner && mPluginWindow &&
      mPluginWindow->type == nsPluginWindowType_Drawable) {
    mContent->SetFocus(mOwner->PresContext());
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
#if !defined(XP_MACOSX)
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
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
  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (mInstance) {
    nsCOMPtr<nsIDOMDragEvent> dragEvent = do_QueryInterface(aEvent);
    if (dragEvent) {
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

nsEventStatus nsPluginInstanceOwner::ProcessEvent(const nsGUIEvent& anEvent)
{
  // printf("nsGUIEvent.message: %d\n", anEvent.message);
  nsEventStatus rv = nsEventStatus_eIgnore;
  if (!mInstance || !mOwner)   // if mInstance is null, we shouldn't be here
    return rv;

#ifdef XP_MACOSX
  // check for null mWidget
  if (mWidget) {
    nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
    if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
      EventRecord macEvent;
      EventRecord* event = (EventRecord*)anEvent.nativeMsg;
      if ((event == NULL) || (event->what == nullEvent)  || 
          (anEvent.message == NS_FOCUS_CONTENT)          || 
          (anEvent.message == NS_BLUR_CONTENT)           || 
          (anEvent.message == NS_MOUSE_MOVE)             ||
          (anEvent.message == NS_MOUSE_ENTER)) {
        GUItoMacEvent(anEvent, event, macEvent);
        event = &macEvent;
      }

      if (anEvent.message == NS_FOCUS_CONTENT) {
        // Work around an issue in the Flash plugin, which can cache a pointer
        // to a doomed TSM document (one that belongs to a NSTSMInputContext)
        // and try to activate it after it has been deleted. See bug 183313.
        ::DeactivateTSMDocument(::TSMGetActiveDocument());
      }

      PRBool eventHandled = PR_FALSE;
      WindowRef window = FixUpPluginWindow(ePluginPaintIgnore);
      if (window) {
        nsPluginEvent pluginEvent = { event, nsPluginPlatformWindowRef(window) };
        mInstance->HandleEvent(&pluginEvent, &eventHandled);
      }

      if (eventHandled && !(anEvent.eventStructType == NS_MOUSE_EVENT &&
                            anEvent.message == NS_MOUSE_BUTTON_DOWN &&
                            static_cast<const nsMouseEvent&>(anEvent).button ==
                              nsMouseEvent::eLeftButton &&
                            !mContentFocused))
        rv = nsEventStatus_eConsumeNoDefault;

      pluginWidget->EndDrawPlugin();
    }
  }
#endif

#ifdef XP_WIN
  // this code supports windowless plugins
  nsPluginEvent * pPluginEvent = (nsPluginEvent *)anEvent.nativeMsg;
  // we can get synthetic events from the nsEventStateManager... these
  // have no nativeMsg
  nsPluginEvent pluginEvent;
  switch (anEvent.eventStructType) {
    case NS_MOUSE_EVENT:
      // XXX we could synthesize Windows mouse events here for our
      // synthetic mouse events (i.e. !pPluginEvent)
      if (pPluginEvent) {
        // Make event coordinates relative to our enclosing widget,
        // not the widget they were received on.
        // See use of nsPluginEvent in widget/src/windows/nsWindow.cpp
        // for why this assert should be safe
        NS_ASSERTION(anEvent.message == NS_MOUSE_BUTTON_DOWN ||
                     anEvent.message == NS_MOUSE_BUTTON_UP ||
                     anEvent.message == NS_MOUSE_DOUBLECLICK ||
                     anEvent.message == NS_MOUSE_ENTER_SYNTH ||
                     anEvent.message == NS_MOUSE_EXIT_SYNTH ||
                     anEvent.message == NS_MOUSE_MOVE,
                     "Incorrect event type for coordinate translation");
        nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mOwner);
        nsPresContext* presContext = mOwner->PresContext();
        nsIntPoint ptPx(presContext->AppUnitsToDevPixels(pt.x),
                        presContext->AppUnitsToDevPixels(pt.y));
        nsIntPoint widgetPtPx = ptPx + mOwner->GetWindowOriginInPixels(PR_TRUE);
        pPluginEvent->lParam = MAKELPARAM(widgetPtPx.x, widgetPtPx.y);
      }
      break;

    case NS_FOCUS_EVENT:
      if (!pPluginEvent) {
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
      break;
  }

  if (pPluginEvent) {
    PRBool eventHandled = PR_FALSE;
    mInstance->HandleEvent(pPluginEvent, &eventHandled);
    if (eventHandled)
      rv = nsEventStatus_eConsumeNoDefault;
  }
#endif

#ifdef MOZ_X11
  // this code supports windowless plugins
  nsIWidget* widget = anEvent.widget;
  nsPluginEvent pluginEvent;
  pluginEvent.event.type = 0;

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
        const nsPresContext* presContext = mOwner->PresContext();
        nsPoint appPoint =
          nsLayoutUtils::GetEventCoordinatesRelativeTo(&anEvent, mOwner); 
        nsIntPoint pluginPoint(presContext->AppUnitsToDevPixels(appPoint.x),
                               presContext->AppUnitsToDevPixels(appPoint.y));
        const nsMouseEvent& mouseEvent =
          static_cast<const nsMouseEvent&>(anEvent);
        // Get reference point relative to screen:
        nsRect windowRect(anEvent.refPoint, nsSize(1, 1));
        nsRect rootPoint(-1,-1,1,1);
        if (widget)
          widget->WidgetToScreen(windowRect, rootPoint);
#ifdef MOZ_WIDGET_GTK2
        Window root = GDK_ROOT_WINDOW();
#else
        Window root = None; // Could XQueryTree, but this is not important.
#endif

        switch (anEvent.message)
          {
          case NS_MOUSE_ENTER_SYNTH:
          case NS_MOUSE_EXIT_SYNTH:
            {
              XCrossingEvent& event = pluginEvent.event.xcrossing;
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
              XMotionEvent& event = pluginEvent.event.xmotion;
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
              XButtonEvent& event = pluginEvent.event.xbutton;
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
      if (anEvent.nativeMsg)
        {
          XKeyEvent &event = pluginEvent.event.xkey;
#ifdef MOZ_WIDGET_GTK2
          event.root = GDK_ROOT_WINDOW();
          event.time = anEvent.time;
          const GdkEventKey* gdkEvent =
            static_cast<const GdkEventKey*>(anEvent.nativeMsg);
          event.keycode = gdkEvent->hardware_keycode;
          event.state = gdkEvent->state;
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
          // Information that could be obtained from nativeMsg but we may not
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
            XFocusChangeEvent &event = pluginEvent.event.xfocus;
            event.type =
              anEvent.message == NS_FOCUS_CONTENT ? FocusIn : FocusOut;
            // information lost:
            event.mode = -1;
            event.detail = NotifyDetailNone;
          }
          break;
        }
    }

  if (!pluginEvent.event.type) {
    PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
           ("Unhandled event message %d with struct type %d\n",
            anEvent.message, anEvent.eventStructType));
    return rv;
  }

  // Fill in (useless) generic event information.
  XAnyEvent& event = pluginEvent.event.xany;
  event.display = widget ?
    static_cast<Display*>(widget->GetNativeData(NS_NATIVE_DISPLAY)) : nsnull;
  event.window = None; // not a real window
  // information lost:
  event.serial = 0;
  event.send_event = False;

  PRBool eventHandled = PR_FALSE;
  mInstance->HandleEvent(&pluginEvent, &eventHandled);
  if (eventHandled)
      rv = nsEventStatus_eConsumeNoDefault;
#endif

  return rv;
}

nsresult
nsPluginInstanceOwner::Destroy()
{
  // stop the timer explicitly to reduce reference count.
  CancelTimer();

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
#ifdef XP_WIN
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

  // Unregister scroll position listener
  nsIFrame* parentWithView = mOwner->GetAncestorWithView();
  nsIView* curView = parentWithView ? parentWithView->GetView() : nsnull;
  while (curView) {
    nsIScrollableView* scrollingView = curView->ToScrollableView();
    if (scrollingView)
      scrollingView->RemoveScrollPositionListener((nsIScrollPositionListener *)this);
    
    curView = curView->GetParent();
  }
}

// Paints are handled differently, so we just simulate an update event.

#ifdef XP_MACOSX
void nsPluginInstanceOwner::Paint(const nsRect& aDirtyRect)
{
  if (!mInstance || !mOwner)
    return;
 
#ifdef DO_DIRTY_INTERSECT   // aDirtyRect isn't always correct, see bug 56128
  nsPoint rel(aDirtyRect.x, aDirtyRect.y);
  nsPoint abs(0,0);
  nsCOMPtr<nsIWidget> containerWidget;

  // Convert dirty rect relative coordinates to absolute and also get the containerWidget
  ConvertRelativeToWindowAbsolute(mOwner, rel, abs, *getter_AddRefs(containerWidget));

  nsRect absDirtyRect = nsRect(abs.x, abs.y, aDirtyRect.width, aDirtyRect.height);

  // Convert to absolute pixel values for the dirty rect
  nsIntRect absDirtyRectInPixels;
  ConvertAppUnitsToPixels(*mOwner->GetPresContext(), absDirtyRect,
                          absDirtyRectInPixels);
#endif

  nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
  if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
    WindowRef window = FixUpPluginWindow(ePluginPaintEnable);
    if (window) {
      EventRecord updateEvent;
      InitializeEventRecord(&updateEvent);
      updateEvent.what = updateEvt;
      updateEvent.message = UInt32(window);
    
      nsPluginEvent pluginEvent = { &updateEvent, nsPluginPlatformWindowRef(window) };
      PRBool eventHandled = PR_FALSE;
      mInstance->HandleEvent(&pluginEvent, &eventHandled);
    }
    pluginWidget->EndDrawPlugin();
  }
}
#endif

#ifdef XP_WIN
void nsPluginInstanceOwner::Paint(const RECT& aDirty, HDC aDC)
{
  if (!mInstance || !mOwner)
    return;

  nsPluginEvent pluginEvent;
  pluginEvent.event = WM_PAINT;
  pluginEvent.wParam = WPARAM(aDC);
  pluginEvent.lParam = LPARAM(&aDirty);
  PRBool eventHandled = PR_FALSE;
  mInstance->HandleEvent(&pluginEvent, &eventHandled);
}
#endif

#ifdef XP_OS2
void nsPluginInstanceOwner::Paint(const nsRect& aDirtyRect, HPS aHPS)
{
  if (!mInstance || !mOwner)
    return;

  nsPluginWindow * window;
  GetWindow(window);
  nsRect relDirtyRect = nsRect(aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  nsIntRect relDirtyRectInPixels;
  ConvertAppUnitsToPixels(*mOwner->PresContext(), relDirtyRect,
                          relDirtyRectInPixels);

  // we got dirty rectangle in relative window coordinates, but we
  // need it in absolute units and in the (left, top, right, bottom) form
  RECTL rectl;
  rectl.xLeft   = relDirtyRectInPixels.x + window->x;
  rectl.yBottom = relDirtyRectInPixels.y + window->y;
  rectl.xRight  = rectl.xLeft + relDirtyRectInPixels.width;
  rectl.yTop    = rectl.yBottom + relDirtyRectInPixels.height;

  nsPluginEvent pluginEvent;
  pluginEvent.event = WM_PAINT;
  pluginEvent.wParam = (uint32)aHPS;
  pluginEvent.lParam = (uint32)&rectl;
  PRBool eventHandled = PR_FALSE;
  mInstance->HandleEvent(&pluginEvent, &eventHandled);
}
#endif

#if defined(MOZ_X11) || defined(MOZ_DFB)
void nsPluginInstanceOwner::Paint(gfxContext* aContext,
                                  const gfxRect& aFrameRect,
                                  const gfxRect& aDirtyRect)
{
  if (!mInstance || !mOwner)
    return;

  // Align to device pixels where sensible
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

  nsPluginWindow* window;
  GetWindow(window);

  Renderer renderer(window, mInstance, pluginSize, pluginDirtyRect);
  PRUint32 rendererFlags =
    Renderer::DRAW_SUPPORTS_OFFSET |
    Renderer::DRAW_SUPPORTS_CLIP_RECT |
    Renderer::DRAW_SUPPORTS_NONDEFAULT_VISUAL |
    Renderer::DRAW_SUPPORTS_ALTERNATE_SCREEN;

  PRBool transparent = PR_TRUE;
  mInstance->GetValue(nsPluginInstanceVariable_TransparentBool,
                      (void *)&transparent);
  if (!transparent)
    rendererFlags |= Renderer::DRAW_IS_OPAQUE;

  // Renderer::Draw() draws a rectangle with top-left at the aContext origin.
  gfxContextAutoSaveRestore autoSR(aContext);
  aContext->Translate(pluginRect.pos);

  renderer.Draw(aContext, window->width, window->height,
                rendererFlags, nsnull);
}

#ifdef MOZ_X11
static int
DepthOfVisual(const Screen* screen, const Visual* visual)
{
  for (int d = 0; d < screen->ndepths; d++) {
    Depth *d_info = &screen->depths[d];
    for (int v = 0; v < d_info->nvisuals; v++) {
      if (&d_info->visuals[v] == visual)
        return d_info->depth;
    }
  }

  NS_ERROR("Visual not on Screen.");
  return 0;
}
#endif

#if defined(MOZ_WIDGET_GTK2)
nsresult
nsPluginInstanceOwner::Renderer::NativeDraw(GdkDrawable * drawable, 
                                            short offsetX, short offsetY,
                                            GdkRectangle * clipRects, 
                                            PRUint32 numClipRects)

{
#ifdef MOZ_X11
  Visual * visual = GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(drawable));
  Colormap colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(drawable));
  Screen * screen = GDK_SCREEN_XSCREEN (gdk_drawable_get_screen(drawable));
#endif
#elif defined(MOZ_WIDGET_QT)
nsresult
nsPluginInstanceOwner::Renderer::NativeDraw(QWidget * drawable,
                                            short offsetX, short offsetY,
                                            QRect * clipRects,
                                            PRUint32 numClipRects)
{
#ifdef MOZ_X11
  QX11Info xinfo = drawable->x11Info();
  Visual * visual = (Visual*) xinfo.visual();
  Colormap colormap = xinfo.colormap();
  Screen * screen = (Screen*) xinfo.screen();
#endif
#endif
  // See if the plugin must be notified of new window parameters.
  PRBool doupdatewindow = PR_FALSE;

  if (mWindow->x != offsetX || mWindow->y != offsetY) {
    mWindow->x = offsetX;
    mWindow->y = offsetY;
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
#if defined(MOZ_WIDGET_GTK2)
    clipRect.x = clipRects[0].x;
    clipRect.y = clipRects[0].y;
    clipRect.width  = clipRects[0].width;
    clipRect.height = clipRects[0].height;
#elif defined(MOZ_WIDGET_QT)
    clipRect.x = clipRects[0].x();
    clipRect.y = clipRects[0].y();
    clipRect.width  = clipRects[0].width();
    clipRect.height = clipRects[0].height();
#endif
  }
  else {
    // nsPluginRect members are unsigned, but
    // we should have been given a clip if an offset is -ve.
    NS_ASSERTION(offsetX >= 0 && offsetY >= 0,
                 "Clip rectangle offsets are negative!");
    clipRect.x = offsetX;
    clipRect.y = offsetY;
    clipRect.width  = mWindow->width;
    clipRect.height = mWindow->height;
  }

  nsPluginRect newClipRect;
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
    ws_info->depth = DepthOfVisual(screen, visual);
    doupdatewindow = PR_TRUE;
  }
#endif

  if (doupdatewindow)
      mInstance->SetWindow(mWindow);

#ifdef MOZ_X11
  // Translate the dirty rect to drawable coordinates.
  nsIntRect dirtyRect = mDirtyRect + nsIntPoint(offsetX, offsetY);
  // Intersect the dirty rect with the clip rect to ensure that it lies within
  // the drawable.
  if (!dirtyRect.IntersectRect(dirtyRect, clipRect))
    return NS_OK;

  nsPluginEvent pluginEvent;
  XGraphicsExposeEvent& exposeEvent = pluginEvent.event.xgraphicsexpose;
  // set the drawing info
  exposeEvent.type = GraphicsExpose;
  exposeEvent.display = DisplayOfScreen(screen);
  exposeEvent.drawable =
#if defined(MOZ_WIDGET_GTK2)
      GDK_DRAWABLE_XID(drawable);
#elif defined(MOZ_WIDGET_QT)
      drawable->x11PictureHandle();
#endif
  exposeEvent.x = mDirtyRect.x + offsetX;
  exposeEvent.y = mDirtyRect.y + offsetY;
  exposeEvent.width  = mDirtyRect.width;
  exposeEvent.height = mDirtyRect.height;
  exposeEvent.count = 0;
  // information not set:
  exposeEvent.serial = 0;
  exposeEvent.send_event = False;
  exposeEvent.major_code = 0;
  exposeEvent.minor_code = 0;

  PRBool eventHandled = PR_FALSE;
  mInstance->HandleEvent(&pluginEvent, &eventHandled);
#endif

  return NS_OK;
}
#endif

// Here's how we give idle time to plugins.

NS_IMETHODIMP nsPluginInstanceOwner::Notify(nsITimer* /* timer */)
{
#ifdef XP_MACOSX
    // validate the plugin clipping information by syncing the plugin window info to
    // reflect the current widget location. This makes sure that everything is updated
    // correctly in the event of scrolling in the window.
    if (mInstance) {
        nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
        if (pluginWidget && NS_SUCCEEDED(pluginWidget->StartDrawPlugin())) {
            WindowRef window = FixUpPluginWindow(ePluginPaintIgnore);
            if (window) {
                EventRecord idleEvent;
                InitializeEventRecord(&idleEvent);
                idleEvent.what = nullEvent;
                    
                // give a bogus 'where' field of our null event when hidden, so Flash
                // won't respond to mouse moves in other tabs, see bug 120875
                if (!mWidgetVisible)
                    idleEvent.where.h = idleEvent.where.v = 20000;
    
                nsPluginEvent pluginEvent = { &idleEvent, nsPluginPlatformWindowRef(window) };
    
                PRBool eventHandled = PR_FALSE;
                mInstance->HandleEvent(&pluginEvent, &eventHandled);
            }
            
            pluginWidget->EndDrawPlugin();
       }
    }
#endif
    return NS_OK;
}

void nsPluginInstanceOwner::StartTimer(unsigned int aDelay)
{
#ifdef XP_MACOSX
  if (!mTimerCanceled)
    return;

  // start a periodic timer to provide null events to the plugin instance.
  if (!mPluginTimer) {
    mPluginTimer = do_CreateInstance("@mozilla.org/timer;1");
  }
  if (mPluginTimer) {
    mTimerCanceled = PR_FALSE;
    mPluginTimer->InitWithCallback(this, aDelay, nsITimer::TYPE_REPEATING_SLACK);
  }
#endif
}

void nsPluginInstanceOwner::CancelTimer()
{
  if (mPluginTimer) {
    mPluginTimer->Cancel();
  }
  mTimerCanceled = PR_TRUE;
}

nsresult nsPluginInstanceOwner::Init(nsPresContext* aPresContext,
                                     nsObjectFrame* aFrame,
                                     nsIContent*    aContent)
{
  mLastEventloopNestingLevel = 0;
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->GetEventloopNestingLevel(&mLastEventloopNestingLevel);
  }

  PR_LOG(nsObjectFrameLM, PR_LOG_DEBUG,
         ("nsPluginInstanceOwner::Init() called on %p for frame %p\n", this,
          aFrame));

  mOwner = aFrame;
  mContent = aContent;

  nsWeakFrame weakFrame(aFrame);

  // Some plugins require a specific sequence of shutdown and startup when
  // a page is reloaded. Shutdown happens usually when the last instance
  // is destroyed. Here we make sure the plugin instance in the old
  // document is destroyed before we try to create the new one.
  aPresContext->EnsureVisible(PR_TRUE);

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
  
  // Register scroll position listener
  // We need to register a scroll pos listener on every scrollable
  // view up to the top
  nsIFrame* parentWithView = mOwner->GetAncestorWithView();
  nsIView* curView = parentWithView ? parentWithView->GetView() : nsnull;
  while (curView) {
    nsIScrollableView* scrollingView = curView->ToScrollableView();
    if (scrollingView)
      scrollingView->AddScrollPositionListener((nsIScrollPositionListener *)this);
    
    curView = curView->GetParent();
  }

  return NS_OK; 
}

nsPluginPort* nsPluginInstanceOwner::GetPluginPort()
{
//!!! Port must be released for windowless plugins on Windows, because it is HDC !!!

  nsPluginPort* result = NULL;
  if (mWidget) {
#ifdef XP_WIN
    if (mPluginWindow && mPluginWindow->type == nsPluginWindowType_Drawable)
      result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_GRAPHIC);
    else
#endif
#ifdef XP_MACOSX
    if (GetDrawingModel() == NPDrawingModelCoreGraphics)
      result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT_CG);
    else
#endif
      result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
  }
  return result;
}

void nsPluginInstanceOwner::ReleasePluginPort(nsPluginPort * pluginPort)
{
#ifdef XP_WIN
  if (mWidget && mPluginWindow &&
      mPluginWindow->type == nsPluginWindowType_Drawable) {
    mWidget->FreeNativeData((HDC)pluginPort, NS_NATIVE_GRAPHIC);
  }
#endif
}

NS_IMETHODIMP nsPluginInstanceOwner::CreateWidget(void)
{
  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  nsIView   *view;
  nsresult  rv = NS_ERROR_FAILURE;

  if (mOwner) {
    // Create view if necessary

    view = mOwner->GetView();

    if (!view || !mWidget) {
      PRBool windowless = PR_FALSE;

      mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool,
                          (void *)&windowless);

      // always create widgets in Twips, not pixels
      nsPresContext* context = mOwner->PresContext();
      rv = mOwner->CreateWidget(context->DevPixelsToAppUnits(mPluginWindow->width),
                                context->DevPixelsToAppUnits(mPluginWindow->height),
                                windowless);
      if (NS_OK == rv) {
        view = mOwner->GetView();

        if (view) {
          mWidget = view->GetWidget();
        }

        if (PR_TRUE == windowless) {
          mPluginWindow->type = nsPluginWindowType_Drawable;

          // this needs to be a HDC according to the spec, but I do
          // not see the right way to release it so let's postpone
          // passing HDC till paint event when it is really
          // needed. Change spec?
          mPluginWindow->window = nsnull;
#ifdef MOZ_X11
          // Fill in the display field.
          nsIWidget* win = mOwner->GetWindow();
          NPSetWindowCallbackStruct* ws_info = 
            static_cast<NPSetWindowCallbackStruct*>(mPluginWindow->ws_info);
          if (win) {
            ws_info->display =
              static_cast<Display*>(win->GetNativeData(NS_NATIVE_DISPLAY));
          }
#ifdef MOZ_WIDGET_GTK2
          else {
            ws_info->display = GDK_DISPLAY();
          }
#endif
#endif
        } else if (mWidget) {
          mWidget->Resize(mPluginWindow->width, mPluginWindow->height,
                          PR_FALSE);

          // mPluginWindow->type is used in |GetPluginPort| so it must
          // be initialized first
          mPluginWindow->type = nsPluginWindowType_Window;
          mPluginWindow->window = GetPluginPort();

          // start the idle timer.
          StartTimer(NORMAL_PLUGIN_DELAY);

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

#if defined(XP_WIN) || (defined(DO_DIRTY_INTERSECT) && defined(XP_MACOSX)) || defined(XP_OS2)
// convert frame coordinates from twips to pixels
static void ConvertAppUnitsToPixels(const nsPresContext& aPresContext, const nsRect& aTwipsRect, nsIntRect& aPixelRect)
{
  aPixelRect.x = aPresContext.AppUnitsToDevPixels(aTwipsRect.x);
  aPixelRect.y = aPresContext.AppUnitsToDevPixels(aTwipsRect.y);
  aPixelRect.width = aPresContext.AppUnitsToDevPixels(aTwipsRect.width);
  aPixelRect.height = aPresContext.AppUnitsToDevPixels(aTwipsRect.height);
}
#endif

  // Mac specific code to fix up the port location and clipping region
#ifdef XP_MACOSX

#ifdef DO_DIRTY_INTERSECT
// Convert from a frame relative coordinate to a coordinate relative to its
// containing window
static void ConvertRelativeToWindowAbsolute(nsIFrame*   aFrame,
                                            nsPoint&    aRel, 
                                            nsPoint&    aAbs,
                                            nsIWidget*& aContainerWidget)
{
  // See if this frame has a view
  nsIView *view = aFrame->GetView();
  if (!view) {
    aAbs.x = 0;
    aAbs.y = 0;
    // Calculate frames offset from its nearest view
    aFrame->GetOffsetFromView(aAbs, &view);
  } else {
    // Store frames offset from its view.
    aAbs = aFrame->GetPosition();
  }

  NS_ASSERTION(view, "the object frame does not have a view");
  if (view) {
    // Calculate the view's offset from its nearest widget
    nsPoint viewOffset;
    aContainerWidget = view->GetNearestWidget(&viewOffset);
    NS_IF_ADDREF(aContainerWidget);
    aAbs += viewOffset;
  }

  // Add relative coordinate to the absolute coordinate that has been calculated
  aAbs += aRel;
}
#endif // DO_DIRTY_INTERSECT

WindowRef nsPluginInstanceOwner::FixUpPluginWindow(PRInt32 inPaintState)
{
  if (!mWidget || !mPluginWindow || !mInstance || !mOwner)
    return nsnull;

  NPDrawingModel drawingModel = GetDrawingModel();

  // If we've already set up a CGContext in nsObjectFrame::PaintPlugin(), we
  // don't want calls to SetPluginPortAndDetectChange() to step on our work.
  nsPluginPort* pluginPort = nsnull;
  if (mInCGPaintLevel > 0) {
    pluginPort = mPluginWindow->window;
  } else {
    pluginPort = SetPluginPortAndDetectChange();
  }

  if (!pluginPort)
    return nsnull;

  // first, check our view for CSS visibility style
  PRBool isVisible =
    mOwner->GetView()->GetVisibility() == nsViewVisibility_kShow;

  nsCOMPtr<nsIPluginWidget> pluginWidget = do_QueryInterface(mWidget);
  
  nsPoint pluginOrigin;
  nsRect widgetClip;
  PRBool widgetVisible;
  pluginWidget->GetPluginClipRect(widgetClip, pluginOrigin, /* out */ widgetVisible);
  
  // printf("GetPluginClipRect returning visible %d\n", widgetVisible);

  isVisible &= widgetVisible;
  if (!isVisible)
    widgetClip.Empty();

#ifndef NP_NO_QUICKDRAW
  // set the port coordinates
  if (drawingModel == NPDrawingModelQuickDraw) {
    mPluginWindow->x = -pluginPort->qdPort.portx;
    mPluginWindow->y = -pluginPort->qdPort.porty;
  }
  else if (drawingModel == NPDrawingModelCoreGraphics)
#endif
  {
    // This would be a lot easier if we could use obj-c here,
    // but we can't. Since we have only nsIWidget and we can't
    // use its native widget (an obj-c object) we have to go
    // from the widget's screen coordinates to its window coords
    // instead of straight to window coords.
    nsRect geckoBounds;
    mWidget->GetBounds(geckoBounds);
    // we need a rect that is the entire *internal* rect, so the
    // x and y coords are 0, width is the same.
    geckoBounds.x = 0;
    geckoBounds.y = 0;
    nsRect geckoScreenCoords;
    mWidget->WidgetToScreen(geckoBounds, geckoScreenCoords);

    Rect windowRect;
    WindowRef window = (WindowRef)pluginPort->cgPort.window;
    ::GetWindowBounds(window, kWindowStructureRgn, &windowRect);

    mPluginWindow->x = geckoScreenCoords.x - windowRect.left;
    mPluginWindow->y = geckoScreenCoords.y - windowRect.top;
  }

  nsPluginRect oldClipRect = mPluginWindow->clipRect;
  
  // fix up the clipping region
  mPluginWindow->clipRect.top    = widgetClip.y;
  mPluginWindow->clipRect.left   = widgetClip.x;

  mWidgetVisible = isVisible;

  if (!mWidgetVisible || inPaintState == ePluginPaintDisable) {
    mPluginWindow->clipRect.bottom = mPluginWindow->clipRect.top;
    mPluginWindow->clipRect.right  = mPluginWindow->clipRect.left;
    // pluginPort = nsnull; // don't uncomment this
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
    // if the clipRect is of size 0, make the null timer fire less often
    CancelTimer();
    if (mPluginWindow->clipRect.left == mPluginWindow->clipRect.right ||
        mPluginWindow->clipRect.top == mPluginWindow->clipRect.bottom) {
      StartTimer(HIDDEN_PLUGIN_DELAY);
    }
    else {
      StartTimer(NORMAL_PLUGIN_DELAY);
    }
  } else if (mPluginPortChanged) {
    mInstance->SetWindow(mPluginWindow);
    mPluginPortChanged = PR_FALSE;
  }

#ifndef NP_NO_QUICKDRAW
  if (drawingModel == NPDrawingModelQuickDraw)
    return ::GetWindowFromPort(pluginPort->qdPort.port);
#endif

  if (drawingModel == NPDrawingModelCoreGraphics)
    return pluginPort->cgPort.window;

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
