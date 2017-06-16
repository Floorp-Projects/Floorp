/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");

/**
 * Singleton object that represents the JSON View in-content tool.
 * It has the same lifetime as the browser. Initialization done by
 * DevTools() object from devtools/client/framework/devtools.js
 */
var JsonView = {
  initialize: function () {
    // Load JSON converter module. This converter is responsible
    // for handling 'application/json' documents and converting
    // them into a simple web-app that allows easy inspection
    // of the JSON data.
    Services.ppmm.loadProcessScript(
      "resource://devtools/client/jsonview/converter-observer.js",
      true);

    // Register for messages coming from the child process.
    Services.ppmm.addMessageListener(
      "devtools:jsonview:save", this.onSave);
  },

  destroy: function () {
    Services.ppmm.removeMessageListener(
      "devtools:jsonview:save", this.onSave);
  },

  // Message handlers for events from child processes

  /**
   * Save JSON to a file needs to be implemented here
   * in the parent process.
   */
  onSave: function (message) {
    let chrome = Services.wm.getMostRecentWindow("navigator:browser");
    let browser = chrome.gBrowser.selectedBrowser;
    if (message.data.url === null) {
      // Save original contents
      chrome.saveBrowser(browser, false, message.data.windowID);
    } else {
      // The following code emulates saveBrowser, but:
      // - Uses the given blob URL containing the custom contents to save.
      // - Obtains the file name from the URL of the document, not the blob.
      let persistable = browser.QueryInterface(Ci.nsIFrameLoaderOwner)
        .frameLoader.QueryInterface(Ci.nsIWebBrowserPersistable);
      persistable.startPersistence(message.data.windowID, {
        onDocumentReady(doc) {
          let uri = chrome.makeURI(doc.documentURI, doc.characterSet);
          let filename = chrome.getDefaultFileName(undefined, uri, doc, null);
          chrome.internalSave(message.data.url, doc, filename, null, doc.contentType,
            false, null, null, null, doc, false, null, undefined);
        },
        onError(status) {
          throw new Error("JSON Viewer's onSave failed in startPersistence");
        }
      });
    }
  }
};

// Exports from this module
module.exports.JsonView = JsonView;
