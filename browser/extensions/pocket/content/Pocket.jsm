  /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Pocket"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");

var Pocket = {
  get site() { return Services.prefs.getCharPref("extensions.pocket.site"); },
  get listURL() { return "https://" + Pocket.site + "/?src=ff_ext"; },

  openList(event) {
    let win = event.view;
    let where = win.whereToOpenLink(event);
    // Never override the current tab unless it's blank:
    if (where == "current" && !win.isTabEmpty(win.gBrowser.selectedTab)) {
      where = "tab";
    }
    win.openTrustedLinkIn(this.listURL, where);
  },

  /**
   * Functions related to the Pocket panel UI.
   */
  onShownInPhotonPageActionPanel(panel, iframe) {
    let window = panel.ownerGlobal;
    window.pktUI.setPhotonPageActionPanelFrame(iframe);
    Pocket._initPanelView(window);
  },

  onPanelViewShowing(event) {
    Pocket._initPanelView(event.target.ownerGlobal);
  },

  _initPanelView(window) {
    let document = window.document;
    let iframe = window.pktUI.getPanelFrame();

    let libraryButton = document.getElementById("library-button");
    if (libraryButton) {
      BrowserUtils.setToolbarButtonHeightProperty(libraryButton);
    }

    let urlToSave = Pocket._urlToSave;
    let titleToSave = Pocket._titleToSave;
    Pocket._urlToSave = null;
    Pocket._titleToSave = null;
    // ViewShowing fires immediately before it creates the contents,
    // in lieu of an AfterViewShowing event, just spin the event loop.
    window.setTimeout(function() {
      if (urlToSave) {
        window.pktUI.tryToSaveUrl(urlToSave, titleToSave);
      } else {
        window.pktUI.tryToSaveCurrentPage();
      }

      // pocketPanelDidHide in main.js set iframe to about:blank when it was
      // hidden, make sure we're loading the save panel.
      if (iframe.contentDocument &&
          iframe.contentDocument.readyState == "complete" &&
          iframe.contentDocument.documentURI != "about:blank") {
        window.pktUI.pocketPanelDidShow();
      } else {
        // iframe didn't load yet. This seems to always be the case when in
        // the toolbar panel, but never the case for a subview.
        // XXX this only being fired when it's a _capturing_ listener!
        iframe.addEventListener("load", Pocket.onFrameLoaded, true);
      }
    }, 0);
  },

  onFrameLoaded(event) {
    let document = event.currentTarget.ownerDocument;
    let window = document.defaultView;
    let iframe = window.pktUI.getPanelFrame();

    iframe.removeEventListener("load", Pocket.onFrameLoaded, true);
    window.pktUI.pocketPanelDidShow();
  },

  onPanelViewHiding(event) {
    let window = event.target.ownerGlobal;
    window.pktUI.pocketPanelDidHide(event);
  },

  _urlToSave: null,
  _titleToSave: null,
  savePage(browser, url, title) {
    if (this.pageAction) {
      this._urlToSave = url;
      this._titleToSave = title;
      this.pageAction.doCommand(browser.ownerGlobal);
    }
  },

  get pageAction() {
    return this._pageAction;
  },
  set pageAction(pageAction) {
    return this._pageAction = pageAction;
  },
  _pageAction: null,
};
