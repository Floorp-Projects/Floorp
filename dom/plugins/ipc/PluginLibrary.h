/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PluginLibrary_h
#define mozilla_PluginLibrary_h 1

#include "prlink.h"
#include "npapi.h"
#include "npfunctions.h"
#include "nscore.h"
#include "nsTArray.h"
#include "nsPluginError.h"

class gfxASurface;
class gfxContext;
class nsCString;
struct nsIntRect;
struct nsIntSize;
class nsNPAPIPlugin;
class nsGUIEvent;

namespace mozilla {
namespace layers {
class Image;
class ImageContainer;
}
}

using namespace mozilla::layers;

namespace mozilla {

class PluginLibrary
{
public:
  virtual ~PluginLibrary() { }

  /**
   * Inform this library about the nsNPAPIPlugin which owns it. This
   * object will hold a weak pointer to the plugin.
   */
  virtual void SetPlugin(nsNPAPIPlugin* plugin) = 0;

  virtual bool HasRequiredFunctions() = 0;

#if defined(XP_UNIX) && !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GONK)
  virtual nsresult NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs, NPError* error) = 0;
#else
  virtual nsresult NP_Initialize(NPNetscapeFuncs* bFuncs, NPError* error) = 0;
#endif
  virtual nsresult NP_Shutdown(NPError* error) = 0;
  virtual nsresult NP_GetMIMEDescription(const char** mimeDesc) = 0;
  virtual nsresult NP_GetValue(void *future, NPPVariable aVariable,
                               void *aValue, NPError* error) = 0;
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(XP_OS2)
  virtual nsresult NP_GetEntryPoints(NPPluginFuncs* pFuncs, NPError* error) = 0;
#endif
  virtual nsresult NPP_New(NPMIMEType pluginType, NPP instance,
                           uint16_t mode, int16_t argc, char* argn[],
                           char* argv[], NPSavedData* saved,
                           NPError* error) = 0;

  virtual nsresult NPP_ClearSiteData(const char* site, uint64_t flags,
                                     uint64_t maxAge) = 0;
  virtual nsresult NPP_GetSitesWithData(InfallibleTArray<nsCString>& aResult) = 0;

  virtual nsresult AsyncSetWindow(NPP instance, NPWindow* window) = 0;
  virtual nsresult GetImageContainer(NPP instance, ImageContainer** aContainer) = 0;
  virtual nsresult GetImageSize(NPP instance, nsIntSize* aSize) = 0;
  virtual bool IsOOP() = 0;
#if defined(XP_MACOSX)
  virtual nsresult IsRemoteDrawingCoreAnimation(NPP instance, bool *aDrawing) = 0;
#endif

  /**
   * The next three methods are the third leg in the trip to
   * PluginInstanceParent.  They approximately follow the ReadbackSink
   * API.
   */
  virtual nsresult SetBackgroundUnknown(NPP instance) = 0;
  virtual nsresult BeginUpdateBackground(NPP instance,
                                         const nsIntRect&, gfxContext**) = 0;
  virtual nsresult EndUpdateBackground(NPP instance,
                                       gfxContext*, const nsIntRect&) = 0;
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
  virtual nsresult HandleGUIEvent(NPP instance, const nsGUIEvent&, bool*) = 0;
#endif
};


} // namespace mozilla

#endif  // ifndef mozilla_PluginLibrary_h
