/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Via webext-panels.xul
/* import-globals-from browser.js */
/* import-globals-from nsContextMenu.js */

ChromeUtils.defineModuleGetter(this, "ExtensionParent",
                               "resource://gre/modules/ExtensionParent.jsm");

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  promiseEvent,
} = ExtensionUtils;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function getBrowser(sidebar) {
  let browser = document.getElementById("webext-panels-browser");
  if (browser) {
    return Promise.resolve(browser);
  }

  let stack = document.createElementNS(XUL_NS, "stack");
  stack.setAttribute("flex", "1");

  browser = document.createElementNS(XUL_NS, "browser");
  browser.setAttribute("id", "webext-panels-browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("flex", "1");
  browser.setAttribute("disableglobalhistory", "true");
  browser.setAttribute("webextension-view-type", "sidebar");
  browser.setAttribute("context", "contentAreaContextMenu");
  browser.setAttribute("tooltip", "aHTMLTooltip");
  browser.setAttribute("autocompletepopup", "PopupAutoComplete");
  browser.setAttribute("selectmenulist", "ContentSelectDropdown");
  browser.setAttribute("onclick", "window.parent.contentAreaClick(event, true);");

  // Ensure that the browser is going to run in the same process of the other
  // extension pages from the same addon.
  browser.sameProcessAsFrameLoader = sidebar.extension.groupFrameLoader;

  let readyPromise;
  if (sidebar.extension.remote) {
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

  stack.appendChild(browser);
  document.documentElement.appendChild(stack);

  return readyPromise.then(() => {
    browser.messageManager.loadFrameScript("chrome://browser/content/content.js", false);
    ExtensionParent.apiManager.emit("extension-browser-inserted", browser);

    if (sidebar.browserStyle) {
      browser.messageManager.loadFrameScript(
        "chrome://extensions/content/ext-browser-content.js", false);

      browser.messageManager.sendAsyncMessage("Extension:InitBrowser", {
        stylesheets: ExtensionParent.extensionStylesheets,
      });
    }
    return browser;
  });
}

// Stub tabbrowser implementation for use by the tab-modal alert code.
var gBrowser = {
  getTabForBrowser(browser) {
    return null;
  },

  getTabModalPromptBox(browser) {
    if (!browser.tabModalPromptBox) {
      browser.tabModalPromptBox = new TabModalPromptBox(browser);
    }
    return browser.tabModalPromptBox;
  },
};

function updatePosition() {
  // We need both of these to make sure we update the position
  // after any lower level updates have finished.
  requestAnimationFrame(() => setTimeout(() => {
    let browser = document.getElementById("webext-panels-browser");
    if (browser && browser.isRemoteBrowser) {
      browser.frameLoader.requestUpdatePosition();
    }
  }, 0));
}

function loadPanel(extensionId, extensionUrl, browserStyle) {
  let browserEl = document.getElementById("webext-panels-browser");
  if (browserEl) {
    if (browserEl.currentURI.spec === extensionUrl) {
      return;
    }
    // Forces runtime disconnect.  Remove the stack (parent).
    browserEl.parentNode.remove();
  }

  let policy = WebExtensionPolicy.getByID(extensionId);
  let sidebar = {
    uri: extensionUrl,
    extension: policy.extension,
    browserStyle,
  };
  getBrowser(sidebar).then(browser => {
    let uri = Services.io.newURI(policy.getURL());
    let triggeringPrincipal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
    browser.loadURI(extensionUrl, {triggeringPrincipal});
  });
}
