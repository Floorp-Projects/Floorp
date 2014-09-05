/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/DownloadsIPC.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");
XPCOMUtils.defineLazyServiceGetter(this, "volumeService",
                                   "@mozilla.org/telephony/volume-service;1",
                                    "nsIVolumeService");

function debug(aStr) {
#ifdef MOZ_DEBUG
  dump("-*- DownloadsAPI.js : " + aStr + "\n");
#endif
}

function DOMDownloadManagerImpl() {
  debug("DOMDownloadManagerImpl constructor");
}

DOMDownloadManagerImpl.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    debug("DownloadsManager init");
    this.initDOMRequestHelper(aWindow,
                              ["Downloads:Added",
                               "Downloads:Removed"]);
  },

  uninit: function() {
    debug("uninit");
    downloadsCache.evict(this._window);
  },

  set ondownloadstart(aHandler) {
    this.__DOM_IMPL__.setEventHandler("ondownloadstart", aHandler);
  },

  get ondownloadstart() {
    return this.__DOM_IMPL__.getEventHandler("ondownloadstart");
  },

  getDownloads: function() {
    debug("getDownloads()");

    return this.createPromise(function (aResolve, aReject) {
      DownloadsIPC.getDownloads().then(
        function(aDownloads) {
          // Turn the list of download objects into DOM objects and
          // send them.
          let array = new this._window.Array();
          for (let id in aDownloads) {
            let dom = createDOMDownloadObject(this._window, aDownloads[id]);
            array.push(this._prepareForContent(dom));
          }
          aResolve(array);
        }.bind(this),
        function() {
          aReject("GetDownloadsError");
        }
      );
    }.bind(this));
  },

  clearAllDone: function() {
    debug("clearAllDone()");
    return this.createPromise(function (aResolve, aReject) {
      DownloadsIPC.clearAllDone().then(
        function(aDownloads) {
          // Turn the list of download objects into DOM objects and
          // send them.
          let array = new this._window.Array();
          for (let id in aDownloads) {
            let dom = createDOMDownloadObject(this._window, aDownloads[id]);
            array.push(this._prepareForContent(dom));
          }
          aResolve(array);
        }.bind(this),
        function() {
          aReject("ClearAllDoneError");
        }
      );
    }.bind(this));
  },

  remove: function(aDownload) {
    debug("remove " + aDownload.url + " " + aDownload.id);
    return this.createPromise(function (aResolve, aReject) {
      if (!downloadsCache.has(this._window, aDownload.id)) {
        debug("no download " + aDownload.id);
        aReject("InvalidDownload");
        return;
      }

      DownloadsIPC.remove(aDownload.id).then(
        function(aResult) {
          let dom = createDOMDownloadObject(this._window, aResult);
          // Change the state right away to not race against the update message.
          dom.wrappedJSObject.state = "finalized";
          aResolve(this._prepareForContent(dom));
        }.bind(this),
        function() {
          aReject("RemoveError");
        }
      );
    }.bind(this));
  },

  /**
    * Turns a chrome download object into a content accessible one.
    * When we have __DOM_IMPL__ available we just use that, otherwise
    * we run _create() with the wrapped js object.
    */
  _prepareForContent: function(aChromeObject) {
    if (aChromeObject.__DOM_IMPL__) {
      return aChromeObject.__DOM_IMPL__;
    }
    let res = this._window.DOMDownload._create(this._window,
                                            aChromeObject.wrappedJSObject);
    return res;
  },

  receiveMessage: function(aMessage) {
    let data = aMessage.data;
    switch(aMessage.name) {
      case "Downloads:Added":
        debug("Adding " + uneval(data));
        let event = new this._window.DownloadEvent("downloadstart", {
          download:
            this._prepareForContent(createDOMDownloadObject(this._window, data))
        });
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
    }
  },

  classID: Components.ID("{c6587afa-0696-469f-9eff-9dac0dd727fe}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

};

/**
  * Keep track of download objects per window.
  */
let downloadsCache = {
  init: function() {
    this.cache = new WeakMap();
  },

  has: function(aWindow, aId) {
    let downloads = this.cache.get(aWindow);
    return !!(downloads && downloads[aId]);
  },

  get: function(aWindow, aDownload) {
    let downloads = this.cache.get(aWindow);
    if (!(downloads && downloads[aDownload.id])) {
      debug("Adding download " + aDownload.id + " to cache.");
      if (!downloads) {
        this.cache.set(aWindow, {});
        downloads = this.cache.get(aWindow);
      }
      // Create the object and add it to the cache.
      let impl = Cc["@mozilla.org/downloads/download;1"]
                   .createInstance(Ci.nsISupports);
      impl.wrappedJSObject._init(aWindow, aDownload);
      downloads[aDownload.id] = impl;
    }
    return downloads[aDownload.id];
  },

  evict: function(aWindow) {
    this.cache.delete(aWindow);
  }
};

downloadsCache.init();

/**
  * The DOM facade of a download object.
  */

function createDOMDownloadObject(aWindow, aDownload) {
  return downloadsCache.get(aWindow, aDownload);
}

function DOMDownloadImpl() {
  debug("DOMDownloadImpl constructor ");

  this.wrappedJSObject = this;
  this.totalBytes = 0;
  this.currentBytes = 0;
  this.url = null;
  this.path = null;
  this.storageName = null;
  this.storagePath = null;
  this.contentType = null;

  /* fields that require getters/setters */
  this._error = null;
  this._startTime = new Date();
  this._state = "stopped";

  /* private fields */
  this.id = null;
}

DOMDownloadImpl.prototype = {

  createPromise: function(aPromiseInit) {
    return new this._window.Promise(aPromiseInit);
  },

  pause: function() {
    debug("DOMDownloadImpl pause");
    let id = this.id;
    // We need to wrap the Promise.jsm promise in a "real" DOM promise...
    return this.createPromise(function(aResolve, aReject) {
      DownloadsIPC.pause(id).then(aResolve, aReject);
    });
  },

  resume: function() {
    debug("DOMDownloadImpl resume");
    let id = this.id;
    // We need to wrap the Promise.jsm promise in a "real" DOM promise...
    return this.createPromise(function(aResolve, aReject) {
      DownloadsIPC.resume(id).then(aResolve, aReject);
    });
  },

  set onstatechange(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onstatechange", aHandler);
  },

  get onstatechange() {
    return this.__DOM_IMPL__.getEventHandler("onstatechange");
  },

  get error() {
    return this._error;
  },

  set error(aError) {
    this._error = aError;
  },

  get startTime() {
    return this._startTime;
  },

  set startTime(aStartTime) {
    if (aStartTime instanceof Date) {
      this._startTime = aStartTime;
    }
    else {
      this._startTime = new Date(aStartTime);
    }
  },

  get state() {
    return this._state;
  },

  // We require a setter here to simplify the internals of the Download Manager
  // since we actually pass dummy JSON objects to the child process and update
  // them. This is the case for all other setters for read-only attributes
  // implemented in this object.
  set state(aState) {
    // We need to ensure that XPCOM consumers of this API respect the enum
    // values as well.
    if (["downloading",
         "stopped",
         "succeeded",
         "finalized"].indexOf(aState) != -1) {
      this._state = aState;
    }
  },

  _init: function(aWindow, aDownload) {
    this._window = aWindow;
    this.id = aDownload.id;
    this._update(aDownload);
    Services.obs.addObserver(this, "downloads-state-change-" + this.id,
                             /* ownsWeak */ true);
    debug("observer set for " + this.id);
  },

  /**
    * Updates the state of the object and fires the statechange event.
    */
  _update: function(aDownload) {
    debug("update " + uneval(aDownload));
    if (this.id != aDownload.id) {
      return;
    }

    let props = ["totalBytes", "currentBytes", "url", "path", "storageName",
                 "storagePath", "state", "contentType", "startTime"];
    let changed = false;
    let changedProps = {};

    props.forEach((prop) => {
      if (aDownload[prop] && (aDownload[prop] != this[prop])) {
        this[prop] = aDownload[prop];
        changedProps[prop] = changed = true;
      }
    });

    // When the path changes, we should update the storage name and
    // storage path used for our downloaded file in case our download
    // was re-targetted to a different storage and/or filename.
    if (changedProps["path"]) {
      let storages = this._window.navigator.getDeviceStorages("sdcard");
      let preferredStorageName;
      // Use the first one or the default storage. Just like jsdownloads picks
      // the default / preferred download directory.
      storages.forEach((aStorage) => {
        if (aStorage.default || !preferredStorageName) {
          preferredStorageName = aStorage.storageName;
        }
      });
      // Now get the path for this storage area.
      let volume;
      if (preferredStorageName) {
        let volume = volumeService.getVolumeByName(preferredStorageName);
        if (volume) {
          // Finally, create the relative path of the file that can be used
          // later on to retrieve the file via DeviceStorage. Our path
          // needs to omit the starting '/'.
          this.storageName = preferredStorageName;
          this.storagePath =
            this.path.substring(this.path.indexOf(volume.mountPoint) +
                                volume.mountPoint.length + 1);
        }
      }
    }

    if (aDownload.error) {
      //
      // When we get a generic error failure back from the js downloads api
      // we will verify the status of device storage to see if we can't provide
      // a better error result value.
      //
      // XXX If these checks expand further, consider moving them into their
      // own function.
      //
      let result = aDownload.error.result;
      let storage = this._window.navigator.getDeviceStorage("sdcard");

      // If we don't have access to device storage we'll opt out of these
      // extra checks as they are all dependent on the state of the storage.
      if (result == Cr.NS_ERROR_FAILURE && storage) {
        // We will delay sending the notification until we've inferred which
        // error is really happening.
        changed = false;
        debug("Attempting to infer error via device storage sanity checks.");
        // Get device storage and request availability status.
        let available = storage.available();
        available.onsuccess = (function() {
          debug("Storage Status = '" + available.result + "'");
          let inferredError = result;
          switch (available.result) {
            case "unavailable":
              inferredError = Cr.NS_ERROR_FILE_NOT_FOUND;
              break;
            case "shared":
              inferredError = Cr.NS_ERROR_FILE_ACCESS_DENIED;
              break;
          }
          this._updateWithError(aDownload, inferredError);
        }).bind(this);
        available.onerror = (function() {
          this._updateWithError(aDownload, result);
        }).bind(this);
      }

      this.error =
        new this._window.DOMError("DownloadError", result);
    } else {
      this.error = null;
    }

    // The visible state has not changed, so no need to fire an event.
    if (!changed) {
      return;
    }

    this._sendStateChange();
  },

  _updateWithError: function(aDownload, aError) {
    this.error =
      new this._window.DOMError("DownloadError", aError);
    this._sendStateChange();
  },

  _sendStateChange: function() {
    // __DOM_IMPL__ may not be available at first update.
    if (this.__DOM_IMPL__) {
      let event = new this._window.DownloadEvent("statechange", {
        download: this.__DOM_IMPL__
      });
      debug("Dispatching statechange event. state=" + this.state);
      this.__DOM_IMPL__.dispatchEvent(event);
    }
  },

  observe: function(aSubject, aTopic, aData) {
    debug("DOMDownloadImpl observe " + aTopic);
    if (aTopic !== "downloads-state-change-" + this.id) {
      return;
    }

    try {
      let download = JSON.parse(aData);
      // We get the start time as milliseconds, not as a Date object.
      if (download.startTime) {
        download.startTime = new Date(download.startTime);
      }
      this._update(download);
    } catch(e) {}
  },

  classID: Components.ID("{96b81b99-aa96-439d-8c59-92eeed34705f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DOMDownloadManagerImpl,
                                                     DOMDownloadImpl]);
