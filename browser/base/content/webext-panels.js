/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Via webext-panels.xhtml
/* import-globals-from browser.js */
/* import-globals-from nsContextMenu.js */

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

var { promiseEvent } = ExtensionUtils;

function getBrowser(panel) {
  let browser = document.getElementById("webext-panels-browser");
  if (browser) {
    return Promise.resolve(browser);
  }

  let stack = document.getElementById("webext-panels-stack");
  if (!stack) {
    stack = document.createXULElement("stack");
    stack.setAttribute("flex", "1");
    stack.setAttribute("id", "webext-panels-stack");
    document.documentElement.appendChild(stack);
  }

  browser = document.createXULElement("browser");
  browser.setAttribute("id", "webext-panels-browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("flex", "1");
  browser.setAttribute("disableglobalhistory", "true");
  browser.setAttribute("messagemanagergroup", "webext-browsers");
  browser.setAttribute("webextension-view-type", panel.viewType);
  browser.setAttribute("context", "contentAreaContextMenu");
  browser.setAttribute("tooltip", "aHTMLTooltip");
  browser.setAttribute("autocompletepopup", "PopupAutoComplete");

  // Ensure that the browser is going to run in the same bc group as the other
  // extension pages from the same addon.
  browser.setAttribute(
    "initialBrowsingContextGroupId",
    panel.extension.policy.browsingContextGroupId
  );

  let readyPromise;
  if (panel.extension.remote) {
    browser.setAttribute("remote", "true");
    let oa = E10SUtils.predictOriginAttributes({ browser });
    browser.setAttribute(
      "remoteType",
      E10SUtils.getRemoteTypeForURI(
        panel.uri,
        /* remote */ true,
        /* fission */ false,
        E10SUtils.EXTENSION_REMOTE_TYPE,
        null,
        oa
      )
    );
    browser.setAttribute("maychangeremoteness", "true");

    readyPromise = promiseEvent(browser, "XULFrameLoaderCreated");
  } else {
    readyPromise = Promise.resolve();
  }

  stack.appendChild(browser);

  browser.addEventListener(
    "DoZoomEnlargeBy10",
    () => {
      let { ZoomManager } = browser.ownerGlobal;
      let zoom = browser.fullZoom;
      zoom += 0.1;
      if (zoom > ZoomManager.MAX) {
        zoom = ZoomManager.MAX;
      }
      browser.fullZoom = zoom;
    },
    true
  );
  browser.addEventListener(
    "DoZoomReduceBy10",
    () => {
      let { ZoomManager } = browser.ownerGlobal;
      let zoom = browser.fullZoom;
      zoom -= 0.1;
      if (zoom < ZoomManager.MIN) {
        zoom = ZoomManager.MIN;
      }
      browser.fullZoom = zoom;
    },
    true
  );

  const initBrowser = () => {
    ExtensionParent.apiManager.emit(
      "extension-browser-inserted",
      browser,
      panel.browserInsertedData
    );

    browser.messageManager.loadFrameScript(
      "chrome://extensions/content/ext-browser-content.js",
      false,
      true
    );

    let options =
      panel.browserStyle !== false
        ? { stylesheets: ExtensionParent.extensionStylesheets }
        : {};
    browser.messageManager.sendAsyncMessage("Extension:InitBrowser", options);
    return browser;
  };

  browser.addEventListener("DidChangeBrowserRemoteness", initBrowser);
  return readyPromise.then(initBrowser);
}

// Stub tabbrowser implementation for use by the tab-modal alert code.
var gBrowser = {
  get selectedBrowser() {
    return document.getElementById("webext-panels-browser");
  },

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
  requestAnimationFrame(() =>
    setTimeout(() => {
      let browser = document.getElementById("webext-panels-browser");
      if (browser && browser.isRemoteBrowser) {
        browser.frameLoader.requestUpdatePosition();
      }
    }, 0)
  );
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
    viewType: "sidebar",
  };

  getBrowser(sidebar).then(browser => {
    let uri = Services.io.newURI(policy.getURL());
    let triggeringPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    browser.loadURI(extensionUrl, { triggeringPrincipal });
  });
}
