/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- NetworkStatsService: " + s + "\n"); }

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["NetworkStatsService"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetworkStatsDB.jsm");

const NET_NETWORKSTATSSERVICE_CONTRACTID = "@mozilla.org/network/netstatsservice;1";
const NET_NETWORKSTATSSERVICE_CID = Components.ID("{18725604-e9ac-488a-8aa0-2471e7f6c0a4}");

const TOPIC_INTERFACE_REGISTERED = "network-interface-registered";
const NETWORK_TYPE_WIFI = Ci.nsINetworkInterface.NETWORK_TYPE_WIFI;
const NETWORK_TYPE_MOBILE = Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE;

XPCOMUtils.defineLazyServiceGetter(this, "gIDBManager",
                                   "@mozilla.org/dom/indexeddb/manager;1",
                                   "nsIIndexedDatabaseManager");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyServiceGetter(this, "networkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

let myGlobal = this;

this.NetworkStatsService = {
  init: function() {
    if (DEBUG) {
      debug("Service started");
    }

    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, TOPIC_INTERFACE_REGISTERED, false);
    Services.obs.addObserver(this, "profile-after-change", false);

    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    this._connectionTypes = Object.create(null);
    this._connectionTypes[NETWORK_TYPE_WIFI] = "wifi";
    this._connectionTypes[NETWORK_TYPE_MOBILE] = "mobile";

    this.messages = ["NetworkStats:Get",
                     "NetworkStats:Clear",
                     "NetworkStats:Types",
                     "NetworkStats:SampleRate",
                     "NetworkStats:MaxStorageSamples"];

    this.messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }, this);

    gIDBManager.initWindowless(myGlobal);
    this._db = new NetworkStatsDB(myGlobal);

    // Stats for all interfaces are updated periodically
    this.timer.initWithCallback(this, this._db.sampleRate,
                                Ci.nsITimer.TYPE_REPEATING_PRECISE);

    this.updateQueue = [];
    this.isQueueRunning = false;
  },

  receiveMessage: function(aMessage) {
    if (!aMessage.target.assertPermission("networkstats-manage")) {
      return;
    }

    if (DEBUG) {
      debug("receiveMessage " + aMessage.name);
    }
    let mm = aMessage.target;
    let msg = aMessage.json;

    switch (aMessage.name) {
      case "NetworkStats:Get":
        this.getStats(mm, msg);
        break;
      case "NetworkStats:Clear":
        this.clearDB(mm, msg);
        break;
      case "NetworkStats:Types":
        // This message is sync.
        let types = [];
        for (let i in this._connectionTypes) {
          types.push(this._connectionTypes[i]);
        }
        return types;
      case "NetworkStats:SampleRate":
        // This message is sync.
        return this._db.sampleRate;
      case "NetworkStats:MaxStorageSamples":
        // This message is sync.
        return this._db.maxStorageSamples;
    }
  },

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case TOPIC_INTERFACE_REGISTERED:
        // If new interface is registered (notified from NetworkManager),
        // the stats are updated for the new interface without waiting to
        // complete the updating period
        let network = subject.QueryInterface(Ci.nsINetworkInterface);
        if (DEBUG) {
          debug("Network " + network.name + " of type " + network.type + " registered");
        }
        this.updateStats(network.type);
        break;
      case "xpcom-shutdown":
        if (DEBUG) {
          debug("Service shutdown");
        }

        this.messages.forEach(function(msgName) {
          ppmm.removeMessageListener(msgName, this);
        }, this);

        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "profile-after-change");
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
  notify: function(timer) {
    this.updateAllStats();
  },

  /*
   * Function called from manager to get stats from database.
   * In order to return updated stats, first is performed a call to
   * updateAllStats function, which will get last stats from netd
   * and update the database.
   * Then, depending on the request (stats per interface or total stats)
   * it retrieve them from database and return to the manager.
   */
  getStats: function getStats(mm, msg) {
    this.updateAllStats(function onStatsUpdated(aResult, aMessage) {

      let options = msg.data;
      if (DEBUG) {
        debug("getstats for: - " + options.connectionType + " -");
      }

      if (!options.connectionType || options.connectionType.length == 0) {
        this._db.findAll(function onStatsFound(error, result) {
          mm.sendAsyncMessage("NetworkStats:Get:Return",
                              { id: msg.id, error: error, result: result });
        }, options);
        return;
      }

      for (let i in this._connectionTypes) {
        if (this._connectionTypes[i] == options.connectionType) {
          this._db.find(function onStatsFound(error, result) {
            mm.sendAsyncMessage("NetworkStats:Get:Return",
                                { id: msg.id, error: error, result: result });
          }, options);
          return;
        }
      }

      mm.sendAsyncMessage("NetworkStats:Get:Return",
                          { id: msg.id, error: "Invalid connectionType", result: null });

    }.bind(this));
  },

  clearDB: function clearDB(mm, msg) {
    this._db.clear(function onDBCleared(error, result) {
      mm.sendAsyncMessage("NetworkStats:Clear:Return",
                          { id: msg.id, error: error, result: result });
    });
  },

  updateAllStats: function updateAllStats(callback) {
    let elements = [];
    let lastElement;

    // For each connectionType create an object containning the type
    // and the 'queueIndex', the 'queueIndex' is an integer representing
    // the index of a connection type in the global queue array. So, if
    // the connection type is already in the queue it is not appended again,
    // else it is pushed in 'elements' array, which later will be pushed to
    // the queue array.
    for (let i in this._connectionTypes) {
      lastElement = { type: i,
                      queueIndex: this.updateQueueIndex(i)};
      if (lastElement.queueIndex == -1) {
        elements.push({type: lastElement.type, callbacks: []});
      }
    }

    if (elements.length > 0) {
      // If length of elements is greater than 0, callback is set to
      // the last element.
      elements[elements.length - 1].callbacks.push(callback);
      this.updateQueue = this.updateQueue.concat(elements);
    } else {
      // Else, it means that all connection types are already in the queue to
      // be updated, so callback for this request is added to
      // the element in the main queue with the index of the last 'lastElement'.
      // But before is checked that element is still in the queue because it can
      // be processed while generating 'elements' array.

      if (!this.updateQueue[lastElement.queueIndex] ||
          this.updateQueue[lastElement.queueIndex].type != lastElement.queueIndex) {
        if (callback) {
          callback();
        }
        return;
      }

      this.updateQueue[lastElement.queueIndex].callbacks.push(callback);
    }

    // Call the function that process the elements of the queue.
    this.processQueue();

    if (DEBUG) {
      this.logAllRecords();
    }
  },

  updateStats: function updateStats(connectionType, callback) {
    // Check if the connection type is in the main queue, push a new element
    // if it is not being processed or add a callback if it is.
    let index = this.updateQueueIndex(connectionType);
    if (index == -1) {
      this.updateQueue.push({type: connectionType, callbacks: [callback]});
    } else {
      this.updateQueue[index].callbacks.push(callback);
    }

    // Call the function that process the elements of the queue.
    this.processQueue();
  },

  /*
   * Find if a connection type is in the main queue array and return its
   * index, if it is not in the array return -1.
   */
  updateQueueIndex: function updateQueueIndex(type) {
    for (let i in this.updateQueue) {
      if (this.updateQueue[i].type == type) {
        return i;
      }
    }
    return -1;
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
      // if isQueueRunning is false it means there is no processing currently being
      // done, so start.
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
    this.update(this.updateQueue[0].type, this.processQueue.bind(this));
  },

  update: function update(connectionType, callback) {
    if (DEBUG) {
      debug("Update stats for " + this._connectionTypes[connectionType]);
    }

    // Check if connection type is valid.
    if (!this._connectionTypes[connectionType]) {
      if (callback) {
        callback(false, "Invalid network type " + connectionType);
      }
      return;
    }

    // Request stats to NetworkManager, which will get stats from netd, passing
    // 'networkStatsAvailable' as a callback.
    if (!networkManager.getNetworkInterfaceStats(connectionType,
                                                 this.networkStatsAvailable
                                                     .bind(this, callback))) {
      if (DEBUG) {
        debug("There is no interface registered for network type " +
              this._connectionTypes[connectionType]);
      }

      // Interface is not registered (up), so nothing to do.
      callback(true, "OK");
    }
  },

  /*
   * Callback of request stats. Store stats in database.
   */
  networkStatsAvailable: function networkStatsAvailable(callback, result, connType, rxBytes, txBytes, date) {
    if (!result) {
      if (callback) {
        callback(false, "Netd IPC error");
      }
      return;
    }

    let stats = { connectionType: this._connectionTypes[connType],
                  date:           date,
                  rxBytes:        txBytes,
                  txBytes:        rxBytes};

    if (DEBUG) {
      debug("Update stats for " + stats.connectionType + ": rx=" + stats.rxBytes +
            " tx=" + stats.txBytes + " timestamp=" + stats.date);
    }
    this._db.saveStats(stats, function onSavedStats(error, result) {
      if (callback) {
        if (error) {
          callback(false, error);
          return;
        }

        callback(true, "OK");
      }
    });
  },

  logAllRecords: function logAllRecords() {
    this._db.logAllRecords(function onResult(error, result) {
      if (error) {
        debug("Error: " + error);
        return;
      }

      debug("===== LOG =====");
      debug("There are " + result.length + " items");
      debug(JSON.stringify(result));
    });
  }
};

NetworkStatsService.init();
