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

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils","resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReadingList", "resource:///modules/readinglist/ReadingList.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITour", "resource:///modules/UITour.jsm");

const gStringBundle = Services.strings.createBundle("chrome://global/locale/aboutReader.properties");

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
        let article = message.data.article;
        ReadingList.getMetadataFromBrowser(message.target).then(function(metadata) {
          if (metadata.previews.length > 0) {
            article.preview = metadata.previews[0];
          }

          ReadingList.addItem({
            url: article.url,
            title: article.title,
            excerpt: article.excerpt,
            preview: article.preview
          });
        });
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
        if (message.target.messageManager) {
          let faviconUrl = PlacesUtils.promiseFaviconLinkUrl(message.data.url);
          faviconUrl.then(function onResolution(favicon) {
            message.target.messageManager.sendAsyncMessage("Reader:FaviconReturn", {
              url: message.data.url,
              faviconUrl: favicon.path.replace(/^favicon:/, "")
            })
          },
          function onRejection(reason) {
            Cu.reportError("Error requesting favicon URL for about:reader content: " + reason);
          }).catch(Cu.reportError);
        }
        break;
      }
      case "Reader:ListStatusRequest":
        ReadingList.hasItemForURL(message.data.url).then(inList => {
          let mm = message.target.messageManager
          // Make sure the target browser is still alive before trying to send data back.
          if (mm) {
            mm.sendAsyncMessage("Reader:ListStatusData",
                                { inReadingList: inList, url: message.data.url });
          }
        });
        break;

      case "Reader:RemoveFromList":
        // We need to get the "real" item to delete it.
        ReadingList.itemForURL(message.data.url).then(item => {
          ReadingList.deleteItem(item)
        });
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
    let command = win.document.getElementById("View:ReaderView");
    if (browser.currentURI.spec.startsWith("about:reader")) {
      button.setAttribute("readeractive", true);
      button.hidden = false;
      let closeText = gStringBundle.GetStringFromName("readerView.close");
      button.setAttribute("tooltiptext", closeText);
      command.setAttribute("label", closeText);
      command.setAttribute("hidden", false);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.close.accesskey"));
    } else {
      button.removeAttribute("readeractive");
      button.hidden = !browser.isArticle;
      let enterText = gStringBundle.GetStringFromName("readerView.enter");
      button.setAttribute("tooltiptext", enterText);
      command.setAttribute("label", enterText);
      command.setAttribute("hidden", !browser.isArticle);
      command.setAttribute("accesskey", gStringBundle.GetStringFromName("readerView.enter.accesskey"));
    }
  },

  forceShowReaderIcon: function(browser) {
    browser.isArticle = true;
    this.updateReaderButton(browser);
  },

  buttonClick: function(event) {
    if (event.button != 0) {
      return;
    }
    this.toggleReaderMode(event);
  },

  toggleReaderMode: function(event) {
    let win = event.target.ownerDocument.defaultView;
    let browser = win.gBrowser.selectedBrowser;
    let url = browser.currentURI.spec;

    if (url.startsWith("about:reader")) {
      let originalURL = ReaderMode.getOriginalUrl(url);
      if (!originalURL) {
        Cu.reportError("Error finding original URL for about:reader URL: " + url);
      } else {
        win.openUILinkIn(originalURL, "current", {"allowPinnedTabHostChange": true});
      }
    } else {
      browser.messageManager.sendAsyncMessage("Reader:ParseDocument", { url: url });
    }
  },

  /**
   * Shows an info panel from the UITour for Reader Mode.
   *
   * @param browser The <browser> that the tour should be started for.
   */
  showReaderModeInfoPanel(browser) {
    let win = browser.ownerDocument.defaultView;
    let targetPromise = UITour.getTarget(win, "readerMode-urlBar");
    targetPromise.then(target => {
      let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
      UITour.showInfo(win, browser.messageManager, target,
                      browserBundle.GetStringFromName("readerView.promo.firstDetectedArticle.title"),
                      browserBundle.GetStringFromName("readerView.promo.firstDetectedArticle.body"),
                      "chrome://browser/skin/reader-tour.png");
    });
  },

  /**
   * Gets an article for a given URL. This method will download and parse a document.
   *
   * @param url The article URL.
   * @param browser The browser where the article is currently loaded.
   * @return {Promise}
   * @resolves JS object representing the article, or null if no article is found.
   */
  _getArticle: Task.async(function* (url, browser) {
    return yield ReaderMode.downloadAndParseDocument(url).catch(e => {
      Cu.reportError("Error downloading and parsing document: " + e);
      return null;
    });
  })
};
