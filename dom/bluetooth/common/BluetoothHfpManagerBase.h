/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanagerbase_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanagerbase_h__

#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHfpManagerBase : public BluetoothProfileManagerBase
{
public:
  /**
   * Returns true if Sco is connected.
   */
  virtual bool IsScoConnected() = 0;

  virtual bool IsNrecEnabled() = 0;
};

#define BT_DECL_HFP_MGR_BASE                  \
  BT_DECL_PROFILE_MGR_BASE                    \
  virtual bool IsScoConnected() override;     \
  virtual bool IsNrecEnabled() override;

END_BLUETOOTH_NAMESPACE

#endif  //#ifndef mozilla_dom_bluetooth_bluetoothhfpmanagerbase_h__
