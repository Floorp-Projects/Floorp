/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// MediaMetadata and MediaPlayStatus are used to keep data from Applications.
// Please see specification of AVRCP 1.3 for more details.
dictionary MediaMetaData
{
  // track title
  DOMString   title = "";
  // artist name
  DOMString   artist = "";
  // album name
  DOMString   album = "";
  // track number
  long long   mediaNumber = -1;
  // number of tracks in the album
  long long   totalMediaCount = -1;
  // playing time (ms)
  long long   duration = -1;
};

dictionary MediaPlayStatus
{
  // current track length (ms)
  long long   duration = -1;
  // playing time (ms)
  long long   position = -1;
  // one of 'STOPPED'/'PLAYING'/'PAUSED'/'FWD_SEEK'/'REV_SEEK'/'ERROR'
  DOMString   playStatus = "";
};

[CheckPermissions="bluetooth"]
interface BluetoothAdapter : EventTarget {
  readonly attribute BluetoothAdapterState  state;
  [AvailableIn=CertifiedApps]
  readonly attribute DOMString              address;
  readonly attribute DOMString              name;
  readonly attribute boolean                discoverable;
  readonly attribute boolean                discovering;

  [AvailableIn=CertifiedApps]
  readonly attribute BluetoothPairingListener pairingReqs;

  // Fired when attribute(s) of BluetoothAdapter changed
           attribute EventHandler   onattributechanged;

  // Fired when a remote device gets paired with the adapter
           attribute EventHandler   ondevicepaired;

  // Fired when a remote device gets unpaired from the adapter
           attribute EventHandler   ondeviceunpaired;

  // Fired when a2dp connection status changed
           attribute EventHandler   ona2dpstatuschanged;

  // Fired when handsfree connection status changed
           attribute EventHandler   onhfpstatuschanged;

  // Fired when sco connection status changed
           attribute EventHandler   onscostatuschanged;

  // Fired when remote devices query current media play status
           attribute EventHandler   onrequestmediaplaystatus;

  /**
   * Enable/Disable a local bluetooth adapter by asynchronus methods and return
   * its result through a Promise.
   *
   * Several onattributechanged events would be triggered during processing the
   * request, and the last one indicates adapter.state becomes enabled/disabled.
   */
  [NewObject, AvailableIn=CertifiedApps]
  Promise<void> enable();
  [NewObject, AvailableIn=CertifiedApps]
  Promise<void> disable();

  [NewObject, AvailableIn=CertifiedApps]
  Promise<void> setName(DOMString aName);
  [NewObject]
  Promise<void> setDiscoverable(boolean aDiscoverable);

  [NewObject]
  Promise<BluetoothDiscoveryHandle> startDiscovery();
  [NewObject]
  Promise<void> stopDiscovery();

  [NewObject]
  Promise<void> pair(DOMString deviceAddress);
  [NewObject]
  Promise<void> unpair(DOMString deviceAddress);

  sequence<BluetoothDevice> getPairedDevices();

  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest getConnectedDevices(unsigned short serviceUuid);

  /**
   * Connect/Disconnect to a specific service of a target remote device.
   * To check the value of service UUIDs, please check "Bluetooth Assigned
   * Numbers" / "Service Discovery Protocol" for more information.
   *
   * Note that service UUID is optional. If it isn't passed when calling
   * Connect, multiple profiles are tried sequentially based on the class of
   * device (CoD). If it isn't passed when calling Disconnect, all connected
   * profiles are going to be closed.
   *
   * Reply success if the connection of any profile is successfully
   * established/released; reply error if we failed to connect/disconnect all
   * of the planned profiles.
   *
   * @param device Remote device
   * @param profile 2-octets service UUID. This is optional.
   */
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest connect(BluetoothDevice device, optional unsigned short serviceUuid);

  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest disconnect(BluetoothDevice device, optional unsigned short serviceUuid);

  // One device can only send one file at a time
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest sendFile(DOMString deviceAddress, Blob blob);
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest stopSendingFile(DOMString deviceAddress);
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest confirmReceivingFile(DOMString deviceAddress, boolean confirmation);

  // Connect/Disconnect SCO (audio) connection
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest connectSco();
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest disconnectSco();
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest isScoConnected();

  /**
   * Additional HFP methods to handle CDMA network.
   *
   * In GSM network we observe call operations from RIL call state changes;
   * however in CDMA network RIL call states do not change under some call
   * operations, so we need these additional methods to be informed of these
   * operations from dialer.
   *
   * For more information please refer to bug 912005 and 925638.
   */
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest answerWaitingCall();
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest ignoreWaitingCall();
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest toggleCalls();

  // AVRCP 1.3 methods
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest sendMediaMetaData(optional MediaMetaData mediaMetaData);
  [NewObject, Throws, AvailableIn=CertifiedApps]
  DOMRequest sendMediaPlayStatus(optional MediaPlayStatus mediaPlayStatus);
};

enum BluetoothAdapterState
{
  "disabled",
  "disabling",
  "enabled",
  "enabling"
};

enum BluetoothAdapterAttribute
{
  "unknown",
  "state",
  "address",
  "name",
  "discoverable",
  "discovering"
};

