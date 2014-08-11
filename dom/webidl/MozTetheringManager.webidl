/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum TetheringType {
  "bluetooth",
  "usb",
  "wifi"
};

enum SecurityType {
  "open",
  "wpa-psk",
  "wpa2-psk"
};

dictionary WifiTetheringConfig {
  DOMString ssid;
  SecurityType security;
  DOMString key;
};

dictionary TetheringConfiguration {
  DOMString ip;
  DOMString prefix;
  DOMString startIp;
  DOMString endIp;
  DOMString dns1;
  DOMString dns2;
  WifiTetheringConfig wifiConfig;
};

[JSImplementation="@mozilla.org/tetheringmanager;1",
 NavigatorProperty="mozTetheringManager",
 AvailableIn="CertifiedApps"]
interface MozTetheringManager {
  /**
   * Enable/Disable tethering.
   * @param enabled True to enable tethering, False to disable tethering.
   * @param type Tethering type to enable/disable.
   * @param config Configuration should have following fields when enable is True:
   *               - ip ip address.
   *               - prefix mask length.
   *               - startIp start ip address allocated by DHCP server for tethering.
   *               - endIp end ip address allocated by DHCP server for tethering.
   *               - dns1 first DNS server address.
   *               - dns2 second DNS server address.
   *               - wifiConfig wifi tethering configuration
   *                  - ssid SSID network name.
   *                  - security open, wpa-psk or wpa2-psk.
   *                  - key password for wpa-psk or wpa2-psk.
   *               config should not be set when enabled is False.
   */
  Promise<any> setTetheringEnabled(boolean enabled,
                                   TetheringType type,
                                   optional TetheringConfiguration config);
};
