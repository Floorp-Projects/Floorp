/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */
/* globals Components, dump */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

// globals XPCOMUtils
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
// globals Services
Cu.import("resource://gre/modules/Services.jsm");
// globals Messaging
Cu.import("resource://gre/modules/Messaging.jsm");

function log(str) {
  // dump("-*- AndroidCastDeviceProvider -*-: " + str + "\n");
}

// Helper function: transfer nsIPresentationChannelDescription to json
function descriptionToString(aDescription) {
  let json = {};
  json.type = aDescription.type;
  switch(aDescription.type) {
    case Ci.nsIPresentationChannelDescription.TYPE_TCP:
      let addresses = aDescription.tcpAddress.QueryInterface(Ci.nsIArray);
      json.tcpAddress = [];
      for (let idx = 0; idx < addresses.length; idx++) {
        let address = addresses.queryElementAt(idx, Ci.nsISupportsCString);
        json.tcpAddress.push(address.data);
      }
      json.tcpPort = aDescription.tcpPort;
      break;
    case Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL:
      json.dataChannelSDP = aDescription.dataChannelSDP;
      break;
  }
  return JSON.stringify(json);
}

const TOPIC_ANDROID_CAST_DEVICE_SYNCDEVICE = "AndroidCastDevice:SyncDevice";
const TOPIC_ANDROID_CAST_DEVICE_ADDED      = "AndroidCastDevice:Added";
const TOPIC_ANDROID_CAST_DEVICE_CHANGED    = "AndroidCastDevice:Changed";
const TOPIC_ANDROID_CAST_DEVICE_REMOVED    = "AndroidCastDevice:Removed";
const TOPIC_ANDROID_CAST_DEVICE_START      = "AndroidCastDevice:Start";
const TOPIC_ANDROID_CAST_DEVICE_STOP       = "AndroidCastDevice:Stop";
const TOPIC_PRESENTATION_VIEW_READY        = "presentation-view-ready";

function LocalControlChannel(aProvider, aDeviceId, aRole) {
  log("LocalControlChannel - create new LocalControlChannel for : "
      + aRole);
  this._provider = aProvider;
  this._deviceId = aDeviceId;
  this._role     = aRole;
}

LocalControlChannel.prototype = {
  _listener: null,
  _provider: null,
  _deviceId: null,
  _role: null,
  _isOnTerminating: false,
  _isOnDisconnecting: false,
  _pendingConnected: false,
  _pendingDisconnect: null,
  _pendingOffer: null,
  _pendingCandidate: null,
  /* For the controller, it would be the control channel of the receiver.
   * For the receiver, it would be the control channel of the controller. */
  _correspondingControlChannel: null,

  set correspondingControlChannel(aCorrespondingControlChannel) {
    this._correspondingControlChannel = aCorrespondingControlChannel;
  },

  get correspondingControlChannel() {
    return this._correspondingControlChannel;
  },

  notifyConnected: function LCC_notifyConnected() {
    this._pendingDisconnect = null;

    if (!this._listener) {
      this._pendingConnected = true;
    } else {
      this._listener.notifyConnected();
    }
  },

  onOffer: function LCC_onOffer(aOffer) {
    if (this._role == Ci.nsIPresentationService.ROLE_CONTROLLER) {
      log("LocalControlChannel - onOffer of controller should not be called.");
      return;
    }
    if (!this._listener) {
      this._pendingOffer = aOffer;
    } else {
      this._listener.onOffer(aOffer);
    }
  },

  onAnswer: function LCC_onAnswer(aAnswer) {
    if (this._role == Ci.nsIPresentationService.ROLE_RECEIVER) {
      log("LocalControlChannel - onAnswer of receiver should not be called.");
      return;
    }
    this._listener.onAnswer(aAnswer);
  },

  notifyIceCandidate: function LCC_notifyIceCandidate(aCandidate) {
    if (!this._listener) {
      this._pendingCandidate = aCandidate;
    } else {
      this._listener.onIceCandidate(aCandidate);
    }
  },

  // nsIPresentationControlChannel
  get listener() {
    return this._listener;
  },

  set listener(aListener) {
    this._listener = aListener;

    if (!this._listener) {
      return;
    }

    if (this._pendingConnected) {
      this.notifyConnected();
      this._pendingConnected = false;
    }

    if (this._pendingOffer) {
      this.onOffer(this._pendingOffer);
      this._pendingOffer = null;
    }

    if (this._pendingCandidate) {
      this.notifyIceCandidate(this._pendingCandidate);
      this._pendingCandidate = null;
    }

    if (this._pendingDisconnect != null) {
      this.disconnect(this._pendingDisconnect);
      this._pendingDisconnect = null;
    }
  },

  sendOffer: function LCC_sendOffer(aOffer) {
    if (this._role == Ci.nsIPresentationService.ROLE_RECEIVER) {
      log("LocalControlChannel - sendOffer of receiver should not be called.");
      return;
    }
    log("LocalControlChannel - sendOffer aOffer=" + descriptionToString(aOffer));
    this._correspondingControlChannel.onOffer(aOffer);
  },

  sendAnswer: function LCC_sendAnswer(aAnswer) {
    if (this._role == Ci.nsIPresentationService.ROLE_CONTROLLER) {
      log("LocalControlChannel - sendAnswer of controller should not be called.");
      return;
    }
    log("LocalControlChannel - sendAnswer aAnswer=" + descriptionToString(aAnswer));
    this._correspondingControlChannel.onAnswer(aAnswer);
  },

  sendIceCandidate: function LCC_sendIceCandidate(aCandidate) {
    log("LocalControlChannel - sendAnswer aCandidate=" + aCandidate);
    this._correspondingControlChannel.notifyIceCandidate(aCandidate);
  },

  launch: function LCC_launch(aPresentationId, aUrl) {
    log("LocalControlChannel - launch aPresentationId="
        + aPresentationId + " aUrl=" + aUrl);
    // Create control channel for receiver directly.
    let controlChannel = new LocalControlChannel(this._provider,
                                                 this._deviceId,
                                                 Ci.nsIPresentationService.ROLE_RECEIVER);

    // Set up the corresponding control channels for both controller and receiver.
    this._correspondingControlChannel = controlChannel;
    controlChannel._correspondingControlChannel = this;

    this._provider.onSessionRequest(this._deviceId,
                                    aUrl,
                                    aPresentationId,
                                    controlChannel);
    controlChannel.notifyConnected();
  },

  terminate: function LCC_terminate(aPresentationId) {
    log("LocalControlChannel - terminate aPresentationId="
        + aPresentationId);

    if (this._isOnTerminating) {
      return;
    }

    // Create control channel for corresponding role directly.
    let correspondingRole = this._role == Ci.nsIPresentationService.ROLE_CONTROLLER
                          ? Ci.nsIPresentationService.ROLE_RECEIVER
                          : Ci.nsIPresentationService.ROLE_CONTROLLER;
    let controlChannel = new LocalControlChannel(this._provider,
                                                 this._deviceId,
                                                 correspondingRole);
    // Prevent the termination recursion.
    controlChannel._isOnTerminating = true;

    // Set up the corresponding control channels for both controller and receiver.
    this._correspondingControlChannel = controlChannel;
    controlChannel._correspondingControlChannel = this;

    this._provider.onTerminateRequest(this._deviceId,
                                      aPresentationId,
                                      controlChannel,
                                      this._role == Ci.nsIPresentationService.ROLE_RECEIVER);
    controlChannel.notifyConnected();
  },

  disconnect: function LCC_disconnect(aReason) {
    log("LocalControlChannel - disconnect aReason=" + aReason);

    if (this._isOnDisconnecting) {
      return;
    }

    this._pendingOffer = null;
    this._pendingCandidate = null;
    this._pendingConnected = false;

    // this._pendingDisconnect is a nsresult.
    // If it is null, it means no pending disconnect.
    // If it is NS_OK, it means this control channel is disconnected normally.
    // If it is other nsresult value, it means this control channel is
    // disconnected abnormally.

    // Remote endpoint closes the control channel with abnormal reason.
    if (aReason == Cr.NS_OK &&
        this._pendingDisconnect != null &&
        this._pendingDisconnect != Cr.NS_OK) {
      aReason = this._pendingDisconnect;
    }

    if (!this._listener) {
      this._pendingDisconnect = aReason;
      return;
    }

    this._isOnDisconnecting = true;
    this._correspondingControlChannel.disconnect(aReason);
    this._listener.notifyDisconnected(aReason);
  },

  reconnect: function LCC_reconnect(aPresentationId, aUrl) {
    log("1-UA on Android doesn't support reconnect.");
    throw Cr.NS_ERROR_FAILURE;
  },

  classID: Components.ID("{c9be9450-e5c7-4294-a287-376971b017fd}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
};

function ChromecastRemoteDisplayDevice(aProvider, aId, aName, aRole) {
  this._provider = aProvider;
  this._id       = aId;
  this._name     = aName;
  this._role     = aRole;
}

ChromecastRemoteDisplayDevice.prototype = {
  _id:          null,
  _name:        null,
  _role:        null,
  _provider:    null,
  _ctrlChannel: null,

  update: function CRDD_update(aName) {
    this._name = aName || this._name;
  },

  // nsIPresentationDevice
  get id() { return this._id; },

  get name() { return this._name; },

  get type() { return "chromecast"; },

  establishControlChannel: function CRDD_establishControlChannel() {
    this._ctrlChannel = new LocalControlChannel(this._provider,
                                                this._id,
                                                this._role);

    if (this._role == Ci.nsIPresentationService.ROLE_CONTROLLER) {
      // Only connect to Chromecast for controller.
      // Monitor the receiver being ready.
      Services.obs.addObserver(this, TOPIC_PRESENTATION_VIEW_READY, true);

      // Launch Chromecast service in Android.
      Messaging.sendRequestForResult({
        type: TOPIC_ANDROID_CAST_DEVICE_START,
        id:   this.id
      }).then(result => {
        log("Chromecast is connected.");
      }).catch(error => {
        log("Can not connect to Chromecast.");
        // If Chromecast can not be launched, remove the observer.
        Services.obs.removeObserver(this, TOPIC_PRESENTATION_VIEW_READY);
        this._ctrlChannel.disconnect(Cr.NS_ERROR_FAILURE);
      });
    } else {
      // If establishControlChannel called from the receiver, we don't need to
      // wait the 'presentation-view-ready' event.
      this._ctrlChannel.notifyConnected();
    }

    return this._ctrlChannel;
  },

  disconnect: function CRDD_disconnect() {
    // Disconnect from Chromecast.
    Messaging.sendRequestForResult({
      type: TOPIC_ANDROID_CAST_DEVICE_STOP,
      id:   this.id
    });
  },

  isRequestedUrlSupported: function CRDD_isRequestedUrlSupported(aUrl) {
    let url = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService)
                .newURI(aUrl);
    return url.scheme == "http" || url.scheme == "https";
  },

  // nsIPresentationLocalDevice
  get windowId() { return this._id; },

  // nsIObserver
  observe: function CRDD_observe(aSubject, aTopic, aData) {
    if (aTopic == TOPIC_PRESENTATION_VIEW_READY) {
      log("ChromecastRemoteDisplayDevice - observe: aTopic="
          + aTopic + " data=" + aData);
      if (this.windowId === aData) {
        Services.obs.removeObserver(this, TOPIC_PRESENTATION_VIEW_READY);
        this._ctrlChannel.notifyConnected();
      }
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice,
                                         Ci.nsIPresentationLocalDevice,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),
};

function AndroidCastDeviceProvider() {
}

AndroidCastDeviceProvider.prototype = {
  _listener: null,
  _deviceList: new Map(),

  onSessionRequest: function APDP_onSessionRequest(aDeviceId,
                                                   aUrl,
                                                   aPresentationId,
                                                   aControlChannel) {
    log("AndroidCastDeviceProvider - onSessionRequest"
        + " aDeviceId=" + aDeviceId);
    let device = this._deviceList.get(aDeviceId);
    let receiverDevice = new ChromecastRemoteDisplayDevice(this,
                                                           device.id,
                                                           device.name,
                                                           Ci.nsIPresentationService.ROLE_RECEIVER);
    this._listener.onSessionRequest(receiverDevice,
                                    aUrl,
                                    aPresentationId,
                                    aControlChannel);
  },

  onTerminateRequest: function APDP_onTerminateRequest(aDeviceId,
                                                       aPresentationId,
                                                       aControlChannel,
                                                       aIsFromReceiver) {
    log("AndroidCastDeviceProvider - onTerminateRequest"
        + " aDeviceId=" + aDeviceId
        + " aPresentationId=" + aPresentationId
        + " aIsFromReceiver=" + aIsFromReceiver);
    let device = this._deviceList.get(aDeviceId);
    this._listener.onTerminateRequest(device,
                                      aPresentationId,
                                      aControlChannel,
                                      aIsFromReceiver);
  },

  // nsIPresentationDeviceProvider
  set listener(aListener) {
    this._listener = aListener;

    // When unload this provider.
    if (!this._listener) {
      // remove observer
      Services.obs.removeObserver(this, TOPIC_ANDROID_CAST_DEVICE_ADDED);
      Services.obs.removeObserver(this, TOPIC_ANDROID_CAST_DEVICE_CHANGED);
      Services.obs.removeObserver(this, TOPIC_ANDROID_CAST_DEVICE_REMOVED);
      return;
    }

    // Sync all device already found by Android.
    Messaging.sendRequest({ type: TOPIC_ANDROID_CAST_DEVICE_SYNCDEVICE });
    // Observer registration
    Services.obs.addObserver(this, TOPIC_ANDROID_CAST_DEVICE_ADDED, false);
    Services.obs.addObserver(this, TOPIC_ANDROID_CAST_DEVICE_CHANGED, false);
    Services.obs.addObserver(this, TOPIC_ANDROID_CAST_DEVICE_REMOVED, false);
  },

  get listener() {
    return this._listener;
  },

  forceDiscovery: function APDP_forceDiscovery() {
    // There is no API to do force discovery in Android SDK.
  },

  // nsIObserver
  observe: function APDP_observe(aSubject, aTopic, aData) {
    log('observe ' + aTopic + ': ' + aData);
    switch (aTopic) {
      case TOPIC_ANDROID_CAST_DEVICE_ADDED:
      case TOPIC_ANDROID_CAST_DEVICE_CHANGED: {
        let deviceInfo = JSON.parse(aData);
        let deviceId   = deviceInfo.uuid;

        if (!this._deviceList.has(deviceId)) {
          let device = new ChromecastRemoteDisplayDevice(this,
                                                         deviceInfo.uuid,
                                                         deviceInfo.friendlyName,
                                                         Ci.nsIPresentationService.ROLE_CONTROLLER);
          this._deviceList.set(device.id, device);
          this._listener.addDevice(device);
        } else {
          let device = this._deviceList.get(deviceId);
          device.update(deviceInfo.friendlyName);
          this._listener.updateDevice(device);
        }
        break;
      }
      case TOPIC_ANDROID_CAST_DEVICE_REMOVED: {
        let deviceId = aData;
        if (!this._deviceList.has(deviceId)) {
          break;
        }

        let device   = this._deviceList.get(deviceId);
        this._listener.removeDevice(device);
        this._deviceList.delete(deviceId);
        break;
      }
    }
  },

  classID: Components.ID("{7394f24c-dbc3-48c8-8a47-cd10169b7c6b}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIPresentationDeviceProvider]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AndroidCastDeviceProvider]);
