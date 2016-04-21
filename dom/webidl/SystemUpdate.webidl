/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary SystemUpdateProviderInfo {
  DOMString name = "";
  DOMString uuid = "";
};

dictionary SystemUpdatePackageInfo {
  DOMString type = "";
  DOMString version = "";
  DOMString description = "";
  DOMTimeStamp buildDate = 0;
  unsigned long long size = 0;
};

[JSImplementation="@mozilla.org/system-update-provider;1",
 ChromeOnly,
 Pref="dom.system_update.enabled"]
interface SystemUpdateProvider : EventTarget {
  readonly attribute DOMString name;
  readonly attribute DOMString uuid;

  attribute EventHandler onupdateavailable;
  attribute EventHandler onprogress;
  attribute EventHandler onupdateready;
  attribute EventHandler onerror;

  void checkForUpdate();
  void startDownload();
  void stopDownload();
  void applyUpdate();
  boolean setParameter(DOMString name, DOMString value);
  DOMString getParameter(DOMString name);
};

[NavigatorProperty="updateManager",
 JSImplementation="@mozilla.org/system-update-manager;1",
 ChromeOnly,
 Pref="dom.system_update.enabled"]
interface SystemUpdateManager {
  Promise<sequence<SystemUpdateProviderInfo>> getProviders();

  Promise<SystemUpdateProvider> setActiveProvider(DOMString uuid);

  Promise<SystemUpdateProvider> getActiveProvider();
};
