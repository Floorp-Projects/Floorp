/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum WPSMethod {
  "pbc",
  "keypad",
  "display"
};

dictionary WPSInfo {
  WPSMethod method;
  DOMString pin;
};

[JSImplementation="@mozilla.org/wifip2pgroupowner;1"]
interface MozWifiP2pGroupOwner {
  readonly attribute DOMString groupName;
  readonly attribute DOMString macAddress;
  readonly attribute DOMString ipAddress;
  readonly attribute DOMString passphrase;
  readonly attribute DOMString ssid;
  readonly attribute any wpsCapabilities;
  readonly attribute unsigned long freq;
  readonly attribute boolean isLocal;
};

[JSImplementation="@mozilla.org/wifip2pmanager;1",
 NavigatorProperty="mozWifiP2pManager",
 Func="Navigator::HasWifiManagerSupport"]
interface MozWifiP2pManager : EventTarget
{
  /**
   * Enable/Disable wifi direct scan.
   *
   * onsuccess: Succeeded in starting/stopping wifi direct scan.
   * onerror:   Failed to start/stop wifi direct scan.
   *
   */
  DOMRequest setScanEnabled(boolean enabled);

  /**
   * Connect to a peer with given configuration.
   *
   * @param address The peer MAC address we are going to connect.
   * @param wpsMethod The WPS method we want to use.
   * @param goIntent Number from 0 ~ 15 to indicate how much we want to be
   *                 the group owner.
   *
   * onsuccess: Succeeded in issueing a 'connect' request. It doesn't mean we
   *            have connected to the peer.
   *
   * onerror:   Failed to issue a 'connect' request, probably due to an
   *            invalid peer address, unsupported wps method or any
   *            preliminary error.
   *
   **/
  DOMRequest connect(DOMString address, WPSMethod wpsMethod, optional byte goIntent);

  /**
   * Disconnect with a peer.
   *
   * @param address The mac address of the peer.
   *
   * onsuccess: Succeeded to issue a 'disconnect' request. It doesn't mean we
   *            have disconnected with the peer.
   *
   * onerror:   Failed to issue a 'disconnect' request, probably due to the
   *            invalid peer address or any preliminary error.
   *
   */
  DOMRequest disconnect(DOMString address);

  /**
   * Get peer list
   *
   * onsuccess: Command success, req.result contains an array of peer objects.
   * onerror: Command failed.
   *
   * Peer object format:
   *   .address          MAC address of the peer (string)
   *   .name             the peer's device name (string)
   *   .isGroupOwner     if the peer is the group owner (boolean)
   *   .wpsCapabilities  array of the supported |WPSMethod|
   *   .connectionStatus one of { "disconnected", "connecting", "connected", "disconnecting" }
   *
   */
  DOMRequest getPeerList();

  /**
   * Set pairing confirmation result.
   *
   * @param accepted Boolean to indicate whether we accepted the request or not.
   * @param pin The user input pin number if the wps method is keypad.
   *
   * onsuccess: Command succeeded.
   * onerror:   Command failed.
   *
   */
  DOMRequest setPairingConfirmation(boolean accepted, optional DOMString pin);

  /**
   * Set device name.
   *
   * @param devieName The new device name we are going to set.
   *
   * onsuccess: Command succeeded.
   * onerror:   Command failed.
   *
   */
  DOMRequest setDeviceName(DOMString deviceName);

  /**
   * Returns if Wifi Direct is enabled.
   *
   */
  readonly attribute boolean enabled;

  /**
   * The current group owner, null if none.
   */
  readonly attribute MozWifiP2pGroupOwner? groupOwner;

  /**
   * An event listener that is called whenever the Wifi Direct peer list is
   * updated. Use getPeerList() to get the up-to-date peer list.
   */
  attribute EventHandler onpeerinfoupdate;

  /**
   * An event listener that is called whenever Wifi Direct status changed.
   * The address of the changed peer will be stored in event.peerList.
   * See MozWifiP2pStatusChangeEvent.webidl.
   */
  attribute EventHandler onstatuschange;

  /**
   * An event listener that is called whenever Wifi Direct is enabled.
   */
  attribute EventHandler onenabled;

  /**
   * An event listener that is called whenever Wifi Direct is disabled.
   */
  attribute EventHandler ondisabled;
};
