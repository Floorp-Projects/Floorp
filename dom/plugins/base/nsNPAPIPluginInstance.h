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

  void NotifyStartedPlaying();
  void NotifyStoppedPlaying();

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

  nsresult CreateAudioChannelAgentIfNeeded();

  // The structure used to communicate between the plugin instance and
  // the browser.
  NPP_t mNPP;

  NPDrawingModel mDrawingModel;

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

void NS_NotifyBeginPluginCall(NSPluginCallReentry aReentryState);
void NS_NotifyPluginCall(NSPluginCallReentry aReentryState);

#define NS_TRY_SAFE_CALL_RETURN(ret, fun, pluginInst, pluginCallReentry) \
PR_BEGIN_MACRO                                     \
  NS_NotifyBeginPluginCall(pluginCallReentry); \
  ret = fun;                                       \
  NS_NotifyPluginCall(pluginCallReentry); \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_VOID(fun, pluginInst, pluginCallReentry) \
PR_BEGIN_MACRO                                     \
  NS_NotifyBeginPluginCall(pluginCallReentry); \
  fun;                                             \
  NS_NotifyPluginCall(pluginCallReentry); \
PR_END_MACRO

#endif // nsNPAPIPluginInstance_h_
