/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum WifiWPSMethod {
  "pbc",
  "pin",
  "cancel"
};

enum ConnectionStatus {
  "connecting",
  "associated",
  "connected",
  "disconnected"
};

dictionary WifiWPSInfo {
  WifiWPSMethod method;
  DOMString? pin;
  DOMString? bssid;
};

dictionary NetworkProperties {
  DOMString ssid;
  sequence<DOMString>? security;
  sequence<DOMString>? capabilities;
  boolean known;
  boolean connected;
  boolean hidden;
};

[Constructor(optional NetworkProperties properties),
 JSImplementation="@mozilla.org/mozwifinetwork;1"]
interface MozWifiNetwork {
  readonly attribute DOMString ssid;
  readonly attribute any security;
  readonly attribute any capabilities;
  readonly attribute boolean known;
  readonly attribute boolean connected;
  readonly attribute boolean hidden;

           attribute DOMString? bssid;
           attribute DOMString? signalStrength;
           attribute long? relSignalStrength;
           attribute DOMString? psk;
           attribute DOMString? keyManagement;
           attribute DOMString? identity;
           attribute DOMString? password;
           attribute DOMString? phase1;
           attribute DOMString? phase2;
           attribute DOMString? eap;
           attribute DOMString? pin;
};

[JSImplementation="@mozilla.org/mozwificonnection;1"]
interface MozWifiConnection {
  readonly attribute ConnectionStatus status;
  readonly attribute MozWifiNetwork? network;
};

[JSImplementation="@mozilla.org/mozwificonnectioninfo;1"]
interface MozWifiConnectionInfo {
  readonly attribute short signalStrength;
  readonly attribute short relSignalStrength;
  readonly attribute long linkSpeed;
  readonly attribute DOMString? ipAddress;
};

dictionary IPConfiguration {
  boolean enabled;
  DOMString ipaddr;
  DOMString proxy;
  short maskLength;
  DOMString gateway;
  DOMString dns1;
  DOMString dns2;
};

[JSImplementation="@mozilla.org/wifimanager;1",
 NavigatorProperty="mozWifiManager",
 Func="Navigator::HasWifiManagerSupport"]
interface MozWifiManager : EventTarget {
  /**
   * Returns the list of currently available networks.
   * onsuccess: We have obtained the current list of networks. request.value
   *            is an object whose property names are SSIDs and values are
   *            network objects.
   * onerror: We were unable to obtain a list of property names.
   */
  DOMRequest getNetworks();

  /**
   * Returns the list of networks known to the system that will be
   * automatically connected to if they're in range.
   * onsuccess: request.value is an object whose property names are
   *            SSIDs and values are network objects.
   * onerror: We were unable to obtain a list of known networks.
   */
  DOMRequest getKnownNetworks();

  /**
   * Takes one of the networks returned from getNetworks and tries to
   * connect to it.
   * @param network A network object with information about the network,
   *                such as the SSID, key management desired, etc.
   * onsuccess: We have started attempting to associate with the network.
   *            request.value is true.
   * onerror: We were unable to select the network. This most likely means a
   *          configuration error.
   */
  DOMRequest associate(MozWifiNetwork network);

  /**
   * Given a network, removes it from the list of networks that we'll
   * automatically connect to. In order to re-connect to the network, it is
   * necessary to call associate on it.
   * @param network A network object with the SSID of the network to remove.
   * onsuccess: We have removed this network. If we were previously
   *            connected to it, we have started reconnecting to the next
   *            network in the list.
   * onerror: We were unable to remove the network.
   */
  DOMRequest forget(MozWifiNetwork network);

  /**
   * Wi-Fi Protected Setup functionality.
   * @param detail WPS detail which has 'method' and 'pin' field.
   *               The possible method field values are:
   *                 - pbc: The Push Button Configuration.
   *                 - pin: The PIN configuration.
   *                 - cancel: Request to cancel WPS in progress.
   *               If method field is 'pin', 'pin' field can exist and has
   *               a PIN number.
   *               If method field is 'pin', 'bssid' field can exist and has
   *               a opposite BSSID.
   * onsuccess: We have successfully started/canceled wps.
   * onerror: We have failed to start/cancel wps.
   */
  DOMRequest wps(optional WifiWPSInfo detail);

  /**
   * Turn on/off wifi power saving mode.
   * @param enabled true or false.
   * onsuccess: We have successfully turn on/off wifi power saving mode.
   * onerror: We have failed to turn on/off wifi power saving mode.
   */
  DOMRequest setPowerSavingMode(boolean enabled);

  /**
   * Given a network, configure using static IP instead of running DHCP
   * @param network A network object with the SSID of the network to set static ip.
   * @param info info should have following field:
   *        - enabled True to enable static IP, false to use DHCP
   *        - ipaddr configured static IP address
   *        - proxy configured proxy server address
   *        - maskLength configured mask length
   *        - gateway configured gateway address
   *        - dns1 configured first DNS server address
   *        - dns2 configured seconf DNS server address
   * onsuccess: We have successfully configure the static ip mode.
   * onerror: We have failed to configure the static ip mode.
   */
  DOMRequest setStaticIpMode(MozWifiNetwork network, optional IPConfiguration info);

  /**
   * Given a network, configure http proxy when using wifi.
   * @param network A network object with the SSID of the network to set http proxy.
   * @param info info should have following field:
   *        - httpProxyHost ip address of http proxy.
   *        - httpProxyPort port of http proxy, set 0 to use default port 8080.
   *        set info to null to clear http proxy.
   * onsuccess: We have successfully configure http proxy.
   * onerror: We have failed to configure http proxy.
   */
  DOMRequest setHttpProxy(MozWifiNetwork network, any info);

  /**
   * Returns whether or not wifi is currently enabled.
   */
  readonly attribute boolean enabled;

  /**
   * Returns the MAC address of the wifi adapter.
   */
  readonly attribute DOMString macAddress;

  /**
   * An non-null object containing the following information:
   *  - status ("disconnected", "connecting", "associated", "connected")
   *  - network
   *
   *  Note that the object returned is read only. Any changes required must
   *  be done by calling other APIs.
   */
  readonly attribute MozWifiConnection? connection;

  /**
   * A connectionInformation object with the same information found in an
   * nsIDOMMozWifiConnectionInfoEvent (but without the network).
   * If we are not currently connected to a network, this will be null.
   */
  readonly attribute MozWifiConnectionInfo? connectionInformation;

  /**
   * State notification listeners. These all take an
   * nsIDOMMozWifiStatusChangeEvent with the new status and a network (which
   * may be null).
   *
   * The possible statuses are:
   *   - connecting: Fires when we start the process of connecting to a
   *                 network.
   *   - associated: Fires when we have connected to an access point but do
   *                 not yet have an IP address.
   *   - connected: Fires once we are fully connected to an access point and
   *                can access the internet.
   *   - disconnected: Fires when we either fail to connect to an access
   *                   point (transition: associated -> disconnected) or
   *                   when we were connected to a network but have
   *                   disconnected for any reason (transition: connected ->
   *                   disconnected).
   */
  attribute EventHandler onstatuschange;

  /**
   * An event listener that is called with information about the signal
   * strength and link speed every 5 seconds.
   */
  attribute EventHandler onconnectionInfoUpdate;

  /**
   * These two events fire when the wifi system is brought online or taken
   * offline.
   */
  attribute EventHandler onenabled;
  attribute EventHandler ondisabled;
};
