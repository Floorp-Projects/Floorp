/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsPluginNativeWindow_h_
#define _nsPluginNativeWindow_h_

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsNPAPIPluginInstance.h"
#include "npapi.h"
#include "nsIWidget.h"

/**
 * base class for native plugin window implementations
 */
class nsPluginNativeWindow : public NPWindow {
 public:
  nsPluginNativeWindow() : NPWindow() { MOZ_COUNT_CTOR(nsPluginNativeWindow); }

  virtual ~nsPluginNativeWindow() { MOZ_COUNT_DTOR(nsPluginNativeWindow); }

  /**
   *   !!! CAUTION !!!
   *
   * The base class |nsPluginWindow| is defined as a struct in nsplugindefs.h,
   * thus it does not have a destructor of its own.
   * One should never attempt to delete |nsPluginNativeWindow| object instance
   * (or derivatives) using a pointer of |nsPluginWindow *| type. Should such
   * necessity occur it must be properly casted first.
   */

 public:
  nsresult GetPluginInstance(RefPtr<nsNPAPIPluginInstance>& aPluginInstance) {
    aPluginInstance = mPluginInstance;
    return NS_OK;
  }
  nsresult SetPluginInstance(nsNPAPIPluginInstance* aPluginInstance) {
    if (mPluginInstance != aPluginInstance) mPluginInstance = aPluginInstance;
    return NS_OK;
  }

  nsresult GetPluginWidget(nsIWidget** aWidget) const {
    NS_IF_ADDREF(*aWidget = mWidget);
    return NS_OK;
  }
  nsresult SetPluginWidget(nsIWidget* aWidget) {
    mWidget = aWidget;
    return NS_OK;
  }

 public:
  virtual nsresult CallSetWindow(
      RefPtr<nsNPAPIPluginInstance>& aPluginInstance) {
    // null aPluginInstance means that we want to call SetWindow(null)
    if (aPluginInstance)
      aPluginInstance->SetWindow(this);
    else if (mPluginInstance)
      mPluginInstance->SetWindow(nullptr);

    SetPluginInstance(aPluginInstance);
    return NS_OK;
  }

 protected:
  RefPtr<nsNPAPIPluginInstance> mPluginInstance;
  nsCOMPtr<nsIWidget> mWidget;
};

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow** aPluginNativeWindow);
nsresult PLUG_DeletePluginNativeWindow(
    nsPluginNativeWindow* aPluginNativeWindow);

#endif  //_nsPluginNativeWindow_h_
