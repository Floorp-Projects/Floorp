/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Assert } = ChromeUtils.import("resource://testing-common/Assert.jsm");
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  CustomizableUI: "resource:///modules/CustomizableUI.jsm",
});

var EXPORTED_SYMBOLS = ["AppUiTestDelegate", "AppUiTestInternals"];

async function promiseAnimationFrame(window) {
  await new Promise(resolve => window.requestAnimationFrame(resolve));

  let { tm } = Services;
  return new Promise(resolve => tm.dispatchToMainThread(resolve));
}

function makeWidgetId(id) {
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

async function getPageActionButtonId(window, extensionId) {
  // This would normally be set automatically on navigation, and cleared
  // when the user types a value into the URL bar, to show and hide page
  // identity info and icons such as page action buttons.
  //
  // Unfortunately, that doesn't happen automatically in browser chrome
  // tests.
  window.gURLBar.setPageProxyState("valid");

  let { gIdentityHandler } = window.gBrowser.ownerGlobal;
  // If the current tab is blank and the previously selected tab was an internal
  // page, the urlbar will now be showing the internal identity box due to the
  // setPageProxyState call above.  The page action button is hidden in that
  // case, so make sure we're not showing the internal identity box.
  gIdentityHandler._identityBox.classList.remove("chromeUI");

  await promiseAnimationFrame(window);

  return window.BrowserPageActions.urlbarButtonNodeIDForActionID(
    makeWidgetId(extensionId)
  );
}

async function getPageActionButton(window, extensionId) {
  let pageActionId = await getPageActionButtonId(window, extensionId);
  return window.document.getElementById(pageActionId);
}

async function clickPageAction(window, extensionId, modifiers = {}) {
  let pageActionId = await getPageActionButtonId(window, extensionId);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    `#${pageActionId}`,
    modifiers,
    window.browsingContext
  );
  return new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
}

function getBrowserActionWidgetId(extensionId) {
  return makeWidgetId(extensionId) + "-browser-action";
}

function getBrowserActionWidget(extensionId) {
  return lazy.CustomizableUI.getWidget(getBrowserActionWidgetId(extensionId));
}

async function showBrowserAction(window, extensionId) {
  let group = getBrowserActionWidget(extensionId);
  let widget = group.forWindow(window);
  if (!widget.node) {
    return;
  }

  let navbar = window.document.getElementById("nav-bar");
  if (group.areaType == lazy.CustomizableUI.TYPE_TOOLBAR) {
    Assert.equal(
      widget.overflowed,
      navbar.hasAttribute("overflowing"),
      "Expect widget overflow state to match toolbar"
    );
  } else if (group.areaType == lazy.CustomizableUI.TYPE_MENU_PANEL) {
    await navbar.overflowable.show();
  }
}

async function clickBrowserAction(window, extensionId, modifiers) {
  await promiseAnimationFrame(window);
  await showBrowserAction(window, extensionId);

  if (modifiers) {
    let widgetId = getBrowserActionWidgetId(extensionId);
    BrowserTestUtils.synthesizeMouseAtCenter(
      `#${widgetId}`,
      modifiers,
      window.browsingContext
    );
  } else {
    let widget = getBrowserActionWidget(extensionId).forWindow(window);
    widget.node.click();
  }
}

async function promisePopupShown(popup) {
  return new Promise(resolve => {
    if (popup.state == "open") {
      resolve();
    } else {
      let onPopupShown = event => {
        popup.removeEventListener("popupshown", onPopupShown);
        resolve();
      };
      popup.addEventListener("popupshown", onPopupShown);
    }
  });
}

function awaitBrowserLoaded(browser) {
  if (
    browser.ownerGlobal.document.readyState === "complete" &&
    browser.currentURI.spec !== "about:blank"
  ) {
    return Promise.resolve();
  }
  return new Promise(resolve => {
    const listener = ev => {
      if (browser.currentURI.spec === "about:blank") {
        return;
      }
      browser.removeEventListener("AppTestDelegate:load", listener);
      resolve();
    };
    browser.addEventListener("AppTestDelegate:load", listener);
  });
}

function getPanelForNode(node) {
  return node.closest("panel");
}

async function awaitExtensionPanel(window, extensionId, awaitLoad = true) {
  let { originalTarget: browser } = await BrowserTestUtils.waitForEvent(
    window.document,
    "WebExtPopupLoaded",
    true,
    event => event.detail.extension.id === extensionId
  );

  await Promise.all([
    promisePopupShown(getPanelForNode(browser)),

    awaitLoad && awaitBrowserLoaded(browser),
  ]);

  return browser;
}

function closeBrowserAction(window, extensionId) {
  let group = getBrowserActionWidget(extensionId);

  let node = window.document.getElementById(group.viewId);
  lazy.CustomizableUI.hidePanelForNode(node);

  return Promise.resolve();
}

function getPageActionPopup(window, extensionId) {
  let panelId = makeWidgetId(extensionId) + "-panel";
  return window.document.getElementById(panelId);
}

function closePageAction(window, extensionId) {
  let node = getPageActionPopup(window, extensionId);
  if (node) {
    return promisePopupShown(node).then(() => {
      node.hidePopup();
    });
  }

  return Promise.resolve();
}

function openNewForegroundTab(window, url, waitForLoad = true) {
  return BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    url,
    waitForLoad
  );
}

async function removeTab(tab) {
  BrowserTestUtils.removeTab(tab);
}

// These metods are exported so that they can be used in head.js but are
// *not* part of the AppUiTestDelegate API.
var AppUiTestInternals = {
  awaitBrowserLoaded,
  getBrowserActionWidget,
  getPageActionButton,
  getPageActionPopup,
  getPanelForNode,
  makeWidgetId,
  promiseAnimationFrame,
  promisePopupShown,
  showBrowserAction,
};

// These methods are part of the TestDelegate API and need to be compatible
// with the `mobile` AppUiTestDelegate counterpart.
var AppUiTestDelegate = {
  awaitExtensionPanel,
  clickBrowserAction,
  clickPageAction,
  closeBrowserAction,
  closePageAction,
  openNewForegroundTab,
  removeTab,
};
