// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ "ReaderParent" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils", "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITour", "resource:///modules/UITour.jsm");

const gStringBundle = Services.strings.createBundle("chrome://global/locale/aboutReader.properties");

var ReaderParent = {
  // Listeners are added in nsBrowserGlue.js
  receiveMessage(message) {
    switch (message.name) {
      case "Reader:FaviconRequest": {
        if (message.target.messageManager) {
          let faviconUrl = PlacesUtils.promiseFaviconLinkUrl(message.data.url);
          faviconUrl.then(function onResolution(favicon) {
            message.target.messageManager.sendAsyncMessage("Reader:FaviconReturn", {
              url: message.data.url,
              faviconUrl: favicon.path.replace(/^favicon:/, "")
            });
          },
          function onRejection(reason) {
            Cu.reportError("Error requesting favicon URL for about:reader content: " + reason);
          }).catch(Cu.reportError);
        }
        break;
      }

      case "Reader:UpdateReaderButton": {
        let browser = message.target;
        if (message.data && message.data.isArticle !== undefined) {
          browser.isArticle = message.data.isArticle;
        }
        this.updateReaderButton(browser);
        break;
      }
    }
  },

  updateReaderButton(browser) {
    let win = browser.ownerGlobal;
    if (browser != win.gBrowser.selectedBrowser) {
      return;
    }

    let button = win.document.getElementById("reader-mode-button");
    let command = win.document.getElementById("View:ReaderView");
    let key = win.document.getElementById("key_toggleReaderMode");
    if (browser.currentURI.spec.startsWith("about:reader")) {
      button.setAttribute("readeractive", true);
      button.hidden = false;
      let closeText = gStringBundle.GetStringFromName("readerView.close");
      button.setAttribute("tooltiptext", closeText);
      command.setAttribute("label", closeText);
      command.setAttribute("hidden", false);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.close.accesskey"));
      key.setAttribute("disabled", false);
    } else {
      button.removeAttribute("readeractive");
      button.hidden = !browser.isArticle;
      let enterText = gStringBundle.GetStringFromName("readerView.enter");
      button.setAttribute("tooltiptext", enterText);
      command.setAttribute("label", enterText);
      command.setAttribute("hidden", !browser.isArticle);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.enter.accesskey"));
      key.setAttribute("disabled", !browser.isArticle);
    }
  },

  forceShowReaderIcon(browser) {
    browser.isArticle = true;
    this.updateReaderButton(browser);
  },

  buttonClick(event) {
    if (event.button != 0) {
      return;
    }
    this.toggleReaderMode(event);
  },

  toggleReaderMode(event) {
    let win = event.target.ownerGlobal;
    let browser = win.gBrowser.selectedBrowser;
    browser.messageManager.sendAsyncMessage("Reader:ToggleReaderMode");
  },

  /**
   * Gets an article for a given URL. This method will download and parse a document.
   *
   * @param url The article URL.
   * @param browser The browser where the article is currently loaded.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  async _getArticle(url, browser) {
    return ReaderMode.downloadAndParseDocument(url).catch(e => {
      if (e && e.newURL) {
        // Pass up the error so we can navigate the browser in question to the new URL:
        throw e;
      }
      Cu.reportError("Error downloading and parsing document: " + e);
      return null;
    });
  }
};
