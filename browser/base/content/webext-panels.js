/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionParent",
                                  "resource://gre/modules/ExtensionParent.jsm");

function loadWebPanel() {
  let sidebarURI = new URL(location);
  let uri = sidebarURI.searchParams.get("panel");
  let remote = sidebarURI.searchParams.get("remote");
  let browser = document.getElementById("webext-panels-browser");
  if (remote) {
    let remoteType = E10SUtils.getRemoteTypeForURI(uri, true,
                                                   E10SUtils.EXTENSION_REMOTE_TYPE);
    browser.setAttribute("remote", "true");
    browser.setAttribute("remoteType", remoteType);
  } else {
    browser.removeAttribute("remote");
    browser.removeAttribute("remoteType");
  }
  browser.loadURI(uri);
}

function load() {
  let browser = document.getElementById("webext-panels-browser");
  browser.messageManager.loadFrameScript("chrome://browser/content/content.js", true);
  ExtensionParent.apiManager.emit("extension-browser-inserted", browser);

  this.loadWebPanel();
}
