/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const NETWORKMANAGER_CONTRACTID = "@mozilla.org/network/manager;1";
const NETWORKMANAGER_CID =
  Components.ID("{33901e46-33b8-11e1-9869-f46d04d25bcc}");
const NETWORKINTERFACE_CONTRACTID = "@mozilla.org/network/interface;1";
const NETWORKINTERFACE_CID =
  Components.ID("{266c3edd-78f0-4512-8178-2d6fee2d35ee}");

const DEFAULT_PREFERRED_NETWORK_TYPE = Ci.nsINetworkInterface.NETWORK_TYPE_WIFI;

const TOPIC_INTERFACE_STATE_CHANGED  = "network-interface-state-changed";
const TOPIC_INTERFACE_REGISTERED     = "network-interface-registered";
const TOPIC_INTERFACE_UNREGISTERED   = "network-interface-unregistered";
const TOPIC_DEFAULT_ROUTE_CHANGED    = "network-default-route-changed";

const DEBUG = false;

/**
 * This component watches for network interfaces changing state and then
 * adjusts routes etc. accordingly.
 */
function NetworkManager() {
  this.networkInterfaces = {};
  Services.obs.addObserver(this, TOPIC_INTERFACE_STATE_CHANGED, true);

  debug("Starting worker.");
  this.worker = new ChromeWorker("resource://gre/modules/net_worker.js");
  this.worker.onmessage = function onmessage(event) {
    this.handleWorkerMessage(event.data);
  }.bind(this);
  this.worker.onerror = function onerror(event) {
    debug("Received error from worker: " + event.filename +
          ":" + event.lineno + ": " + event.message + "\n");
    // Prevent the event from bubbling any further.
    event.preventDefault();
  };
}
NetworkManager.prototype = {
  classID:   NETWORKMANAGER_CID,
  classInfo: XPCOMUtils.generateCI({classID: NETWORKMANAGER_CID,
                                    contractID: NETWORKMANAGER_CONTRACTID,
                                    classDescription: "Network Manager",
                                    interfaces: [Ci.nsINetworkManager]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkManager,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIWorkerHolder]),

  // nsIWorkerHolder

  worker: null,

  // nsIObserver

  observe: function observe(subject, topic, data) {
    if (topic != TOPIC_INTERFACE_STATE_CHANGED) {
      return;
    }
    let network = subject.QueryInterface(Ci.nsINetworkInterface);
    debug("Network '" + network.name + "' changed state to " + network.state);
    switch (network.state) {
      case Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED:
      case Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTING:
        this.setAndConfigureActive();
        break;
    }
  },

  // nsINetworkManager

  registerNetworkInterface: function registerNetworkInterface(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    if (network.name in this.networkInterfaces) {
      throw Components.Exception("Network with that name already registered!",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    this.networkInterfaces[network.name] = network;
    this.setAndConfigureActive();
    Services.obs.notifyObservers(network, TOPIC_INTERFACE_REGISTERED, null);
    debug("Network '" + network.name + "' registered.");
  },

  unregisterNetworkInterface: function unregisterNetworkInterface(network) {
    if (!(network instanceof Ci.nsINetworkInterface)) {
      throw Components.Exception("Argument must be nsINetworkInterface.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    if (!(network.name in this.networkInterfaces)) {
      throw Components.Exception("No network with that name registered.",
                                 Cr.NS_ERROR_INVALID_ARG);
    }
    delete this.networkInterfaces[network.name];
    this.setAndConfigureActive();
    Services.obs.notifyObservers(network, TOPIC_INTERFACE_UNREGISTERED, null);
    debug("Network '" + network.name + "' unregistered.");
  },

  networkInterfaces: null,

  _preferredNetworkType: DEFAULT_PREFERRED_NETWORK_TYPE,
  get preferredNetworkType() {
    return this._preferredNetworkType;
  },
  set preferredNetworkType(val) {
    if ([Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
         Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS].indexOf(val) == -1) {
      throw "Invalid network type";
    }
    this._preferredNetworkType = val;
  },

  active: null,
  _overriddenActive: null,

  overrideActive: function overrideActive(network) {
    this._overriddenActive = network;
    this.setAndConfigureActive();
  },

  // Helpers

  handleWorkerMessage: function handleWorkerMessage(message) {
    debug("NetworkManager received message from worker: " + JSON.stringify(message));
  },

  /**
   * Determine the active interface and configure it.
   */
  setAndConfigureActive: function setAndConfigureActive() {
    debug("Evaluating whether active network needs to be changed.");

    if (this._overriddenActive) {
      debug("We have an override for the active network: " +
            this._overriddenActive.name);
      // The override was just set, so reconfigure the network.
      if (this.active != this._overriddenActive) {
        this.active = this._overriddenActive;
        this.setDefaultRouteAndDNS();
      }
      return;
    }

    // If the active network is already of the preferred type, nothing to do.
    if (this.active && this.active.type == this._preferredNetworkType) {
      debug("Active network is already our preferred type. Not doing anything.");
      return;
    }

    // Find a suitable network interface to activate.
    let oldActive = this.active;
    this.active = null;
    for each (let network in this.networkInterfaces) {
      if (network.state != Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED) {
        continue;
      }
      this.active = network;
      if (network.type == this.preferredNetworkType) {
        debug("Found our preferred type of network: " + network.name);
        break;
      }
    }
    if (this.active && (oldActive != this.active)) {
      this.setDefaultRouteAndDNS();
    }
  },

  setDefaultRouteAndDNS: function setDefaultRouteAndDNS() {
    debug("Going to change route and DNS to " + this.active.name);
    if (this.active.dhcp) {
      this.worker.postMessage({cmd: "runDHCPAndSetDefaultRouteAndDNS",
                               ifname: this.active.name});
    } else {
      this.worker.postMessage({cmd: "setDefaultRouteAndDNS",
                               ifname: this.active.name});
    }
  },

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([NetworkManager]);


let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- NetworkManager: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
