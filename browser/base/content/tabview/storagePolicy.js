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
 * The Original Code is storagePolicy.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Tim Taubert <ttaubert@mozilla.com>
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
// Title: storagePolicy.js

// ##########
// Class: StoragePolicy
// Singleton for implementing a storage policy for sensitive data.
let StoragePolicy = {
  // Pref that controls whether we can store SSL content on disk
  PREF_DISK_CACHE_SSL: "browser.cache.disk_cache_ssl",

  // Used to keep track of disk_cache_ssl preference
  _enablePersistentHttpsCaching: null,

  // Used to keep track of browsers whose data we shouldn't store permanently
  _deniedBrowsers: [],

  // ----------
  // Function: toString
  // Prints [StoragePolicy] for debug use.
  toString: function StoragePolicy_toString() {
    return "[StoragePolicy]";
  },

  // ----------
  // Function: init
  // Initializes the StoragePolicy object.
  init: function StoragePolicy_init() {
    // store the preference value
    this._enablePersistentHttpsCaching =
      Services.prefs.getBoolPref(this.PREF_DISK_CACHE_SSL);

    Services.prefs.addObserver(this.PREF_DISK_CACHE_SSL, this, false);

    // tabs are already loaded before UI is initialized so cache-control
    // values are unknown. We add browsers with https to the list for now.
    if (!this._enablePersistentHttpsCaching)
      Array.forEach(gBrowser.browsers, this._initializeBrowser.bind(this));

    // make sure to remove tab browsers when tabs get closed
    this._onTabClose = this._onTabClose.bind(this);
    gBrowser.tabContainer.addEventListener("TabClose", this._onTabClose, false);

    let mm = gWindow.messageManager;

    // add message listeners for storage granted
    this._onGranted = this._onGranted.bind(this);
    mm.addMessageListener("Panorama:StoragePolicy:granted", this._onGranted);

    // add message listeners for storage denied
    this._onDenied = this._onDenied.bind(this);
    mm.addMessageListener("Panorama:StoragePolicy:denied", this._onDenied);
  },

  // ----------
  // Function: _initializeBrowser
  // Initializes the given browser and checks if we need to add it to our
  // internal exclusion list.
  _initializeBrowser: function StoragePolicy__initializeBrowser(browser) {
    let self = this;

    function checkExclusion() {
      if (browser.currentURI.schemeIs("https"))
        self._deniedBrowsers.push(browser);
    }

    function waitForDocumentLoad() {
      mm.addMessageListener("Panorama:Load", function onLoad(cx) {
        mm.removeMessageListener(cx.name, onLoad);
        checkExclusion(browser);
      });
    }

    this._isDocumentLoaded(browser, function (isLoaded) {
      if (isLoaded)
        checkExclusion();
      else
        waitForDocumentLoad();
    });
  },

  // ----------
  // Function: _isDocumentLoaded
  // Check if the given browser's document is loaded.
  _isDocumentLoaded: function StoragePolicy__isDocumentLoaded(browser, callback) {
    let mm = browser.messageManager;
    let message = "Panorama:isDocumentLoaded";

    mm.addMessageListener(message, function onMessage(cx) {
      mm.removeMessageListener(cx.name, onMessage);
      callback(cx.json.isLoaded);
    });

    mm.sendAsyncMessage(message);
  },

  // ----------
  // Function: uninit
  // Is called by UI.init() when the browser windows is closed.
  uninit: function StoragePolicy_uninit() {
    Services.prefs.removeObserver(this.PREF_DISK_CACHE_SSL, this);
    gBrowser.removeTabsProgressListener(this);
    gBrowser.tabContainer.removeEventListener("TabClose", this._onTabClose, false);

    let mm = gWindow.messageManager;

    // remove message listeners
    mm.removeMessageListener("Panorama:StoragePolicy:granted", this._onGranted);
    mm.removeMessageListener("Panorama:StoragePolicy:denied", this._onDenied);
  },

  // ----------
  // Function: _onGranted
  // Handle the 'granted' message and remove the given browser from the list
  // of denied browsers.
  _onGranted: function StoragePolicy__onGranted(cx) {
    let index = this._deniedBrowsers.indexOf(cx.target);

    if (index > -1)
      this._deniedBrowsers.splice(index, 1);
  },

  // ----------
  // Function: _onDenied
  // Handle the 'denied' message and add the given browser to the list of denied
  // browsers.
  _onDenied: function StoragePolicy__onDenied(cx) {
    // exclusion is optional because cache-control is not no-store or public and
    // the protocol is https. don't exclude when persistent https caching is
    // enabled.
    if ("https" == cx.json.reason && this._enablePersistentHttpsCaching)
      return;

    let browser = cx.target;

    if (this._deniedBrowsers.indexOf(browser) == -1)
      this._deniedBrowsers.push(browser);
  },

  // ----------
  // Function: _onTabClose
  // Remove the browser from our internal exclusion list when a tab gets closed.
  _onTabClose: function StoragePolicy__onTabClose(event) {
    let browser = event.target.linkedBrowser;
    let index = this._deniedBrowsers.indexOf(browser);

    if (index > -1)
      this._deniedBrowsers.splice(index, 1);
  },

  // ----------
  // Function: canStoreThumbnailForTab
  // Returns whether we're allowed to store the thumbnail of the given tab.
  canStoreThumbnailForTab: function StoragePolicy_canStoreThumbnailForTab(tab) {
    return (this._deniedBrowsers.indexOf(tab.linkedBrowser) == -1);
  },

  // ----------
  // Function: observe
  // Observe pref changes.
  observe: function StoragePolicy_observe(subject, topic, data) {
    this._enablePersistentHttpsCaching =
      Services.prefs.getBoolPref(this.PREF_DISK_CACHE_SSL);
  }
};
