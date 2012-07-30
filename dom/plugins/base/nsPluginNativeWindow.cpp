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

class nsPluginNativeWindowPLATFORM : public nsPluginNativeWindow {
public: 
  nsPluginNativeWindowPLATFORM();
  virtual ~nsPluginNativeWindowPLATFORM();
};

nsPluginNativeWindowPLATFORM::nsPluginNativeWindowPLATFORM() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nullptr; 
  x = 0; 
  y = 0; 
  width = 0; 
  height = 0; 
  memset(&clipRect, 0, sizeof(clipRect));
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  ws_info = nullptr;
#endif
  type = NPWindowTypeWindow;
}

nsPluginNativeWindowPLATFORM::~nsPluginNativeWindowPLATFORM() 
{
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  *aPluginNativeWindow = new nsPluginNativeWindowPLATFORM();
  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowPLATFORM *p = (nsPluginNativeWindowPLATFORM *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}
