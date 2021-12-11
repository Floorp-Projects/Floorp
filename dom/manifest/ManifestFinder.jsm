/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
"use strict";

var ManifestFinder = {
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
  async browserHasManifestLink(aBrowser) {
    if (!isXULBrowser(aBrowser)) {
      throw new TypeError("Invalid input.");
    }

    const actor = aBrowser.browsingContext.currentWindowGlobal.getActor(
      "ManifestMessages"
    );
    const reply = await actor.sendQuery("DOM:WebManifest:hasManifestLink");
    return reply.result;
  },
};

function isXULBrowser(aBrowser) {
  if (!aBrowser || !aBrowser.namespaceURI || !aBrowser.localName) {
    return false;
  }
  const XUL_NS =
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  return aBrowser.namespaceURI === XUL_NS && aBrowser.localName === "browser";
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

var EXPORTED_SYMBOLS = ["ManifestFinder"];
