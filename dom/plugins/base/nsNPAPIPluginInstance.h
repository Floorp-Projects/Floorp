/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#endif

#include "mozilla/TimeStamp.h"
#include "mozilla/PluginLibrary.h"

struct JSObject;

class nsPluginStreamListenerPeer; // browser-initiated stream class
class nsNPAPIPluginStreamListener; // plugin-initiated stream class
class nsIPluginInstanceOwner;
class nsIPluginStreamListener;
class nsIOutputStream;

#if defined(OS_WIN)
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelSyncWin;
#elif defined(MOZ_X11)
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelSyncX;
#else
#ifndef NP_NO_QUICKDRAW
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelQuickDraw;
#else
const NPDrawingModel kDefaultDrawingModel = NPDrawingModelCoreGraphics;
#endif
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

  nsresult Initialize(nsIPluginInstanceOwner* aOwner, const char* aMIMEType);
  nsresult Start();
  nsresult Stop();
  nsresult SetWindow(NPWindow* window);
  nsresult NewStreamToPlugin(nsIPluginStreamListener** listener);
  nsresult NewStreamFromPlugin(const char* type, const char* target, nsIOutputStream* *result);
  nsresult Print(NPPrint* platformPrint);
#ifdef MOZ_WIDGET_ANDROID
  nsresult PostEvent(void* event) { return 0; };
#endif
  nsresult HandleEvent(void* event, PRInt16* result);
  nsresult GetValueFromPlugin(NPPVariable variable, void* value);
  nsresult GetDrawingModel(PRInt32* aModel);
  nsresult IsRemoteDrawingCoreAnimation(bool* aDrawing);
  nsresult GetJSObject(JSContext *cx, JSObject** outObject);
  nsresult DefineJavaProperties();
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

  void SetURI(nsIURI* uri);
  nsIURI* GetURI();

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

  bool IsOnScreen() {
    return mOnScreen;
  }

  PRUint32 GetANPDrawingModel() { return mANPDrawingModel; }
  void SetANPDrawingModel(PRUint32 aModel);

  // This stuff is for kSurface_ANPDrawingModel
  void* GetJavaSurface();
  void SetJavaSurface(void* aSurface);
  void RequestJavaSurface();
#endif

  nsresult NewStreamListener(const char* aURL, void* notifyData,
                             nsIPluginStreamListener** listener);

  nsNPAPIPluginInstance(nsNPAPIPlugin* plugin);
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

  nsresult PrivateModeStateChanged();

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
  nsresult InitializePlugin();

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

  nsCOMPtr<nsIURI> mURI;

  bool mUsePluginLayersPref;
#ifdef MOZ_WIDGET_ANDROID
  void* mSurface;
  bool mOnScreen;
#endif
};

#endif // nsNPAPIPluginInstance_h_
