// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ "ReaderParent" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");

const gStringBundle = Services.strings.createBundle("chrome://browser/locale/readerMode.properties");

let ReaderParent = {

  MESSAGES: [
    "Reader:AddToList",
    "Reader:ArticleGet",
    "Reader:FaviconRequest",
    "Reader:ListStatusRequest",
    "Reader:RemoveFromList",
    "Reader:Share",
    "Reader:SystemUIVisibility",
    "Reader:UpdateReaderButton",
    "Reader:SetIntPref",
    "Reader:SetCharPref",
  ],

  init: function() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Reader:AddToList":
        // XXX: To implement.
        break;

      case "Reader:ArticleGet":
        this._getArticle(message.data.url, message.target).then((article) => {
          // Make sure the target browser is still alive before trying to send data back.
          if (message.target.messageManager) {
            message.target.messageManager.sendAsyncMessage("Reader:ArticleData", { article: article });
          }
        });
        break;

      case "Reader:FaviconRequest": {
        // XXX: To implement.
        break;
      }
      case "Reader:ListStatusRequest":
        // XXX: To implement.
        break;

      case "Reader:RemoveFromList":
        // XXX: To implement.
        break;

      case "Reader:Share":
        // XXX: To implement.
        break;

      case "Reader:SystemUIVisibility":
        // XXX: To implement.
        break;

      case "Reader:UpdateReaderButton": {
        let browser = message.target;
        if (message.data && message.data.isArticle !== undefined) {
          browser.isArticle = message.data.isArticle;
        }
        this.updateReaderButton(browser);
        break;
      }
      case "Reader:SetIntPref": {
        if (message.data && message.data.name !== undefined) {
          Services.prefs.setIntPref(message.data.name, message.data.value);
        }
        break;
      }
      case "Reader:SetCharPref": {
        if (message.data && message.data.name !== undefined) {
          Services.prefs.setCharPref(message.data.name, message.data.value);
        }
        break;
      }
    }
  },

  updateReaderButton: function(browser) {
    let win = browser.ownerDocument.defaultView;
    if (browser != win.gBrowser.selectedBrowser) {
      return;
    }

    let button = win.document.getElementById("reader-mode-button");
    if (browser.currentURI.spec.startsWith("about:reader")) {
      button.setAttribute("readeractive", true);
      button.hidden = false;
      button.setAttribute("tooltiptext", gStringBundle.GetStringFromName("readerView.exit"));
    } else {
      button.removeAttribute("readeractive");
      button.setAttribute("tooltiptext", gStringBundle.GetStringFromName("readerView.enter"));
      button.hidden = !browser.isArticle;
    }
  },

  handleReaderButtonEvent: function(event) {
    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != Ci.nsIDOMKeyEvent.DOM_VK_SPACE &&
         event.keyCode != Ci.nsIDOMKeyEvent.DOM_VK_RETURN)) {
      return; // Left click, space or enter only
    }

    let win = event.target.ownerDocument.defaultView;
    let url = win.gBrowser.selectedBrowser.currentURI.spec;

    if (url.startsWith("about:reader")) {
      let originalURL = this._getOriginalUrl(url);
      if (!originalURL) {
        Cu.reportError("Error finding original URL for about:reader URL: " + url);
      } else {
        win.openUILinkIn(originalURL, "current");
      }
    } else {
      win.openUILinkIn("about:reader?url=" + encodeURIComponent(url), "current");
    }
  },

  parseReaderUrl: function(url) {
    if (!url.startsWith("about:reader?")) {
      return null;
    }
    return this._getOriginalUrl(url);
  },

  /**
   * Returns original URL from an about:reader URL.
   *
   * @param url An about:reader URL.
   * @return The original URL for the article, or null if we did not find
   *         a properly formatted about:reader URL.
   */
  _getOriginalUrl: function(url) {
    let searchParams = new URLSearchParams(url.substring("about:reader?".length));
    if (!searchParams.has("url")) {
      return null;
    }
    return decodeURIComponent(searchParams.get("url"));
  },

  /**
   * Gets an article for a given URL. This method will download and parse a document
   * if it does not find the article in the tab data or the cache.
   *
   * @param url The article URL.
   * @param browser The browser where the article is currently loaded.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  _getArticle: Task.async(function* (url, browser) {
    // First, look for a saved article.
    let article = yield this._getSavedArticle(browser);
    if (article && article.url == url) {
      return article;
    }

    // Next, try to find a parsed article in the cache.
    let uri = Services.io.newURI(url, null, null);
    article = yield ReaderMode.getArticleFromCache(uri);
    if (article) {
      return article;
    }

    // Article hasn't been found in the cache, we need to
    // download the page and parse the article out of it.
    return yield ReaderMode.downloadAndParseDocument(url);
  }),

  _getSavedArticle: function(browser) {
    return new Promise((resolve, reject) => {
      let mm = browser.messageManager;
      let listener = (message) => {
        mm.removeMessageListener("Reader:SavedArticleData", listener);
        resolve(message.data.article);
      };
      mm.addMessageListener("Reader:SavedArticleData", listener);
      mm.sendAsyncMessage("Reader:SavedArticleGet");
    });
  }
};
