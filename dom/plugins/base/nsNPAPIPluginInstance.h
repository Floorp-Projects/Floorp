/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNPAPIPluginInstance_h_
#define nsNPAPIPluginInstance_h_

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsPIDOMWindow.h"
#include "nsITimer.h"
#include "nsIPluginTagInfo.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsInterfaceHashtable.h"
#include "nsHashKeys.h"
#ifdef MOZ_WIDGET_ANDROID
#include "nsIRunnable.h"
class PluginEventRunnable;
#endif

#include "mozilla/TimeStamp.h"
#include "mozilla/PluginLibrary.h"

struct JSObject;

class nsPluginStreamListenerPeer; // browser-initiated stream class
class nsNPAPIPluginStreamListener; // plugin-initiated stream class
class nsIPluginInstanceOwner;
class nsIOutputStream;

#if defined(OS_WIN)
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelSyncWin;
#elif defined(MOZ_X11)
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelSyncX;
#elif defined(XP_MACOSX)
#ifndef NP_NO_QUICKDRAW
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelQuickDraw;
#else
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelCoreGraphics;
#endif
#else
const NPDrawingModel kDefaultDrawingModel = static_cast<NPDrawingModel>(0);
#endif

class nsNPAPITimer
{
public:
  NPP npp;
  uint32_t id;
  nsCOMPtr<nsITimer> timer;
  void (*callback)(NPP npp, uint32_t timerID);
  bool inCallback;
};

class nsNPAPIPluginInstance : public nsISupports
{
private:
  typedef mozilla::PluginLibrary PluginLibrary;

public:
  NS_DECL_ISUPPORTS

  nsresult Initialize(nsNPAPIPlugin *aPlugin, nsIPluginInstanceOwner* aOwner, const char* aMIMEType);
  nsresult Start();
  nsresult Stop();
  nsresult SetWindow(NPWindow* window);
  nsresult NewStreamFromPlugin(const char* type, const char* target, nsIOutputStream* *result);
  nsresult Print(NPPrint* platformPrint);
  nsresult HandleEvent(void* event, PRInt16* result);
  nsresult GetValueFromPlugin(NPPVariable variable, void* value);
  nsresult GetDrawingModel(PRInt32* aModel);
  nsresult IsRemoteDrawingCoreAnimation(bool* aDrawing);
  nsresult GetJSObject(JSContext *cx, JSObject** outObject);
  bool ShouldCache();
  nsresult IsWindowless(bool* isWindowless);
  nsresult AsyncSetWindow(NPWindow* window);
  nsresult GetImageContainer(ImageContainer **aContainer);
  nsresult GetImageSize(nsIntSize* aSize);
  nsresult NotifyPainted(void);
  nsresult UseAsyncPainting(bool* aIsAsync);
  nsresult SetBackgroundUnknown();
  nsresult BeginUpdateBackground(nsIntRect* aRect, gfxContext** aContext);
  nsresult EndUpdateBackground(gfxContext* aContext, nsIntRect* aRect);
  nsresult IsTransparent(bool* isTransparent);
  nsresult GetFormValue(nsAString& aValue);
  nsresult PushPopupsEnabledState(bool aEnabled);
  nsresult PopPopupsEnabledState();
  nsresult GetPluginAPIVersion(PRUint16* version);
  nsresult InvalidateRect(NPRect *invalidRect);
  nsresult InvalidateRegion(NPRegion invalidRegion);
  nsresult GetMIMEType(const char* *result);
  nsresult GetJSContext(JSContext* *outContext);
  nsresult GetOwner(nsIPluginInstanceOwner **aOwner);
  nsresult SetOwner(nsIPluginInstanceOwner *aOwner);
  nsresult ShowStatus(const char* message);
  nsresult InvalidateOwner();
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
  nsresult HandleGUIEvent(const nsGUIEvent& anEvent, bool* handled);
#endif

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
#endif

#ifdef MOZ_WIDGET_ANDROID
  void NotifyForeground(bool aForeground);
  void NotifyOnScreen(bool aOnScreen);
  void MemoryPressure();
  void NotifyFullScreen(bool aFullScreen);

  bool IsOnScreen() {
    return mOnScreen;
  }

  PRUint32 GetANPDrawingModel() { return mANPDrawingModel; }
  void SetANPDrawingModel(PRUint32 aModel);

  void* GetJavaSurface();

  void PostEvent(void* event);

  // These are really mozilla::dom::ScreenOrientation, but it's
  // difficult to include that here
  PRUint32 FullScreenOrientation() { return mFullScreenOrientation; }
  void SetFullScreenOrientation(PRUint32 orientation);

  void SetWakeLock(bool aLock);
#endif

  nsresult NewStreamListener(const char* aURL, void* notifyData,
                             nsNPAPIPluginStreamListener** listener);

  nsNPAPIPluginInstance();
  virtual ~nsNPAPIPluginInstance();

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
  nsresult SetCached(bool aCache);

  already_AddRefed<nsPIDOMWindow> GetDOMWindow();

  nsresult PrivateModeStateChanged(bool aEnabled);

  nsresult GetDOMElement(nsIDOMElement* *result);

  nsNPAPITimer* TimerWithID(uint32_t id, PRUint32* index);
  uint32_t      ScheduleTimer(uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID));
  void          UnscheduleTimer(uint32_t timerID);
  NPError       PopUpContextMenu(NPMenu* menu);
  NPBool        ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);


  nsTArray<nsNPAPIPluginStreamListener*> *StreamListeners();

  nsTArray<nsPluginStreamListenerPeer*> *FileCachedStreamListeners();

  nsresult AsyncSetWindow(NPWindow& window);

  void URLRedirectResponse(void* notifyData, NPBool allow);

  NPError InitAsyncSurface(NPSize *size, NPImageFormat format,
                           void *initData, NPAsyncSurface *surface);
  NPError FinalizeAsyncSurface(NPAsyncSurface *surface);
  void SetCurrentAsyncSurface(NPAsyncSurface *surface, NPRect *changed);

  // Called when the instance fails to instantiate beceause the Carbon
  // event model is not supported.
  void CarbonNPAPIFailure();

protected:

  nsresult GetTagType(nsPluginTagType *result);
  nsresult GetAttributes(PRUint16& n, const char*const*& names,
                         const char*const*& values);
  nsresult GetParameters(PRUint16& n, const char*const*& names,
                         const char*const*& values);
  nsresult GetMode(PRInt32 *result);

  // The structure used to communicate between the plugin instance and
  // the browser.
  NPP_t mNPP;

  NPDrawingModel mDrawingModel;

#ifdef MOZ_WIDGET_ANDROID
  PRUint32 mANPDrawingModel;
  nsCOMPtr<nsIRunnable> mSurfaceGetter;

  friend class PluginEventRunnable;

  nsTArray<nsCOMPtr<PluginEventRunnable>> mPostedEvents;
  void PopPostedEvent(PluginEventRunnable* r);

  PRUint32 mFullScreenOrientation;
  bool mWakeLocked;
  bool mFullScreen;
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
  nsNPAPIPlugin* mPlugin;

  nsTArray<nsNPAPIPluginStreamListener*> mStreamListeners;

  nsTArray<nsPluginStreamListenerPeer*> mFileCachedStreamListeners;

  nsTArray<PopupControlState> mPopupStates;

  char* mMIMEType;

  // Weak pointer to the owner. The owner nulls this out (by calling
  // InvalidateOwner()) when it's no longer our owner.
  nsIPluginInstanceOwner *mOwner;

  nsTArray<nsNPAPITimer*> mTimers;

  // non-null during a HandleEvent call
  void* mCurrentPluginEvent;

  // Timestamp for the last time this plugin was stopped.
  // This is only valid when the plugin is actually stopped!
  mozilla::TimeStamp mStopTime;

  bool mUsePluginLayersPref;
#ifdef MOZ_WIDGET_ANDROID
  void* mSurface;
  bool mOnScreen;
#endif
};

#endif // nsNPAPIPluginInstance_h_
