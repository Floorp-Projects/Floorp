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
    "Reader:UpdateIsArticle",
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
          message.target.messageManager.sendAsyncMessage("Reader:ArticleData", { article: article });
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

      case "Reader:UpdateIsArticle": {
        // XXX: To implement.
        break;
      }
    }
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
