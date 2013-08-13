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

  [SetterThrows]
           attribute EventHandler   ondevicefound;

  // Fired when pairing process is completed
  [SetterThrows]
           attribute EventHandler   onpairedstatuschanged;

  // Fired when a2dp connection status changed
  [SetterThrows]
           attribute EventHandler   ona2dpstatuschanged;

  // Fired when handsfree connection status changed
  [SetterThrows]
           attribute EventHandler   onhfpstatuschanged;

  // Fired when sco connection status changed
  [SetterThrows]
           attribute EventHandler   onscostatuschanged;

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
  DOMRequest getConnectedDevices(unsigned short profile);
  [Creator, Throws]
  DOMRequest setPinCode(DOMString deviceAddress, DOMString pinCode);
  [Creator, Throws]
  DOMRequest setPasskey(DOMString deviceAddress, unsigned long passkey);
  [Creator, Throws]
  DOMRequest setPairingConfirmation(DOMString deviceAddress, boolean confirmation);
  [Creator, Throws]
  DOMRequest setAuthorization(DOMString deviceAddress, boolean allow);

  /**
   * Connect/Disconnect to a specific service of a target remote device.
   * To check the value of service UUIDs, please check "Bluetooth Assigned
   * Numbers" / "Service Discovery Protocol" for more information.
   *
   * @param deviceAddress Remote device address
   * @param profile 2-octets service UUID
   */
  [Creator, Throws]
  DOMRequest connect(DOMString deviceAddress, unsigned short profile);
  [Creator, Throws]
  DOMRequest disconnect(unsigned short profile);

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

  // AVRCP 1.3 methods
  [Creator,Throws]
  DOMRequest sendMediaMetaData(optional MediaMetaData mediaMetaData);
  [Creator,Throws]
  DOMRequest sendMediaPlayStatus(optional MediaPlayStatus mediaPlayStatus);
};
