/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Dan Rosen <dr@netscape.com>
 *   Vidur Apparao <vidur@netscape.com>
 *   Johnny Stenback <jst@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_dom_Navigator_h
#define mozilla_dom_Navigator_h

#include "nsIDOMNavigator.h"
#include "nsIDOMNavigatorGeolocation.h"
#include "nsIDOMNavigatorDesktopNotification.h"
#include "nsIDOMClientInformation.h"
#include "nsIDOMNavigatorBattery.h"
#include "nsIDOMNavigatorSms.h"
#include "nsIDOMNavigatorNetwork.h"
#include "nsAutoPtr.h"
#include "nsWeakReference.h"

class nsPluginArray;
class nsMimeTypeArray;
class nsGeolocation;
class nsDesktopNotificationCenter;
class nsPIDOMWindow;
class nsIDOMMozConnection;

#ifdef MOZ_B2G_RIL
#include "nsIDOMNavigatorTelephony.h"
class nsIDOMTelephony;
#endif

#ifdef MOZ_B2G_BT
#include "nsIDOMNavigatorBluetooth.h"
#endif

class nsIDOMAdapter;
//*****************************************************************************
// Navigator: Script "navigator" object
//*****************************************************************************

namespace mozilla {
namespace dom {

namespace battery {
class BatteryManager;
} // namespace battery

namespace sms {
class SmsManager;
} // namespace sms

namespace network {
class Connection;
class MobileConnection;
} // namespace Connection;

namespace power {
class PowerManager;
} // namespace power

class Navigator : public nsIDOMNavigator
                , public nsIDOMClientInformation
                , public nsIDOMNavigatorGeolocation
                , public nsIDOMNavigatorDesktopNotification
                , public nsIDOMMozNavigatorBattery
                , public nsIDOMMozNavigatorSms
#ifdef MOZ_B2G_RIL
                , public nsIDOMNavigatorTelephony
#endif
                , public nsIDOMMozNavigatorNetwork
#ifdef MOZ_B2G_BT
                , public nsIDOMNavigatorBluetooth
#endif
{
public:
  Navigator(nsPIDOMWindow *aInnerWindow);
  virtual ~Navigator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMNAVIGATOR
  NS_DECL_NSIDOMCLIENTINFORMATION
  NS_DECL_NSIDOMNAVIGATORGEOLOCATION
  NS_DECL_NSIDOMNAVIGATORDESKTOPNOTIFICATION
  NS_DECL_NSIDOMMOZNAVIGATORBATTERY
  NS_DECL_NSIDOMMOZNAVIGATORSMS
#ifdef MOZ_B2G_RIL
  NS_DECL_NSIDOMNAVIGATORTELEPHONY
#endif
  NS_DECL_NSIDOMMOZNAVIGATORNETWORK

#ifdef MOZ_B2G_BT
  NS_DECL_NSIDOMNAVIGATORBLUETOOTH
#endif

  static void Init();

  void Invalidate();
  nsPIDOMWindow *GetWindow();

  void RefreshMIMEArray();

  static bool HasDesktopNotificationSupport();

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  /**
   * For use during document.write where our inner window changes.
   */
  void SetWindow(nsPIDOMWindow *aInnerWindow);

private:
  bool IsSmsAllowed() const;
  bool IsSmsSupported() const;

  nsRefPtr<nsMimeTypeArray> mMimeTypes;
  nsRefPtr<nsPluginArray> mPlugins;
  nsRefPtr<nsGeolocation> mGeolocation;
  nsRefPtr<nsDesktopNotificationCenter> mNotification;
  nsRefPtr<battery::BatteryManager> mBatteryManager;
  nsRefPtr<power::PowerManager> mPowerManager;
  nsRefPtr<sms::SmsManager> mSmsManager;
#ifdef MOZ_B2G_RIL
  nsCOMPtr<nsIDOMTelephony> mTelephony;
#endif
  nsRefPtr<network::Connection> mConnection;
  nsRefPtr<network::MobileConnection> mMobileConnection;
#ifdef MOZ_B2G_BT
  nsCOMPtr<nsIDOMBluetoothAdapter> mBluetooth;
#endif
  nsWeakPtr mWindow;
};

} // namespace dom
} // namespace mozilla

nsresult NS_GetNavigatorUserAgent(nsAString& aUserAgent);
nsresult NS_GetNavigatorPlatform(nsAString& aPlatform);
nsresult NS_GetNavigatorAppVersion(nsAString& aAppVersion);
nsresult NS_GetNavigatorAppName(nsAString& aAppName);

#endif // mozilla_dom_Navigator_h
