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

#ifndef nsPluginInstanceOwner_h_
#define nsPluginInstanceOwner_h_

#include "prtypes.h"
#include "npapi.h"
#include "nsCOMPtr.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIPluginTagInfo.h"
#include "nsIDOMEventListener.h"
#include "nsIScrollPositionListener.h"
#include "nsPluginHost.h"
#include "nsPluginNativeWindow.h"
#include "gfxRect.h"

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

#ifdef XP_MACOSX
#include "nsCoreAnimationSupport.h"
#include <ApplicationServices/ApplicationServices.h>
#endif

class nsIInputStream;
class nsIntRect;
class nsPluginDOMContextMenuListener;
class nsObjectFrame;
class nsDisplayListBuilder;

#ifdef MOZ_X11
class gfxXlibSurface;
#endif

#ifdef MOZ_WIDGET_GTK2
#include "gfxXlibNativeRenderer.h"
#endif

#ifdef MOZ_WIDGET_QT
#ifdef MOZ_X11
#include "gfxQtNativeRenderer.h"
#endif
#endif

#ifdef XP_OS2
#define INCL_PM
#define INCL_GPI
#include <os2.h>
#endif

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

class nsPluginInstanceOwner : public nsIPluginInstanceOwner,
                              public nsIPluginTagInfo,
                              public nsIDOMEventListener,
                              public nsIScrollPositionListener
{
public:
  nsPluginInstanceOwner();
  virtual ~nsPluginInstanceOwner();
  
  NS_DECL_ISUPPORTS
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
  
  // nsIDOMEventListener interfaces 
  NS_DECL_NSIDOMEVENTLISTENER
  
  nsresult MouseDown(nsIDOMEvent* aKeyEvent);
  nsresult KeyPress(nsIDOMEvent* aKeyEvent);
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
  nsresult Text(nsIDOMEvent* aTextEvent);
#endif

  nsresult Destroy();  
  
  void PrepareToStop(PRBool aDelayedStop);
  
#ifdef XP_WIN
  void Paint(const RECT& aDirty, HDC aDC);
#elif defined(XP_MACOSX)
  void Paint(const gfxRect& aDirtyRect, CGContextRef cgContext);  
  void RenderCoreAnimation(CGContextRef aCGContext, int aWidth, int aHeight);
  void DoCocoaEventDrawRect(const gfxRect& aDrawRect, CGContextRef cgContext);
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
  enum { ePluginPaintEnable, ePluginPaintDisable };
  
  NPDrawingModel GetDrawingModel();
  PRBool IsRemoteDrawingCoreAnimation();
  NPEventModel GetEventModel();
  static void CARefresh(nsITimer *aTimer, void *aClosure);
  static void AddToCARefreshTimer(nsPluginInstanceOwner *aPluginInstance);
  static void RemoveFromCARefreshTimer(nsPluginInstanceOwner *aPluginInstance);
  void SetupCARefresh();
  void* FixUpPluginWindow(PRInt32 inPaintState);
  void HidePluginWindow();
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
#else // XP_MACOSX
  void UpdateWindowPositionAndClipRect(PRBool aSetWindow);
  void UpdateWindowVisibility(PRBool aVisible);
  void UpdateDocumentActiveState(PRBool aIsActive);
#endif // XP_MACOSX
  void CallSetWindow();
  
  void SetOwner(nsObjectFrame *aOwner)
  {
    mObjectFrame = aOwner;
  }
  nsObjectFrame* GetOwner() {
    return mObjectFrame;
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
    // XXX we should remove the plugin name check
    return mPluginWindow->type == NPWindowTypeDrawable &&
    (MatchPluginName("Shockwave Flash") ||
     MatchPluginName("Test Plug-in"));
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
  
  void NotifyPaintWaiter(nsDisplayListBuilder* aBuilder);
  // Return true if we set image with valid surface
  PRBool SetCurrentImage(ImageContainer* aContainer);
  /**
   * Returns the bounds of the current async-rendered surface. This can only
   * change in response to messages received by the event loop (i.e. not during
   * painting).
   */
  nsIntSize GetCurrentImageSize();
  
  // Methods to update the background image we send to async plugins.
  // The eventual target of these operations is PluginInstanceParent,
  // but it takes several hops to get there.
  void SetBackgroundUnknown();
  already_AddRefed<gfxContext> BeginUpdateBackground(const nsIntRect& aRect);
  void EndUpdateBackground(gfxContext* aContext, const nsIntRect& aRect);
  
  PRBool UseAsyncRendering();
  
private:
  
  // return FALSE if LayerSurface dirty (newly created and don't have valid plugin content yet)
  PRBool IsUpToDate()
  {
    nsIntSize size;
    return NS_SUCCEEDED(mInstance->GetImageSize(&size)) &&
    size == nsIntSize(mPluginWindow->width, mPluginWindow->height);
  }
  
  void FixUpURLS(const nsString &name, nsAString &value);
  
  nsPluginNativeWindow       *mPluginWindow;
  nsRefPtr<nsNPAPIPluginInstance> mInstance;
  nsObjectFrame              *mObjectFrame; // owns nsPluginInstanceOwner
  nsCOMPtr<nsIContent>        mContent;
  nsCString                   mDocumentBase;
  char                       *mTagText;
  nsCOMPtr<nsIWidget>         mWidget;
  nsRefPtr<nsPluginHost>      mPluginHost;
  
#ifdef XP_MACOSX
  NP_CGContext                              mCGPluginPortCopy;
#ifndef NP_NO_QUICKDRAW
  NP_Port                                   mQDPluginPortCopy;
#endif
  PRInt32                                   mInCGPaintLevel;
  nsRefPtr<nsIOSurface>                     mIOSurface;
  nsCARenderer                              mCARenderer;
  CGColorSpaceRef                           mColorProfile;
  static nsCOMPtr<nsITimer>                *sCATimer;
  static nsTArray<nsPluginInstanceOwner*>  *sCARefreshListeners;
  PRBool                                    mSentInitialTopLevelWindowEvent;
#endif
  // We need to know if async hide window is required since we
  // can not check UseAsyncRendering when executing StopPlugin
  PRBool                                    mAsyncHidePluginWindow;
  
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
  PRPackedBool                mPluginWindowVisible;
  PRPackedBool                mPluginDocumentActiveState;
  
  // If true, destroy the widget on destruction. Used when plugin stop
  // is being delayed to a safer point in time.
  PRPackedBool                mDestroyWidget;
  PRUint16          mNumCachedAttrs;
  PRUint16          mNumCachedParams;
  char              **mCachedAttrParamNames;
  char              **mCachedAttrParamValues;
  
#ifdef XP_MACOSX
  NPEventModel mEventModel;
#endif
  
  // pointer to wrapper for nsIDOMContextMenuListener
  nsRefPtr<nsPluginDOMContextMenuListener> mCXMenuListener;
  
  nsresult DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent);
  nsresult DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent);
  nsresult DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent);
  
  nsresult EnsureCachedAttrParamArrays();
  
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

  PRPackedBool mWaitingForPaint;
};

#endif // nsPluginInstanceOwner_h_

