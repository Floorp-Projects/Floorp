/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals Services, XPCOMUtils, RemotePages, RemoteNewTabLocation, RemoteNewTabUtils, Task  */
/* globals BackgroundPageThumbs, PageThumbs, DirectoryLinksProvider */
/* exported RemoteAboutNewTab */

"use strict";

let Ci = Components.interfaces;
let Cu = Components.utils;
const XHTML_NAMESPACE = "http://www.w3.org/1999/xhtml";

this.EXPORTED_SYMBOLS = ["RemoteAboutNewTab"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.importGlobalProperties(["URL"]);

XPCOMUtils.defineLazyModuleGetter(this, "RemotePages",
  "resource://gre/modules/RemotePageManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RemoteNewTabUtils",
  "resource:///modules/RemoteNewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BackgroundPageThumbs",
  "resource://gre/modules/BackgroundPageThumbs.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DirectoryLinksProvider",
  "resource:///modules/DirectoryLinksProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RemoteNewTabLocation",
  "resource:///modules/RemoteNewTabLocation.jsm");

let RemoteAboutNewTab = {

  pageListener: null,

  /**
   * Initialize the RemotePageManager and add all message listeners for this page
   */
  init: function() {
    this.pageListener = new RemotePages("about:remote-newtab");
    this.pageListener.addMessageListener("NewTab:InitializeGrid", this.initializeGrid.bind(this));
    this.pageListener.addMessageListener("NewTab:UpdateGrid", this.updateGrid.bind(this));
    this.pageListener.addMessageListener("NewTab:CaptureBackgroundPageThumbs",
        this.captureBackgroundPageThumb.bind(this));
    this.pageListener.addMessageListener("NewTab:PageThumbs", this.createPageThumb.bind(this));
    this.pageListener.addMessageListener("NewTabFrame:GetInit", this.initContentFrame.bind(this));

    this._addObservers();
  },

  /**
   * Initializes the grid for the first time when the page loads.
   * Fetch all the links and send them down to the child to populate
   * the grid with.
   *
   * @param {Object} message
   *        A RemotePageManager message.
   */
  initializeGrid: function(message) {
    RemoteNewTabUtils.links.populateCache(() => {
      message.target.sendAsyncMessage("NewTab:InitializeLinks", {
        links: RemoteNewTabUtils.links.getLinks(),
        enhancedLinks: this.getEnhancedLinks(),
      });
    });
  },

  /**
   * Inits the content iframe with the newtab location
   */
  initContentFrame: function(message) {
    message.target.sendAsyncMessage("NewTabFrame:Init", {
      href: RemoteNewTabLocation.href,
      origin: RemoteNewTabLocation.origin
    });
  },

  /**
   * Updates the grid by getting a new set of links.
   *
   * @param {Object} message
   *        A RemotePageManager message.
   */
  updateGrid: function(message) {
    message.target.sendAsyncMessage("NewTab:UpdateLinks", {
      links: RemoteNewTabUtils.links.getLinks(),
      enhancedLinks: this.getEnhancedLinks(),
    });
  },

  /**
   * Captures the site's thumbnail in the background, then attemps to show the thumbnail.
   *
   * @param {Object} message
   *        A RemotePageManager message with the following data:
   *
   *        link (Object):
   *          A link object that contains:
   *
   *          baseDomain (String)
   *          blockState (Boolean)
   *          frecency (Integer)
   *          lastVisiteDate (Integer)
   *          pinState (Boolean)
   *          title (String)
   *          type (String)
   *          url (String)
   */
  captureBackgroundPageThumb: Task.async(function* (message) {
    try {
      yield BackgroundPageThumbs.captureIfMissing(message.data.link.url);
      this.createPageThumb(message);
    } catch (err) {
      Cu.reportError("error: " + err);
    }
  }),

  /**
   * Creates the thumbnail to display for each site based on the unique URL
   * of the site and it's type (regular or enhanced). If the thumbnail is of
   * type "regular", we create a blob and send that down to the child. If the
   * thumbnail is of type "enhanced", get the file path for the URL and create
   * and enhanced URI that will be sent down to the child.
   *
   * @param {Object} message
   *        A RemotePageManager message with the following data:
   *
   *        link (Object):
   *          A link object that contains:
   *
   *          baseDomain (String)
   *          blockState (Boolean)
   *          frecency (Integer)
   *          lastVisiteDate (Integer)
   *          pinState (Boolean)
   *          title (String)
   *          type (String)
   *          url (String)
   */
  createPageThumb: function(message) {
    let imgSrc = PageThumbs.getThumbnailURL(message.data.link.url);
    let doc = Services.appShell.hiddenDOMWindow.document;
    let img = doc.createElementNS(XHTML_NAMESPACE, "img");
    let canvas = doc.createElementNS(XHTML_NAMESPACE, "canvas");
    let enhanced = Services.prefs.getBoolPref("browser.newtabpage.enhanced");

    img.onload = function(e) { // jshint ignore:line
      canvas.width = img.naturalWidth;
      canvas.height = img.naturalHeight;
      var ctx = canvas.getContext("2d");
      ctx.drawImage(this, 0, 0, this.naturalWidth, this.naturalHeight);
      canvas.toBlob(function(blob) {
        let host = new URL(message.data.link.url).host;
        RemoteAboutNewTab.pageListener.sendAsyncMessage("NewTab:RegularThumbnailURI", {
          thumbPath: "/pagethumbs/" + host,
          enhanced,
          url: message.data.link.url,
          blob,
        });
      });
    };
    img.src = imgSrc;
  },

  /**
   * Get the set of enhanced links (if any) from the Directory Links Provider.
   */
  getEnhancedLinks: function() {
    let enhancedLinks = [];
    for (let link of RemoteNewTabUtils.links.getLinks()) {
      if (link) {
        enhancedLinks.push(DirectoryLinksProvider.getEnhancedLink(link));
      }
    }
    return enhancedLinks;
  },

  /**
   * Listens for a preference change or session purge for all pages and sends
   * a message to update the pages that are open. If a session purge occured,
   * also clear the links cache and update the set of links to display, as they
   * may have changed, then proceed with the page update.
   */
  observe: function(aSubject, aTopic, aData) { // jshint ignore:line
    let extraData;
    let refreshPage = false;
    if (aTopic === "browser:purge-session-history") {
      RemoteNewTabUtils.links.resetCache();
      RemoteNewTabUtils.links.populateCache(() => {
        this.pageListener.sendAsyncMessage("NewTab:UpdateLinks", {
          links: RemoteNewTabUtils.links.getLinks(),
          enhancedLinks: this.getEnhancedLinks(),
        });
      });
    }

    if (extraData !== undefined || aTopic === "page-thumbnail:create") {
      if (aTopic !== "page-thumbnail:create") {
        // Change the topic for enhanced and enabled observers.
        aTopic = aData;
      }
      this.pageListener.sendAsyncMessage("NewTab:Observe", {topic: aTopic, data: extraData});
    }
  },

  /**
   * Add all observers that about:newtab page must listen for.
   */
  _addObservers: function() {
    Services.obs.addObserver(this, "page-thumbnail:create", true);
    Services.obs.addObserver(this, "browser:purge-session-history", true);
  },

  /**
   * Remove all observers on the page.
   */
  _removeObservers: function() {
    Services.obs.removeObserver(this, "page-thumbnail:create");
    Services.obs.removeObserver(this, "browser:purge-session-history");
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  uninit: function() {
    this._removeObservers();
    this.pageListener.destroy();
    this.pageListener = null;
  },
};
