/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const CONNECTION_STATUS_DISCONNECTED  = "disconnected";
const CONNECTION_STATUS_CONNECTING    = "connecting";
const CONNECTION_STATUS_CONNECTED     = "connected";
const CONNECTION_STATUS_DISCONNECTING = "disconnecting";

const DEBUG = false;

this.EXPORTED_SYMBOLS = ["WifiP2pWorkerObserver"];

// WifiP2pWorkerObserver resides in WifiWorker to handle DOM message
// by either 1) returning internally maintained information or
//           2) delegating to aDomMsgResponder. It is also responsible
// for observing events from WifiP2pManager and dispatch to DOM.
//
// @param aDomMsgResponder handles DOM messages, including
//        - setScanEnabled
//        - connect
//        - disconnect
//        - setPairingConfirmation
//        The instance is actually WifiP2pManager.
this.WifiP2pWorkerObserver = function(aDomMsgResponder) {
  function debug(aMsg) {
    if (DEBUG) {
      dump('-------------- WifiP2pWorkerObserver: ' + aMsg);
    }
  }

  // Private member variables.
  let _localDevice;
  let _peerList = {}; // List of P2pDevice.
  let _domManagers = [];
  let _enabled = false;
  let _groupOwner = null;
  let _currentPeer = null;

  // Constructor of P2pDevice. It will be exposed to DOM.
  //
  // @param aPeer object representing a P2P device:
  //   .name: string for the device name.
  //   .address: Mac address.
  //   .isGroupOwner: boolean to indicate if this device is the group owner.
  //   .wpsCapabilities: array of string of {"pbc", "display", "keypad"}.
  function P2pDevice(aPeer) {
    this.address = aPeer.address;
    this.name = (aPeer.name ? aPeer.name : aPeer.address);
    this.isGroupOwner = aPeer.isGroupOwner;
    this.wpsCapabilities = aPeer.wpsCapabilities;
    this.connectionStatus = CONNECTION_STATUS_DISCONNECTED;

    // Since this object will be exposed to web, defined the exposed
    // properties here.
    this.__exposedProps__ = {
      address: "r",
      name: "r",
      isGroupOwner: "r",
      wpsCapabilities: "r",
      connectionStatus: "r"
    };
  }

  // Constructor of P2pGroupOwner.
  //
  // @param aGroupOwner:
  //   .macAddress
  //   .ipAddress
  //   .passphrase
  //   .ssid
  //   .freq
  //   .isLocal
  function P2pGroupOwner(aGroupOwner) {
    this.macAddress = aGroupOwner.macAddress; // The identifier to get further information.
    this.ipAddress = aGroupOwner.ipAddress;
    this.passphrase = aGroupOwner.passphrase;
    this.ssid = aGroupOwner.ssid; // e.g. DIRECT-xy.
    this.freq = aGroupOwner.freq;
    this.isLocal = aGroupOwner.isLocal;

    let detail = _peerList[aGroupOwner.macAddress];
    if (detail) {
      this.name = detail.name;
      this.wpsCapabilities = detail.wpsCapabilities;
    } else if (_localDevice.address === this.macAddress) {
      this.name = _localDevice.name;
      this.wpsCapabilities = _localDevice.wpsCapabilities;
    } else {
      debug("We don't know this group owner: " + aGroupOwner.macAddress);
      this.name = aGroupOwner.macAddress;
      this.wpsCapabilities = [];
    }
  }

  function fireEvent(aMessage, aData) {
    debug('domManager: ' + JSON.stringify(_domManagers));
    _domManagers.forEach(function(manager) {
      // Note: We should never have a dead message manager here because we
      // observe our child message managers shutting down below.
      manager.sendAsyncMessage("WifiP2pManager:" + aMessage, aData);
    });
  }

  function addDomManager(aMsg) {
    if (-1 === _domManagers.indexOf(aMsg.manager)) {
      _domManagers.push(aMsg.manager);
    }
  }

  function returnMessage(aMessage, aSuccess, aData, aMsg) {
    let rMsg = aMessage + ":Return:" + (aSuccess ? "OK" : "NO");
    aMsg.manager.sendAsyncMessage(rMsg,
                                 { data: aData, rid: aMsg.rid, mid: aMsg.mid });
  }

  function handlePeerListUpdated() {
    fireEvent("onpeerinfoupdate", {});
  }

  // Return a literal object as the constructed object.
  return {
    onLocalDeviceChanged: function(aDevice) {
      _localDevice = aDevice;
      debug('Local device updated to: ' + JSON.stringify(_localDevice));
    },

    onEnabled: function() {
      _peerList = [];
      _enabled = true;
      fireEvent("p2pUp", {});
    },

    onDisbaled: function() {
      _enabled = false;
      fireEvent("p2pDown", {});
    },

    onPeerFound: function(aPeer) {
      let newFoundPeer = new P2pDevice(aPeer);
      let origianlPeer = _peerList[aPeer.address];
      _peerList[aPeer.address] = newFoundPeer;
      if (origianlPeer) {
        newFoundPeer.connectionStatus = origianlPeer.connectionStatus;
      }
      handlePeerListUpdated();
    },

    onPeerLost: function(aPeer) {
      let lostPeer = _peerList[aPeer.address];
      if (!lostPeer) {
        debug('Unknown peer lost: ' + aPeer.address);
        return;
      }
      delete _peerList[aPeer.address];
      handlePeerListUpdated();
    },

    onConnecting: function(aPeer) {
      let peer = _peerList[aPeer.address];
      if (!peer) {
        debug('Unknown peer connecting: ' + aPeer.address);
        peer = new P2pDevice(aPeer);
        _peerList[aPeer.address] = peer;
        handlePeerListUpdated();
      }
      peer.connectionStatus = CONNECTION_STATUS_CONNECTING;

      fireEvent('onconnecting', { peer: peer });
    },

    onConnected: function(aGroupOwner, aPeer) {
      let go = new P2pGroupOwner(aGroupOwner);
      let peer = _peerList[aPeer.address];
      if (!peer) {
        debug('Unknown peer connected: ' + aPeer.address);
        peer = new P2pDevice(aPeer);
        _peerList[aPeer.address] = peer;
        handlePeerListUpdated();
      }
      peer.connectionStatus = CONNECTION_STATUS_CONNECTED;
      peer.isGroupOwner = (aPeer.address === aGroupOwner.address);

      _groupOwner = go;
      _currentPeer = peer;

      fireEvent('onconnected', { groupOwner: go, peer: peer });
    },

    onDisconnected: function(aPeer) {
      let peer = _peerList[aPeer.address];
      if (!peer) {
        debug('Unknown peer disconnected: ' + aPeer.address);
        return;
      }

      peer.connectionStatus = CONNECTION_STATUS_DISCONNECTED;

      _groupOwner = null;
      _currentPeer = null;

      fireEvent('ondisconnected', { peer: peer });
    },

    getObservedDOMMessages: function() {
      return [
        "WifiP2pManager:getState",
        "WifiP2pManager:getPeerList",
        "WifiP2pManager:setScanEnabled",
        "WifiP2pManager:connect",
        "WifiP2pManager:disconnect",
        "WifiP2pManager:setPairingConfirmation",
        "WifiP2pManager:setDeviceName"
      ];
    },

    onDOMMessage: function(aMessage) {
      let msg = aMessage.data || {};
      msg.manager = aMessage.target;

      if ("child-process-shutdown" === aMessage.name) {
        let i;
        if (-1 !== (i = _domManagers.indexOf(msg.manager))) {
          _domManagers.splice(i, 1);
        }
        return;
      }

      if (!aMessage.target.assertPermission("wifi-manage")) {
        return;
      }

      switch (aMessage.name) {
        case "WifiP2pManager:getState": // A new DOM manager is created.
          addDomManager(msg);
          return { // Synchronous call. Simply return it.
            enabled: _enabled,
            groupOwner: _groupOwner,
            currentPeer: _currentPeer
          };

        case "WifiP2pManager:setScanEnabled":
          {
            let enabled = msg.data;

            aDomMsgResponder.setScanEnabled(enabled, function(success) {
              returnMessage(aMessage.name, success, (success ? true : "ERROR"), msg);
            });
          }
          break;

        case "WifiP2pManager:getPeerList":
          {
            // Convert the object to an array.
            let peerArray = [];
            for (let key in _peerList) {
              if (_peerList.hasOwnProperty(key)) {
                peerArray.push(_peerList[key]);
              }
            }

            returnMessage(aMessage.name, true, peerArray, msg);
          }
          break;

        case "WifiP2pManager:connect":
          {
            let peer = msg.data;

            let onDoConnect = function(success) {
              returnMessage(aMessage.name, success, (success ? true : "ERROR"), msg);
            };

            aDomMsgResponder.connect(peer.address, peer.wpsMethod,
                                     peer.goIntent, onDoConnect);
          }
          break;

        case "WifiP2pManager:disconnect":
          {
            let address = msg.data;

            aDomMsgResponder.disconnect(address, function(success) {
              returnMessage(aMessage.name, success, (success ? true : "ERROR"), msg);
            });
          }
          break;

        case "WifiP2pManager:setPairingConfirmation":
          {
            let result = msg.data;
            aDomMsgResponder.setPairingConfirmation(result);
            returnMessage(aMessage.name, true, true, msg);
          }
          break;

        case "WifiP2pManager:setDeviceName":
          {
            let newDeviceName = msg.data;
            aDomMsgResponder.setDeviceName(newDeviceName, function(success) {
              returnMessage(aMessage.name, success, (success ? true : "ERROR"), msg);
            });
          }
          break;

        default:
          if (0 === aMessage.name.indexOf("WifiP2pManager:")) {
            debug("DOM WifiP2pManager message not handled: " + aMessage.name);
          }
      } // End of switch.
    }
  };
};
