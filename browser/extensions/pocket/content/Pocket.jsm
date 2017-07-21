/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["Pocket"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");

var Pocket = {
  get site() { return Services.prefs.getCharPref("extensions.pocket.site"); },
  get listURL() { return "https://" + Pocket.site + "/?src=ff_ext"; },

  /**
   * Functions related to the Pocket panel UI.
   */
  onPanelViewShowing(event) {
    let document = event.target.ownerDocument;
    let window = document.defaultView;
    let iframe = window.pktUI.getPanelFrame();

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
    let document = browser.ownerDocument;
    let pocketWidget = document.getElementById("pocket-button");
    let placement = CustomizableUI.getPlacementOfWidget("pocket-button");
    if (!placement)
      return;

    this._urlToSave = url;
    this._titleToSave = title;
    if (placement.area == CustomizableUI.AREA_PANEL) {
      let win = document.defaultView;
      win.PanelUI.show().then(function() {
        pocketWidget = document.getElementById("pocket-button");
        pocketWidget.doCommand();
      });
    } else {
      pocketWidget.doCommand();
    }
  },
};
