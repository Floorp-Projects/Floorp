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
  "authenticating",
  "associated",
  "connected",
  "disconnected",
  "wps-timedout",
  "wps-failed",
  "wps-overlapped",
  "connectingfailed"
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
  DOMString bssid;
  DOMString signalStrength;
  long relSignalStrength;
  DOMString psk;
  DOMString wep;
  DOMString wep_key0;
  DOMString wep_key1;
  DOMString wep_key2;
  DOMString wep_key3;
  long wep_tx_keyidx;
  long priority;
  long scan_ssid;
  DOMString keyManagement;
  DOMString identity;
  DOMString password;
  DOMString auth_alg;
  DOMString phase1;
  DOMString phase2;
  DOMString eap;
  DOMString pin;
  boolean dontConnect;
  DOMString serverCertificate;
  DOMString subjectMatch;
};

[Constructor(optional NetworkProperties properties),
 JSImplementation="@mozilla.org/mozwifinetwork;1",
 Func="Navigator::HasWifiManagerSupport"]
interface MozWifiNetwork {
  readonly attribute DOMString ssid;
  [Constant, Cached] readonly attribute sequence<DOMString>? security;
  [Constant, Cached] readonly attribute sequence<DOMString>? capabilities;
  readonly attribute boolean known;
  readonly attribute boolean connected;
  readonly attribute boolean hidden;

           attribute DOMString? bssid;
           attribute DOMString? signalStrength;
           attribute long? relSignalStrength;
           attribute DOMString? psk;
           attribute DOMString? wep;
           attribute DOMString? wep_key0;
           attribute DOMString? wep_key1;
           attribute DOMString? wep_key2;
           attribute DOMString? wep_key3;
           attribute long? wep_tx_keyidx;
           attribute long? priority;
           attribute long? scan_ssid;
           attribute DOMString? keyManagement;
           attribute DOMString? identity;
           attribute DOMString? password;
           attribute DOMString? auth_alg;
           attribute DOMString? phase1;
           attribute DOMString? phase2;
           attribute DOMString? eap;
           attribute DOMString? pin;
           attribute boolean? dontConnect;
           attribute DOMString? serverCertificate;
           attribute DOMString? subjectMatch;
};

[JSImplementation="@mozilla.org/mozwificonnection;1",
 ChromeOnly]
interface MozWifiConnection {
  readonly attribute ConnectionStatus status;
  readonly attribute MozWifiNetwork? network;
};

[JSImplementation="@mozilla.org/mozwificonnectioninfo;1",
 ChromeOnly]
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
   * Turn on/off wifi functionality.
   * @param enable true for enable, false for disable.
   * onsuccess: Wifi enable/disable successfully, including no status change.
   * onerror: Wifi enable/disable failed or prohibited.
   */
  DOMRequest setWifiEnabled(boolean enabled);

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
   * Import a certificate file, only support CA certificate now.
   * @param certBlob A Blob object containing raw data of certificate to be imported.
   *                 Supported format: binary/base64 encoded DER certificates.
   *                                   (.der/.crt/.cer/.pem)
   *                 Cause error if importing certificates already imported.
   * @param certPassword Password of certificate.
   * @param certNickname User assigned nickname for imported certificate.
   *                     Nickname must be unique, it causes error on redundant nickname.
   * onsuccess: We have successfully imported certificate. request.result is an
   *            object, containing information of imported CA:
   *            {
   *              nickname:  Nickname of imported CA, String.
   *              usage:     Supported usage of imported CA, Array of String,
   *                         includes: "ServerCert".
   *            }
   * onerror: We have failed to import certificate.
   */
  DOMRequest importCert(Blob certBlob,
                        DOMString certPassword,
                        DOMString certNickname);

  /**
   * Get list of imported WIFI certificates.
   * onsuccess: We have successfully gotten imported certificate list.
   *            request.result is an object using nickname as key, array of usage
   *            string as value.
   *            request.result[USAGE] = [CA_NICKNAME1, CA_NICKNAME2, ...]
   *            USAGE string includes: "ServerCert".
   * onerror: We have failed to list certificate.
   */
  DOMRequest getImportedCerts();

  /**
   * Delete an imported certificate.
   * @param certNickname Nickname of imported to be deleted.
   * onsuccess: We have successfully deleted certificate.
   * onerror: We have failed to delete certificate.
   */
  DOMRequest deleteCert(DOMString certNickname);

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
  readonly attribute MozWifiConnection connection;

  /**
   * A connectionInformation object with the same information found in an
   * MozWifiConnectionInfoEvent (but without the network).
   * If we are not currently connected to a network, this will be null.
   */
  readonly attribute MozWifiConnectionInfo? connectionInformation;

  /**
   * Capabilities of Wifi.
   */
  readonly attribute MozWifiCapabilities? capabilities;

  /**
   * State notification listeners. These all take an
   * MozWifiStatusChangeEvent with the new status and a network (which may be
   * null).
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
  attribute EventHandler onconnectioninfoupdate;

  /**
   * These two events fire when the wifi system is brought online or taken
   * offline.
   */
  attribute EventHandler onenabled;
  attribute EventHandler ondisabled;

  /**
   * An event listener that is called with information about the number
   * of wifi stations connected to wifi hotspot every 5 seconds.
   */
  attribute EventHandler onstationinfoupdate;
};
