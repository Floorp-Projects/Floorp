/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) {
  if (DEBUG) {
    dump("-*- NetworkStatsService: " + s + "\n");
  }
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["NetworkStatsService"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetworkStatsDB.jsm");

const NET_NETWORKSTATSSERVICE_CONTRACTID = "@mozilla.org/network/netstatsservice;1";
const NET_NETWORKSTATSSERVICE_CID = Components.ID("{18725604-e9ac-488a-8aa0-2471e7f6c0a4}");

const TOPIC_BANDWIDTH_CONTROL = "netd-bandwidth-control"

const TOPIC_INTERFACE_REGISTERED   = "network-interface-registered";
const TOPIC_INTERFACE_UNREGISTERED = "network-interface-unregistered";
const NET_TYPE_WIFI = Ci.nsINetworkInterface.NETWORK_TYPE_WIFI;
const NET_TYPE_MOBILE = Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE;

// Networks have different status that NetworkStats API needs to be aware of.
// Network is present and ready, so NetworkManager provides the whole info.
const NETWORK_STATUS_READY   = 0;
// Network is present but hasn't established a connection yet (e.g. SIM that has not
// enabled 3G since boot).
const NETWORK_STATUS_STANDBY = 1;
// Network is not present, but stored in database by the previous connections.
const NETWORK_STATUS_AWAY    = 2;

// The maximum traffic amount can be saved in the |cachedStats|.
const MAX_CACHED_TRAFFIC = 500 * 1000 * 1000; // 500 MB

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyServiceGetter(this, "gRil",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "networkService",
                                   "@mozilla.org/network/service;1",
                                   "nsINetworkService");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

XPCOMUtils.defineLazyServiceGetter(this, "messenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

this.NetworkStatsService = {
  init: function() {
    debug("Service started");

    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, TOPIC_INTERFACE_REGISTERED, false);
    Services.obs.addObserver(this, TOPIC_INTERFACE_UNREGISTERED, false);
    Services.obs.addObserver(this, TOPIC_BANDWIDTH_CONTROL, false);
    Services.obs.addObserver(this, "profile-after-change", false);

    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    // Object to store network interfaces, each network interface is composed
    // by a network object (network type and network Id) and a interfaceName
    // that contains the name of the physical interface (wlan0, rmnet0, etc.).
    // The network type can be 0 for wifi or 1 for mobile. On the other hand,
    // the network id is '0' for wifi or the iccid for mobile (SIM).
    // Each networkInterface is placed in the _networks object by the index of
    // 'networkId + networkType'.
    //
    // _networks object allows to map available network interfaces at low level
    // (wlan0, rmnet0, etc.) to a network. It's not mandatory to have a
    // networkInterface per network but can't exist a networkInterface not
    // being mapped to a network.

    this._networks = Object.create(null);

    // There is no way to know a priori if wifi connection is available,
    // just when the wifi driver is loaded, but it is unloaded when
    // wifi is switched off. So wifi connection is hardcoded
    let netId = this.getNetworkId('0', NET_TYPE_WIFI);
    this._networks[netId] = { network:       { id: '0',
                                               type: NET_TYPE_WIFI },
                              interfaceName: null,
                              status:        NETWORK_STATUS_STANDBY };

    this.messages = ["NetworkStats:Get",
                     "NetworkStats:Clear",
                     "NetworkStats:ClearAll",
                     "NetworkStats:SetAlarm",
                     "NetworkStats:GetAlarms",
                     "NetworkStats:RemoveAlarms",
                     "NetworkStats:GetAvailableNetworks",
                     "NetworkStats:GetAvailableServiceTypes",
                     "NetworkStats:SampleRate",
                     "NetworkStats:MaxStorageAge"];

    this.messages.forEach(function(aMsgName) {
      ppmm.addMessageListener(aMsgName, this);
    }, this);

    this._db = new NetworkStatsDB();

    // Stats for all interfaces are updated periodically
    this.timer.initWithCallback(this, this._db.sampleRate,
                                Ci.nsITimer.TYPE_REPEATING_PRECISE);

    // Stats not from netd are firstly stored in the cached.
    this.cachedStats = Object.create(null);
    this.cachedStatsDate = new Date();

    this.updateQueue = [];
    this.isQueueRunning = false;

    this._currentAlarms = {};
    this.initAlarms();
  },

  receiveMessage: function(aMessage) {
    if (!aMessage.target.assertPermission("networkstats-manage")) {
      return;
    }

    debug("receiveMessage " + aMessage.name);

    let mm = aMessage.target;
    let msg = aMessage.json;

    switch (aMessage.name) {
      case "NetworkStats:Get":
        this.getSamples(mm, msg);
        break;
      case "NetworkStats:Clear":
        this.clearInterfaceStats(mm, msg);
        break;
      case "NetworkStats:ClearAll":
        this.clearDB(mm, msg);
        break;
      case "NetworkStats:SetAlarm":
        this.setAlarm(mm, msg);
        break;
      case "NetworkStats:GetAlarms":
        this.getAlarms(mm, msg);
        break;
      case "NetworkStats:RemoveAlarms":
        this.removeAlarms(mm, msg);
        break;
      case "NetworkStats:GetAvailableNetworks":
        this.getAvailableNetworks(mm, msg);
        break;
      case "NetworkStats:GetAvailableServiceTypes":
        this.getAvailableServiceTypes(mm, msg);
        break;
      case "NetworkStats:SampleRate":
        // This message is sync.
        return this._db.sampleRate;
      case "NetworkStats:MaxStorageAge":
        // This message is sync.
        return this._db.maxStorageSamples * this._db.sampleRate;
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_INTERFACE_REGISTERED:
      case TOPIC_INTERFACE_UNREGISTERED:

        // If new interface is registered (notified from NetworkService),
        // the stats are updated for the new interface without waiting to
        // complete the updating period.

        let network = aSubject.QueryInterface(Ci.nsINetworkInterface);
        debug("Network " + network.name + " of type " + network.type + " status change");

        let netId = this.convertNetworkInterface(network);
        if (!netId) {
          break;
        }

        this._updateCurrentAlarm(netId);

        debug("NetId: " + netId);
        this.updateStats(netId);
        break;

      case TOPIC_BANDWIDTH_CONTROL:
        debug("Bandwidth message from netd: " + JSON.stringify(aData));

        let interfaceName = aData.substring(aData.lastIndexOf(" ") + 1);
        for (let networkId in this._networks) {
          if (interfaceName == this._networks[networkId].interfaceName) {
            let currentAlarm = this._currentAlarms[networkId];
            if (Object.getOwnPropertyNames(currentAlarm).length !== 0) {
              this._fireAlarm(currentAlarm.alarm);
            }
            break;
          }
        }
        break;

      case "xpcom-shutdown":
        debug("Service shutdown");

        this.messages.forEach(function(aMsgName) {
          ppmm.removeMessageListener(aMsgName, this);
        }, this);

        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "profile-after-change");
        Services.obs.removeObserver(this, TOPIC_INTERFACE_REGISTERED);
        Services.obs.removeObserver(this, TOPIC_INTERFACE_UNREGISTERED);
        Services.obs.removeObserver(this, TOPIC_BANDWIDTH_CONTROL);

        this.timer.cancel();
        this.timer = null;

        // Update stats before shutdown
        this.updateAllStats();
        break;
    }
  },

  /*
   * nsITimerCallback
   * Timer triggers the update of all stats
   */
  notify: function(aTimer) {
    this.updateAllStats();
  },

  /*
   * nsINetworkStatsService
   */
  getRilNetworks: function() {
    let networks = {};
    let numRadioInterfaces = gRil.numRadioInterfaces;
    for (let i = 0; i < numRadioInterfaces; i++) {
      let radioInterface = gRil.getRadioInterface(i);
      if (radioInterface.rilContext.iccInfo) {
        let netId = this.getNetworkId(radioInterface.rilContext.iccInfo.iccid,
                                      NET_TYPE_MOBILE);
        networks[netId] = { id : radioInterface.rilContext.iccInfo.iccid,
                            type: NET_TYPE_MOBILE };
      }
    }
    return networks;
  },

  convertNetworkInterface: function(aNetwork) {
    if (aNetwork.type != NET_TYPE_MOBILE &&
        aNetwork.type != NET_TYPE_WIFI) {
      return null;
    }

    let id = '0';
    if (aNetwork.type == NET_TYPE_MOBILE) {
      if (!(aNetwork instanceof Ci.nsIRilNetworkInterface)) {
        debug("Error! Mobile network should be an nsIRilNetworkInterface!");
        return null;
      }

      let rilNetwork = aNetwork.QueryInterface(Ci.nsIRilNetworkInterface);
      id = rilNetwork.iccId;
    }

    let netId = this.getNetworkId(id, aNetwork.type);

    if (!this._networks[netId]) {
      this._networks[netId] = Object.create(null);
      this._networks[netId].network = { id: id,
                                        type: aNetwork.type };
    }

    this._networks[netId].status = NETWORK_STATUS_READY;
    this._networks[netId].interfaceName = aNetwork.name;
    return netId;
  },

  getNetworkId: function getNetworkId(aIccId, aNetworkType) {
    return aIccId + '' + aNetworkType;
  },

  /* Function to ensure that one network is valid. The network is valid if its status is
   * NETWORK_STATUS_READY, NETWORK_STATUS_STANDBY or NETWORK_STATUS_AWAY.
   *
   * The result is |netId| or null in case of a non-valid network
   * aCallback is signatured as |function(netId)|.
   */
  validateNetwork: function validateNetwork(aNetwork, aCallback) {
    let netId = this.getNetworkId(aNetwork.id, aNetwork.type);

    if (this._networks[netId]) {
      aCallback(netId);
      return;
    }

    // Check if network is valid (RIL entry) but has not established a connection yet.
    // If so add to networks list with empty interfaceName.
    let rilNetworks = this.getRilNetworks();
    if (rilNetworks[netId]) {
      this._networks[netId] = Object.create(null);
      this._networks[netId].network = rilNetworks[netId];
      this._networks[netId].status = NETWORK_STATUS_STANDBY;
      this._currentAlarms[netId] = Object.create(null);
      aCallback(netId);
      return;
    }

    // Check if network is available in the DB.
    this._db.isNetworkAvailable(aNetwork, function(aError, aResult) {
      if (aResult) {
        this._networks[netId] = Object.create(null);
        this._networks[netId].network = aNetwork;
        this._networks[netId].status = NETWORK_STATUS_AWAY;
        this._currentAlarms[netId] = Object.create(null);
        aCallback(netId);
      }

      aCallback(null);
    });
  },

  getAvailableNetworks: function getAvailableNetworks(mm, msg) {
    let self = this;
    let rilNetworks = this.getRilNetworks();
    this._db.getAvailableNetworks(function onGetNetworks(aError, aResult) {

      // Also return the networks that are valid but have not
      // established connections yet.
      for (let netId in rilNetworks) {
        let found = false;
        for (let i = 0; i < aResult.length; i++) {
          if (netId == self.getNetworkId(aResult[i].id, aResult[i].type)) {
            found = true;
            break;
          }
        }
        if (!found) {
          aResult.push(rilNetworks[netId]);
        }
      }

      mm.sendAsyncMessage("NetworkStats:GetAvailableNetworks:Return",
                          { id: msg.id, error: aError, result: aResult });
    });
  },

  getAvailableServiceTypes: function getAvailableServiceTypes(mm, msg) {
    this._db.getAvailableServiceTypes(function onGetServiceTypes(aError, aResult) {
      mm.sendAsyncMessage("NetworkStats:GetAvailableServiceTypes:Return",
                          { id: msg.id, error: aError, result: aResult });
    });
  },

  initAlarms: function initAlarms() {
    debug("Init usage alarms");
    let self = this;

    for (let netId in this._networks) {
      this._currentAlarms[netId] = Object.create(null);

      this._db.getFirstAlarm(netId, function getResult(error, result) {
        if (!error && result) {
          self._setAlarm(result, function onSet(error, success) {
            if (error == "InvalidStateError") {
              self._fireAlarm(result);
            }
          });
        }
      });
    }
  },

  /*
   * Function called from manager to get stats from database.
   * In order to return updated stats, first is performed a call to
   * updateAllStats function, which will get last stats from netd
   * and update the database.
   * Then, depending on the request (stats per appId or total stats)
   * it retrieve them from database and return to the manager.
   */
  getSamples: function getSamples(mm, msg) {
    let self = this;
    let network = msg.network;
    let netId = this.getNetworkId(network.id, network.type);

    let appId = 0;
    let appManifestURL = msg.appManifestURL;
    if (appManifestURL) {
      appId = appsService.getAppLocalIdByManifestURL(appManifestURL);

      if (!appId) {
        mm.sendAsyncMessage("NetworkStats:Get:Return",
                            { id: msg.id,
                              error: "Invalid appManifestURL", result: null });
        return;
      }
    }

    let serviceType = msg.serviceType || "";

    let start = new Date(msg.start);
    let end = new Date(msg.end);

    this.validateNetwork(network, function onValidateNetwork(aNetId) {
      if (!aNetId) {
        mm.sendAsyncMessage("NetworkStats:Get:Return",
                            { id: msg.id, error: "Invalid connectionType", result: null });
        return;
      }

      // If network is currently active we need to update the cached stats first before
      // retrieving stats from the DB.
      if (self._networks[aNetId].status == NETWORK_STATUS_READY) {
        self.updateStats(aNetId, function onStatsUpdated(aResult, aMessage) {
          debug("getstats for network " + network.id + " of type " + network.type);
          debug("appId: " + appId + " from appManifestURL: " + appManifestURL);

          self.updateCachedStats(function onStatsUpdated(aResult, aMessage) {
            self._db.find(function onStatsFound(aError, aResult) {
              mm.sendAsyncMessage("NetworkStats:Get:Return",
                                  { id: msg.id, error: aError, result: aResult });
            }, appId, serviceType, network, start, end, appManifestURL);
          });
        });
        return;
      }

      // Network not active, so no need to update
      self._db.find(function onStatsFound(aError, aResult) {
        mm.sendAsyncMessage("NetworkStats:Get:Return",
                            { id: msg.id, error: aError, result: aResult });
      }, appId, serviceType, network, start, end, appManifestURL);
    });
  },

  clearInterfaceStats: function clearInterfaceStats(mm, msg) {
    let self = this;
    let network = msg.network;

    debug("clear stats for network " + network.id + " of type " + network.type);

    this.validateNetwork(network, function onValidateNetwork(aNetId) {
      if (!aNetId) {
        mm.sendAsyncMessage("NetworkStats:Clear:Return",
                            { id: msg.id, error: "Invalid connectionType", result: null });
        return;
      }

      self._db.clearInterfaceStats(network, function onDBCleared(aError, aResult) {
          mm.sendAsyncMessage("NetworkStats:Clear:Return",
                              { id: msg.id, error: aError, result: aResult });
      });
    });
  },

  clearDB: function clearDB(mm, msg) {
    let self = this;
    this._db.getAvailableNetworks(function onGetNetworks(aError, aResult) {
      if (aError) {
        mm.sendAsyncMessage("NetworkStats:ClearAll:Return",
                            { id: msg.id, error: aError, result: aResult });
        return;
      }

      let networks = aResult;
      self._db.clearStats(networks, function onDBCleared(aError, aResult) {
        mm.sendAsyncMessage("NetworkStats:ClearAll:Return",
                            { id: msg.id, error: aError, result: aResult });
      });
    });
  },

  updateAllStats: function updateAllStats(aCallback) {
    // Update |cachedStats|.
    this.updateCachedStats();

    let elements = [];
    let lastElement = null;

    // For each connectionType create an object containning the type
    // and the 'queueIndex', the 'queueIndex' is an integer representing
    // the index of a connection type in the global queue array. So, if
    // the connection type is already in the queue it is not appended again,
    // else it is pushed in 'elements' array, which later will be pushed to
    // the queue array.
    for (let netId in this._networks) {
      if (this._networks[netId].status != NETWORK_STATUS_READY) {
        continue;
      }

      lastElement = { netId: netId,
                      queueIndex: this.updateQueueIndex(netId)};

      if (lastElement.queueIndex == -1) {
        elements.push({ netId: lastElement.netId, callbacks: [] });
      }
    }

    if (!lastElement) {
      // No elements need to be updated, probably because status is different than
      // NETWORK_STATUS_READY.
      if (aCallback) {
        aCallback(true, "OK");
      }
      return;
    }

    if (elements.length > 0) {
      // If length of elements is greater than 0, callback is set to
      // the last element.
      elements[elements.length - 1].callbacks.push(aCallback);
      this.updateQueue = this.updateQueue.concat(elements);
    } else {
      // Else, it means that all connection types are already in the queue to
      // be updated, so callback for this request is added to
      // the element in the main queue with the index of the last 'lastElement'.
      // But before is checked that element is still in the queue because it can
      // be processed while generating 'elements' array.
      let element = this.updateQueue[lastElement.queueIndex];
      if (aCallback &&
         (!element || element.netId != lastElement.netId)) {
        aCallback();
        return;
      }

      this.updateQueue[lastElement.queueIndex].callbacks.push(aCallback);
    }

    // Call the function that process the elements of the queue.
    this.processQueue();

    if (DEBUG) {
      this.logAllRecords();
    }
  },

  updateStats: function updateStats(aNetId, aCallback) {
    // Check if the connection is in the main queue, push a new element
    // if it is not being processed or add a callback if it is.
    let index = this.updateQueueIndex(aNetId);
    if (index == -1) {
      this.updateQueue.push({netId: aNetId, callbacks: [aCallback]});
    } else {
      this.updateQueue[index].callbacks.push(aCallback);
      return;
    }

    // Call the function that process the elements of the queue.
    this.processQueue();
  },

  /*
   * Find if a connection is in the main queue array and return its
   * index, if it is not in the array return -1.
   */
  updateQueueIndex: function updateQueueIndex(aNetId) {
    return this.updateQueue.map(function(e) { return e.netId; }).indexOf(aNetId);
  },

  /*
   * Function responsible of process all requests in the queue.
   */
  processQueue: function processQueue(aResult, aMessage) {
    // If aResult is not undefined, the caller of the function is the result
    // of processing an element, so remove that element and call the callbacks
    // it has.
    if (aResult != undefined) {
      let item = this.updateQueue.shift();
      for (let callback of item.callbacks) {
        if(callback) {
          callback(aResult, aMessage);
        }
      }
    } else {
      // The caller is a function that has pushed new elements to the queue,
      // if isQueueRunning is false it means there is no processing currently
      // being done, so start.
      if (this.isQueueRunning) {
        if(this.updateQueue.length > 1) {
          return;
        }
      } else {
        this.isQueueRunning = true;
      }
    }

    // Check length to determine if queue is empty and stop processing.
    if (this.updateQueue.length < 1) {
      this.isQueueRunning = false;
      return;
    }

    // Call the update function for the next element.
    this.update(this.updateQueue[0].netId, this.processQueue.bind(this));
  },

  update: function update(aNetId, aCallback) {
    // Check if connection type is valid.
    if (!this._networks[aNetId]) {
      if (aCallback) {
        aCallback(false, "Invalid network " + aNetId);
      }
      return;
    }

    let interfaceName = this._networks[aNetId].interfaceName;
    debug("Update stats for " + interfaceName);

    // Request stats to NetworkService, which will get stats from netd, passing
    // 'networkStatsAvailable' as a callback.
    if (interfaceName) {
      networkService.getNetworkInterfaceStats(interfaceName,
                this.networkStatsAvailable.bind(this, aCallback, aNetId));
      return;
    }

    if (aCallback) {
      aCallback(true, "ok");
    }
  },

  /*
   * Callback of request stats. Store stats in database.
   */
  networkStatsAvailable: function networkStatsAvailable(aCallback, aNetId,
                                                        aResult, aRxBytes,
                                                        aTxBytes, aDate) {
    if (!aResult) {
      if (aCallback) {
        aCallback(false, "Netd IPC error");
      }
      return;
    }

    let stats = { appId:          0,
                  serviceType:    "",
                  networkId:      this._networks[aNetId].network.id,
                  networkType:    this._networks[aNetId].network.type,
                  date:           aDate,
                  rxBytes:        aTxBytes,
                  txBytes:        aRxBytes,
                  isAccumulative: true };

    debug("Update stats for: " + JSON.stringify(stats));

    this._db.saveStats(stats, function onSavedStats(aError, aResult) {
      if (aCallback) {
        if (aError) {
          aCallback(false, aError);
          return;
        }

        aCallback(true, "OK");
      }
    });
  },

  /*
   * Function responsible for receiving stats which are not from netd.
   */
  saveStats: function saveStats(aAppId, aServiceType, aNetwork, aTimeStamp,
                                aRxBytes, aTxBytes, aIsAccumulative,
                                aCallback) {
    let netId = this.convertNetworkInterface(aNetwork);
    if (!netId) {
      if (aCallback) {
        aCallback.notify(false, "Invalid network type");
      }
      return;
    }

    debug("saveStats: " + aAppId + " " + aServiceType + " " + netId + " " +
          aTimeStamp + " " + aRxBytes + " " + aTxBytes);

    // Check if |aConnectionType|, |aAppId| and |aServiceType| are valid.
    // There are two invalid cases for the combination of |aAppId| and
    // |aServiceType|:
    // a. Both |aAppId| is non-zero and |aServiceType| is non-empty.
    // b. Both |aAppId| is zero and |aServiceType| is empty.
    if (!this._networks[netId] || (aAppId && aServiceType) ||
        (!aAppId && !aServiceType)) {
      debug("Invalid network interface, appId or serviceType");
      return;
    }

    let stats = { appId:          aAppId,
                  serviceType:    aServiceType,
                  networkId:      this._networks[netId].network.id,
                  networkType:    this._networks[netId].network.type,
                  date:           new Date(aTimeStamp),
                  rxBytes:        aRxBytes,
                  txBytes:        aTxBytes,
                  isAccumulative: aIsAccumulative };

    // Generate an unique key from |appId|, |serviceType| and |netId|,
    // which is used to retrieve data in |cachedStats|.
    let key = stats.appId + "" + stats.serviceType + "" + netId;

    // |cachedStats| only keeps the data with the same date.
    // If the incoming date is different from |cachedStatsDate|,
    // both |cachedStats| and |cachedStatsDate| will get updated.
    let diff = (this._db.normalizeDate(stats.date) -
                this._db.normalizeDate(this.cachedStatsDate)) /
               this._db.sampleRate;
    if (diff != 0) {
      this.updateCachedStats(function onUpdated(success, message) {
        this.cachedStatsDate = stats.date;
        this.cachedStats[key] = stats;

        if (!aCallback) {
          return;
        }

        if (!success) {
          aCallback.notify(false, message);
          return;
        }

        aCallback.notify(true, "ok");
      }.bind(this));

      return;
    }

    // Try to find the matched row in the cached by |appId| and |connectionType|.
    // If not found, save the incoming data into the cached.
    let cachedStats = this.cachedStats[key];
    if (!cachedStats) {
      this.cachedStats[key] = stats;
      return;
    }

    // Find matched row, accumulate the traffic amount.
    cachedStats.rxBytes += stats.rxBytes;
    cachedStats.txBytes += stats.txBytes;

    // If new rxBytes or txBytes exceeds MAX_CACHED_TRAFFIC
    // the corresponding row will be saved to indexedDB.
    // Then, the row will be removed from the cached.
    if (cachedStats.rxBytes > MAX_CACHED_TRAFFIC ||
        cachedStats.txBytes > MAX_CACHED_TRAFFIC) {
      this._db.saveStats(cachedStats,
        function (error, result) {
          debug("Application stats inserted in indexedDB");
        }
      );
      delete this.cachedStats[key];
    }
  },

  updateCachedStats: function updateCachedStats(aCallback) {
    debug("updateCachedStats: " + this.cachedStatsDate);

    let stats = Object.keys(this.cachedStats);
    if (stats.length == 0) {
      // |cachedStats| is empty, no need to update.
      if (aCallback) {
        aCallback(true, "no need to update");
      }

      return;
    }

    let index = 0;
    this._db.saveStats(this.cachedStats[stats[index]],
      function onSavedStats(error, result) {
        if (DEBUG) {
          debug("Application stats inserted in indexedDB");
        }

        // Clean up the |cachedStats| after updating.
        if (index == stats.length - 1) {
          this.cachedStats = Object.create(null);

          if (!aCallback) {
            return;
          }

          if (error) {
            aCallback(false, error);
            return;
          }

          aCallback(true, "ok");
          return;
        }

        // Update is not finished, keep updating.
        index += 1;
        this._db.saveStats(this.cachedStats[stats[index]],
                           onSavedStats.bind(this, error, result));
      }.bind(this));
  },

  get maxCachedTraffic () {
    return MAX_CACHED_TRAFFIC;
  },

  logAllRecords: function logAllRecords() {
    this._db.logAllRecords(function onResult(aError, aResult) {
      if (aError) {
        debug("Error: " + aError);
        return;
      }

      debug("===== LOG =====");
      debug("There are " + aResult.length + " items");
      debug(JSON.stringify(aResult));
    });
  },

  getAlarms: function getAlarms(mm, msg) {
    let self = this;
    let network = msg.data.network;
    let manifestURL = msg.data.manifestURL;

    if (network) {
      this.validateNetwork(network, function onValidateNetwork(aNetId) {
        if (!aNetId) {
          mm.sendAsyncMessage("NetworkStats:GetAlarms:Return",
                              { id: msg.id, error: "InvalidInterface", result: null });
          return;
        }

        self._getAlarms(mm, msg, aNetId, manifestURL);
      });
      return;
    }

    this._getAlarms(mm, msg, null, manifestURL);
  },

  _getAlarms: function _getAlarms(mm, msg, aNetId, aManifestURL) {
    let self = this;
    this._db.getAlarms(aNetId, aManifestURL, function onCompleted(error, result) {
      if (error) {
        mm.sendAsyncMessage("NetworkStats:GetAlarms:Return",
                            { id: msg.id, error: error, result: result });
        return;
      }

      let alarms = []
      // NetworkStatsManager must return the network instead of the networkId.
      for (let i = 0; i < result.length; i++) {
        let alarm = result[i];
        alarms.push({ id: alarm.id,
                      network: self._networks[alarm.networkId].network,
                      threshold: alarm.threshold,
                      data: alarm.data });
      }

      mm.sendAsyncMessage("NetworkStats:GetAlarms:Return",
                          { id: msg.id, error: null, result: alarms });
    });
  },

  removeAlarms: function removeAlarms(mm, msg) {
    let alarmId = msg.data.alarmId;
    let manifestURL = msg.data.manifestURL;

    let self = this;
    let callback = function onRemove(error, result) {
      if (error) {
        mm.sendAsyncMessage("NetworkStats:RemoveAlarms:Return",
                            { id: msg.id, error: error, result: result });
        return;
      }

      for (let i in self._currentAlarms) {
        let currentAlarm = self._currentAlarms[i].alarm;
        if (currentAlarm && ((alarmId == currentAlarm.id) ||
            (alarmId == -1 && currentAlarm.manifestURL == manifestURL))) {

          self._updateCurrentAlarm(currentAlarm.networkId);
        }
      }

      mm.sendAsyncMessage("NetworkStats:RemoveAlarms:Return",
                          { id: msg.id, error: error, result: true });
    };

    if (alarmId == -1) {
      this._db.removeAlarms(manifestURL, callback);
    } else {
      this._db.removeAlarm(alarmId, manifestURL, callback);
    }
  },

  /*
   * Function called from manager to set an alarm.
   */
  setAlarm: function setAlarm(mm, msg) {
    let options = msg.data;
    let network = options.network;
    let threshold = options.threshold;

    debug("Set alarm at " + threshold + " for " + JSON.stringify(network));

    if (threshold < 0) {
      mm.sendAsyncMessage("NetworkStats:SetAlarm:Return",
                          { id: msg.id, error: "InvalidThresholdValue", result: null });
      return;
    }

    let self = this;
    this.validateNetwork(network, function onValidateNetwork(aNetId) {
      if (!aNetId) {
        mm.sendAsyncMessage("NetworkStats:SetAlarm:Return",
                            { id: msg.id, error: "InvalidiConnectionType", result: null });
        return;
      }

      let newAlarm = {
        id: null,
        networkId: aNetId,
        threshold: threshold,
        absoluteThreshold: null,
        startTime: options.startTime,
        data: options.data,
        pageURL: options.pageURL,
        manifestURL: options.manifestURL
      };

      self._updateThreshold(newAlarm, function onUpdate(error, _threshold) {
        if (error) {
          mm.sendAsyncMessage("NetworkStats:SetAlarm:Return",
                              { id: msg.id, error: error, result: null });
          return;
        }

        newAlarm.absoluteThreshold = _threshold.absoluteThreshold;
        self._db.addAlarm(newAlarm, function addSuccessCb(error, newId) {
          if (error) {
            mm.sendAsyncMessage("NetworkStats:SetAlarm:Return",
                                { id: msg.id, error: error, result: null });
            return;
          }

          newAlarm.id = newId;
          self._setAlarm(newAlarm, function onSet(error, success) {
            mm.sendAsyncMessage("NetworkStats:SetAlarm:Return",
                                { id: msg.id, error: error, result: newId });

            if (error == "InvalidStateError") {
              self._fireAlarm(newAlarm);
            }
          });
        });
      });
    });
  },

  _setAlarm: function _setAlarm(aAlarm, aCallback) {
    let currentAlarm = this._currentAlarms[aAlarm.networkId];
    if ((Object.getOwnPropertyNames(currentAlarm).length !== 0 &&
         aAlarm.absoluteThreshold > currentAlarm.alarm.absoluteThreshold) ||
        this._networks[aAlarm.networkId].status != NETWORK_STATUS_READY) {
      aCallback(null, true);
      return;
    }

    let self = this;

    this._updateThreshold(aAlarm, function onUpdate(aError, aThreshold) {
      if (aError) {
        aCallback(aError, null);
        return;
      }

      let callback = function onAlarmSet(aError) {
        if (aError) {
          debug("Set alarm error: " + aError);
          aCallback("netdError", null);
          return;
        }

        self._currentAlarms[aAlarm.networkId].alarm = aAlarm;

        aCallback(null, true);
      };

      debug("Set alarm " + JSON.stringify(aAlarm));
      let interfaceName = self._networks[aAlarm.networkId].interfaceName;
      if (interfaceName) {
        networkService.setNetworkInterfaceAlarm(interfaceName,
                                                aThreshold.systemThreshold,
                                                callback);
        return;
      }

      aCallback(null, true);
    });
  },

  _updateThreshold: function _updateThreshold(aAlarm, aCallback) {
    let self = this;
    this.updateStats(aAlarm.networkId, function onStatsUpdated(aResult, aMessage) {
      self._db.getCurrentStats(self._networks[aAlarm.networkId].network,
                               aAlarm.startTime,
                               function onStatsFound(error, result) {
        if (error) {
          debug("Error getting stats for " +
                JSON.stringify(self._networks[aAlarm.networkId]) + ": " + error);
          aCallback(error, result);
          return;
        }

        if (!result) {
          // There are no stats for the network of the alarm, set them to default 0 in
          // order to be able to calculate the offset, systemThreshold and
          // absoluteThreshold.
          result = { rxTotalBytes:  0, txTotalBytes:  0,
                     rxSystemBytes: 0, txSystemBytes: 0 };
        }

        let offset = aAlarm.threshold - result.rxTotalBytes - result.txTotalBytes;

        // Alarm set to a threshold lower than current rx/tx bytes.
        if (offset <= 0) {
          aCallback("InvalidStateError", null);
          return;
        }

        let threshold = {
          systemThreshold: result.rxSystemBytes + result.txSystemBytes + offset,
          absoluteThreshold: result.rxTotalBytes + result.txTotalBytes + offset
        };

        aCallback(null, threshold);
      });
    });
  },

  _fireAlarm: function _fireAlarm(aAlarm) {
    debug("Fire alarm");

    let self = this;
    this._db.removeAlarm(aAlarm.id, null, function onRemove(aError, aResult){
      if (!aError && !aResult) {
        return;
      }

      self._fireSystemMessage(aAlarm);
      self._updateCurrentAlarm(aAlarm.networkId);
    });
  },

  _updateCurrentAlarm: function _updateCurrentAlarm(aNetworkId) {
    this._currentAlarms[aNetworkId] = Object.create(null);

    let self = this;
    this._db.getFirstAlarm(aNetworkId, function onGet(error, result){
      if (error) {
        debug("Error getting the first alarm");
        return;
      }

      if (!result) {
        let interfaceName = self._networks[aNetworkId].interfaceName;
        networkService.setNetworkInterfaceAlarm(interfaceName, -1,
                                                function onComplete(){});
        return;
      }

      self._setAlarm(result, function onSet(error, success){
        if (error == "InvalidStateError") {
          self._fireAlarm(result);
          return;
        }
      });
    });
  },

  _fireSystemMessage: function _fireSystemMessage(aAlarm) {
    debug("Fire system message: " + JSON.stringify(aAlarm));

    let manifestURI = Services.io.newURI(aAlarm.manifestURL, null, null);
    let pageURI = Services.io.newURI(aAlarm.pageURL, null, null);

    let alarm = { "id":        aAlarm.id,
                  "threshold": aAlarm.threshold,
                  "data":      aAlarm.data };
    messenger.sendMessage("networkstats-alarm", alarm, pageURI, manifestURI);
  }
};

NetworkStatsService.init();
