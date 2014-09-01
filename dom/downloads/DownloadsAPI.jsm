/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Downloads.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

function debug(aStr) {
#ifdef MOZ_DEBUG
  dump("-*- DownloadsAPI.jsm : " + aStr + "\n");
#endif
}

function sendPromiseMessage(aMm, aMessageName, aData, aError) {
  debug("sendPromiseMessage " + aMessageName);
  let msg = {
    id: aData.id,
    promiseId: aData.promiseId
  };

  if (aError) {
    msg.error = aError;
  }

  aMm.sendAsyncMessage(aMessageName, msg);
}

let DownloadsAPI = {
  init: function() {
    debug("init");

    this._ids = new WeakMap(); // Maps toolkit download objects to ids.
    this._index = {};          // Maps ids to downloads.

    ["Downloads:GetList",
     "Downloads:ClearAllDone",
     "Downloads:Remove",
     "Downloads:Pause",
     "Downloads:Resume"].forEach((msgName) => {
      ppmm.addMessageListener(msgName, this);
    });

    let self = this;
    Task.spawn(function () {
      let list = yield Downloads.getList(Downloads.ALL);
      yield list.addView(self);

      debug("view added to download list.");
    }).then(null, Components.utils.reportError);

    this._currentId = 0;
  },

  /**
    * Returns a unique id for each download, hashing the url and the path.
    */
  downloadId: function(aDownload) {
    let id = this._ids.get(aDownload, null);
    if (!id) {
      id = "download-" + this._currentId++;
      this._ids.set(aDownload, id);
      this._index[id] = aDownload;
    }
    return id;
  },

  getDownloadById: function(aId) {
    return this._index[aId];
  },

  /**
    * Converts a download object into a plain json object that we'll
    * send to the DOM side.
    */
  jsonDownload: function(aDownload) {
    let res = {
      totalBytes: aDownload.totalBytes,
      currentBytes: aDownload.currentBytes,
      url: aDownload.source.url,
      path: aDownload.target.path,
      contentType: aDownload.contentType,
      startTime: aDownload.startTime.getTime()
    };

    if (aDownload.error) {
      res.error = aDownload.error;
    }

    res.id = this.downloadId(aDownload);

    // The state of the download. Can be any of "downloading", "stopped",
    // "succeeded", finalized".

    // Default to "stopped"
    res.state = "stopped";
    if (!aDownload.stopped &&
        !aDownload.canceled &&
        !aDownload.succeeded &&
        !aDownload.DownloadError) {
      res.state = "downloading";
    } else if (aDownload.succeeded) {
      res.state = "succeeded";
    }
    return res;
  },

  /**
    * download view methods.
    */
  onDownloadAdded: function(aDownload) {
    let download = this.jsonDownload(aDownload);
    debug("onDownloadAdded " + uneval(download));
    ppmm.broadcastAsyncMessage("Downloads:Added", download);
  },

  onDownloadRemoved: function(aDownload) {
    let download = this.jsonDownload(aDownload);
    download.state = "finalized";
    debug("onDownloadRemoved " + uneval(download));
    ppmm.broadcastAsyncMessage("Downloads:Removed", download);
    this._index[this._ids.get(aDownload)] = null;
    this._ids.delete(aDownload);
  },

  onDownloadChanged: function(aDownload) {
    let download = this.jsonDownload(aDownload);
    debug("onDownloadChanged " + uneval(download));
    ppmm.broadcastAsyncMessage("Downloads:Changed", download);
  },

  receiveMessage: function(aMessage) {
    if (!aMessage.target.assertPermission("downloads")) {
      debug("No 'downloads' permission!");
      return;
    }

    debug("message: " + aMessage.name);

    switch (aMessage.name) {
    case "Downloads:GetList":
      this.getList(aMessage.data, aMessage.target);
      break;
    case "Downloads:ClearAllDone":
      this.clearAllDone(aMessage.data, aMessage.target);
      break;
    case "Downloads:Remove":
      this.remove(aMessage.data, aMessage.target);
      break;
    case "Downloads:Pause":
      this.pause(aMessage.data, aMessage.target);
      break;
    case "Downloads:Resume":
      this.resume(aMessage.data, aMessage.target);
      break;
    default:
      debug("Invalid message: " + aMessage.name);
    }
  },

  getList: function(aData, aMm) {
    debug("getList called!");
    let self = this;
    Task.spawn(function () {
      let list = yield Downloads.getList(Downloads.ALL);
      let downloads = yield list.getAll();
      let res = [];
      downloads.forEach((aDownload) => {
        res.push(self.jsonDownload(aDownload));
      });
      aMm.sendAsyncMessage("Downloads:GetList:Return", res);
    }).then(null, Components.utils.reportError);
  },

  clearAllDone: function(aData, aMm) {
    debug("clearAllDone called!");
    let self = this;
    Task.spawn(function () {
      let list = yield Downloads.getList(Downloads.ALL);
      yield list.removeFinished();
      list = yield Downloads.getList(Downloads.ALL);
      let downloads = yield list.getAll();
      let res = [];
      downloads.forEach((aDownload) => {
        res.push(self.jsonDownload(aDownload));
      });
      aMm.sendAsyncMessage("Downloads:ClearAllDone:Return", res);
    }).then(null, Components.utils.reportError);
  },

  remove: function(aData, aMm) {
    debug("remove id " + aData.id);
    let download = this.getDownloadById(aData.id);
    if (!download) {
      sendPromiseMessage(aMm, "Downloads:Remove:Return",
                         aData, "NoSuchDownload");
      return;
    }

    Task.spawn(function() {
      yield download.finalize(true);
      let list = yield Downloads.getList(Downloads.ALL);
      yield list.remove(download);
    }).then(
      function() {
        sendPromiseMessage(aMm, "Downloads:Remove:Return", aData);
      },
      function() {
        sendPromiseMessage(aMm, "Downloads:Remove:Return",
                           aData, "RemoveError");
      }
    );
  },

  pause: function(aData, aMm) {
    debug("pause id " + aData.id);
    let download = this.getDownloadById(aData.id);
    if (!download) {
      sendPromiseMessage(aMm, "Downloads:Pause:Return",
                         aData, "NoSuchDownload");
      return;
    }

    download.cancel().then(
      function() {
        sendPromiseMessage(aMm, "Downloads:Pause:Return", aData);
      },
      function() {
        sendPromiseMessage(aMm, "Downloads:Pause:Return",
                           aData, "PauseError");
      }
    );
  },

  resume: function(aData, aMm) {
    debug("resume id " + aData.id);
    let download = this.getDownloadById(aData.id);
    if (!download) {
      sendPromiseMessage(aMm, "Downloads:Resume:Return",
                         aData, "NoSuchDownload");
      return;
    }

    download.start().then(
      function() {
        sendPromiseMessage(aMm, "Downloads:Resume:Return", aData);
      },
      function() {
        sendPromiseMessage(aMm, "Downloads:Resume:Return",
                           aData, "ResumeError");
      }
    );
  }
};

DownloadsAPI.init();
