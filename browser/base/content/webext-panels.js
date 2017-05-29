/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Via webext-panels.xul
/* import-globals-from browser.js */
/* import-globals-from nsContextMenu.js */

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionParent",
                                  "resource://gre/modules/ExtensionParent.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  extensionStylesheets,
  promiseEvent,
} = ExtensionUtils;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function getBrowser(sidebar) {
  let browser = document.getElementById("webext-panels-browser");
  if (browser) {
    return Promise.resolve(browser);
  }

  browser = document.createElementNS(XUL_NS, "browser");
  browser.setAttribute("id", "webext-panels-browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("flex", "1");
  browser.setAttribute("disableglobalhistory", "true");
  browser.setAttribute("webextension-view-type", "sidebar");
  browser.setAttribute("context", "contentAreaContextMenu");
  browser.setAttribute("tooltip", "aHTMLTooltip");
  browser.setAttribute("onclick", "window.parent.contentAreaClick(event, true);");

  let readyPromise;
  if (sidebar.remote) {
    browser.setAttribute("remote", "true");
    browser.setAttribute("remoteType",
                         E10SUtils.getRemoteTypeForURI(sidebar.uri, true,
                                                       E10SUtils.EXTENSION_REMOTE_TYPE));
    readyPromise = promiseEvent(browser, "XULFrameLoaderCreated");

    window.messageManager.addMessageListener("contextmenu", openContextMenu);
    window.addEventListener("unload", () => {
      window.messageManager.removeMessageListener("contextmenu", openContextMenu);
    }, {once: true});
  } else {
    readyPromise = Promise.resolve();
  }
  document.documentElement.appendChild(browser);

  return readyPromise.then(() => {
    browser.messageManager.loadFrameScript("chrome://browser/content/content.js", false);
    ExtensionParent.apiManager.emit("extension-browser-inserted", browser);

    if (sidebar.browserStyle) {
      browser.messageManager.loadFrameScript(
        "chrome://extensions/content/ext-browser-content.js", false);

      browser.messageManager.sendAsyncMessage("Extension:InitBrowser", {
        stylesheets: extensionStylesheets,
      });
    }
    return browser;
  });
}

function loadWebPanel() {
  let sidebarURI = new URL(location);
  let sidebar = {
    uri: sidebarURI.searchParams.get("panel"),
    remote: sidebarURI.searchParams.get("remote"),
    browserStyle: sidebarURI.searchParams.get("browser-style"),
  };
  getBrowser(sidebar).then(browser => {
    browser.loadURI(sidebar.uri);
  });
}

function load() {
  this.loadWebPanel();
}
