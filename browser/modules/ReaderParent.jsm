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

let ReaderParent = {

  MESSAGES: [
    "Reader:AddToList",
    "Reader:ArticleGet",
    "Reader:FaviconRequest",
    "Reader:ListStatusRequest",
    "Reader:RemoveFromList",
    "Reader:Share",
    "Reader:ShowToast",
    "Reader:ToolbarVisibility",
    "Reader:SystemUIVisibility",
    "Reader:UpdateReaderButton",
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

      case "Reader:ShowToast":
        // XXX: To implement.
        break;

      case "Reader:SystemUIVisibility":
        // XXX: To implement.
        break;

      case "Reader:ToolbarVisibility":
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
    } else {
      button.removeAttribute("readeractive");
      button.hidden = !browser.isArticle;
    }
  },

  toggleReaderMode: function(event) {
    let win = event.target.ownerDocument.defaultView;
    let url = win.gBrowser.selectedBrowser.currentURI.spec;
    if (url.startsWith("about:reader")) {
      win.openUILinkIn(this._getOriginalUrl(url), "current");
    } else {
      win.openUILinkIn("about:reader?url=" + encodeURIComponent(url), "current");
    }
  },

  /**
   * Returns original URL from an about:reader URL.
   *
   * @param url An about:reader URL.
   */
  _getOriginalUrl: function(url) {
    let searchParams = new URLSearchParams(url.split("?")[1]);
    if (!searchParams.has("url")) {
      Cu.reportError("Error finding original URL for about:reader URL: " + url);
      return url;
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
