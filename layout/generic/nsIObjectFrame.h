/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for rendering objects for replaced elements implemented by
 * a plugin
 */

#ifndef nsIObjectFrame_h___
#define nsIObjectFrame_h___

#include "nsQueryFrame.h"

class nsNPAPIPluginInstance;
class nsIWidget;

class nsIObjectFrame : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIObjectFrame)

  NS_IMETHOD GetPluginInstance(nsNPAPIPluginInstance** aPluginInstance) = 0;

  /**
   * Get the native widget for the plugin, if any.
   */
  virtual nsIWidget* GetWidget() = 0;

  /**
   * Update plugin active state. Frame should update if it is on an active tab
   * or not and forward that information to the plugin to make it possible to
   * throttle down plugin instance in non active case.
   */
  virtual void SetIsDocumentActive(bool aIsActive) = 0;
};

#endif /* nsIObjectFrame_h___ */
