/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginSafety_h_
#define nsPluginSafety_h_

#include "npapi.h"
#include "nsPluginHost.h"
#include <prinrval.h>

// On Android, we need to guard against plugin code leaking entries in the local
// JNI ref table. See https://bugzilla.mozilla.org/show_bug.cgi?id=780831#c21
#ifdef MOZ_WIDGET_ANDROID
  #include "AndroidBridge.h"

  #define MAIN_THREAD_JNI_REF_GUARD mozilla::AutoLocalJNIFrame jniFrame
#else
  #define MAIN_THREAD_JNI_REF_GUARD
#endif

#if defined(XP_WIN)
#define CALL_SAFETY_ON
#endif

PRIntervalTime NS_NotifyBeginPluginCall();
void NS_NotifyPluginCall(PRIntervalTime);

#ifdef CALL_SAFETY_ON

#include "mozilla/Preferences.h"

extern bool gSkipPluginSafeCalls;

#define NS_INIT_PLUGIN_SAFE_CALLS                                  \
PR_BEGIN_MACRO                                                     \
  gSkipPluginSafeCalls =                                           \
    ::mozilla::Preferences::GetBool("plugin.dont_try_safe_calls",  \
                                    gSkipPluginSafeCalls);         \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_RETURN(ret, fun, pluginInst) \
PR_BEGIN_MACRO                                     \
  MAIN_THREAD_JNI_REF_GUARD;                       \
  PRIntervalTime startTime = NS_NotifyBeginPluginCall(); \
  if(gSkipPluginSafeCalls)                         \
    ret = fun;                                     \
  else                                             \
  {                                                \
    MOZ_SEH_TRY                                    \
    {                                              \
      ret = fun;                                   \
    }                                              \
    MOZ_SEH_EXCEPT(true)                           \
    {                                              \
      nsresult res;                                \
      nsCOMPtr<nsIPluginHost> host(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &res));\
      if(NS_SUCCEEDED(res) && (host != nullptr))    \
        static_cast<nsPluginHost*>(host.get())->HandleBadPlugin(nullptr, pluginInst); \
      ret = (NPError)NS_ERROR_FAILURE;             \
    }                                              \
  }                                                \
  NS_NotifyPluginCall(startTime);		   \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_VOID(fun, pluginInst) \
PR_BEGIN_MACRO                              \
  MAIN_THREAD_JNI_REF_GUARD;                \
  PRIntervalTime startTime = NS_NotifyBeginPluginCall(); \
  if(gSkipPluginSafeCalls)                  \
    fun;                                    \
  else                                      \
  {                                         \
    MOZ_SEH_TRY                             \
    {                                       \
      fun;                                  \
    }                                       \
    MOZ_SEH_EXCEPT(true)                    \
    {                                       \
      nsresult res;                         \
      nsCOMPtr<nsIPluginHost> host(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &res));\
      if(NS_SUCCEEDED(res) && (host != nullptr))\
        static_cast<nsPluginHost*>(host.get())->HandleBadPlugin(nullptr, pluginInst);\
    }                                       \
  }                                         \
  NS_NotifyPluginCall(startTime);		   \
PR_END_MACRO

#else // vanilla calls

#define NS_TRY_SAFE_CALL_RETURN(ret, fun, pluginInst) \
PR_BEGIN_MACRO                                     \
  MAIN_THREAD_JNI_REF_GUARD;                       \
  PRIntervalTime startTime = NS_NotifyBeginPluginCall(); \
  ret = fun;                                       \
  NS_NotifyPluginCall(startTime);		               \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_VOID(fun, pluginInst)     \
PR_BEGIN_MACRO                                     \
  MAIN_THREAD_JNI_REF_GUARD;                       \
  PRIntervalTime startTime = NS_NotifyBeginPluginCall(); \
  fun;                                             \
  NS_NotifyPluginCall(startTime);		               \
PR_END_MACRO

#endif // CALL_SAFETY_ON

#endif //nsPluginSafety_h_
