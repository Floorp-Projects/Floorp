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

const TOPIC_INTERFACE_REGISTERED   = "network-interface-registered";
const TOPIC_INTERFACE_UNREGISTERED = "network-interface-unregistered";
const NET_TYPE_WIFI = Ci.nsINetworkInterface.NETWORK_TYPE_WIFI;
const NET_TYPE_MOBILE = Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE;
const NET_TYPE_UNKNOWN = Ci.nsINetworkInterface.NETWORK_TYPE_UNKNOWN;

// The maximum traffic amount can be saved in the |cachedAppStats|.
const MAX_CACHED_TRAFFIC = 500 * 1000 * 1000; // 500 MB

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyServiceGetter(this, "networkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

this.NetworkStatsService = {
  init: function() {
    if (DEBUG) {
      debug("Service started");
    }

    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, TOPIC_INTERFACE_REGISTERED, false);
    Services.obs.addObserver(this, TOPIC_INTERFACE_UNREGISTERED, false);
    Services.obs.addObserver(this, "profile-after-change", false);

    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    this._connectionTypes = Object.create(null);
    this._connectionTypes[NET_TYPE_WIFI] = { name: "wifi",
                                             network: Object.create(null) };
    this._connectionTypes[NET_TYPE_MOBILE] = { name: "mobile",
                                               network: Object.create(null) };


    this.messages = ["NetworkStats:Get",
                     "NetworkStats:Clear",
                     "NetworkStats:Types",
                     "NetworkStats:SampleRate",
                     "NetworkStats:MaxStorageSamples"];

    this.messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }, this);

    this._db = new NetworkStatsDB(this._connectionTypes);

    // Stats for all interfaces are updated periodically
    this.timer.initWithCallback(this, this._db.sampleRate,
                                Ci.nsITimer.TYPE_REPEATING_PRECISE);

    // App stats are firstly stored in the cached.
    this.cachedAppStats = Object.create(null);
    this.cachedAppStatsDate = new Date();

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
          types.push(this._connectionTypes[i].name);
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
      case TOPIC_INTERFACE_UNREGISTERED:
        // If new interface is registered (notified from NetworkManager),
        // the stats are updated for the new interface without waiting to
        // complete the updating period
        let network = subject.QueryInterface(Ci.nsINetworkInterface);
        if (DEBUG) {
          debug("Network " + network.name + " of type " + network.type + " status change");
        }
        if (this._connectionTypes[network.type]) {
          this._connectionTypes[network.type].network = network;
          this.updateStats(network.type);
        }
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
        Services.obs.removeObserver(this, TOPIC_INTERFACE_REGISTERED);
        Services.obs.removeObserver(this, TOPIC_INTERFACE_UNREGISTERED);

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
   * Then, depending on the request (stats per appId or total stats)
   * it retrieve them from database and return to the manager.
   */
  getStats: function getStats(mm, msg) {
    this.updateAllStats(function onStatsUpdated(aResult, aMessage) {

      let data = msg.data;

      let options = { appId:          0,
                      connectionType: data.connectionType,
                      start:          data.start,
                      end:            data.end };

      let manifestURL = data.manifestURL;
      if (manifestURL) {
        let appId = appsService.getAppLocalIdByManifestURL(manifestURL);
        if (DEBUG) {
          debug("get appId: " + appId + " from manifestURL: " + manifestURL);
        }

        if (!appId) {
          mm.sendAsyncMessage("NetworkStats:Get:Return",
                              { id: msg.id, error: "Invalid manifestURL", result: null });
          return;
        }

        options.appId = appId;
        options.manifestURL = manifestURL;
      }

      if (DEBUG) {
        debug("getStats for options: " + JSON.stringify(options));
      }

      if (!options.connectionType || options.connectionType.length == 0) {
        this._db.findAll(function onStatsFound(error, result) {
          mm.sendAsyncMessage("NetworkStats:Get:Return",
                              { id: msg.id, error: error, result: result });
        }, options);
        return;
      }

      for (let i in this._connectionTypes) {
        if (this._connectionTypes[i].name == options.connectionType) {
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
    // Update |cachedAppStats|.
    this.updateCachedAppStats();

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
    // Check if connection type is valid.
    if (!this._connectionTypes[connectionType]) {
      if (callback) {
        callback(false, "Invalid network type " + connectionType);
      }
      return;
    }

    if (DEBUG) {
      debug("Update stats for " + this._connectionTypes[connectionType].name);
    }

    // Request stats to NetworkManager, which will get stats from netd, passing
    // 'networkStatsAvailable' as a callback.
    let networkName = this._connectionTypes[connectionType].network.name;
    if (networkName) {
      networkManager.getNetworkInterfaceStats(networkName,
                this.networkStatsAvailable.bind(this, callback, connectionType));
      return;
    }
    if (callback) {
      callback(true, "ok");
    }
  },

  /*
   * Callback of request stats. Store stats in database.
   */
  networkStatsAvailable: function networkStatsAvailable(callback, connType, result, rxBytes, txBytes, date) {
    if (!result) {
      if (callback) {
        callback(false, "Netd IPC error");
      }
      return;
    }

    let stats = { appId:          0,
                  connectionType: this._connectionTypes[connType].name,
                  date:           date,
                  rxBytes:        rxBytes,
                  txBytes:        txBytes };

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

  /*
   * Function responsible for receiving per-app stats.
   */
  saveAppStats: function saveAppStats(aAppId, aConnectionType, aTimeStamp, aRxBytes, aTxBytes, aCallback) {
    if (DEBUG) {
      debug("saveAppStats: " + aAppId + " " + aConnectionType + " " +
            aTimeStamp + " " + aRxBytes + " " + aTxBytes);
    }

    // Check if |aAppId| and |aConnectionType| are valid.
    if (!aAppId || aConnectionType == NET_TYPE_UNKNOWN) {
      return;
    }

    let stats = { appId: aAppId,
                  connectionType: this._connectionTypes[aConnectionType].name,
                  date: new Date(aTimeStamp),
                  rxBytes: aRxBytes,
                  txBytes: aTxBytes };

    // Generate an unique key from |appId| and |connectionType|,
    // which is used to retrieve data in |cachedAppStats|.
    let key = stats.appId + stats.connectionType;

    // |cachedAppStats| only keeps the data with the same date.
    // If the incoming date is different from |cachedAppStatsDate|,
    // both |cachedAppStats| and |cachedAppStatsDate| will get updated.
    let diff = (this._db.normalizeDate(stats.date) -
                this._db.normalizeDate(this.cachedAppStatsDate)) /
               this._db.sampleRate;
    if (diff != 0) {
      this.updateCachedAppStats(function onUpdated(success, message) {
        this.cachedAppStatsDate = stats.date;
        this.cachedAppStats[key] = stats;

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
    let appStats = this.cachedAppStats[key];
    if (!appStats) {
      this.cachedAppStats[key] = stats;
      return;
    }

    // Find matched row, accumulate the traffic amount.
    appStats.rxBytes += stats.rxBytes;
    appStats.txBytes += stats.txBytes;

    // If new rxBytes or txBytes exceeds MAX_CACHED_TRAFFIC
    // the corresponding row will be saved to indexedDB.
    // Then, the row will be removed from the cached.
    if (appStats.rxBytes > MAX_CACHED_TRAFFIC ||
        appStats.txBytes > MAX_CACHED_TRAFFIC) {
      this._db.saveStats(appStats,
        function (error, result) {
          if (DEBUG) {
            debug("Application stats inserted in indexedDB");
          }
        }
      );
      delete this.cachedAppStats[key];
    }
  },

  updateCachedAppStats: function updateCachedAppStats(callback) {
    if (DEBUG) {
      debug("updateCachedAppStats: " + this.cachedAppStatsDate);
    }

    let stats = Object.keys(this.cachedAppStats);
    if (stats.length == 0) {
      // |cachedAppStats| is empty, no need to update.
      return;
    }

    let index = 0;
    this._db.saveStats(this.cachedAppStats[stats[index]],
      function onSavedStats(error, result) {
        if (DEBUG) {
          debug("Application stats inserted in indexedDB");
        }

        // Clean up the |cachedAppStats| after updating.
        if (index == stats.length - 1) {
          this.cachedAppStats = Object.create(null);

          if (!callback) {
            return;
          }

          if (error) {
            callback(false, error);
            return;
          }

          callback(true, "ok");
          return;
        }

        // Update is not finished, keep updating.
        index += 1;
        this._db.saveStats(this.cachedAppStats[stats[index]],
                           onSavedStats.bind(this, error, result));
      }.bind(this));
  },

  get maxCachedTraffic () {
    return MAX_CACHED_TRAFFIC;
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
