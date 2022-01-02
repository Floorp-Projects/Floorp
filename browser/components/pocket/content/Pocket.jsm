/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Pocket"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUIUtils",
  "resource:///modules/BrowserUIUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);

var Pocket = {
  get site() {
    return Services.prefs.getCharPref("extensions.pocket.site");
  },
  get listURL() {
    return "https://" + Pocket.site + "/firefox_learnmore?src=ff_library";
  },

  _initPanelView(window) {
    let document = window.document;

    let libraryButton = document.getElementById("library-button");
    if (libraryButton) {
      BrowserUIUtils.setToolbarButtonHeightProperty(libraryButton);
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
    }, 0);
  },

  _urlToSave: null,
  _titleToSave: null,
  savePage(browser, url, title) {
    if (!browser?.ownerDocument || !browser?.ownerGlobal?.PanelUI) {
      return;
    }
    let { ownerDocument, ownerGlobal } = browser;

    let widget = CustomizableUI.getWidget("save-to-pocket-button");
    let anchorNode = widget.areaType
      ? widget.forWindow(ownerGlobal).anchor
      : ownerDocument.getElementById("PanelUI-menu-button");

    this._urlToSave = url;
    this._titleToSave = title;
    ownerGlobal.PanelUI.showSubView("PanelUI-savetopocket", anchorNode);
  },
};
