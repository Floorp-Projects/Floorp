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

interface BluetoothAdapter : EventTarget {
  readonly attribute DOMString      address;
  readonly attribute unsigned long  class;
  readonly attribute boolean        discovering;
  readonly attribute DOMString      name;
  readonly attribute boolean        discoverable;
  readonly attribute unsigned long  discoverableTimeout; // in seconds

  // array of type BluetoothDevice[]
  [GetterThrows]
  readonly attribute any            devices;

  // array of type DOMString[]
  [GetterThrows]
  readonly attribute any            uuids;

           attribute EventHandler   ondevicefound;

  // Fired when pairing process is completed
           attribute EventHandler   onpairedstatuschanged;

  // Fired when a2dp connection status changed
           attribute EventHandler   ona2dpstatuschanged;

  // Fired when handsfree connection status changed
           attribute EventHandler   onhfpstatuschanged;

  // Fired when sco connection status changed
           attribute EventHandler   onscostatuschanged;

  // Fired when remote devices query current media play status
           attribute EventHandler   onrequestmediaplaystatus;

  [Creator, Throws]
  DOMRequest setName(DOMString name);
  [Creator, Throws]
  DOMRequest setDiscoverable(boolean discoverable);
  [Creator, Throws]
  DOMRequest setDiscoverableTimeout(unsigned long timeout);
  [Creator, Throws]
  DOMRequest startDiscovery();
  [Creator, Throws]
  DOMRequest stopDiscovery();
  [Creator, Throws]
  DOMRequest pair(BluetoothDevice device);
  [Creator, Throws]
  DOMRequest unpair(BluetoothDevice device);
  [Creator, Throws]
  DOMRequest getPairedDevices();
  [Creator, Throws]
  DOMRequest getConnectedDevices(unsigned short serviceUuid);
  [Creator, Throws]
  DOMRequest setPinCode(DOMString deviceAddress, DOMString pinCode);
  [Creator, Throws]
  DOMRequest setPasskey(DOMString deviceAddress, unsigned long passkey);
  [Creator, Throws]
  DOMRequest setPairingConfirmation(DOMString deviceAddress, boolean confirmation);

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
  [Creator, Throws]
  DOMRequest connect(BluetoothDevice device, optional unsigned short serviceUuid);

  [Creator, Throws]
  DOMRequest disconnect(BluetoothDevice device, optional unsigned short serviceUuid);

  // One device can only send one file at a time
  [Creator, Throws]
  DOMRequest sendFile(DOMString deviceAddress, Blob blob);
  [Creator, Throws]
  DOMRequest stopSendingFile(DOMString deviceAddress);
  [Creator, Throws]
  DOMRequest confirmReceivingFile(DOMString deviceAddress, boolean confirmation);

  // Connect/Disconnect SCO (audio) connection
  [Creator, Throws]
  DOMRequest connectSco();
  [Creator, Throws]
  DOMRequest disconnectSco();
  [Creator, Throws]
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
  [Creator, Throws]
  DOMRequest answerWaitingCall();
  [Creator, Throws]
  DOMRequest ignoreWaitingCall();
  [Creator, Throws]
  DOMRequest toggleCalls();

  // AVRCP 1.3 methods
  [Creator,Throws]
  DOMRequest sendMediaMetaData(optional MediaMetaData mediaMetaData);
  [Creator,Throws]
  DOMRequest sendMediaPlayStatus(optional MediaPlayStatus mediaPlayStatus);
};
