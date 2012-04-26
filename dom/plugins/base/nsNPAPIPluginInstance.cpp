/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "prlog.h"
#include "prmem.h"
#include "nscore.h"
#include "prenv.h"

#include "nsNPAPIPluginInstance.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsPluginHost.h"
#include "nsPluginSafety.h"
#include "nsPluginLogging.h"
#include "nsContentUtils.h"

#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsDirectoryServiceDefs.h"
#include "nsJSNPRuntime.h"
#include "nsPluginStreamListenerPeer.h"
#include "nsSize.h"
#include "nsNetCID.h"
#include "nsIContent.h"

#include "mozilla/Preferences.h"

#ifdef MOZ_WIDGET_ANDROID
#include "ANPBase.h"
#include <android/log.h>
#include "android_npapi.h"
#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "AndroidBridge.h"
#endif

using namespace mozilla;
using namespace mozilla::plugins::parent;

static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

NS_IMPL_THREADSAFE_ISUPPORTS0(nsNPAPIPluginInstance)

nsNPAPIPluginInstance::nsNPAPIPluginInstance()
  :
    mDrawingModel(kDefaultDrawingModel),
#ifdef MOZ_WIDGET_ANDROID
    mSurface(nsnull),
    mANPDrawingModel(0),
    mOnScreen(true),
#endif
    mRunning(NOT_STARTED),
    mWindowless(false),
    mTransparent(false),
    mCached(false),
    mUsesDOMForCursor(false),
    mInPluginInitCall(false),
    mPlugin(nsnull),
    mMIMEType(nsnull),
    mOwner(nsnull),
    mCurrentPluginEvent(nsnull),
#if defined(MOZ_X11) || defined(XP_WIN) || defined(XP_MACOSX)
    mUsePluginLayersPref(true)
#else
    mUsePluginLayersPref(false)
#endif
{
  mNPP.pdata = NULL;
  mNPP.ndata = this;

  mUsePluginLayersPref =
    Preferences::GetBool("plugins.use_layers", mUsePluginLayersPref);

  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance ctor: this=%p\n",this));
}

nsNPAPIPluginInstance::~nsNPAPIPluginInstance()
{
  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance dtor: this=%p\n",this));

  if (mMIMEType) {
    PR_Free((void *)mMIMEType);
    mMIMEType = nsnull;
  }
}

void
nsNPAPIPluginInstance::Destroy()
{
  Stop();
  mPlugin = nsnull;
}

TimeStamp
nsNPAPIPluginInstance::StopTime()
{
  return mStopTime;
}

nsresult nsNPAPIPluginInstance::Initialize(nsNPAPIPlugin *aPlugin, nsIPluginInstanceOwner* aOwner, const char* aMIMEType)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Initialize this=%p\n",this));

  NS_ENSURE_ARG_POINTER(aPlugin);
  NS_ENSURE_ARG_POINTER(aOwner);

  mPlugin = aPlugin;
  mOwner = aOwner;

  if (aMIMEType) {
    mMIMEType = (char*)PR_Malloc(PL_strlen(aMIMEType) + 1);
    if (mMIMEType) {
      PL_strcpy(mMIMEType, aMIMEType);
    }
  }
  
  return Start();
}

nsresult nsNPAPIPluginInstance::Stop()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Stop this=%p\n",this));

  // Make sure the plugin didn't leave popups enabled.
  if (mPopupStates.Length() > 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();

    if (window) {
      window->PopPopupControlState(openAbused);
    }
  }

  if (RUNNING != mRunning) {
    return NS_OK;
  }

  // clean up all outstanding timers
  for (PRUint32 i = mTimers.Length(); i > 0; i--)
    UnscheduleTimer(mTimers[i - 1]->id);

  // If there's code from this plugin instance on the stack, delay the
  // destroy.
  if (PluginDestructionGuard::DelayDestroy(this)) {
    return NS_OK;
  }

  // Make sure we lock while we're writing to mRunning after we've
  // started as other threads might be checking that inside a lock.
  {
    AsyncCallbackAutoLock lock;
    mRunning = DESTROYING;
    mStopTime = TimeStamp::Now();
  }

  OnPluginDestroy(&mNPP);

  // clean up open streams
  while (mStreamListeners.Length() > 0) {
    nsRefPtr<nsNPAPIPluginStreamListener> currentListener(mStreamListeners[0]);
    currentListener->CleanUpStream(NPRES_USER_BREAK);
    mStreamListeners.RemoveElement(currentListener);
  }

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  NPError error = NPERR_GENERIC_ERROR;
  if (pluginFunctions->destroy) {
    NPSavedData *sdata = 0;

    NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->destroy)(&mNPP, &sdata), this);

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

already_AddRefed<nsPIDOMWindow>
nsNPAPIPluginInstance::GetDOMWindow()
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return nsnull;

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return nsnull;

  nsPIDOMWindow *window = doc->GetWindow();
  NS_IF_ADDREF(window);

  return window;
}

nsresult
nsNPAPIPluginInstance::GetTagType(nsPluginTagType *result)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetTagType(result);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetAttributes(PRUint16& n, const char*const*& names,
                                     const char*const*& values)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetAttributes(n, names, values);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetParameters(PRUint16& n, const char*const*& names,
                                     const char*const*& values)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetParameters(n, names, values);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetMode(PRInt32 *result)
{
  if (mOwner)
    return mOwner->GetMode(result);
  else
    return NS_ERROR_FAILURE;
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

  PluginDestructionGuard guard(this);

  PRUint16 count = 0;
  const char* const* names = nsnull;
  const char* const* values = nsnull;
  nsPluginTagType tagtype;
  nsresult rv = GetTagType(&tagtype);
  if (NS_SUCCEEDED(rv)) {
    // Note: If we failed to get the tag type, we may be a full page plugin, so no arguments
    rv = GetAttributes(count, names, values);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // nsPluginTagType_Object or Applet may also have PARAM tags
    // Note: The arrays handed back by GetParameters() are
    // crafted specially to be directly behind the arrays from GetAttributes()
    // with a null entry as a separator. This is for 4.x backwards compatibility!
    // see bug 111008 for details
    if (tagtype != nsPluginTagType_Embed) {
      PRUint16 pcount = 0;
      const char* const* pnames = nsnull;
      const char* const* pvalues = nsnull;    
      if (NS_SUCCEEDED(GetParameters(pcount, pnames, pvalues))) {
        // Android expects an empty string as the separator instead of null
#ifdef MOZ_WIDGET_ANDROID
        NS_ASSERTION(PL_strcmp(values[count], "") == 0, "attribute/parameter array not setup correctly for Android NPAPI plugins");
#else
        NS_ASSERTION(!values[count], "attribute/parameter array not setup correctly for NPAPI plugins");
#endif
        if (pcount)
          count += ++pcount; // if it's all setup correctly, then all we need is to
                             // change the count (attrs + PARAM/blank + params)
      }
    }
  }

  PRInt32       mode;
  const char*   mimetype;
  NPError       error = NPERR_GENERIC_ERROR;

  GetMode(&mode);
  GetMIMEType(&mimetype);

  // Some older versions of Flash have a bug in them
  // that causes the stack to become currupt if we
  // pass swliveconnect=1 in the NPP_NewProc arrays.
  // See bug 149336 (UNIX), bug 186287 (Mac)
  //
  // The code below disables the attribute unless
  // the environment variable:
  // MOZILLA_PLUGIN_DISABLE_FLASH_SWLIVECONNECT_HACK
  // is set.
  //
  // It is okay to disable this attribute because
  // back in 4.x, scripting required liveconnect to
  // start Java which was slow. Scripting no longer
  // requires starting Java and is quick plus controled
  // from the browser, so Flash now ignores this attribute.
  //
  // This code can not be put at the time of creating
  // the array because we may need to examine the
  // stream header to determine we want Flash.

  static const char flashMimeType[] = "application/x-shockwave-flash";
  static const char blockedParam[] = "swliveconnect";
  if (count && !PL_strcasecmp(mimetype, flashMimeType)) {
    static int cachedDisableHack = 0;
    if (!cachedDisableHack) {
       if (PR_GetEnv("MOZILLA_PLUGIN_DISABLE_FLASH_SWLIVECONNECT_HACK"))
         cachedDisableHack = -1;
       else
         cachedDisableHack = 1;
    }
    if (cachedDisableHack > 0) {
      for (PRUint16 i=0; i<count; i++) {
        if (!PL_strcasecmp(names[i], blockedParam)) {
          // BIG FAT WARNIG:
          // I'm ugly casting |const char*| to |char*| and altering it
          // because I know we do malloc it values in
          // http://bonsai.mozilla.org/cvsblame.cgi?file=mozilla/layout/html/base/src/nsObjectFrame.cpp&rev=1.349&root=/cvsroot#3020
          // and free it at line #2096, so it couldn't be a const ptr to string literal
          char *val = (char*) values[i];
          if (val && *val) {
            // we cannot just *val=0, it won't be free properly in such case
            val[0] = '0';
            val[1] = 0;
          }
          break;
        }
      }
    }
  }

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

  nsresult newResult = library->NPP_New((char*)mimetype, &mNPP, (PRUint16)mode, count, (char**)names, (char**)values, NULL, &error);
  mInPluginInitCall = oldVal;

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP New called: this=%p, npp=%p, mime=%s, mode=%d, argc=%d, return=%d\n",
  this, &mNPP, mimetype, mode, count, error));

  if (NS_FAILED(newResult) || error != NPERR_NO_ERROR) {
    mRunning = DESTROYED;
    nsJSNPRuntime::OnPluginDestroy(&mNPP);
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

nsresult nsNPAPIPluginInstance::SetWindow(NPWindow* window)
{
  // NPAPI plugins don't want a SetWindow(NULL).
  if (!window || RUNNING != mRunning)
    return NS_OK;

#if defined(MOZ_WIDGET_GTK2)
  // bug 108347, flash plugin on linux doesn't like window->width <=
  // 0, but Java needs wants this call.
  if (!nsPluginHost::IsJavaMIMEType(mMIMEType) && window->type == NPWindowTypeWindow &&
      (window->width <= 0 || window->height <= 0)) {
    return NS_OK;
  }
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
    NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setwindow)(&mNPP, (NPWindow*)window), this);

    mInPluginInitCall = oldVal;

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP SetWindow called: this=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d], return=%d\n",
    this, window->x, window->y, window->width, window->height,
    window->clipRect.top, window->clipRect.bottom, window->clipRect.left, window->clipRect.right, error));
  }
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::NewStreamFromPlugin(const char* type, const char* target,
                                           nsIOutputStream* *result)
{
  nsPluginStreamToFile* stream = new nsPluginStreamToFile(target, mOwner);
  if (!stream)
    return NS_ERROR_OUT_OF_MEMORY;

  return stream->QueryInterface(kIOutputStreamIID, (void**)result);
}

nsresult
nsNPAPIPluginInstance::NewStreamListener(const char* aURL, void* notifyData,
                                         nsIPluginStreamListener** listener)
{
  nsNPAPIPluginStreamListener* stream = new nsNPAPIPluginStreamListener(this, notifyData, aURL);
  NS_ENSURE_TRUE(stream, NS_ERROR_OUT_OF_MEMORY);

  mStreamListeners.AppendElement(stream);

  return stream->QueryInterface(kIPluginStreamListenerIID, (void**)listener);
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
  PRUint16 sdkmajorversion = (pluginFunctions->version & 0xff00)>>8;
  PRUint16 sdkminorversion = pluginFunctions->version & 0x00ff;
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
      NS_TRY_SAFE_CALL_VOID((*pluginFunctions->print)(&mNPP, thePrint), this);

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

nsresult nsNPAPIPluginInstance::HandleEvent(void* event, PRInt16* result)
{
  if (RUNNING != mRunning)
    return NS_OK;

  if (!event)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  if (!mPlugin || !mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  PRInt16 tmpResult = kNPEventNotHandled;

  if (pluginFunctions->event) {
    mCurrentPluginEvent = event;
#if defined(XP_WIN) || defined(XP_OS2)
    NS_TRY_SAFE_CALL_RETURN(tmpResult, (*pluginFunctions->event)(&mNPP, event), this);
#else
    tmpResult = (*pluginFunctions->event)(&mNPP, event);
#endif
    NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP HandleEvent called: this=%p, npp=%p, event=%p, return=%d\n", 
      this, &mNPP, event, tmpResult));

    if (result)
      *result = tmpResult;
    mCurrentPluginEvent = nsnull;
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
    NS_TRY_SAFE_CALL_RETURN(pluginError, (*pluginFunctions->getvalue)(&mNPP, variable, value), this);
    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP GetValue called: this=%p, npp=%p, var=%d, value=%d, return=%d\n", 
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

  if (mMIMEType) {
    // bug 558434 - Prior to 3.6.4, we assumed windowless was transparent.
    // Silverlight apparently relied on this quirk, so we default to
    // transparent unless they specify otherwise after setting the windowless
    // property. (Last tested version: sl 4.0).
    // Changes to this code should be matched with changes in
    // PluginInstanceChild::InitQuirksMode.
    NS_NAMED_LITERAL_CSTRING(silverlight, "application/x-silverlight");
    if (!PL_strncasecmp(mMIMEType, silverlight.get(), silverlight.Length())) {
      mTransparent = true;
    }
  }

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
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner) {
    NS_WARNING("Trying to set event model without a plugin instance owner!");
    return;
  }

  owner->SetEventModel(aModel);
}
#endif

#if defined(MOZ_WIDGET_ANDROID)

static void SendLifecycleEvent(nsNPAPIPluginInstance* aInstance, PRUint32 aAction)
{
  ANPEvent event;
  event.inSize = sizeof(ANPEvent);
  event.eventType = kLifecycle_ANPEventType;
  event.data.lifecycle.action = aAction;
  aInstance->HandleEvent(&event, nsnull);
}

void nsNPAPIPluginInstance::NotifyForeground(bool aForeground)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::SetForeground this=%p\n foreground=%d",this, aForeground));
  if (RUNNING != mRunning)
    return;

  SendLifecycleEvent(this, aForeground ? kResume_ANPLifecycleAction : kPause_ANPLifecycleAction);
}

void nsNPAPIPluginInstance::NotifyOnScreen(bool aOnScreen)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::SetOnScreen this=%p\n onScreen=%d",this, aOnScreen));
  if (RUNNING != mRunning || mOnScreen == aOnScreen)
    return;

  mOnScreen = aOnScreen;
  SendLifecycleEvent(this, aOnScreen ? kOnScreen_ANPLifecycleAction : kOffScreen_ANPLifecycleAction);
}

void nsNPAPIPluginInstance::MemoryPressure()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::MemoryPressure this=%p\n",this));
  if (RUNNING != mRunning)
    return;

  SendLifecycleEvent(this, kFreeMemory_ANPLifecycleAction);
}

void nsNPAPIPluginInstance::SetANPDrawingModel(PRUint32 aModel)
{
  mANPDrawingModel = aModel;
}

class SurfaceGetter : public nsRunnable {
public:
  SurfaceGetter(nsNPAPIPluginInstance* aInstance, NPPluginFuncs* aPluginFunctions, NPP_t aNPP) : 
    mInstance(aInstance), mPluginFunctions(aPluginFunctions), mNPP(aNPP) {
  }
  ~SurfaceGetter() {
  }
  nsresult Run() {
    void* surface;
    (*mPluginFunctions->getvalue)(&mNPP, kJavaSurface_ANPGetValue, &surface);
    mInstance->SetJavaSurface(surface);
    return NS_OK;
  }
  void RequestSurface() {
    JNIEnv* env = GetJNIForThread();
    if (!env)
      return;

    if (!mozilla::AndroidBridge::Bridge()) {
      PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance null AndroidBridge"));
      return;
    }
    mozilla::AndroidBridge::Bridge()->PostToJavaThread(env, this);
  }
private:
  nsNPAPIPluginInstance* mInstance;
  NPP_t mNPP;
  NPPluginFuncs* mPluginFunctions;
};


void* nsNPAPIPluginInstance::GetJavaSurface()
{
  if (mANPDrawingModel != kSurface_ANPDrawingModel)
    return nsnull;
  
  return mSurface;
}

void nsNPAPIPluginInstance::SetJavaSurface(void* aSurface)
{
  mSurface = aSurface;
}

void nsNPAPIPluginInstance::RequestJavaSurface()
{
  if (mSurfaceGetter.get())
    return;

  mSurfaceGetter = new SurfaceGetter(this, mPlugin->PluginFuncs(), mNPP);

  ((SurfaceGetter*)mSurfaceGetter.get())->RequestSurface();
}

#endif

nsresult nsNPAPIPluginInstance::GetDrawingModel(PRInt32* aModel)
{
#if defined(XP_MACOSX)
  *aModel = (PRInt32)mDrawingModel;
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
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
nsNPAPIPluginInstance::GetJSObject(JSContext *cx, JSObject** outObject)
{
  NPObject *npobj = nsnull;
  nsresult rv = GetValueFromPlugin(NPPVpluginScriptableNPObject, &npobj);
  if (NS_FAILED(rv) || !npobj)
    return NS_ERROR_FAILURE;

  *outObject = nsNPObjWrapper::GetNewOrUsed(&mNPP, cx, npobj);

  _releaseobject(npobj);

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::DefineJavaProperties()
{
  NPObject *plugin_obj = nsnull;

  // The dummy Java plugin's scriptable object is what we want to
  // expose as window.Packages. And Window.Packages.java will be
  // exposed as window.java.

  // Get the scriptable plugin object.
  nsresult rv = GetValueFromPlugin(NPPVpluginScriptableNPObject, &plugin_obj);

  if (NS_FAILED(rv) || !plugin_obj) {
    return NS_ERROR_FAILURE;
  }

  // Get the NPObject wrapper for window.
  NPObject *window_obj = _getwindowobject(&mNPP);

  if (!window_obj) {
    _releaseobject(plugin_obj);

    return NS_ERROR_FAILURE;
  }

  NPIdentifier java_id = _getstringidentifier("java");
  NPIdentifier packages_id = _getstringidentifier("Packages");

  NPObject *java_obj = nsnull;
  NPVariant v;
  OBJECT_TO_NPVARIANT(plugin_obj, v);

  // Define the properties.

  bool ok = _setproperty(&mNPP, window_obj, packages_id, &v);
  if (ok) {
    ok = _getproperty(&mNPP, plugin_obj, java_id, &v);

    if (ok && NPVARIANT_IS_OBJECT(v)) {
      // Set java_obj so that we properly release it at the end of
      // this function.
      java_obj = NPVARIANT_TO_OBJECT(v);

      ok = _setproperty(&mNPP, window_obj, java_id, &v);
    }
  }

  _releaseobject(window_obj);
  _releaseobject(plugin_obj);
  _releaseobject(java_obj);

  if (!ok)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::SetCached(bool aCache)
{
  mCached = aCache;
  return NS_OK;
}

bool
nsNPAPIPluginInstance::ShouldCache()
{
  return mCached;
}

nsresult
nsNPAPIPluginInstance::IsWindowless(bool* isWindowless)
{
#ifdef MOZ_WIDGET_ANDROID
  // On android, pre-honeycomb, all plugins are treated as windowless.
  *isWindowless = true;
#else
  *isWindowless = mWindowless;
#endif
  return NS_OK;
}

class NS_STACK_CLASS AutoPluginLibraryCall
{
public:
  AutoPluginLibraryCall(nsNPAPIPluginInstance* aThis)
    : mThis(aThis), mGuard(aThis), mLibrary(nsnull)
  {
    nsNPAPIPlugin* plugin = mThis->GetPlugin();
    if (plugin)
      mLibrary = plugin->GetLibrary();
  }
  operator bool() { return !!mLibrary; }
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

#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
nsresult
nsNPAPIPluginInstance::HandleGUIEvent(const nsGUIEvent& anEvent, bool* handled)
{
  if (RUNNING != mRunning) {
    *handled = false;
    return NS_OK;
  }

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  return library->HandleGUIEvent(&mNPP, anEvent, handled);
}
#endif

nsresult
nsNPAPIPluginInstance::GetImageContainer(ImageContainer**aContainer)
{
  *aContainer = nsnull;

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

nsresult
nsNPAPIPluginInstance::NotifyPainted(void)
{
  NS_NOTREACHED("Dead code, shouldn't be called.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsNPAPIPluginInstance::UseAsyncPainting(bool* aIsAsync)
{
  if (!mUsePluginLayersPref) {
    *aIsAsync = mUsePluginLayersPref;
    return NS_OK;
  }

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  *aIsAsync = library->UseAsyncPainting();
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
                                             gfxContext** aContext)
{
  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  return library->BeginUpdateBackground(&mNPP, *aRect, aContext);
}

nsresult
nsNPAPIPluginInstance::EndUpdateBackground(gfxContext* aContext,
                                           nsIntRect* aRect)
{
  if (RUNNING != mRunning)
    return NS_OK;

  AutoPluginLibraryCall library(this);
  if (!library)
    return NS_ERROR_FAILURE;

  return library->EndUpdateBackground(&mNPP, aContext, *aRect);
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

  char *value = nsnull;
  nsresult rv = GetValueFromPlugin(NPPVformValue, &value);
  if (NS_FAILED(rv) || !value)
    return NS_ERROR_FAILURE;

  CopyUTF8toUTF16(value, aValue);

  // NPPVformValue allocates with NPN_MemAlloc(), which uses
  // nsMemory.
  nsMemory::Free(value);

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::PushPopupsEnabledState(bool aEnabled)
{
  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
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
  PRInt32 last = mPopupStates.Length() - 1;

  if (last < 0) {
    // Nothing to pop.
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  PopupControlState &oldState = mPopupStates[last];

  window->PopPopupControlState(oldState);

  mPopupStates.RemoveElementAt(last);
  
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::GetPluginAPIVersion(PRUint16* version)
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
  NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setvalue)(&mNPP, NPNVprivateModeBool, &value), this);
  return (error == NPERR_NO_ERROR) ? NS_OK : NS_ERROR_FAILURE;
}

class DelayUnscheduleEvent : public nsRunnable {
public:
  nsRefPtr<nsNPAPIPluginInstance> mInstance;
  uint32_t mTimerID;
  DelayUnscheduleEvent(nsNPAPIPluginInstance* aInstance, uint32_t aTimerId)
    : mInstance(aInstance)
    , mTimerID(aTimerId)
  {}

  ~DelayUnscheduleEvent() {}

  NS_IMETHOD Run();
};

NS_IMETHODIMP
DelayUnscheduleEvent::Run()
{
  mInstance->UnscheduleTimer(mTimerID);
  return NS_OK;
}


static void
PluginTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsNPAPITimer* t = (nsNPAPITimer*)aClosure;
  NPP npp = t->npp;
  uint32_t id = t->id;

  // Some plugins (Flash on Android) calls unscheduletimer
  // from this callback.
  t->inCallback = true;
  (*(t->callback))(npp, id);
  t->inCallback = false;

  // Make sure we still have an instance and the timer is still alive
  // after the callback.
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;
  if (!inst || !inst->TimerWithID(id, NULL))
    return;

  // use UnscheduleTimer to clean up if this is a one-shot timer
  PRUint32 timerType;
  t->timer->GetType(&timerType);
  if (timerType == nsITimer::TYPE_ONE_SHOT)
      inst->UnscheduleTimer(id);
}

nsNPAPITimer*
nsNPAPIPluginInstance::TimerWithID(uint32_t id, PRUint32* index)
{
  PRUint32 len = mTimers.Length();
  for (PRUint32 i = 0; i < len; i++) {
    if (mTimers[i]->id == id) {
      if (index)
        *index = i;
      return mTimers[i];
    }
  }
  return nsnull;
}

uint32_t
nsNPAPIPluginInstance::ScheduleTimer(uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
  nsNPAPITimer *newTimer = new nsNPAPITimer();

  newTimer->inCallback = false;
  newTimer->npp = &mNPP;

  // generate ID that is unique to this instance
  uint32_t uniqueID = mTimers.Length();
  while ((uniqueID == 0) || TimerWithID(uniqueID, NULL))
    uniqueID++;
  newTimer->id = uniqueID;

  // create new xpcom timer, scheduled correctly
  nsresult rv;
  nsCOMPtr<nsITimer> xpcomTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    delete newTimer;
    return 0;
  }
  const short timerType = (repeat ? (short)nsITimer::TYPE_REPEATING_SLACK : (short)nsITimer::TYPE_ONE_SHOT);
  xpcomTimer->InitWithFuncCallback(PluginTimerCallback, newTimer, interval, timerType);
  newTimer->timer = xpcomTimer;

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
  PRUint32 index;
  nsNPAPITimer* t = TimerWithID(timerID, &index);
  if (!t)
    return;

  if (t->inCallback) {
    nsCOMPtr<nsIRunnable> e = new DelayUnscheduleEvent(this, timerID);
    NS_DispatchToCurrentThread(e);
    return;
  }

  // cancel the timer
  t->timer->Cancel();

  // remove timer struct from array
  mTimers.RemoveElementAt(index);

  // delete timer
  delete t;
}

// Show the context menu at the location for the current event.
// This can only be called from within an NPP_SendEvent call.
NPError
nsNPAPIPluginInstance::PopUpContextMenu(NPMenu* menu)
{
  if (mOwner && mCurrentPluginEvent)
    return mOwner->ShowNativeContextMenu(menu, mCurrentPluginEvent);

  return NPERR_GENERIC_ERROR;
}

NPBool
nsNPAPIPluginInstance::ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                    double *destX, double *destY, NPCoordinateSpace destSpace)
{
  if (mOwner)
    return mOwner->ConvertPoint(sourceX, sourceY, sourceSpace, destX, destY, destSpace);

  return false;
}

nsresult
nsNPAPIPluginInstance::GetDOMElement(nsIDOMElement* *result)
{
  if (!mOwner) {
    *result = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
  if (tinfo)
    return tinfo->GetDOMElement(result);

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::InvalidateRect(NPRect *invalidRect)
{
  if (RUNNING != mRunning)
    return NS_OK;

  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->InvalidateRect(invalidRect);
}

nsresult
nsNPAPIPluginInstance::InvalidateRegion(NPRegion invalidRegion)
{
  if (RUNNING != mRunning)
    return NS_OK;

  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->InvalidateRegion(invalidRegion);
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

nsresult
nsNPAPIPluginInstance::GetJSContext(JSContext* *outContext)
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  *outContext = NULL;
  nsCOMPtr<nsIDocument> document;

  nsresult rv = owner->GetDocument(getter_AddRefs(document));

  if (NS_SUCCEEDED(rv) && document) {
    nsIScriptGlobalObject *global = document->GetScriptGlobalObject();

    if (global) {
      nsIScriptContext *context = global->GetContext();

      if (context) {
        *outContext = context->GetNativeContext();
      }
    }
  }

  return rv;
}

nsresult
nsNPAPIPluginInstance::GetOwner(nsIPluginInstanceOwner **aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);
  *aOwner = mOwner;
  NS_IF_ADDREF(mOwner);
  return (mOwner ? NS_OK : NS_ERROR_FAILURE);
}

nsresult
nsNPAPIPluginInstance::SetOwner(nsIPluginInstanceOwner *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::ShowStatus(const char* message)
{
  if (mOwner)
    return mOwner->ShowStatus(message);

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::InvalidateOwner()
{
  mOwner = nsnull;

  return NS_OK;
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

  PRUint32 listenerCount = mStreamListeners.Length();
  for (PRUint32 i = 0; i < listenerCount; i++) {
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
  if (mOwner)
    return mOwner->InitAsyncSurface(size, format, initData, surface);

  return NPERR_GENERIC_ERROR;
}

NPError
nsNPAPIPluginInstance::FinalizeAsyncSurface(NPAsyncSurface *surface)
{
  if (mOwner)
    return mOwner->FinalizeAsyncSurface(surface);

  return NPERR_GENERIC_ERROR;
}

void
nsNPAPIPluginInstance::SetCurrentAsyncSurface(NPAsyncSurface *surface, NPRect *changed)
{
  if (mOwner)
    mOwner->SetCurrentAsyncSurface(surface, changed);
}

class CarbonEventModelFailureEvent : public nsRunnable {
public:
  nsCOMPtr<nsIContent> mContent;

  CarbonEventModelFailureEvent(nsIContent* aContent)
    : mContent(aContent)
  {}

  ~CarbonEventModelFailureEvent() {}

  NS_IMETHOD Run();
};

NS_IMETHODIMP
CarbonEventModelFailureEvent::Run()
{
  nsString type = NS_LITERAL_STRING("npapi-carbon-event-model-failure");
  nsContentUtils::DispatchTrustedEvent(mContent->GetDocument(), mContent,
                                       type, true, true);
  return NS_OK;
}

void
nsNPAPIPluginInstance::CarbonNPAPIFailure()
{
  nsCOMPtr<nsIDOMElement> element;
  GetDOMElement(getter_AddRefs(element));
  if (!element) {
    return;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(element));
  if (!content) {
    return;
  }

  nsCOMPtr<nsIRunnable> e = new CarbonEventModelFailureEvent(content);
  nsresult rv = NS_DispatchToCurrentThread(e);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch CarbonEventModelFailureEvent.");
  }
}
