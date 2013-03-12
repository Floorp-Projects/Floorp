/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PluginPRLibrary.h"

// Some plugins on Windows, notably Quake Live, implement NP_Initialize using
// cdecl instead of the documented stdcall. In order to work around this,
// we force the caller to use a frame pointer.
#if defined(XP_WIN) && defined(_M_IX86)
#include <malloc.h>

// gNotOptimized exists so that the compiler will not optimize the alloca
// below.
static int gNotOptimized;
#define CALLING_CONVENTION_HACK void* foo = _alloca(gNotOptimized);
#else
#define CALLING_CONVENTION_HACK
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "android_npapi.h"
#include <android/log.h>
#undef ALOG
#define ALOG(args...) __android_log_print(ANDROID_LOG_INFO, "GeckoJavaEnv", ## args)
#endif

using namespace mozilla::layers;

namespace mozilla {
#ifdef MOZ_WIDGET_ANDROID
nsresult
PluginPRLibrary::NP_Initialize(NPNetscapeFuncs* bFuncs,
			       NPPluginFuncs* pFuncs, NPError* error)
{
  JNIEnv* env = GetJNIForThread();
  if (!env)
    return NS_ERROR_FAILURE;

  mozilla::AutoLocalJNIFrame jniFrame(env);

  if (mNP_Initialize) {
    *error = mNP_Initialize(bFuncs, pFuncs, env);
  } else {
    NP_InitializeFunc pfNP_Initialize = (NP_InitializeFunc)
      PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
    if (!pfNP_Initialize)
      return NS_ERROR_FAILURE;
    *error = pfNP_Initialize(bFuncs, pFuncs, env);
  }

  // Save pointers to functions that get called through PluginLibrary itself.
  mNPP_New = pFuncs->newp;
  mNPP_ClearSiteData = pFuncs->clearsitedata;
  mNPP_GetSitesWithData = pFuncs->getsiteswithdata;
  return NS_OK;
}
#elif defined(MOZ_WIDGET_GONK)
nsresult
PluginPRLibrary::NP_Initialize(NPNetscapeFuncs* bFuncs, NPError* error)
{
  return NS_OK;
}
#elif defined(XP_UNIX) && !defined(XP_MACOSX)
nsresult
PluginPRLibrary::NP_Initialize(NPNetscapeFuncs* bFuncs,
                               NPPluginFuncs* pFuncs, NPError* error)
{
  if (mNP_Initialize) {
    *error = mNP_Initialize(bFuncs, pFuncs);
  } else {
    NP_InitializeFunc pfNP_Initialize = (NP_InitializeFunc)
      PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
    if (!pfNP_Initialize)
      return NS_ERROR_FAILURE;
    *error = pfNP_Initialize(bFuncs, pFuncs);
  }


  // Save pointers to functions that get called through PluginLibrary itself.
  mNPP_New = pFuncs->newp;
  mNPP_ClearSiteData = pFuncs->clearsitedata;
  mNPP_GetSitesWithData = pFuncs->getsiteswithdata;
  return NS_OK;
}
#else
nsresult
PluginPRLibrary::NP_Initialize(NPNetscapeFuncs* bFuncs, NPError* error)
{
  CALLING_CONVENTION_HACK

  if (mNP_Initialize) {
    *error = mNP_Initialize(bFuncs);
  } else {
    NP_InitializeFunc pfNP_Initialize = (NP_InitializeFunc)
      PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
    if (!pfNP_Initialize)
      return NS_ERROR_FAILURE;
    *error = pfNP_Initialize(bFuncs);
  }

  return NS_OK;
}
#endif

nsresult
PluginPRLibrary::NP_Shutdown(NPError* error)
{
  CALLING_CONVENTION_HACK

  if (mNP_Shutdown) {
    *error = mNP_Shutdown();
  } else {
    NP_ShutdownFunc pfNP_Shutdown = (NP_ShutdownFunc)
      PR_FindFunctionSymbol(mLibrary, "NP_Shutdown");
    if (!pfNP_Shutdown)
      return NS_ERROR_FAILURE;
    *error = pfNP_Shutdown();
  }

  return NS_OK;
}

nsresult
PluginPRLibrary::NP_GetMIMEDescription(const char** mimeDesc)
{
  CALLING_CONVENTION_HACK

  if (mNP_GetMIMEDescription) {
    *mimeDesc = mNP_GetMIMEDescription();
  }
  else {
    NP_GetMIMEDescriptionFunc pfNP_GetMIMEDescription =
      (NP_GetMIMEDescriptionFunc)
      PR_FindFunctionSymbol(mLibrary, "NP_GetMIMEDescription");
    if (!pfNP_GetMIMEDescription) {
      *mimeDesc = "";
      return NS_ERROR_FAILURE;
    }
    *mimeDesc = pfNP_GetMIMEDescription();
  }

  return NS_OK;
}

nsresult
PluginPRLibrary::NP_GetValue(void *future, NPPVariable aVariable,
			     void *aValue, NPError* error)
{
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  if (mNP_GetValue) {
    *error = mNP_GetValue(future, aVariable, aValue);
  } else {
    NP_GetValueFunc pfNP_GetValue = (NP_GetValueFunc)PR_FindFunctionSymbol(mLibrary, "NP_GetValue");
    if (!pfNP_GetValue)
      return NS_ERROR_FAILURE;
    *error = pfNP_GetValue(future, aVariable, aValue);
  }
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_OS2)
nsresult
PluginPRLibrary::NP_GetEntryPoints(NPPluginFuncs* pFuncs, NPError* error)
{
  CALLING_CONVENTION_HACK

  if (mNP_GetEntryPoints) {
    *error = mNP_GetEntryPoints(pFuncs);
  } else {
    NP_GetEntryPointsFunc pfNP_GetEntryPoints = (NP_GetEntryPointsFunc)
      PR_FindFunctionSymbol(mLibrary, "NP_GetEntryPoints");
    if (!pfNP_GetEntryPoints)
      return NS_ERROR_FAILURE;
    *error = pfNP_GetEntryPoints(pFuncs);
  }

  // Save pointers to functions that get called through PluginLibrary itself.
  mNPP_New = pFuncs->newp;
  mNPP_ClearSiteData = pFuncs->clearsitedata;
  mNPP_GetSitesWithData = pFuncs->getsiteswithdata;
  return NS_OK;
}
#endif

nsresult
PluginPRLibrary::NPP_New(NPMIMEType pluginType, NPP instance,
			 uint16_t mode, int16_t argc, char* argn[],
			 char* argv[], NPSavedData* saved,
			 NPError* error)
{
  if (!mNPP_New)
    return NS_ERROR_FAILURE;

  MAIN_THREAD_JNI_REF_GUARD;
  *error = mNPP_New(pluginType, instance, mode, argc, argn, argv, saved);
  return NS_OK;
}

nsresult
PluginPRLibrary::NPP_ClearSiteData(const char* site, uint64_t flags,
                                   uint64_t maxAge)
{
  if (!mNPP_ClearSiteData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MAIN_THREAD_JNI_REF_GUARD;
  NPError result = mNPP_ClearSiteData(site, flags, maxAge);

  switch (result) {
  case NPERR_NO_ERROR:
    return NS_OK;
  case NPERR_TIME_RANGE_NOT_SUPPORTED:
    return NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED;
  case NPERR_MALFORMED_SITE:
    return NS_ERROR_INVALID_ARG;
  default:
    return NS_ERROR_FAILURE;
  }
}

nsresult
PluginPRLibrary::NPP_GetSitesWithData(InfallibleTArray<nsCString>& result)
{
  if (!mNPP_GetSitesWithData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  result.Clear();

  MAIN_THREAD_JNI_REF_GUARD;
  char** sites = mNPP_GetSitesWithData();
  if (!sites) {
    return NS_OK;
  }

  char** iterator = sites;
  while (*iterator) {
    result.AppendElement(*iterator);
    NS_Free(*iterator);
    ++iterator;
  }
  NS_Free(sites);

  return NS_OK;
}

nsresult
PluginPRLibrary::AsyncSetWindow(NPP instance, NPWindow* window)
{
  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*)instance->ndata;
  NS_ENSURE_TRUE(inst, NS_ERROR_NULL_POINTER);
  return NS_ERROR_NOT_IMPLEMENTED;
}

#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
nsresult
PluginPRLibrary::HandleGUIEvent(NPP instance, const nsGUIEvent& anEvent,
                                bool* handled)
{
  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*)instance->ndata;
  NS_ENSURE_TRUE(inst, NS_ERROR_NULL_POINTER);
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

nsresult
PluginPRLibrary::GetImageContainer(NPP instance, ImageContainer** aContainer)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#if defined(XP_MACOSX)
nsresult
PluginPRLibrary::IsRemoteDrawingCoreAnimation(NPP instance, bool *aDrawing)
{
  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*)instance->ndata;
  NS_ENSURE_TRUE(inst, NS_ERROR_NULL_POINTER);
  *aDrawing = false; 
  return NS_OK;
}
nsresult
PluginPRLibrary::ContentsScaleFactorChanged(NPP instance, double aContentsScaleFactor)
{
  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*)instance->ndata;
  NS_ENSURE_TRUE(inst, NS_ERROR_NULL_POINTER);
  return NS_OK;
}
#endif

nsresult
PluginPRLibrary::GetImageSize(NPP instance, nsIntSize* aSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginPRLibrary::SetBackgroundUnknown(NPP instance)
{
  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*)instance->ndata;
  NS_ENSURE_TRUE(inst, NS_ERROR_NULL_POINTER);
  NS_ERROR("Unexpected use of async APIs for in-process plugin.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginPRLibrary::BeginUpdateBackground(NPP instance,
                                       const nsIntRect&, gfxContext** aCtx)
{
  nsNPAPIPluginInstance* inst = (nsNPAPIPluginInstance*)instance->ndata;
  NS_ENSURE_TRUE(inst, NS_ERROR_NULL_POINTER);
  NS_ERROR("Unexpected use of async APIs for in-process plugin.");
  *aCtx = nullptr;
  return NS_OK;
}

nsresult
PluginPRLibrary::EndUpdateBackground(NPP instance,
                                     gfxContext*, const nsIntRect&)
{
  NS_RUNTIMEABORT("This should never be called");
  return NS_ERROR_NOT_AVAILABLE;
}

} // namespace mozilla
