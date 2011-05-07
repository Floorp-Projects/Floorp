/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is storage.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ehsan Akhgari <ehsan@mozilla.com>
 * Ian Gilman <ian@iangilman.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// **********
// Title: storage.js

// ##########
// Class: Storage
// Singleton for permanent storage of TabView data.
let Storage = {
  GROUP_DATA_IDENTIFIER: "tabview-group",
  GROUPS_DATA_IDENTIFIER: "tabview-groups",
  TAB_DATA_IDENTIFIER: "tabview-tab",
  UI_DATA_IDENTIFIER: "tabview-ui",
  CACHE_CLIENT_IDENTIFIER: "tabview-cache",
  CACHE_PREFIX: "moz-panorama:",

  // ----------
  // Function: toString
  // Prints [Storage] for debug use
  toString: function Storage_toString() {
    return "[Storage]";
  },

  // ----------
  // Function: init
  // Sets up the object.
  init: function Storage_init() {
    this._sessionStore =
      Cc["@mozilla.org/browser/sessionstore;1"].
        getService(Ci.nsISessionStore);
    
    // Create stream-based cache session for tabview
    let cacheService = 
      Cc["@mozilla.org/network/cache-service;1"].
        getService(Ci.nsICacheService);
    this._cacheSession = cacheService.createSession(
      this.CACHE_CLIENT_IDENTIFIER, Ci.nsICache.STORE_ON_DISK, true);
    this.StringInputStream = Components.Constructor(
      "@mozilla.org/io/string-input-stream;1", "nsIStringInputStream",
      "setData");
    this.StorageStream = Components.Constructor(
      "@mozilla.org/storagestream;1", "nsIStorageStream", 
      "init");
  },

  // ----------
  // Function: uninit
  uninit: function Storage_uninit () {
    this._sessionStore = null;
    this._cacheSession = null;
    this.StringInputStream = null;
    this.StorageStream = null;
  },

  // ----------
  // Function: wipe
  // Cleans out all the stored data, leaving empty objects.
  wipe: function Storage_wipe() {
    try {
      var self = this;

      // ___ Tabs
      AllTabs.tabs.forEach(function(tab) {
        if (tab.ownerDocument.defaultView != gWindow)
          return;

        self.saveTab(tab, null);
      });

      // ___ Other
      this.saveGroupItemsData(gWindow, {});
      this.saveUIData(gWindow, {});

      this._sessionStore.setWindowValue(gWindow, this.GROUP_DATA_IDENTIFIER,
        JSON.stringify({}));
    } catch (e) {
      Utils.log("Error in wipe: "+e);
    }
  },

  // ----------
  // Function: _openCacheEntry
  // Opens a cache entry for the given <url> and requests access <access>.
  // Calls <successCallback>(entry) when the entry was successfully opened with
  // requested access rights. Otherwise calls <errorCallback>().
  _openCacheEntry: function Storage__openCacheEntry(url, access, successCallback, errorCallback) {
    let onCacheEntryAvailable = function (entry, accessGranted, status) {
      if (entry && access == accessGranted && Components.isSuccessCode(status)) {
        successCallback(entry);
      } else {
        entry && entry.close();
        errorCallback();
      }
    }

    let key = this.CACHE_PREFIX + url;

    // switch to synchronous mode if parent window is about to close
    if (UI.isDOMWindowClosing) {
      let entry = this._cacheSession.openCacheEntry(key, access, true);
      let status = Components.results.NS_OK;
      onCacheEntryAvailable(entry, entry.accessGranted, status);
    } else {
      let listener = new CacheListener(onCacheEntryAvailable);
      this._cacheSession.asyncOpenCacheEntry(key, access, listener);
    }
  },

  // ----------
  // Function: saveThumbnail
  // Saves the <imageData> to the cache using the given <url> as key.
  // Calls <callback>(status) when finished (passing true or false indicating
  // whether the operation succeeded).
  saveThumbnail: function Storage_saveThumbnail(url, imageData, callback) {
    Utils.assert(url, "url");
    Utils.assert(imageData, "imageData");
    Utils.assert(typeof callback == "function", "callback arg must be a function");

    let self = this;
    let StringInputStream = this.StringInputStream;

    let onCacheEntryAvailable = function (entry) {
      let outputStream = entry.openOutputStream(0);

      let cleanup = function () {
        outputStream.close();
        entry.close();
      }

      // switch to synchronous mode if parent window is about to close
      if (UI.isDOMWindowClosing) {
        outputStream.write(imageData, imageData.length);
        cleanup();
        callback(true);
        return;
      }

      // asynchronous mode
      let inputStream = new StringInputStream(imageData, imageData.length);
      gNetUtil.asyncCopy(inputStream, outputStream, function (result) {
        cleanup();
        inputStream.close();
        callback(Components.isSuccessCode(result));
      });
    }

    let onCacheEntryUnavailable = function () {
      callback(false);
    }

    this._openCacheEntry(url, Ci.nsICache.ACCESS_WRITE,
        onCacheEntryAvailable, onCacheEntryUnavailable);
  },

  // ----------
  // Function: loadThumbnail
  // Asynchrously loads image data from the cache using the given <url> as key.
  // Calls <callback>(status, data) when finished, passing true or false
  // (indicating whether the operation succeeded) and the retrieved image data.
  loadThumbnail: function Storage_loadThumbnail(url, callback) {
    Utils.assert(url, "url");
    Utils.assert(typeof callback == "function", "callback arg must be a function");

    let self = this;

    let onCacheEntryAvailable = function (entry) {
      let imageChunks = [];
      let nativeInputStream = entry.openInputStream(0);

      const CHUNK_SIZE = 0x10000; // 65k
      const PR_UINT32_MAX = 0xFFFFFFFF;
      let storageStream = new self.StorageStream(CHUNK_SIZE, PR_UINT32_MAX, null);
      let storageOutStream = storageStream.getOutputStream(0);

      let cleanup = function () {
        nativeInputStream.close();
        storageStream.close();
        storageOutStream.close();
        entry.close();
      }

      gNetUtil.asyncCopy(nativeInputStream, storageOutStream, function (result) {
        // cancel if parent window has already been closed
        if (typeof UI == "undefined") {
          cleanup();
          return;
        }

        let imageData = null;
        let isSuccess = Components.isSuccessCode(result);

        if (isSuccess) {
          let storageInStream = storageStream.newInputStream(0);
          imageData = gNetUtil.readInputStreamToString(storageInStream,
            storageInStream.available());
          storageInStream.close();
        }

        cleanup();
        callback(isSuccess, imageData);
      });
    }

    let onCacheEntryUnavailable = function () {
      callback(false);
    }

    this._openCacheEntry(url, Ci.nsICache.ACCESS_READ,
        onCacheEntryAvailable, onCacheEntryUnavailable);
  },

  // ----------
  // Function: saveTab
  // Saves the data for a single tab.
  saveTab: function Storage_saveTab(tab, data) {
    Utils.assert(tab, "tab");

    if (data != null) {
      let imageData = data.imageData;
      // Remove imageData from payload
      delete data.imageData;
      if (imageData != null) {
        this.saveThumbnail(data.url, imageData, function (status) {
          if (status) {
            // Notify subscribers
            tab._tabViewTabItem._sendToSubscribers("savedCachedImageData");
          } else {
            Utils.log("Error while saving thumbnail: " + e);
          }
        });
      }
    }

    this._sessionStore.setTabValue(tab, this.TAB_DATA_IDENTIFIER,
      JSON.stringify(data));
  },

  // ----------
  // Function: getTabData
  // Load tab data from session store and return it. Asynchrously loads the tab's
  // thumbnail from the cache and calls <callback>(imageData) when done.
  getTabData: function Storage_getTabData(tab, callback) {
    Utils.assert(tab, "tab");
    Utils.assert(typeof callback == "function", "callback arg must be a function");

    let existingData = null;

    try {
      let tabData = this._sessionStore.getTabValue(tab, this.TAB_DATA_IDENTIFIER);
      if (tabData != "") {
        existingData = JSON.parse(tabData);
      }
    } catch (e) {
      // getTabValue will fail if the property doesn't exist.
      Utils.log(e);
    }

    if (existingData) {
      this.loadThumbnail(existingData.url, function (status, imageData) {
        if (status) {
          callback(imageData);

          // Notify subscribers
          tab._tabViewTabItem._sendToSubscribers("loadedCachedImageData");
        } else {
          Utils.log("Error while loading thumbnail");
        }
      });
    }

    return existingData;
  },

  // ----------
  // Function: saveGroupItem
  // Saves the data for a single groupItem, associated with a specific window.
  saveGroupItem: function Storage_saveGroupItem(win, data) {
    var id = data.id;
    var existingData = this.readGroupItemData(win);
    existingData[id] = data;
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
  // Function: deleteGroupItem
  // Deletes the data for a single groupItem from the given window.
  deleteGroupItem: function Storage_deleteGroupItem(win, id) {
    var existingData = this.readGroupItemData(win);
    delete existingData[id];
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
  // Function: readGroupItemData
  // Returns the data for all groupItems associated with the given window.
  readGroupItemData: function Storage_readGroupItemData(win) {
    var existingData = {};
    let data;
    try {
      data = this._sessionStore.getWindowValue(win, this.GROUP_DATA_IDENTIFIER);
      if (data)
        existingData = JSON.parse(data);
    } catch (e) {
      // getWindowValue will fail if the property doesn't exist
      Utils.log("Error in readGroupItemData: "+e, data);
    }
    return existingData;
  },

  // ----------
  // Function: saveGroupItemsData
  // Saves the global data for the <GroupItems> singleton for the given window.
  saveGroupItemsData: function Storage_saveGroupItemsData(win, data) {
    this.saveData(win, this.GROUPS_DATA_IDENTIFIER, data);
  },

  // ----------
  // Function: readGroupItemsData
  // Reads the global data for the <GroupItems> singleton for the given window.
  readGroupItemsData: function Storage_readGroupItemsData(win) {
    return this.readData(win, this.GROUPS_DATA_IDENTIFIER);
  },

  // ----------
  // Function: saveUIData
  // Saves the global data for the <UIManager> singleton for the given window.
  saveUIData: function Storage_saveUIData(win, data) {
    this.saveData(win, this.UI_DATA_IDENTIFIER, data);
  },

  // ----------
  // Function: readUIData
  // Reads the global data for the <UIManager> singleton for the given window.
  readUIData: function Storage_readUIData(win) {
    return this.readData(win, this.UI_DATA_IDENTIFIER);
  },

  // ----------
  // Function: saveVisibilityData
  // Saves visibility for the given window.
  saveVisibilityData: function Storage_saveVisibilityData(win, data) {
    this._sessionStore.setWindowValue(
      win, win.TabView.VISIBILITY_IDENTIFIER, data);
  },

  // ----------
  // Function: saveData
  // Generic routine for saving data to a window.
  saveData: function Storage_saveData(win, id, data) {
    try {
      this._sessionStore.setWindowValue(win, id, JSON.stringify(data));
    } catch (e) {
      Utils.log("Error in saveData: "+e);
    }
  },

  // ----------
  // Function: readData
  // Generic routine for reading data from a window.
  readData: function Storage_readData(win, id) {
    var existingData = {};
    try {
      var data = this._sessionStore.getWindowValue(win, id);
      if (data)
        existingData = JSON.parse(data);
    } catch (e) {
      Utils.log("Error in readData: "+e);
    }

    return existingData;
  }
};

// ##########
// Class: CacheListener
// Generic CacheListener for feeding to asynchronous cache calls.
// Calls <callback>(entry, access, status) when the requested cache entry
// is available.
function CacheListener(callback) {
  Utils.assert(typeof callback == "function", "callback arg must be a function");
  this.callback = callback;
};

CacheListener.prototype = {
  // ----------
  // Function: toString
  // Prints [CacheListener] for debug use
  toString: function CacheListener_toString() {
    return "[CacheListener]";
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsICacheListener]),
  onCacheEntryAvailable: function (entry, access, status) {
    this.callback(entry, access, status);
  }
};
