/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/Logging.h"
#include "nscore.h"
#include "prenv.h"

#include "nsNPAPIPluginInstance.h"
#include "nsPluginHost.h"
#include "nsPluginLogging.h"
#include "nsContentUtils.h"

#include "nsThreadUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsDirectoryServiceDefs.h"
#include "nsSize.h"
#include "nsNetCID.h"
#include "nsIContent.h"
#include "nsVersionComparator.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsILoadContext.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "AudioChannelService.h"
#include "mozilla/ProfilerLabels.h"

using namespace mozilla;
using namespace mozilla::dom;

using namespace mozilla;
using namespace mozilla::layers;

NS_IMPL_ISUPPORTS(nsNPAPIPluginInstance, nsIAudioChannelAgentCallback)

nsNPAPIPluginInstance::nsNPAPIPluginInstance()
    : mDrawingModel(kDefaultDrawingModel),
      mRunning(NOT_STARTED),
      mWindowless(false),
      mTransparent(false),
      mCached(false),
      mUsesDOMForCursor(false),
      mInPluginInitCall(false),
      mMIMEType(nullptr)
#ifdef XP_MACOSX
      ,
      mCurrentPluginEvent(nullptr)
#endif
      ,
      mCachedParamLength(0),
      mCachedParamNames(nullptr),
      mCachedParamValues(nullptr) {
  mNPP.pdata = nullptr;
  mNPP.ndata = this;

  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance ctor: this=%p\n", this));
}

nsNPAPIPluginInstance::~nsNPAPIPluginInstance() {
  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance dtor: this=%p\n", this));

  if (mMIMEType) {
    free(mMIMEType);
    mMIMEType = nullptr;
  }

  if (!mCachedParamValues || !mCachedParamNames) {
    return;
  }
  MOZ_ASSERT(mCachedParamValues && mCachedParamNames);

  for (uint32_t i = 0; i < mCachedParamLength; i++) {
    if (mCachedParamNames[i]) {
      free(mCachedParamNames[i]);
      mCachedParamNames[i] = nullptr;
    }
    if (mCachedParamValues[i]) {
      free(mCachedParamValues[i]);
      mCachedParamValues[i] = nullptr;
    }
  }

  free(mCachedParamNames);
  mCachedParamNames = nullptr;

  free(mCachedParamValues);
  mCachedParamValues = nullptr;
}

uint32_t nsNPAPIPluginInstance::gInUnsafePluginCalls = 0;

void nsNPAPIPluginInstance::Destroy() {
  Stop();
  mAudioChannelAgent = nullptr;
}

TimeStamp nsNPAPIPluginInstance::StopTime() { return mStopTime; }

nsresult nsNPAPIPluginInstance::Stop() { return NS_ERROR_FAILURE; }

already_AddRefed<nsPIDOMWindowOuter> nsNPAPIPluginInstance::GetDOMWindow() {
  return nullptr;
}

nsresult nsNPAPIPluginInstance::Start() { return NS_ERROR_FAILURE; }

nsresult nsNPAPIPluginInstance::SetWindow(NPWindow* window) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::Print(NPPrint* platformPrint) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::HandleEvent(
    void* event, int16_t* result, NSPluginCallReentry aSafeToReenterGecko) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::GetValueFromPlugin(NPPVariable variable,
                                                   void* value) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::GetNPP(NPP* aNPP) {
  if (aNPP)
    *aNPP = &mNPP;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}

NPError nsNPAPIPluginInstance::SetWindowless(bool aWindowless) {
  mWindowless = aWindowless;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetTransparent(bool aTransparent) {
  mTransparent = aTransparent;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetUsesDOMForCursor(bool aUsesDOMForCursor) {
  mUsesDOMForCursor = aUsesDOMForCursor;
  return NPERR_NO_ERROR;
}

bool nsNPAPIPluginInstance::UsesDOMForCursor() { return mUsesDOMForCursor; }

void nsNPAPIPluginInstance::SetDrawingModel(NPDrawingModel aModel) {
  mDrawingModel = aModel;
}

void nsNPAPIPluginInstance::RedrawPlugin() {}

#if defined(XP_MACOSX)
void nsNPAPIPluginInstance::SetEventModel(NPEventModel aModel) {}
#endif

nsresult nsNPAPIPluginInstance::GetDrawingModel(int32_t* aModel) {
  *aModel = (int32_t)mDrawingModel;
  return NS_OK;
}

nsresult nsNPAPIPluginInstance::IsRemoteDrawingCoreAnimation(bool* aDrawing) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::ContentsScaleFactorChanged(
    double aContentsScaleFactor) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::CSSZoomFactorChanged(float aCSSZoomFactor) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::GetJSObject(JSContext* cx,
                                            JSObject** outObject) {
  return NS_ERROR_FAILURE;
}

void nsNPAPIPluginInstance::SetCached(bool aCache) { mCached = aCache; }

bool nsNPAPIPluginInstance::ShouldCache() { return mCached; }

nsresult nsNPAPIPluginInstance::IsWindowless(bool* isWindowless) {
#if defined(XP_MACOSX)
  // All OS X plugins are windowless.
  *isWindowless = true;
#else
  *isWindowless = mWindowless;
#endif
  return NS_OK;
}

nsresult nsNPAPIPluginInstance::AsyncSetWindow(NPWindow* window) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::GetImageContainer(ImageContainer** aContainer) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::GetImageSize(nsIntSize* aSize) {
  return NS_ERROR_FAILURE;
}

#if defined(XP_WIN)
nsresult nsNPAPIPluginInstance::GetScrollCaptureContainer(
    ImageContainer** aContainer) {
  return NS_ERROR_FAILURE;
}
#endif

nsresult nsNPAPIPluginInstance::HandledWindowedPluginKeyEvent(
    const NativeEventData& aKeyEventData, bool aIsConsumed) {
  if (NS_WARN_IF(!mPlugin)) {
    return NS_ERROR_FAILURE;
  }

  PluginLibrary* library = mPlugin->GetLibrary();
  if (NS_WARN_IF(!library)) {
    return NS_ERROR_FAILURE;
  }
  return library->HandledWindowedPluginKeyEvent(&mNPP, aKeyEventData,
                                                aIsConsumed);
  return NS_ERROR_FAILURE;
}

void nsNPAPIPluginInstance::DidComposite() {}

nsresult nsNPAPIPluginInstance::NotifyPainted(void) {
  MOZ_ASSERT_UNREACHABLE("Dead code, shouldn't be called.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsNPAPIPluginInstance::GetIsOOP(bool* aIsAsync) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::SetBackgroundUnknown() {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::BeginUpdateBackground(
    nsIntRect* aRect, DrawTarget** aDrawTarget) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::EndUpdateBackground(nsIntRect* aRect) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::IsTransparent(bool* isTransparent) {
  *isTransparent = mTransparent;
  return NS_OK;
}

nsresult nsNPAPIPluginInstance::GetFormValue(nsAString& aValue) {
  aValue.Truncate();

  char* value = nullptr;
  nsresult rv = GetValueFromPlugin(NPPVformValue, &value);
  if (NS_FAILED(rv) || !value) return NS_ERROR_FAILURE;

  CopyUTF8toUTF16(MakeStringSpan(value), aValue);

  // NPPVformValue allocates with NPN_MemAlloc(), which uses
  // nsMemory.
  free(value);

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::PushPopupsEnabledState(bool aEnabled) {
  nsCOMPtr<nsPIDOMWindowOuter> window = GetDOMWindow();
  if (!window) return NS_ERROR_FAILURE;

  PopupBlocker::PopupControlState oldState =
      PopupBlocker::PushPopupControlState(
          aEnabled ? PopupBlocker::openAllowed : PopupBlocker::openAbused,
          true);

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  mPopupStates.AppendElement(oldState);

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::PopPopupsEnabledState() {
  if (mPopupStates.IsEmpty()) {
    // Nothing to pop.
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = GetDOMWindow();
  if (!window) return NS_ERROR_FAILURE;

  PopupBlocker::PopPopupControlState(mPopupStates.PopLastElement());

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::GetPluginAPIVersion(uint16_t* version) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::PrivateModeStateChanged(bool enabled) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::IsPrivateBrowsing(bool* aEnabled) {
  return NS_ERROR_FAILURE;
}

static void PluginTimerCallback(nsITimer* aTimer, void* aClosure) {
  nsNPAPITimer* t = (nsNPAPITimer*)aClosure;
  NPP npp = t->npp;
  uint32_t id = t->id;

  PLUGIN_LOG(PLUGIN_LOG_NOISY,
             ("nsNPAPIPluginInstance running plugin timer callback this=%p\n",
              npp->ndata));

  // Some plugins (Flash on Android) calls unscheduletimer
  // from this callback.
  t->inCallback = true;
  (*(t->callback))(npp, id);
  t->inCallback = false;

  // Make sure we still have an instance and the timer is still alive
  // after the callback.
  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*)npp->ndata;
  if (!inst || !inst->TimerWithID(id, nullptr)) return;

  // use UnscheduleTimer to clean up if this is a one-shot timer
  uint32_t timerType;
  t->timer->GetType(&timerType);
  if (t->needUnschedule || timerType == nsITimer::TYPE_ONE_SHOT)
    inst->UnscheduleTimer(id);
}

nsNPAPITimer* nsNPAPIPluginInstance::TimerWithID(uint32_t id, uint32_t* index) {
  uint32_t len = mTimers.Length();
  for (uint32_t i = 0; i < len; i++) {
    if (mTimers[i]->id == id) {
      if (index) *index = i;
      return mTimers[i];
    }
  }
  return nullptr;
}

uint32_t nsNPAPIPluginInstance::ScheduleTimer(
    uint32_t interval, NPBool repeat,
    void (*timerFunc)(NPP npp, uint32_t timerID)) {
  if (RUNNING != mRunning) return 0;

  nsNPAPITimer* newTimer = new nsNPAPITimer();

  newTimer->inCallback = newTimer->needUnschedule = false;
  newTimer->npp = &mNPP;

  // generate ID that is unique to this instance
  uint32_t uniqueID = mTimers.Length();
  while ((uniqueID == 0) || TimerWithID(uniqueID, nullptr)) uniqueID++;
  newTimer->id = uniqueID;

  // create new xpcom timer, scheduled correctly
  nsresult rv;
  const short timerType = (repeat ? (short)nsITimer::TYPE_REPEATING_SLACK
                                  : (short)nsITimer::TYPE_ONE_SHOT);
  rv = NS_NewTimerWithFuncCallback(
      getter_AddRefs(newTimer->timer), PluginTimerCallback, newTimer, interval,
      timerType, "nsNPAPIPluginInstance::ScheduleTimer");
  if (NS_FAILED(rv)) {
    delete newTimer;
    return 0;
  }

  // save callback function
  newTimer->callback = timerFunc;

  // add timer to timers array
  mTimers.AppendElement(newTimer);

  return newTimer->id;
}

void nsNPAPIPluginInstance::UnscheduleTimer(uint32_t timerID) {
  // find the timer struct by ID
  uint32_t index;
  nsNPAPITimer* t = TimerWithID(timerID, &index);
  if (!t) return;

  if (t->inCallback) {
    t->needUnschedule = true;
    return;
  }

  // cancel the timer
  t->timer->Cancel();

  // remove timer struct from array
  mTimers.RemoveElementAt(index);

  // delete timer
  delete t;
}

NPBool nsNPAPIPluginInstance::ConvertPoint(double sourceX, double sourceY,
                                           NPCoordinateSpace sourceSpace,
                                           double* destX, double* destY,
                                           NPCoordinateSpace destSpace) {
  return false;
}

nsresult nsNPAPIPluginInstance::GetDOMElement(Element** result) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::InvalidateRect(NPRect* invalidRect) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::InvalidateRegion(NPRegion invalidRegion) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::GetMIMEType(const char** result) {
  if (!mMIMEType)
    *result = "";
  else
    *result = mMIMEType;

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::AsyncSetWindow(NPWindow& window) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsNPAPIPluginInstance::URLRedirectResponse(void* notifyData,
                                                NPBool allow) {}

NPError nsNPAPIPluginInstance::InitAsyncSurface(NPSize* size,
                                                NPImageFormat format,
                                                void* initData,
                                                NPAsyncSurface* surface) {
  return NPERR_GENERIC_ERROR;
}

NPError nsNPAPIPluginInstance::FinalizeAsyncSurface(NPAsyncSurface* surface) {
  return NPERR_GENERIC_ERROR;
}

void nsNPAPIPluginInstance::SetCurrentAsyncSurface(NPAsyncSurface* surface,
                                                   NPRect* changed) {}

double nsNPAPIPluginInstance::GetContentsScaleFactor() { return -1.0; }

float nsNPAPIPluginInstance::GetCSSZoomFactor() { return -1.0f; }

nsresult nsNPAPIPluginInstance::GetRunID(uint32_t* aRunID) {
  return NS_ERROR_FAILURE;
}

nsresult nsNPAPIPluginInstance::CreateAudioChannelAgentIfNeeded() {
  if (mAudioChannelAgent) {
    return NS_OK;
  }

  mAudioChannelAgent = new AudioChannelAgent();

  nsCOMPtr<nsPIDOMWindowOuter> window = GetDOMWindow();
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mAudioChannelAgent->Init(window->GetCurrentInnerWindow(), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

void nsNPAPIPluginInstance::NotifyStartedPlaying() {
  nsresult rv = CreateAudioChannelAgentIfNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MOZ_ASSERT(mAudioChannelAgent);
  rv = mAudioChannelAgent->NotifyStartedPlaying(
      mIsMuted ? AudioChannelService::AudibleState::eNotAudible
               : AudioChannelService::AudibleState::eAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mAudioChannelAgent->PullInitialUpdate();
}

void nsNPAPIPluginInstance::NotifyStoppedPlaying() {
  MOZ_ASSERT(mAudioChannelAgent);
  nsresult rv = mAudioChannelAgent->NotifyStoppedPlaying();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

NS_IMETHODIMP
nsNPAPIPluginInstance::WindowVolumeChanged(float aVolume, bool aMuted) {
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("nsNPAPIPluginInstance, WindowVolumeChanged, "
           "this = %p, aVolume = %f, aMuted = %s\n",
           this, aVolume, aMuted ? "true" : "false"));
  // We just support mute/unmute
  if (mWindowMuted != aMuted) {
    mWindowMuted = aMuted;
    return UpdateMutedIfNeeded();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::WindowSuspendChanged(nsSuspendedTypes aSuspend) {
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("nsNPAPIPluginInstance, WindowSuspendChanged, "
           "this = %p, aSuspend = %s\n",
           this, SuspendTypeToStr(aSuspend)));
  const bool isSuspended = aSuspend != nsISuspendedTypes::NONE_SUSPENDED;
  if (mWindowSuspended != isSuspended) {
    mWindowSuspended = isSuspended;
    // It doesn't support suspending, so we just do something like mute/unmute.
    return UpdateMutedIfNeeded();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::WindowAudioCaptureChanged(bool aCapture) {
  return NS_OK;
}

void nsNPAPIPluginInstance::NotifyAudibleStateChanged() const {
  // This happens when global window destroyed, we would notify agent's callback
  // to mute its volume, but the nsNSNPAPI had released the agent before that.
  if (!mAudioChannelAgent) {
    return;
  }
  AudioChannelService::AudibleState audibleState =
      mIsMuted ? AudioChannelService::AudibleState::eNotAudible
               : AudioChannelService::AudibleState::eAudible;
  // Because we don't really support suspending nsNPAPI, so all audible changes
  // come from changing its volume.
  mAudioChannelAgent->NotifyStartedAudible(
      audibleState, AudioChannelService::AudibleChangedReasons::eVolumeChanged);
}

nsresult nsNPAPIPluginInstance::UpdateMutedIfNeeded() {
  const bool shouldMute = mWindowSuspended || mWindowMuted;
  if (mIsMuted == shouldMute) {
    return NS_OK;
  }

  mIsMuted = shouldMute;
  NotifyAudibleStateChanged();
  nsresult rv = SetMuted(mIsMuted);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetMuted failed");
  return rv;
}

nsresult nsNPAPIPluginInstance::SetMuted(bool aIsMuted) {
  return NS_ERROR_FAILURE;
}
