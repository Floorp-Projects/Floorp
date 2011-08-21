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
 * The Original Code is thumbnailStorage.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
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
// Title: thumbnailStorage.js

// ##########
// Class: ThumbnailStorage
// Singleton for persistent storage of thumbnail data.
let ThumbnailStorage = {
  CACHE_CLIENT_IDENTIFIER: "tabview-cache",
  CACHE_PREFIX: "moz-panorama:",

  // Holds the cache session reference
  _cacheSession: null,

  // Holds the string input stream reference
  _stringInputStream: null,

  // Holds the storage stream reference
  _storageStream: null,

  // ----------
  // Function: toString
  // Prints [ThumbnailStorage] for debug use.
  toString: function ThumbnailStorage_toString() {
    return "[ThumbnailStorage]";
  },

  // ----------
  // Function: init
  // Should be called when UI is initialized.
  init: function ThumbnailStorage_init() {
    // Create stream-based cache session for tabview
    let cacheService = 
      Cc["@mozilla.org/network/cache-service;1"].
        getService(Ci.nsICacheService);
    this._cacheSession = cacheService.createSession(
      this.CACHE_CLIENT_IDENTIFIER, Ci.nsICache.STORE_ON_DISK, true);
    this._stringInputStream = Components.Constructor(
      "@mozilla.org/io/string-input-stream;1", "nsIStringInputStream",
      "setData");
    this._storageStream = Components.Constructor(
      "@mozilla.org/storagestream;1", "nsIStorageStream", 
      "init");
  },

  // ----------
  // Function: _openCacheEntry
  // Opens a cache entry for the given <url> and requests access <access>.
  // Calls <successCallback>(entry) when the entry was successfully opened with
  // requested access rights. Otherwise calls <errorCallback>().
  _openCacheEntry: function ThumbnailStorage__openCacheEntry(url, access, successCallback, errorCallback) {
    let onCacheEntryAvailable = function(entry, accessGranted, status) {
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
      let status = Cr.NS_OK;
      onCacheEntryAvailable(entry, entry.accessGranted, status);
    } else {
      let listener = new CacheListener(onCacheEntryAvailable);
      this._cacheSession.asyncOpenCacheEntry(key, access, listener);
    }
  },

  // ----------
  // Function: saveThumbnail
  // Saves the <imageData> to the cache using the given <url> as key.
  // Calls <callback>(status, data) when finished, passing true or false
  // (indicating whether the operation succeeded).
  saveThumbnail: function ThumbnailStorage_saveThumbnail(tab, imageData, callback) {
    Utils.assert(tab, "tab");
    Utils.assert(imageData, "imageData");

    if (!StoragePolicy.canStoreThumbnailForTab(tab)) {
      tab._tabViewTabItem._sendToSubscribers("deniedToCacheImageData");
      if (callback)
        callback(false);
      return;
    }

    let self = this;

    let completed = function(status) {
      if (callback)
        callback(status);

      if (status) {
        // Notify subscribers
        tab._tabViewTabItem._sendToSubscribers("savedCachedImageData");
      } else {
        Utils.log("Error while saving thumbnail: " + e);
      }
    };

    let onCacheEntryAvailable = function(entry) {
      let outputStream = entry.openOutputStream(0);

      let cleanup = function() {
        outputStream.close();
        entry.close();
      }

      // switch to synchronous mode if parent window is about to close
      if (UI.isDOMWindowClosing) {
        outputStream.write(imageData, imageData.length);
        cleanup();
        completed(true);
        return;
      }

      // asynchronous mode
      let inputStream = new self._stringInputStream(imageData, imageData.length);
      gNetUtil.asyncCopy(inputStream, outputStream, function (result) {
        cleanup();
        inputStream.close();
        completed(Components.isSuccessCode(result));
      });
    }

    let onCacheEntryUnavailable = function() {
      completed(false);
    }

    this._openCacheEntry(tab.linkedBrowser.currentURI.spec, 
        Ci.nsICache.ACCESS_WRITE, onCacheEntryAvailable, 
        onCacheEntryUnavailable);
  },

  // ----------
  // Function: loadThumbnail
  // Asynchrously loads image data from the cache using the given <url> as key.
  // Calls <callback>(status, data) when finished, passing true or false
  // (indicating whether the operation succeeded) and the retrieved image data.
  loadThumbnail: function ThumbnailStorage_loadThumbnail(tab, url, callback) {
    Utils.assert(tab, "tab");
    Utils.assert(url, "url");
    Utils.assert(typeof callback == "function", "callback arg must be a function");

    let self = this;

    let completed = function(status, imageData) {
      callback(status, imageData);

      if (status) {
        // Notify subscribers
        tab._tabViewTabItem._sendToSubscribers("loadedCachedImageData");
      } else {
        Utils.log("Error while loading thumbnail");
      }
    }

    let onCacheEntryAvailable = function(entry) {
      let imageChunks = [];
      let nativeInputStream = entry.openInputStream(0);

      const CHUNK_SIZE = 0x10000; // 65k
      const PR_UINT32_MAX = 0xFFFFFFFF;
      let storageStream = new self._storageStream(CHUNK_SIZE, PR_UINT32_MAX, null);
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
        completed(isSuccess, imageData);
      });
    }

    let onCacheEntryUnavailable = function() {
      completed(false);
    }

    this._openCacheEntry(url, Ci.nsICache.ACCESS_READ,
        onCacheEntryAvailable, onCacheEntryUnavailable);
  }
}

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
  onCacheEntryAvailable: function CacheListener_onCacheEntryAvailable(
    entry, access, status) {
    this.callback(entry, access, status);
  }
};

