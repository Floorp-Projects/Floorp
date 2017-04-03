/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNPAPIPluginInstance_h_
#define nsNPAPIPluginInstance_h_

#include "nsSize.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsPIDOMWindow.h"
#include "nsITimer.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsHashKeys.h"
#include <prinrval.h>
#include "js/TypeDecls.h"
#include "nsIAudioChannelAgent.h"
#ifdef MOZ_WIDGET_ANDROID
#include "nsIRunnable.h"
#include "GLContextTypes.h"
#include "AndroidSurfaceTexture.h"
#include "AndroidBridge.h"
#include <map>
class PluginEventRunnable;
#endif

#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/PluginLibrary.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"

class nsPluginStreamListenerPeer; // browser-initiated stream class
class nsNPAPIPluginStreamListener; // plugin-initiated stream class
class nsIPluginInstanceOwner;
class nsIOutputStream;
class nsPluginInstanceOwner;

#if defined(OS_WIN)
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelSyncWin;
#elif defined(MOZ_X11)
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelSyncX;
#elif defined(XP_MACOSX)
#ifndef NP_NO_QUICKDRAW
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelQuickDraw; // Not supported
#else
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelCoreGraphics;
#endif
#else
const NPDrawingModel kDefaultDrawingModel = static_cast<NPDrawingModel>(0);
#endif

/**
 * Used to indicate whether it's OK to reenter Gecko and repaint, flush frames,
 * run scripts, etc, during this plugin call.
 * When NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO is set, we try to avoid dangerous
 * Gecko activities when the plugin spins a nested event loop, on a best-effort
 * basis.
 */
enum NSPluginCallReentry {
  NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO,
  NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO
};

class nsNPAPITimer
{
public:
  NPP npp;
  uint32_t id;
  nsCOMPtr<nsITimer> timer;
  void (*callback)(NPP npp, uint32_t timerID);
  bool inCallback;
  bool needUnschedule;
};

class nsNPAPIPluginInstance final : public nsIAudioChannelAgentCallback
                                  , public mozilla::SupportsWeakPtr<nsNPAPIPluginInstance>
{
private:
  typedef mozilla::PluginLibrary PluginLibrary;

public:
  typedef mozilla::gfx::DrawTarget DrawTarget;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(nsNPAPIPluginInstance)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  nsresult Initialize(nsNPAPIPlugin *aPlugin, nsPluginInstanceOwner* aOwner, const nsACString& aMIMEType);
  nsresult Start();
  nsresult Stop();
  nsresult SetWindow(NPWindow* window);
  nsresult NewStreamFromPlugin(const char* type, const char* target, nsIOutputStream* *result);
  nsresult Print(NPPrint* platformPrint);
  nsresult HandleEvent(void* event, int16_t* result,
                       NSPluginCallReentry aSafeToReenterGecko = NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);
  nsresult GetValueFromPlugin(NPPVariable variable, void* value);
  nsresult GetDrawingModel(int32_t* aModel);
  nsresult IsRemoteDrawingCoreAnimation(bool* aDrawing);
  nsresult ContentsScaleFactorChanged(double aContentsScaleFactor);
  nsresult CSSZoomFactorChanged(float aCSSZoomFactor);
  nsresult GetJSObject(JSContext *cx, JSObject** outObject);
  bool ShouldCache();
  nsresult IsWindowless(bool* isWindowless);
  nsresult AsyncSetWindow(NPWindow* window);
  nsresult GetImageContainer(mozilla::layers::ImageContainer **aContainer);
  nsresult GetImageSize(nsIntSize* aSize);
  nsresult NotifyPainted(void);
  nsresult GetIsOOP(bool* aIsOOP);
  nsresult SetBackgroundUnknown();
  nsresult BeginUpdateBackground(nsIntRect* aRect, DrawTarget** aContext);
  nsresult EndUpdateBackground(nsIntRect* aRect);
  nsresult IsTransparent(bool* isTransparent);
  nsresult GetFormValue(nsAString& aValue);
  nsresult PushPopupsEnabledState(bool aEnabled);
  nsresult PopPopupsEnabledState();
  nsresult GetPluginAPIVersion(uint16_t* version);
  nsresult InvalidateRect(NPRect *invalidRect);
  nsresult InvalidateRegion(NPRegion invalidRegion);
  nsresult GetMIMEType(const char* *result);
#if defined(XP_WIN)
  nsresult GetScrollCaptureContainer(mozilla::layers::ImageContainer **aContainer);
#endif
  nsresult HandledWindowedPluginKeyEvent(
             const mozilla::NativeEventData& aKeyEventData,
             bool aIsConsumed);
  nsPluginInstanceOwner* GetOwner();
  void SetOwner(nsPluginInstanceOwner *aOwner);
  void DidComposite();

  bool HasAudioChannelAgent() const
  {
    return !!mAudioChannelAgent;
  }

  nsresult GetOrCreateAudioChannelAgent(nsIAudioChannelAgent** aAgent);

  nsresult SetMuted(bool aIsMuted);

  nsNPAPIPlugin* GetPlugin();

  nsresult GetNPP(NPP * aNPP);

  NPError SetWindowless(bool aWindowless);

  NPError SetTransparent(bool aTransparent);

  NPError SetWantsAllNetworkStreams(bool aWantsAllNetworkStreams);

  NPError SetUsesDOMForCursor(bool aUsesDOMForCursor);
  bool UsesDOMForCursor();

  void SetDrawingModel(NPDrawingModel aModel);
  void RedrawPlugin();
#ifdef XP_MACOSX
  void SetEventModel(NPEventModel aModel);

  void* GetCurrentEvent() {
    return mCurrentPluginEvent;
  }
#endif

#ifdef MOZ_WIDGET_ANDROID
  void NotifyForeground(bool aForeground);
  void NotifyOnScreen(bool aOnScreen);
  void MemoryPressure();
  void NotifyFullScreen(bool aFullScreen);
  void NotifySize(nsIntSize size);

  nsIntSize CurrentSize() { return mCurrentSize; }

  bool IsOnScreen() {
    return mOnScreen;
  }

  uint32_t GetANPDrawingModel() { return mANPDrawingModel; }
  void SetANPDrawingModel(uint32_t aModel);

  void* GetJavaSurface();

  void PostEvent(void* event);

  // These are really mozilla::dom::ScreenOrientation, but it's
  // difficult to include that here
  uint32_t FullScreenOrientation() { return mFullScreenOrientation; }
  void SetFullScreenOrientation(uint32_t orientation);

  void SetWakeLock(bool aLock);

  mozilla::gl::GLContext* GLContext();

  // For ANPOpenGL
  class TextureInfo {
  public:
    TextureInfo() :
      mTexture(0), mWidth(0), mHeight(0), mInternalFormat(0)
    {
    }

    TextureInfo(GLuint aTexture, int32_t aWidth, int32_t aHeight, GLuint aInternalFormat) :
      mTexture(aTexture), mWidth(aWidth), mHeight(aHeight), mInternalFormat(aInternalFormat)
    {
    }

    GLuint mTexture;
    int32_t mWidth;
    int32_t mHeight;
    GLuint mInternalFormat;
  };

  // For ANPNativeWindow
  void* AcquireContentWindow();

  mozilla::gl::AndroidSurfaceTexture* AsSurfaceTexture();

  // For ANPVideo
  class VideoInfo {
  public:
    VideoInfo(mozilla::gl::AndroidSurfaceTexture* aSurfaceTexture) :
      mSurfaceTexture(aSurfaceTexture)
    {
    }

    ~VideoInfo()
    {
      mSurfaceTexture = nullptr;
    }

    RefPtr<mozilla::gl::AndroidSurfaceTexture> mSurfaceTexture;
    gfxRect mDimensions;
  };

  void* AcquireVideoWindow();
  void ReleaseVideoWindow(void* aWindow);
  void SetVideoDimensions(void* aWindow, gfxRect aDimensions);

  void GetVideos(nsTArray<VideoInfo*>& aVideos);

  void SetOriginPos(mozilla::gl::OriginPos aOriginPos) {
    mOriginPos = aOriginPos;
  }
  mozilla::gl::OriginPos OriginPos() const { return mOriginPos; }

  static nsNPAPIPluginInstance* GetFromNPP(NPP npp);
#endif

  nsresult NewStreamListener(const char* aURL, void* notifyData,
                             nsNPAPIPluginStreamListener** listener);

  nsNPAPIPluginInstance();

  // To be called when an instance becomes orphaned, when
  // it's plugin is no longer guaranteed to be around.
  void Destroy();

  // Indicates whether the plugin is running normally.
  bool IsRunning() {
    return RUNNING == mRunning;
  }
  bool HasStartedDestroying() {
    return mRunning >= DESTROYING;
  }

  // Indicates whether the plugin is running normally or being shut down
  bool CanFireNotifications() {
    return mRunning == RUNNING || mRunning == DESTROYING;
  }

  // return is only valid when the plugin is not running
  mozilla::TimeStamp StopTime();

  // cache this NPAPI plugin
  void SetCached(bool aCache);

  already_AddRefed<nsPIDOMWindowOuter> GetDOMWindow();

  nsresult PrivateModeStateChanged(bool aEnabled);

  nsresult IsPrivateBrowsing(bool *aEnabled);

  nsresult GetDOMElement(nsIDOMElement* *result);

  nsNPAPITimer* TimerWithID(uint32_t id, uint32_t* index);
  uint32_t      ScheduleTimer(uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID));
  void          UnscheduleTimer(uint32_t timerID);
  NPBool        ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);


  nsTArray<nsNPAPIPluginStreamListener*> *StreamListeners();

  nsTArray<nsPluginStreamListenerPeer*> *FileCachedStreamListeners();

  nsresult AsyncSetWindow(NPWindow& window);

  void URLRedirectResponse(void* notifyData, NPBool allow);

  NPError InitAsyncSurface(NPSize *size, NPImageFormat format,
                           void *initData, NPAsyncSurface *surface);
  NPError FinalizeAsyncSurface(NPAsyncSurface *surface);
  void SetCurrentAsyncSurface(NPAsyncSurface *surface, NPRect *changed);

  // Returns the contents scale factor of the screen the plugin is drawn on.
  double GetContentsScaleFactor();

  // Returns the css zoom factor of the document the plugin is drawn on.
  float GetCSSZoomFactor();

  nsresult GetRunID(uint32_t *aRunID);

  static bool InPluginCallUnsafeForReentry() { return gInUnsafePluginCalls > 0; }
  static void BeginPluginCall(NSPluginCallReentry aReentryState)
  {
    if (aReentryState == NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO) {
      ++gInUnsafePluginCalls;
    }
  }
  static void EndPluginCall(NSPluginCallReentry aReentryState)
  {
    if (aReentryState == NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO) {
      NS_ASSERTION(gInUnsafePluginCalls > 0, "Must be in plugin call");
      --gInUnsafePluginCalls;
    }
  }

protected:

  virtual ~nsNPAPIPluginInstance();

  nsresult GetTagType(nsPluginTagType *result);

  // check if this is a Java applet and affected by bug 750480
  void CheckJavaC2PJSObjectQuirk(uint16_t paramCount,
                                 const char* const* names,
                                 const char* const* values);

  // The structure used to communicate between the plugin instance and
  // the browser.
  NPP_t mNPP;

  NPDrawingModel mDrawingModel;

#ifdef MOZ_WIDGET_ANDROID
  uint32_t mANPDrawingModel;

  friend class PluginEventRunnable;

  nsTArray<RefPtr<PluginEventRunnable>> mPostedEvents;
  void PopPostedEvent(PluginEventRunnable* r);
  void OnSurfaceTextureFrameAvailable();

  uint32_t mFullScreenOrientation;
  bool mWakeLocked;
  bool mFullScreen;
  mozilla::gl::OriginPos mOriginPos;

  RefPtr<mozilla::gl::AndroidSurfaceTexture> mContentSurface;
#endif

  enum {
    NOT_STARTED,
    RUNNING,
    DESTROYING,
    DESTROYED
  } mRunning;

  // these are used to store the windowless properties
  // which the browser will later query
  bool mWindowless;
  bool mTransparent;
  bool mCached;
  bool mUsesDOMForCursor;

public:
  // True while creating the plugin, or calling NPP_SetWindow() on it.
  bool mInPluginInitCall;

  nsXPIDLCString mFakeURL;

private:
  RefPtr<nsNPAPIPlugin> mPlugin;

  nsTArray<nsNPAPIPluginStreamListener*> mStreamListeners;

  nsTArray<nsPluginStreamListenerPeer*> mFileCachedStreamListeners;

  nsTArray<PopupControlState> mPopupStates;

  char* mMIMEType;

  // Weak pointer to the owner. The owner nulls this out (by calling
  // InvalidateOwner()) when it's no longer our owner.
  nsPluginInstanceOwner *mOwner;

  nsTArray<nsNPAPITimer*> mTimers;

#ifdef XP_MACOSX
  // non-null during a HandleEvent call
  void* mCurrentPluginEvent;
#endif

  // Timestamp for the last time this plugin was stopped.
  // This is only valid when the plugin is actually stopped!
  mozilla::TimeStamp mStopTime;

#ifdef MOZ_WIDGET_ANDROID
  already_AddRefed<mozilla::gl::AndroidSurfaceTexture> CreateSurfaceTexture();

  std::map<void*, VideoInfo*> mVideos;
  bool mOnScreen;

  nsIntSize mCurrentSize;
#endif

  // is this instance Java and affected by bug 750480?
  bool mHaveJavaC2PJSObjectQuirk;

  static uint32_t gInUnsafePluginCalls;

  // The arrays can only be released when the plugin instance is destroyed,
  // because the plugin, in in-process mode, might keep a reference to them.
  uint32_t mCachedParamLength;
  char **mCachedParamNames;
  char **mCachedParamValues;

  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;
  bool mMuted;
};

// On Android, we need to guard against plugin code leaking entries in the local
// JNI ref table. See https://bugzilla.mozilla.org/show_bug.cgi?id=780831#c21
#ifdef MOZ_WIDGET_ANDROID
  #define MAIN_THREAD_JNI_REF_GUARD mozilla::AutoLocalJNIFrame jniFrame
#else
  #define MAIN_THREAD_JNI_REF_GUARD
#endif

void NS_NotifyBeginPluginCall(NSPluginCallReentry aReentryState);
void NS_NotifyPluginCall(NSPluginCallReentry aReentryState);

#define NS_TRY_SAFE_CALL_RETURN(ret, fun, pluginInst, pluginCallReentry) \
PR_BEGIN_MACRO                                     \
  MAIN_THREAD_JNI_REF_GUARD;                       \
  NS_NotifyBeginPluginCall(pluginCallReentry); \
  ret = fun;                                       \
  NS_NotifyPluginCall(pluginCallReentry); \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_VOID(fun, pluginInst, pluginCallReentry) \
PR_BEGIN_MACRO                                     \
  MAIN_THREAD_JNI_REF_GUARD;                       \
  NS_NotifyBeginPluginCall(pluginCallReentry); \
  fun;                                             \
  NS_NotifyPluginCall(pluginCallReentry); \
PR_END_MACRO

#endif // nsNPAPIPluginInstance_h_
