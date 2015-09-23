/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import('resource://gre/modules/PlacesUtils.jsm');

var FavIcons = {
  // Pref that controls whether to display site icons.
  PREF_CHROME_SITE_ICONS: "browser.chrome.site_icons",

  // Pref that controls whether to display fav icons.
  PREF_CHROME_FAVICONS: "browser.chrome.favicons",

  // Lazy getter for pref browser.chrome.site_icons.
  get _prefSiteIcons() {
    delete this._prefSiteIcons;
    this._prefSiteIcons = Services.prefs.getBoolPref(this.PREF_CHROME_SITE_ICONS);
  },

  // Lazy getter for pref browser.chrome.favicons.
  get _prefFavicons() {
    delete this._prefFavicons;
    this._prefFavicons = Services.prefs.getBoolPref(this.PREF_CHROME_FAVICONS);
  },

  get defaultFavicon() {
    return this._favIconService.defaultFavicon.spec;
  },

  init: function FavIcons_init() {
    XPCOMUtils.defineLazyServiceGetter(this, "_favIconService",
      "@mozilla.org/browser/favicon-service;1", "nsIFaviconService");

    Services.prefs.addObserver(this.PREF_CHROME_SITE_ICONS, this, false);
    Services.prefs.addObserver(this.PREF_CHROME_FAVICONS, this, false);
  },

  uninit: function FavIcons_uninit() {
    Services.prefs.removeObserver(this.PREF_CHROME_SITE_ICONS, this);
    Services.prefs.removeObserver(this.PREF_CHROME_FAVICONS, this);
  },

  observe: function FavIcons_observe(subject, topic, data) {
    let value = Services.prefs.getBoolPref(data);

    if (data == this.PREF_CHROME_SITE_ICONS)
      this._prefSiteIcons = value;
    else if (data == this.PREF_CHROME_FAVICONS)
      this._prefFavicons = value;
  },

  // ----------
  // Function: getFavIconUrlForTab
  // Gets the "favicon link URI" for the given xul:tab, or null if unavailable.
  getFavIconUrlForTab: function FavIcons_getFavIconUrlForTab(tab, callback) {
    this._isImageDocument(tab, function (isImageDoc) {
      if (isImageDoc) {
        callback(tab.pinned ? tab.image : null);
      } else {
        this._getFavIconForNonImageDocument(tab, callback);
      }
    }.bind(this));
  },

  // ----------
  // Function: _getFavIconForNonImageDocument
  // Retrieves the favicon for a tab containing a non-image document.
  _getFavIconForNonImageDocument:
    function FavIcons_getFavIconForNonImageDocument(tab, callback) {

    if (tab.image)
      this._getFavIconFromTabImage(tab, callback);
    else if (this._shouldLoadFavIcon(tab))
      this._getFavIconForHttpDocument(tab, callback);
    else
      callback(null);
  },

  // ----------
  // Function: _getFavIconFromTabImage
  // Retrieves the favicon for tab with a tab image.
  _getFavIconFromTabImage:
    function FavIcons_getFavIconFromTabImage(tab, callback) {

    let tabImage = gBrowser.getIcon(tab);

    // If the tab image's url starts with http(s), fetch icon from favicon
    // service via the moz-anno protocol.
    if (/^https?:/.test(tabImage)) {
      let tabImageURI = gWindow.makeURI(tabImage);
      tabImage = this._favIconService.getFaviconLinkForIcon(tabImageURI).spec;
    }

    if (tabImage) {
      tabImage = PlacesUtils.getImageURLForResolution(window, tabImage);
    }

    callback(tabImage);
  },

  // ----------
  // Function: _getFavIconForHttpDocument
  // Retrieves the favicon for tab containg a http(s) document.
  _getFavIconForHttpDocument:
    function FavIcons_getFavIconForHttpDocument(tab, callback) {

    let {currentURI} = tab.linkedBrowser;
    this._favIconService.getFaviconURLForPage(currentURI, function (uri) {
      if (uri) {
        let icon = PlacesUtils.getImageURLForResolution(window,
                     this._favIconService.getFaviconLinkForIcon(uri).spec);
        callback(icon);
      } else {
        callback(this.defaultFavicon);
      }
    }.bind(this));
  },

  // ----------
  // Function: _isImageDocument
  // Checks whether an image is loaded into the given tab.
  _isImageDocument: function UI__isImageDocument(tab, callback) {
    let mm = tab.linkedBrowser.messageManager;
    let message = "Panorama:isImageDocument";

    mm.addMessageListener(message, function onMessage(cx) {
      mm.removeMessageListener(cx.name, onMessage);
      callback(cx.json.isImageDocument);
    });

    mm.sendAsyncMessage(message);
  },

  // ----------
  // Function: _shouldLoadFavIcon
  // Checks whether fav icon should be loaded for a given tab.
  _shouldLoadFavIcon: function FavIcons_shouldLoadFavIcon(tab) {
    // No need to load a favicon if the user doesn't want site or favicons.
    if (!this._prefSiteIcons || !this._prefFavicons)
      return false;

    let uri = tab.linkedBrowser.currentURI;

    // Stop here if we don't have a valid nsIURI.
    if (!uri || !(uri instanceof Ci.nsIURI))
      return false;

    // Load favicons for http(s) pages only.
    return uri.schemeIs("http") || uri.schemeIs("https");
  }
};
