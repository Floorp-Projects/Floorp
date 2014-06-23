/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginInstanceOwner_h_
#define nsPluginInstanceOwner_h_

#include "mozilla/Attributes.h"
#include "npapi.h"
#include "nsCOMPtr.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsIDOMEventListener.h"
#include "nsPluginHost.h"
#include "nsPluginNativeWindow.h"
#include "nsWeakReference.h"
#include "gfxRect.h"

#ifdef XP_MACOSX
#include "mozilla/gfx/QuartzSupport.h"
#include <ApplicationServices/ApplicationServices.h>
#endif

class nsIInputStream;
struct nsIntRect;
class nsPluginDOMContextMenuListener;
class nsObjectFrame;
class nsDisplayListBuilder;

#ifdef MOZ_X11
class gfxXlibSurface;
#ifdef MOZ_WIDGET_QT
#include "gfxQtNativeRenderer.h"
#else
#include "gfxXlibNativeRenderer.h"
#endif
#endif

class nsPluginInstanceOwner : public nsIPluginInstanceOwner,
                              public nsIDOMEventListener,
                              public nsIPrivacyTransitionObserver,
                              public nsSupportsWeakReference
{
public:
  nsPluginInstanceOwner();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGININSTANCEOWNER
  NS_DECL_NSIPRIVACYTRANSITIONOBSERVER
  
  NS_IMETHOD GetURL(const char *aURL, const char *aTarget,
                    nsIInputStream *aPostStream, 
                    void *aHeadersData, uint32_t aHeadersDataLen) MOZ_OVERRIDE;
  
  NS_IMETHOD ShowStatus(const char16_t *aStatusMsg) MOZ_OVERRIDE;
  
  NPError    ShowNativeContextMenu(NPMenu* menu, void* event) MOZ_OVERRIDE;
  
  NPBool     ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                          double *destX, double *destY, NPCoordinateSpace destSpace) MOZ_OVERRIDE;
  
  virtual NPError InitAsyncSurface(NPSize *size, NPImageFormat format,
                                   void *initData, NPAsyncSurface *surface) MOZ_OVERRIDE;
  virtual NPError FinalizeAsyncSurface(NPAsyncSurface *surface) MOZ_OVERRIDE;
  virtual void SetCurrentAsyncSurface(NPAsyncSurface *surface, NPRect *changed) MOZ_OVERRIDE;

  /**
   * Get the type of the HTML tag that was used ot instantiate this
   * plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
   */
  NS_IMETHOD GetTagType(nsPluginTagType *aResult);

  /**
   * Get a ptr to the paired list of parameter names and values,
   * returns the length of the array.
   *
   * Each name or value is a null-terminated string.
   */
  NS_IMETHOD GetParameters(uint16_t& aCount,
                           const char*const*& aNames,
                           const char*const*& aValues);

  /**
   * Get the value for the named parameter.  Returns null
   * if the parameter was not set.
   *
   * @param aName   - name of the parameter
   * @param aResult - parameter value
   * @result        - NS_OK if this operation was successful
   */
  NS_IMETHOD GetParameter(const char* aName, const char* *aResult);

  /**
   * QueryInterface on nsIPluginInstancePeer to get this.
   *
   * (Corresponds to NPP_New's argc, argn, and argv arguments.)
   * Get a ptr to the paired list of attribute names and values,
   * returns the length of the array.
   *
   * Each name or value is a null-terminated string.
   */
  NS_IMETHOD GetAttributes(uint16_t& aCount,
                           const char*const*& aNames,
                           const char*const*& aValues);


  /**
   * Gets the value for the named attribute.
   *
   * @param aName   - the name of the attribute to find
   * @param aResult - the resulting attribute
   * @result - NS_OK if this operation was successful, NS_ERROR_FAILURE if
   * this operation failed. result is set to NULL if the attribute is not found
   * else to the found value.
   */
  NS_IMETHOD GetAttribute(const char* aName, const char* *aResult);

  /**
   * Returns the DOM element corresponding to the tag which references
   * this plugin in the document.
   *
   * @param aDOMElement - resulting DOM element
   * @result - NS_OK if this operation was successful
   */
  NS_IMETHOD GetDOMElement(nsIDOMElement* * aResult);
  
  // nsIDOMEventListener interfaces 
  NS_DECL_NSIDOMEVENTLISTENER
  
  nsresult ProcessMouseDown(nsIDOMEvent* aKeyEvent);
  nsresult ProcessKeyPress(nsIDOMEvent* aKeyEvent);
  nsresult Destroy();  

#ifdef XP_WIN
  void Paint(const RECT& aDirty, HDC aDC);
#elif defined(XP_MACOSX)
  void Paint(const gfxRect& aDirtyRect, CGContextRef cgContext);  
  void RenderCoreAnimation(CGContextRef aCGContext, int aWidth, int aHeight);
  void DoCocoaEventDrawRect(const gfxRect& aDrawRect, CGContextRef cgContext);
#elif defined(MOZ_X11) || defined(ANDROID)
  void Paint(gfxContext* aContext,
             const gfxRect& aFrameRect,
             const gfxRect& aDirtyRect);
#endif

  //locals
  
  nsresult Init(nsIContent* aContent);
  
  void* GetPluginPortFromWidget();
  void ReleasePluginPort(void* pluginPort);

  nsEventStatus ProcessEvent(const mozilla::WidgetGUIEvent& anEvent);
  
#ifdef XP_MACOSX
  enum { ePluginPaintEnable, ePluginPaintDisable };
  
  NPDrawingModel GetDrawingModel();
  bool IsRemoteDrawingCoreAnimation();
  nsresult ContentsScaleFactorChanged(double aContentsScaleFactor);
  NPEventModel GetEventModel();
  static void CARefresh(nsITimer *aTimer, void *aClosure);
  void AddToCARefreshTimer();
  void RemoveFromCARefreshTimer();
  // This calls into the plugin (NPP_SetWindow) and can run script.
  void* FixUpPluginWindow(int32_t inPaintState);
  void HidePluginWindow();
  // Set a flag that (if true) indicates the plugin port info has changed and
  // SetWindow() needs to be called.
  void SetPluginPortChanged(bool aState) { mPluginPortChanged = aState; }
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
  void UpdateWindowPositionAndClipRect(bool aSetWindow);
  void UpdateWindowVisibility(bool aVisible);
  void UpdateDocumentActiveState(bool aIsActive);
#endif // XP_MACOSX

  void SetFrame(nsObjectFrame *aFrame);
  nsObjectFrame* GetFrame();

  uint32_t GetLastEventloopNestingLevel() const {
    return mLastEventloopNestingLevel; 
  }
  
  static uint32_t GetEventloopNestingLevel();
  
  void ConsiderNewEventloopNestingLevel() {
    uint32_t currentLevel = GetEventloopNestingLevel();
    
    if (currentLevel < mLastEventloopNestingLevel) {
      mLastEventloopNestingLevel = currentLevel;
    }
  }
  
  const char* GetPluginName()
  {
    if (mInstance && mPluginHost) {
      const char* name = nullptr;
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
  
  bool SendNativeEvents()
  {
#ifdef XP_WIN
    // XXX we should remove the plugin name check
    return mPluginWindow->type == NPWindowTypeDrawable &&
    (MatchPluginName("Shockwave Flash") ||
     MatchPluginName("Test Plug-in"));
#elif defined(MOZ_X11) || defined(XP_MACOSX)
    return true;
#else
    return false;
#endif
  }
  
  bool MatchPluginName(const char *aPluginName)
  {
    return strncmp(GetPluginName(), aPluginName, strlen(aPluginName)) == 0;
  }
  
  void NotifyPaintWaiter(nsDisplayListBuilder* aBuilder);

  // Returns the image container that has our currently displayed image.
  already_AddRefed<mozilla::layers::ImageContainer> GetImageContainer();

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
  
  bool UseAsyncRendering();

  already_AddRefed<nsIURI> GetBaseURI() const;

#ifdef MOZ_WIDGET_ANDROID
  // Returns the image container for the specified VideoInfo
  void GetVideos(nsTArray<nsNPAPIPluginInstance::VideoInfo*>& aVideos);
  already_AddRefed<mozilla::layers::ImageContainer> GetImageContainerForVideo(nsNPAPIPluginInstance::VideoInfo* aVideoInfo);

  void Invalidate();

  void RequestFullScreen();
  void ExitFullScreen();

  // Called from AndroidJNI when we removed the fullscreen view.
  static void ExitFullScreen(jobject view);
#endif
  
private:
  virtual ~nsPluginInstanceOwner();

  // return FALSE if LayerSurface dirty (newly created and don't have valid plugin content yet)
  bool IsUpToDate()
  {
    nsIntSize size;
    return NS_SUCCEEDED(mInstance->GetImageSize(&size)) &&
    size == nsIntSize(mPluginWindow->width, mPluginWindow->height);
  }
  
  void FixUpURLS(const nsString &name, nsAString &value);
#ifdef MOZ_WIDGET_ANDROID
  mozilla::LayoutDeviceRect GetPluginRect();
  bool AddPluginView(const mozilla::LayoutDeviceRect& aRect = mozilla::LayoutDeviceRect(0, 0, 0, 0));
  void RemovePluginView();

  bool mFullScreen;
  void* mJavaView;
#endif 
 
  nsPluginNativeWindow       *mPluginWindow;
  nsRefPtr<nsNPAPIPluginInstance> mInstance;
  nsObjectFrame              *mObjectFrame;
  nsIContent                 *mContent; // WEAK, content owns us
  nsCString                   mDocumentBase;
  bool                        mWidgetCreationComplete;
  nsCOMPtr<nsIWidget>         mWidget;
  nsRefPtr<nsPluginHost>      mPluginHost;
  
#ifdef XP_MACOSX
  NP_CGContext                              mCGPluginPortCopy;
  int32_t                                   mInCGPaintLevel;
  mozilla::RefPtr<MacIOSurface>             mIOSurface;
  mozilla::RefPtr<nsCARenderer>             mCARenderer;
  CGColorSpaceRef                           mColorProfile;
  static nsCOMPtr<nsITimer>                *sCATimer;
  static nsTArray<nsPluginInstanceOwner*>  *sCARefreshListeners;
  bool                                      mSentInitialTopLevelWindowEvent;
#endif

  // Initially, the event loop nesting level we were created on, it's updated
  // if we detect the appshell is on a lower level as long as we're not stopped.
  // We delay DoStopPlugin() until the appshell reaches this level or lower.
  uint32_t                    mLastEventloopNestingLevel;
  bool                        mContentFocused;
  bool                        mWidgetVisible;    // used on Mac to store our widget's visible state
#ifdef XP_MACOSX
  bool                        mPluginPortChanged;
#endif
#ifdef MOZ_X11
  // Used with windowless plugins only, initialized in CreateWidget().
  bool                        mFlash10Quirks;
#endif
  bool                        mPluginWindowVisible;
  bool                        mPluginDocumentActiveState;

  uint16_t          mNumCachedAttrs;
  uint16_t          mNumCachedParams;
  char              **mCachedAttrParamNames;
  char              **mCachedAttrParamValues;
  
#ifdef XP_MACOSX
  NPEventModel mEventModel;
  // This is a hack! UseAsyncRendering() can incorrectly return false
  // when we don't have an object frame (possible as of bug 90268).
  // We hack around this by always returning true if we've ever
  // returned true.
  bool mUseAsyncRendering;
#endif
  
  // pointer to wrapper for nsIDOMContextMenuListener
  nsRefPtr<nsPluginDOMContextMenuListener> mCXMenuListener;
  
  nsresult DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent);
  nsresult DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent,
                                 bool aAllowPropagate = false);
  nsresult DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent);

  int mLastMouseDownButtonType;
  
  nsresult EnsureCachedAttrParamArrays();
  
#ifdef MOZ_X11
  class Renderer
#if defined(MOZ_WIDGET_QT)
  : public gfxQtNativeRenderer
#else
  : public gfxXlibNativeRenderer
#endif
  {
  public:
    Renderer(NPWindow* aWindow, nsPluginInstanceOwner* aInstanceOwner,
             const nsIntSize& aPluginSize, const nsIntRect& aDirtyRect)
    : mWindow(aWindow), mInstanceOwner(aInstanceOwner),
    mPluginSize(aPluginSize), mDirtyRect(aDirtyRect)
    {}
    virtual nsresult DrawWithXlib(cairo_surface_t* surface,
                                  nsIntPoint offset,
                                  nsIntRect* clipRects, uint32_t numClipRects) MOZ_OVERRIDE;
  private:
    NPWindow* mWindow;
    nsPluginInstanceOwner* mInstanceOwner;
    const nsIntSize& mPluginSize;
    const nsIntRect& mDirtyRect;
  };
#endif

  bool mWaitingForPaint;
};

#endif // nsPluginInstanceOwner_h_

