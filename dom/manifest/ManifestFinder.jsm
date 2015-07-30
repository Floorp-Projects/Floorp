/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* globals Components, Task, PromiseMessage */
"use strict";
const {
  utils: Cu
} = Components;
Cu.import("resource://gre/modules/PromiseMessage.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.ManifestFinder = {// jshint ignore:line
  /**
  * Check from content process if DOM Window has a conforming
  * manifest link relationship.
  * @param aContent DOM Window to check.
  * @return {Promise<Boolean>}
  */
  contentHasManifestLink(aContent) {
    if (!aContent || isXULBrowser(aContent)) {
      throw new TypeError("Invalid input.");
    }
    return checkForManifest(aContent);
  },

  /**
  * Check from a XUL browser (parent process) if it's content document has a
  * manifest link relationship.
  * @param aBrowser The XUL browser to check.
  * @return {Promise}
  */
  browserHasManifestLink: Task.async(
    function* (aBrowser) {
      if (!isXULBrowser(aBrowser)) {
        throw new TypeError("Invalid input.");
      }
      const msgKey = "DOM:WebManifest:hasManifestLink";
      const mm = aBrowser.messageManager;
      const reply = yield PromiseMessage.send(mm, msgKey);
      return reply.data.result;
    }
  )
};

function isXULBrowser(aBrowser) {
  if (!aBrowser || !aBrowser.namespaceURI || !aBrowser.localName) {
    return false;
  }
  const XUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  return (aBrowser.namespaceURI === XUL && aBrowser.localName === "browser");
}

function checkForManifest(aWindow) {
  // Only top-level browsing contexts are valid.
  if (!aWindow || aWindow.top !== aWindow) {
    return false;
  }
  const elem = aWindow.document.querySelector("link[rel~='manifest']");
  // Only if we have an element and a non-empty href attribute.
  if (!elem || !elem.getAttribute("href")) {
    return false;
  }
  return true;
}

this.EXPORTED_SYMBOLS = [// jshint ignore:line
  "ManifestFinder"
];
