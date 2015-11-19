/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  This file is the Qt implementation of plugin native window.
 */

#include "nsDebug.h"
#include "nsPluginNativeWindow.h"
#include "npapi.h"

/**
 * Qt implementation of plugin window
 */
class nsPluginNativeWindowQt : public nsPluginNativeWindow
{
public:
  nsPluginNativeWindowQt();
  virtual ~nsPluginNativeWindowQt();

  virtual nsresult CallSetWindow(RefPtr<nsNPAPIPluginInstance> &aPluginInstance);
private:

  NPSetWindowCallbackStruct mWsInfo;
};

nsPluginNativeWindowQt::nsPluginNativeWindowQt() : nsPluginNativeWindow()
{
  //Initialize member variables
#ifdef DEBUG
  fprintf(stderr,"\n\n\nCreating plugin native window %p\n\n\n", (void *) this);
#endif
  window = nullptr;
  x = 0;
  y = 0;
  width = 0;
  height = 0;
  memset(&clipRect, 0, sizeof(clipRect));
  ws_info = &mWsInfo;
  type = NPWindowTypeWindow;
  mWsInfo.type = 0;
#if defined(MOZ_X11)
  mWsInfo.display = nullptr;
  mWsInfo.visual = nullptr;
  mWsInfo.colormap = 0;
  mWsInfo.depth = 0;
#endif
}

nsPluginNativeWindowQt::~nsPluginNativeWindowQt()
{
#ifdef DEBUG
  fprintf(stderr,"\n\n\nDestoying plugin native window %p\n\n\n", (void *) this);
#endif
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow **aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  *aPluginNativeWindow = new nsPluginNativeWindowQt();
  return NS_OK;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowQt *p = (nsPluginNativeWindowQt *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}

nsresult nsPluginNativeWindowQt::CallSetWindow(RefPtr<nsNPAPIPluginInstance> &aPluginInstance)
{
  if (aPluginInstance) {
    if (type == NPWindowTypeWindow) {
      return NS_ERROR_FAILURE;
    } // NPWindowTypeWindow
    aPluginInstance->SetWindow(this);
  }
  else if (mPluginInstance)
    mPluginInstance->SetWindow(nullptr);

  SetPluginInstance(aPluginInstance);
  return NS_OK;
}
