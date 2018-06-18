/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/Logging.h"
#include "nscore.h"
#include "prenv.h"

#include "nsNPAPIPluginInstance.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsPluginHost.h"
#include "nsPluginLogging.h"
#include "nsContentUtils.h"
#include "nsPluginInstanceOwner.h"

#include "nsThreadUtils.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsDirectoryServiceDefs.h"
#include "nsJSNPRuntime.h"
#include "nsPluginStreamListenerPeer.h"
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

using namespace mozilla;
using namespace mozilla::dom;

using namespace mozilla;
using namespace mozilla::plugins::parent;
using namespace mozilla::layers;

NS_IMPL_ISUPPORTS(nsNPAPIPluginInstance, nsIAudioChannelAgentCallback)

nsNPAPIPluginInstance::nsNPAPIPluginInstance()
  : mDrawingModel(kDefaultDrawingModel)
  , mRunning(NOT_STARTED)
  , mWindowless(false)
  , mTransparent(false)
  , mCached(false)
  , mUsesDOMForCursor(false)
  , mInPluginInitCall(false)
  , mPlugin(nullptr)
  , mMIMEType(nullptr)
  , mOwner(nullptr)
#ifdef XP_MACOSX
  , mCurrentPluginEvent(nullptr)
#endif
  , mCachedParamLength(0)
  , mCachedParamNames(nullptr)
  , mCachedParamValues(nullptr)
  , mMuted(false)
{
  mNPP.pdata = nullptr;
  mNPP.ndata = this;

  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance ctor: this=%p\n",this));
}

nsNPAPIPluginInstance::~nsNPAPIPluginInstance()
{
  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance dtor: this=%p\n",this));

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

void
nsNPAPIPluginInstance::Destroy()
{
  Stop();
  mPlugin = nullptr;
  mAudioChannelAgent = nullptr;
}

TimeStamp
nsNPAPIPluginInstance::StopTime()
{
  return mStopTime;
}

nsresult nsNPAPIPluginInstance::Initialize(nsNPAPIPlugin *aPlugin, nsPluginInstanceOwner* aOwner, const nsACString& aMIMEType)
{
  AUTO_PROFILER_LABEL("nsNPAPIPlugin::Initialize", OTHER);
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Initialize this=%p\n",this));

  NS_ENSURE_ARG_POINTER(aPlugin);
  NS_ENSURE_ARG_POINTER(aOwner);

  mPlugin = aPlugin;
  mOwner = aOwner;

  if (!aMIMEType.IsEmpty()) {
    mMIMEType = ToNewCString(aMIMEType);
  }

  return Start();
}

nsresult nsNPAPIPluginInstance::Stop()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Stop this=%p\n",this));

  // Make sure the plugin didn't leave popups enabled.
  if (mPopupStates.Length() > 0) {
    nsCOMPtr<nsPIDOMWindowOuter> window = GetDOMWindow();

    if (window) {
      window->PopPopupControlState(openAbused);
    }
  }

  if (RUNNING != mRunning) {
    return NS_OK;
  }

  // clean up all outstanding timers
  for (uint32_t i = mTimers.Length(); i > 0; i--)
    UnscheduleTimer(mTimers[i - 1]->id);

  // If there's code from this plugin instance on the stack, delay the
  // destroy.
  if (PluginDestructionGuard::DelayDestroy(this)) {
    return NS_OK;
  }

  mRunning = DESTROYING;
  mStopTime = TimeStamp::Now();

  // clean up open streams
  while (mStreamListeners.Length() > 0) {
    RefPtr<nsNPAPIPluginStreamListener> currentListener(mStreamListeners[0]);
    currentListener->CleanUpStream(NPRES_USER_BREAK);
    mStreamListeners.RemoveElement(currentListener);
  }

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  NPError error = NPERR_GENERIC_ERROR;
  if (pluginFunctions->destroy) {
    NPSavedData *sdata = 0;

    NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->destroy)(&mNPP, &sdata), this,
                            NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                   ("NPP Destroy called: this=%p, npp=%p, return=%d\n", this, &mNPP, error));
  }
  mRunning = DESTROYED;

  nsJSNPRuntime::OnPluginDestroy(&mNPP);

  if (error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

already_AddRefed<nsPIDOMWindowOuter>
nsNPAPIPluginInstance::GetDOMWindow()
{
  if (!mOwner)
    return nullptr;

  RefPtr<nsPluginInstanceOwner> kungFuDeathGrip(mOwner);

  nsCOMPtr<nsIDocument> doc;
  kungFuDeathGrip->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return nullptr;

  RefPtr<nsPIDOMWindowOuter> window = doc->GetWindow();

  return window.forget();
}

nsresult
nsNPAPIPluginInstance::GetTagType(nsPluginTagType *result)
{
  if (!mOwner) {
    return NS_ERROR_FAILURE;
  }

  return mOwner->GetTagType(result);
}

nsTArray<nsNPAPIPluginStreamListener*>*
nsNPAPIPluginInstance::StreamListeners()
{
  return &mStreamListeners;
}

nsTArray<nsPluginStreamListenerPeer*>*
nsNPAPIPluginInstance::FileCachedStreamListeners()
{
  return &mFileCachedStreamListeners;
}

nsresult
nsNPAPIPluginInstance::Start()
{
  if (mRunning == RUNNING) {
    return NS_OK;
  }

  if (!mOwner) {
    MOZ_ASSERT(false, "Should not be calling Start() on unowned plugin.");
    return NS_ERROR_FAILURE;
  }

  PluginDestructionGuard guard(this);

  nsTArray<MozPluginParameter> attributes;
  nsTArray<MozPluginParameter> params;

  nsPluginTagType tagtype;
  nsresult rv = GetTagType(&tagtype);
  if (NS_SUCCEEDED(rv)) {
    mOwner->GetAttributes(attributes);
    mOwner->GetParameters(params);
  } else {
    MOZ_ASSERT(false, "Failed to get tag type.");
  }

  mCachedParamLength = attributes.Length() + 1 + params.Length();

  // We add an extra entry "PARAM" as a separator between the attribute
  // and param values, but we don't count it if there are no <param> entries.
  // Legacy behavior quirk.
  uint32_t quirkParamLength = params.Length() ?
                                mCachedParamLength : attributes.Length();

  mCachedParamNames = (char**)moz_xmalloc(sizeof(char*) * mCachedParamLength);
  mCachedParamValues = (char**)moz_xmalloc(sizeof(char*) * mCachedParamLength);

  for (uint32_t i = 0; i < attributes.Length(); i++) {
    mCachedParamNames[i] = ToNewUTF8String(attributes[i].mName);
    mCachedParamValues[i] = ToNewUTF8String(attributes[i].mValue);
  }

  mCachedParamNames[attributes.Length()] = ToNewUTF8String(NS_LITERAL_STRING("PARAM"));
  mCachedParamValues[attributes.Length()] = nullptr;

  for (uint32_t i = 0, pos = attributes.Length() + 1; i < params.Length(); i ++) {
    mCachedParamNames[pos] = ToNewUTF8String(params[i].mName);
    mCachedParamValues[pos] = ToNewUTF8String(params[i].mValue);
    pos++;
  }

  const char*   mimetype;
  NPError       error = NPERR_GENERIC_ERROR;

  GetMIMEType(&mimetype);

  bool oldVal = mInPluginInitCall;
  mInPluginInitCall = true;

  // Need this on the stack before calling NPP_New otherwise some callbacks that
  // the plugin may make could fail (NPN_HasProperty, for example).
  NPPAutoPusher autopush(&mNPP);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  // Mark this instance as running before calling NPP_New because the plugin may
  // call other NPAPI functions, like NPN_GetURLNotify, that assume this is set
  // before returning. If the plugin returns failure, we'll clear it out below.
  mRunning = RUNNING;

  nsresult newResult = library->NPP_New((char*)mimetype, &mNPP,
                                        quirkParamLength, mCachedParamNames,
                                        mCachedParamValues, nullptr, &error);
  mInPluginInitCall = oldVal;

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP New called: this=%p, npp=%p, mime=%s, argc=%d, return=%d\n",
  this, &mNPP, mimetype, quirkParamLength, error));

  if (NS_FAILED(newResult) || error != NPERR_NO_ERROR) {
    mRunning = DESTROYED;
    nsJSNPRuntime::OnPluginDestroy(&mNPP);
    return NS_ERROR_FAILURE;
  }

  return newResult;
}

nsresult nsNPAPIPluginInstance::SetWindow(NPWindow* window)
{
  // NPAPI plugins don't want a SetWindow(nullptr).
  if (!window || RUNNING != mRunning)
    return NS_OK;

#if MOZ_WIDGET_GTK
  // bug 108347, flash plugin on linux doesn't like window->width <= 0
  return NS_OK;
#endif

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  if (pluginFunctions->setwindow) {
    PluginDestructionGuard guard(this);

    // XXX Turns out that NPPluginWindow and NPWindow are structurally
    // identical (on purpose!), so there's no need to make a copy.

    PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::SetWindow (about to call it) this=%p\n",this));

    bool oldVal = mInPluginInitCall;
    mInPluginInitCall = true;

    NPPAutoPusher nppPusher(&mNPP);

    NPError error;
    NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setwindow)(&mNPP, (NPWindow*)window), this,
                            NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);
    // 'error' is only used if this is a logging-enabled build.
    // That is somewhat complex to check, so we just use "unused"
    // to suppress any compiler warnings in build configurations
    // where the logging is a no-op.
    mozilla::Unused << error;

    mInPluginInitCall = oldVal;

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP SetWindow called: this=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d], return=%d\n",
    this, window->x, window->y, window->width, window->height,
    window->clipRect.top, window->clipRect.bottom, window->clipRect.left, window->clipRect.right, error));
  }
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::NewStreamListener(const char* aURL, void* notifyData,
                                         nsNPAPIPluginStreamListener** listener)
{
  RefPtr<nsNPAPIPluginStreamListener> sl = new nsNPAPIPluginStreamListener(this, notifyData, aURL);

  mStreamListeners.AppendElement(sl);

  sl.forget(listener);

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::Print(NPPrint* platformPrint)
{
  NS_ENSURE_TRUE(platformPrint, NS_ERROR_NULL_POINTER);

  PluginDestructionGuard guard(this);

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  NPPrint* thePrint = (NPPrint *)platformPrint;

  // to be compatible with the older SDK versions and to match what
  // NPAPI and other browsers do, overwrite |window.type| field with one
  // more copy of |platformPrint|. See bug 113264
  uint16_t sdkmajorversion = (pluginFunctions->version & 0xff00)>>8;
  uint16_t sdkminorversion = pluginFunctions->version & 0x00ff;
  if ((sdkmajorversion == 0) && (sdkminorversion < 11)) {
    // Let's copy platformPrint bytes over to where it was supposed to be
    // in older versions -- four bytes towards the beginning of the struct
    // but we should be careful about possible misalignments
    if (sizeof(NPWindowType) >= sizeof(void *)) {
      void* source = thePrint->print.embedPrint.platformPrint;
      void** destination = (void **)&(thePrint->print.embedPrint.window.type);
      *destination = source;
    } else {
      NS_ERROR("Incompatible OS for assignment");
    }
  }

  if (pluginFunctions->print)
      NS_TRY_SAFE_CALL_VOID((*pluginFunctions->print)(&mNPP, thePrint), this,
                            NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP PrintProc called: this=%p, pDC=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d]\n",
  this,
  platformPrint->print.embedPrint.platformPrint,
  platformPrint->print.embedPrint.window.x,
  platformPrint->print.embedPrint.window.y,
  platformPrint->print.embedPrint.window.width,
  platformPrint->print.embedPrint.window.height,
  platformPrint->print.embedPrint.window.clipRect.top,
  platformPrint->print.embedPrint.window.clipRect.bottom,
  platformPrint->print.embedPrint.window.clipRect.left,
  platformPrint->print.embedPrint.window.clipRect.right));

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::HandleEvent(void* event, int16_t* result,
                                            NSPluginCallReentry aSafeToReenterGecko)
{
  if (RUNNING != mRunning)
    return NS_OK;

  AUTO_PROFILER_LABEL("nsNPAPIPluginInstance::HandleEvent", OTHER);

  if (!event)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  int16_t tmpResult = kNPEventNotHandled;

  if (pluginFunctions->event) {
#ifdef XP_MACOSX
    mCurrentPluginEvent = event;
#endif
#if defined(XP_WIN)
    NS_TRY_SAFE_CALL_RETURN(tmpResult, (*pluginFunctions->event)(&mNPP, event), this,
                            aSafeToReenterGecko);
#else
    tmpResult = (*pluginFunctions->event)(&mNPP, event);
#endif
    NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP HandleEvent called: this=%p, npp=%p, event=%p, return=%d\n",
      this, &mNPP, event, tmpResult));

    if (result)
      *result = tmpResult;
#ifdef XP_MACOSX
    mCurrentPluginEvent = nullptr;
#endif
  }

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::GetValueFromPlugin(NPPVariable variable, void* value)
{
  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  nsresult rv = NS_ERROR_FAILURE;

  if (pluginFunctions->getvalue && RUNNING == mRunning) {
    PluginDestructionGuard guard(this);

    NPError pluginError = NPERR_GENERIC_ERROR;
    NS_TRY_SAFE_CALL_RETURN(pluginError, (*pluginFunctions->getvalue)(&mNPP, variable, value), this,
                            NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);
    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP GetValue called: this=%p, npp=%p, var=%d, value=%p, return=%d\n",
    this, &mNPP, variable, value, pluginError));

    if (pluginError == NPERR_NO_ERROR) {
      rv = NS_OK;
    }
  }

  return rv;
}

nsNPAPIPlugin* nsNPAPIPluginInstance::GetPlugin()
{
  return mPlugin;
}

nsresult nsNPAPIPluginInstance::GetNPP(NPP* aNPP)
{
  if (aNPP)
    *aNPP = &mNPP;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}

NPError nsNPAPIPluginInstance::SetWindowless(bool aWindowless)
{
  mWindowless = aWindowless;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetTransparent(bool aTransparent)
{
  mTransparent = aTransparent;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetUsesDOMForCursor(bool aUsesDOMForCursor)
{
  mUsesDOMForCursor = aUsesDOMForCursor;
  return NPERR_NO_ERROR;
}

bool
nsNPAPIPluginInstance::UsesDOMForCursor()
{
  return mUsesDOMForCursor;
}

void nsNPAPIPluginInstance::SetDrawingModel(NPDrawingModel aModel)
{
  mDrawingModel = aModel;
}

void nsNPAPIPluginInstance::RedrawPlugin()
{
  mOwner->RedrawPlugin();
}

#if defined(XP_MACOSX)
void nsNPAPIPluginInstance::SetEventModel(NPEventModel aModel)
{
  // the event model needs to be set for the object frame immediately
  if (!mOwner) {
    NS_WARNING("Trying to set event model without a plugin instance owner!");
    return;
  }

  mOwner->SetEventModel(aModel);
}
#endif

nsresult nsNPAPIPluginInstance::GetDrawingModel(int32_t* aModel)
{
  *aModel = (int32_t)mDrawingModel;
  return NS_OK;
}

nsresult nsNPAPIPluginInstance::IsRemoteDrawingCoreAnimation(bool* aDrawing)
{
#ifdef XP_MACOSX
  if (!mPlugin)
      return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
      return NS_ERROR_FAILURE;

  return library->IsRemoteDrawingCoreAnimation(&mNPP, aDrawing);
#else
  return NS_ERROR_FAILURE;
#endif
}

nsresult
nsNPAPIPluginInstance::ContentsScaleFactorChanged(double aContentsScaleFactor)
{
#if defined(XP_MACOSX) || defined(XP_WIN)
  if (!mPlugin)
      return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
      return NS_ERROR_FAILURE;

  // We only need to call this if the plugin is running OOP.
  if (!library->IsOOP())
      return NS_OK;

  return library->ContentsScaleFactorChanged(&mNPP, aContentsScaleFactor);
#else
  return NS_ERROR_FAILURE;
#endif
}

nsresult
nsNPAPIPluginInstance::CSSZoomFactorChanged(float aCSSZoomFactor)
{
  if (RUNNING != mRunning)
    return NS_OK;

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance informing plugin of CSS Zoom Factor change this=%p\n",this));

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  if (!pluginFunctions->setvalue)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  NPError error;
  double value = static_cast<double>(aCSSZoomFactor);
  NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setvalue)(&mNPP, NPNVCSSZoomFactor, &value), this,
                          NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);
  return (error == NPERR_NO_ERROR) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetJSObject(JSContext *cx, JSObject** outObject)
{
  NPObject *npobj = nullptr;
  nsresult rv = GetValueFromPlugin(NPPVpluginScriptableNPObject, &npobj);
  if (NS_FAILED(rv) || !npobj)
    return NS_ERROR_FAILURE;

  *outObject = nsNPObjWrapper::GetNewOrUsed(&mNPP, cx, npobj);

  _releaseobject(npobj);

  return NS_OK;
}

void
nsNPAPIPluginInstance::SetCached(bool aCache)
{
  mCached = aCache;
}

bool
nsNPAPIPluginInstance::ShouldCache()
{
  return mCached;
}

nsresult
nsNPAPIPluginInstance::IsWindowless(bool* isWindowless)
{
#if defined(XP_MACOSX)
  // All OS X plugins are windowless.
  *isWindowless = true;
#else
  *isWindowless = mWindowless;
#endif
  return NS_OK;
}

class MOZ_STACK_CLASS AutoPluginLibraryCall
{
public:
  explicit AutoPluginLibraryCall(nsNPAPIPluginInstance* aThis)
    : mThis(aThis), mGuard(aThis), mLibrary(nullptr)
  {
    nsNPAPIPlugin* plugin = mThis->GetPlugin();
    if (plugin)
      mLibrary = plugin->GetLibrary();
  }
  explicit operator bool() { return !!mLibrary; }
  PluginLibrary* operator->() { return mLibrary; }

private:
  nsNPAPIPluginInstance* mThis;
  PluginDestructionGuard mGuard;
  PluginLibrary* mLibrary;
};

nsresult
nsNPAPIPluginInstance::AsyncSetWindow(NPWindow* window)
{
  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  return library->AsyncSetWindow(&mNPP, window);
}

nsresult
nsNPAPIPluginInstance::GetImageContainer(ImageContainer**aContainer)
{
  *aContainer = nullptr;

  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  return !library ? NS_ERROR_FAILURE : library->GetImageContainer(&mNPP, aContainer);
}

nsresult
nsNPAPIPluginInstance::GetImageSize(nsIntSize* aSize)
{
  *aSize = nsIntSize(0, 0);

  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  return !library ? NS_ERROR_FAILURE : library->GetImageSize(&mNPP, aSize);
}

#if defined(XP_WIN)
nsresult
nsNPAPIPluginInstance::GetScrollCaptureContainer(ImageContainer**aContainer)
{
  *aContainer = nullptr;

  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  return !library ? NS_ERROR_FAILURE : library->GetScrollCaptureContainer(&mNPP, aContainer);
}
#endif

nsresult
nsNPAPIPluginInstance::HandledWindowedPluginKeyEvent(
                         const NativeEventData& aKeyEventData,
                         bool aIsConsumed)
{
  if (NS_WARN_IF(!mPlugin)) {
    return NS_ERROR_FAILURE;
  }

  PluginLibrary* library = mPlugin->GetLibrary();
  if (NS_WARN_IF(!library)) {
    return NS_ERROR_FAILURE;
  }
  return library->HandledWindowedPluginKeyEvent(&mNPP, aKeyEventData,
                                                aIsConsumed);
}

void
nsNPAPIPluginInstance::DidComposite()
{
  if (RUNNING != mRunning)
    return;

  AutoPluginLibraryCall library(this);
  library->DidComposite(&mNPP);
}

nsresult
nsNPAPIPluginInstance::NotifyPainted(void)
{
  MOZ_ASSERT_UNREACHABLE("Dead code, shouldn't be called.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsNPAPIPluginInstance::GetIsOOP(bool* aIsAsync)
{
  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  *aIsAsync = library->IsOOP();
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::SetBackgroundUnknown()
{
  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  return library->SetBackgroundUnknown(&mNPP);
}

nsresult
nsNPAPIPluginInstance::BeginUpdateBackground(nsIntRect* aRect,
                                             DrawTarget** aDrawTarget)
{
  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  return library->BeginUpdateBackground(&mNPP, *aRect, aDrawTarget);
}

nsresult
nsNPAPIPluginInstance::EndUpdateBackground(nsIntRect* aRect)
{
  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  return library->EndUpdateBackground(&mNPP, *aRect);
}

nsresult
nsNPAPIPluginInstance::IsTransparent(bool* isTransparent)
{
  *isTransparent = mTransparent;
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::GetFormValue(nsAString& aValue)
{
  aValue.Truncate();

  char *value = nullptr;
  nsresult rv = GetValueFromPlugin(NPPVformValue, &value);
  if (NS_FAILED(rv) || !value)
    return NS_ERROR_FAILURE;

  CopyUTF8toUTF16(value, aValue);

  // NPPVformValue allocates with NPN_MemAlloc(), which uses
  // nsMemory.
  free(value);

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::PushPopupsEnabledState(bool aEnabled)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = GetDOMWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  PopupControlState oldState =
    window->PushPopupControlState(aEnabled ? openAllowed : openAbused,
                                  true);

  if (!mPopupStates.AppendElement(oldState)) {
    // Appending to our state stack failed, pop what we just pushed.
    window->PopPopupControlState(oldState);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::PopPopupsEnabledState()
{
  int32_t last = mPopupStates.Length() - 1;

  if (last < 0) {
    // Nothing to pop.
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = GetDOMWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  PopupControlState &oldState = mPopupStates[last];

  window->PopPopupControlState(oldState);

  mPopupStates.RemoveElementAt(last);

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::GetPluginAPIVersion(uint16_t* version)
{
  NS_ENSURE_ARG_POINTER(version);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  if (!mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  *version = pluginFunctions->version;

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::PrivateModeStateChanged(bool enabled)
{
  if (RUNNING != mRunning)
    return NS_OK;

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance informing plugin of private mode state change this=%p\n",this));

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  if (!pluginFunctions->setvalue)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  NPError error;
  NPBool value = static_cast<NPBool>(enabled);
  NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setvalue)(&mNPP, NPNVprivateModeBool, &value), this,
                          NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);
  return (error == NPERR_NO_ERROR) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::IsPrivateBrowsing(bool* aEnabled)
{
  if (!mOwner)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  mOwner->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowOuter> domwindow = doc->GetWindow();
  NS_ENSURE_TRUE(domwindow, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell = domwindow->GetDocShell();
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
  *aEnabled = (loadContext && loadContext->UsePrivateBrowsing());
  return NS_OK;
}

static void
PluginTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsNPAPITimer* t = (nsNPAPITimer*)aClosure;
  NPP npp = t->npp;
  uint32_t id = t->id;

  PLUGIN_LOG(PLUGIN_LOG_NOISY, ("nsNPAPIPluginInstance running plugin timer callback this=%p\n", npp->ndata));

  // Some plugins (Flash on Android) calls unscheduletimer
  // from this callback.
  t->inCallback = true;
  (*(t->callback))(npp, id);
  t->inCallback = false;

  // Make sure we still have an instance and the timer is still alive
  // after the callback.
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;
  if (!inst || !inst->TimerWithID(id, nullptr))
    return;

  // use UnscheduleTimer to clean up if this is a one-shot timer
  uint32_t timerType;
  t->timer->GetType(&timerType);
  if (t->needUnschedule || timerType == nsITimer::TYPE_ONE_SHOT)
    inst->UnscheduleTimer(id);
}

nsNPAPITimer*
nsNPAPIPluginInstance::TimerWithID(uint32_t id, uint32_t* index)
{
  uint32_t len = mTimers.Length();
  for (uint32_t i = 0; i < len; i++) {
    if (mTimers[i]->id == id) {
      if (index)
        *index = i;
      return mTimers[i];
    }
  }
  return nullptr;
}

uint32_t
nsNPAPIPluginInstance::ScheduleTimer(uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
  if (RUNNING != mRunning)
    return 0;

  nsNPAPITimer *newTimer = new nsNPAPITimer();

  newTimer->inCallback = newTimer->needUnschedule = false;
  newTimer->npp = &mNPP;

  // generate ID that is unique to this instance
  uint32_t uniqueID = mTimers.Length();
  while ((uniqueID == 0) || TimerWithID(uniqueID, nullptr))
    uniqueID++;
  newTimer->id = uniqueID;

  // create new xpcom timer, scheduled correctly
  nsresult rv;
  const short timerType = (repeat ? (short)nsITimer::TYPE_REPEATING_SLACK : (short)nsITimer::TYPE_ONE_SHOT);
  rv = NS_NewTimerWithFuncCallback(getter_AddRefs(newTimer->timer),
                                   PluginTimerCallback,
                                   newTimer,
                                   interval,
                                   timerType,
                                   "nsNPAPIPluginInstance::ScheduleTimer");
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

void
nsNPAPIPluginInstance::UnscheduleTimer(uint32_t timerID)
{
  // find the timer struct by ID
  uint32_t index;
  nsNPAPITimer* t = TimerWithID(timerID, &index);
  if (!t)
    return;

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

NPBool
nsNPAPIPluginInstance::ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                    double *destX, double *destY, NPCoordinateSpace destSpace)
{
  if (mOwner) {
    return mOwner->ConvertPoint(sourceX, sourceY, sourceSpace, destX, destY, destSpace);
  }

  return false;
}

nsresult
nsNPAPIPluginInstance::GetDOMElement(Element** result)
{
  if (!mOwner) {
    *result = nullptr;
    return NS_ERROR_FAILURE;
  }

  return mOwner->GetDOMElement(result);
}

nsresult
nsNPAPIPluginInstance::InvalidateRect(NPRect *invalidRect)
{
  if (RUNNING != mRunning)
    return NS_OK;

  if (!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->InvalidateRect(invalidRect);
}

nsresult
nsNPAPIPluginInstance::InvalidateRegion(NPRegion invalidRegion)
{
  if (RUNNING != mRunning)
    return NS_OK;

  if (!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->InvalidateRegion(invalidRegion);
}

nsresult
nsNPAPIPluginInstance::GetMIMEType(const char* *result)
{
  if (!mMIMEType)
    *result = "";
  else
    *result = mMIMEType;

  return NS_OK;
}

nsPluginInstanceOwner*
nsNPAPIPluginInstance::GetOwner()
{
  return mOwner;
}

void
nsNPAPIPluginInstance::SetOwner(nsPluginInstanceOwner *aOwner)
{
  mOwner = aOwner;
}

nsresult
nsNPAPIPluginInstance::AsyncSetWindow(NPWindow& window)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsNPAPIPluginInstance::URLRedirectResponse(void* notifyData, NPBool allow)
{
  if (!notifyData) {
    return;
  }

  uint32_t listenerCount = mStreamListeners.Length();
  for (uint32_t i = 0; i < listenerCount; i++) {
    nsNPAPIPluginStreamListener* currentListener = mStreamListeners[i];
    if (currentListener->GetNotifyData() == notifyData) {
      currentListener->URLRedirectResponse(allow);
    }
  }
}

NPError
nsNPAPIPluginInstance::InitAsyncSurface(NPSize *size, NPImageFormat format,
                                        void *initData, NPAsyncSurface *surface)
{
  if (mOwner) {
    return mOwner->InitAsyncSurface(size, format, initData, surface);
  }

  return NPERR_GENERIC_ERROR;
}

NPError
nsNPAPIPluginInstance::FinalizeAsyncSurface(NPAsyncSurface *surface)
{
  if (mOwner) {
    return mOwner->FinalizeAsyncSurface(surface);
  }

  return NPERR_GENERIC_ERROR;
}

void
nsNPAPIPluginInstance::SetCurrentAsyncSurface(NPAsyncSurface *surface, NPRect *changed)
{
  if (mOwner) {
    mOwner->SetCurrentAsyncSurface(surface, changed);
  }
}

double
nsNPAPIPluginInstance::GetContentsScaleFactor()
{
  double scaleFactor = 1.0;
  if (mOwner) {
    mOwner->GetContentsScaleFactor(&scaleFactor);
  }
  return scaleFactor;
}

float
nsNPAPIPluginInstance::GetCSSZoomFactor()
{
  float zoomFactor = 1.0;
  if (mOwner) {
    mOwner->GetCSSZoomFactor(&zoomFactor);
  }
  return zoomFactor;
}

nsresult
nsNPAPIPluginInstance::GetRunID(uint32_t* aRunID)
{
  if (NS_WARN_IF(!aRunID)) {
    return NS_ERROR_INVALID_POINTER;
  }

  if (NS_WARN_IF(!mPlugin)) {
    return NS_ERROR_FAILURE;
  }

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library) {
    return NS_ERROR_FAILURE;
  }

  return library->GetRunID(aRunID);
}

nsresult
nsNPAPIPluginInstance::CreateAudioChannelAgentIfNeeded()
{
  if (mAudioChannelAgent) {
    return NS_OK;
  }

  nsresult rv;
  mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1", &rv);
  if (NS_WARN_IF(!mAudioChannelAgent)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = GetDOMWindow();
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_FAILURE;
  }

  rv = mAudioChannelAgent->Init(window->GetCurrentInnerWindow(), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

void
nsNPAPIPluginInstance::NotifyStartedPlaying()
{
  nsresult rv = CreateAudioChannelAgentIfNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MOZ_ASSERT(mAudioChannelAgent);
  dom::AudioPlaybackConfig config;
  rv = mAudioChannelAgent->NotifyStartedPlaying(&config,
                                                dom::AudioChannelService::AudibleState::eAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = WindowVolumeChanged(config.mVolume, config.mMuted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Since we only support muting for now, the implementation of suspend
  // is equal to muting. Therefore, if we have already muted the plugin,
  // then we don't need to call WindowSuspendChanged() again.
  if (config.mMuted) {
    return;
  }

  rv = WindowSuspendChanged(config.mSuspend);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void
nsNPAPIPluginInstance::NotifyStoppedPlaying()
{
  MOZ_ASSERT(mAudioChannelAgent);

  // Reset the attribute.
  mMuted = false;
  nsresult rv = mAudioChannelAgent->NotifyStoppedPlaying();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

NS_IMETHODIMP
nsNPAPIPluginInstance::WindowVolumeChanged(float aVolume, bool aMuted)
{
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("nsNPAPIPluginInstance, WindowVolumeChanged, "
          "this = %p, aVolume = %f, aMuted = %s\n",
          this, aVolume, aMuted ? "true" : "false"));

  // We just support mute/unmute
  nsresult rv = SetMuted(aMuted);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetMuted failed");
  if (mMuted != aMuted) {
    mMuted = aMuted;
    if (mAudioChannelAgent) {
      AudioChannelService::AudibleState audible = aMuted ?
        AudioChannelService::AudibleState::eNotAudible :
        AudioChannelService::AudibleState::eAudible;
      mAudioChannelAgent->NotifyStartedAudible(audible,
                                               AudioChannelService::AudibleChangedReasons::eVolumeChanged);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::WindowSuspendChanged(nsSuspendedTypes aSuspend)
{
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("nsNPAPIPluginInstance, WindowSuspendChanged, "
          "this = %p, aSuspend = %s\n", this, SuspendTypeToStr(aSuspend)));

  // It doesn't support suspended, so we just do something like mute/unmute.
  WindowVolumeChanged(1.0, /* useless */
                      aSuspend != nsISuspendedTypes::NONE_SUSPENDED);
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::WindowAudioCaptureChanged(bool aCapture)
{
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::SetMuted(bool aIsMuted)
{
  if (RUNNING != mRunning)
    return NS_OK;

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance informing plugin of mute state change this=%p\n",this));

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  if (!pluginFunctions->setvalue)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  NPError error;
  NPBool value = static_cast<NPBool>(aIsMuted);
  NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setvalue)(&mNPP, NPNVmuteAudioBool, &value), this,
                          NS_PLUGIN_CALL_UNSAFE_TO_REENTER_GECKO);
  return (error == NPERR_NO_ERROR) ? NS_OK : NS_ERROR_FAILURE;
}
