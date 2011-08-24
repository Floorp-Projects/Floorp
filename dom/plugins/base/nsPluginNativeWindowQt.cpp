/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robin Lu <robin.lu@sun.com>
 *   Miika Jarvinen <mjarvin@gmail.com>
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

  virtual nsresult CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance);
private:

  NPSetWindowCallbackStruct mWsInfo;
};

nsPluginNativeWindowQt::nsPluginNativeWindowQt() : nsPluginNativeWindow()
{
  //Initialize member variables
#ifdef DEBUG
  fprintf(stderr,"\n\n\nCreating plugin native window %p\n\n\n", (void *) this);
#endif
  window = nsnull;
  x = 0;
  y = 0;
  width = 0;
  height = 0;
  memset(&clipRect, 0, sizeof(clipRect));
  ws_info = &mWsInfo;
  type = NPWindowTypeWindow;
  mWsInfo.type = 0;
#if defined(MOZ_X11)
  mWsInfo.display = nsnull;
  mWsInfo.visual = nsnull;
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
  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowQt *p = (nsPluginNativeWindowQt *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}

nsresult nsPluginNativeWindowQt::CallSetWindow(nsRefPtr<nsNPAPIPluginInstance> &aPluginInstance)
{
  if (aPluginInstance) {
    if (type == NPWindowTypeWindow) {
      return NS_ERROR_FAILURE;
    } // NPWindowTypeWindow
    aPluginInstance->SetWindow(this);
  }
  else if (mPluginInstance)
    mPluginInstance->SetWindow(nsnull);

  SetPluginInstance(aPluginInstance);
  return NS_OK;
}
