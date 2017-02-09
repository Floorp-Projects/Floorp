/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  This file is the default implementation of plugin native window
 *  empty stubs, it should be replaced with real platform implementation
 *  for every platform
 */

#include "nsDebug.h"
#include "nsPluginNativeWindow.h"

class nsPluginNativeWindowImpl : public nsPluginNativeWindow
{
public:
  nsPluginNativeWindowImpl();
  virtual ~nsPluginNativeWindowImpl();

#ifdef MOZ_WIDGET_GTK
  NPSetWindowCallbackStruct mWsInfo;
#endif
};

nsPluginNativeWindowImpl::nsPluginNativeWindowImpl() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nullptr;
  x = 0;
  y = 0;
  width = 0;
  height = 0;
  memset(&clipRect, 0, sizeof(clipRect));
  type = NPWindowTypeWindow;

#ifdef MOZ_WIDGET_GTK
  ws_info = &mWsInfo;
  mWsInfo.type = 0;
  mWsInfo.display = nullptr;
  mWsInfo.visual = nullptr;
  mWsInfo.colormap = 0;
  mWsInfo.depth = 0;
#elif defined(XP_UNIX) && !defined(XP_MACOSX)
  ws_info = nullptr;
#endif
}

nsPluginNativeWindowImpl::~nsPluginNativeWindowImpl()
{
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  *aPluginNativeWindow = new nsPluginNativeWindowImpl();
  return NS_OK;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  delete static_cast<nsPluginNativeWindowImpl*>(aPluginNativeWindow);
  return NS_OK;
}
