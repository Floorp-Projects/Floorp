/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported CustomizableUI makeWidgetId focusWindow forceGC
 *          getBrowserActionWidget
 *          clickBrowserAction clickPageAction
 *          getBrowserActionPopup getPageActionPopup
 *          closeBrowserAction closePageAction
 *          promisePopupShown promisePopupHidden
 *          openContextMenu closeContextMenu
 *          openContextMenuInSidebar openContextMenuInPopup
 *          openExtensionContextMenu closeExtensionContextMenu
 *          openActionContextMenu openSubmenu closeActionContextMenu
 *          openTabContextMenu closeTabContextMenu
 *          openToolsMenu closeToolsMenu
 *          imageBuffer imageBufferFromDataURI
 *          getListStyleImage getPanelForNode
 *          awaitExtensionPanel awaitPopupResize
 *          promiseContentDimensions alterContent
 *          promisePrefChangeObserved openContextMenuInFrame
 *          promiseAnimationFrame getCustomizableUIPanelID
 *          awaitEvent BrowserWindowIterator
 *          navigateTab historyPushState promiseWindowRestored
 */

// There are shutdown issues for which multiple rejections are left uncaught.
// This bug should be fixed, but for the moment this directory is whitelisted.
//
// NOTE: Entire directory whitelisting should be kept to a minimum. Normally you
//       should use "expectUncaughtRejection" to flag individual failures.
const {PromiseTestUtils} = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", {});
PromiseTestUtils.whitelistRejectionsGlobally(/Message manager disconnected/);
PromiseTestUtils.whitelistRejectionsGlobally(/No matching message handler/);
PromiseTestUtils.whitelistRejectionsGlobally(/Receiving end does not exist/);

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm", {});
const {CustomizableUI} = ChromeUtils.import("resource:///modules/CustomizableUI.jsm", {});
const {Preferences} = ChromeUtils.import("resource://gre/modules/Preferences.jsm", {});

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const {Management} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

// We run tests under two different configurations, from browser.ini and
// browser-remote.ini. When running from browser-remote.ini, the tests are
// copied to the sub-directory "test-oop-extensions", which we detect here, and
// use to select our configuration.
let remote = gTestPath.includes("test-oop-extensions");
SpecialPowers.pushPrefEnv({set: [
  ["extensions.webextensions.remote", remote],
]});
if (remote) {
  // We don't want to reset this at the end of the test, so that we don't have
  // to spawn a new extension child process for each test unit.
  SpecialPowers.setIntPref("dom.ipc.keepProcessesAlive.extension", 1);
}

// Don't try to create screenshots of sites we load during tests.
Services.prefs.getDefaultBranch("browser.newtabpage.activity-stream.")
        .setBoolPref("feeds.topsites", false);

{
  // Touch the recipeParentPromise lazy getter so we don't get
  // `this._recipeManager is undefined` errors during tests.
  const {LoginManagerParent} = ChromeUtils.import("resource://gre/modules/LoginManagerParent.jsm", null);
  void LoginManagerParent.recipeParentPromise;
}

// Bug 1239884: Our tests occasionally hit a long GC pause at unpredictable
// times in debug builds, which results in intermittent timeouts. Until we have
// a better solution, we force a GC after certain strategic tests, which tend to
// accumulate a high number of unreaped windows.
function forceGC() {
  if (AppConstants.DEBUG) {
    Cu.forceGC();
  }
}

function makeWidgetId(id) {
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

var focusWindow = async function focusWindow(win) {
  if (Services.focus.activeWindow == win) {
    return;
  }

  let promise = new Promise(resolve => {
    win.addEventListener("focus", function() {
      resolve();
    }, {capture: true, once: true});
  });

  win.focus();
  await promise;
};

function imageBufferFromDataURI(encodedImageData) {
  let decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}

let img = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAAAABQABh6FO1AAAAABJRU5ErkJggg==";
var imageBuffer = imageBufferFromDataURI(img);

function getListStyleImage(button) {
  let style = button.ownerGlobal.getComputedStyle(button);

  let match = /^url\("(.*)"\)$/.exec(style.listStyleImage);

  return match && match[1];
}

async function promiseAnimationFrame(win = window) {
  await new Promise(resolve => win.requestAnimationFrame(resolve));

  let {tm} = Services;
  return new Promise(resolve => tm.dispatchToMainThread(resolve));
}

function promisePopupShown(popup) {
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

function promisePopupHidden(popup) {
  return new Promise(resolve => {
    let onPopupHidden = event => {
      popup.removeEventListener("popuphidden", onPopupHidden);
      resolve();
    };
    popup.addEventListener("popuphidden", onPopupHidden);
  });
}

function promisePossiblyInaccurateContentDimensions(browser) {
  return ContentTask.spawn(browser, null, async function() {
    function copyProps(obj, props) {
      let res = {};
      for (let prop of props) {
        res[prop] = obj[prop];
      }
      return res;
    }

    return {
      window: copyProps(
        content,
        ["innerWidth", "innerHeight", "outerWidth", "outerHeight",
         "scrollX", "scrollY", "scrollMaxX", "scrollMaxY"]),
      body: copyProps(
        content.document.body,
        ["clientWidth", "clientHeight", "scrollWidth", "scrollHeight"]),
      root: copyProps(
        content.document.documentElement,
        ["clientWidth", "clientHeight", "scrollWidth", "scrollHeight"]),
      isStandards: content.document.compatMode !== "BackCompat",
    };
  });
}

function delay(ms = 0) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function promiseContentDimensions(browser) {
  // For remote browsers, each resize operation requires an asynchronous
  // round-trip to resize the content window. Since there's a certain amount of
  // unpredictability in the timing, mainly due to the unpredictability of
  // reflows, we need to wait until the content window dimensions match the
  // <browser> dimensions before returning data.

  let dims = await promisePossiblyInaccurateContentDimensions(browser);
  while (browser.clientWidth !== dims.window.innerWidth ||
         browser.clientHeight !== dims.window.innerHeight) {
    await delay(50);
    dims = await promisePossiblyInaccurateContentDimensions(browser);
  }

  return dims;
}

async function awaitPopupResize(browser) {
  await BrowserTestUtils.waitForEvent(browser, "WebExtPopupResized",
                                      event => event.detail === "delayed");

  return promiseContentDimensions(browser);
}

function alterContent(browser, task, arg = null) {
  return Promise.all([
    ContentTask.spawn(browser, arg, task),
    awaitPopupResize(browser),
  ]).then(([, dims]) => dims);
}

function getPanelForNode(node) {
  while (node.localName != "panel") {
    node = node.parentNode;
  }
  return node;
}

var awaitBrowserLoaded = browser => ContentTask.spawn(browser, null, () => {
  if (content.document.readyState !== "complete" ||
      content.document.documentURI === "about:blank") {
    return ContentTaskUtils.waitForEvent(this, "load", true, event => {
      return content.document.documentURI !== "about:blank";
    }).then(() => {});
  }
});

var awaitExtensionPanel = async function(extension, win = window, awaitLoad = true) {
  let {originalTarget: browser} = await BrowserTestUtils.waitForEvent(
    win.document, "WebExtPopupLoaded", true,
    event => event.detail.extension.id === extension.id);

  await Promise.all([
    promisePopupShown(getPanelForNode(browser)),

    awaitLoad && awaitBrowserLoaded(browser),
  ]);

  return browser;
};

function getCustomizableUIPanelID() {
  return CustomizableUI.AREA_FIXED_OVERFLOW_PANEL;
}

function getBrowserActionWidget(extension) {
  return CustomizableUI.getWidget(makeWidgetId(extension.id) + "-browser-action");
}

function getBrowserActionPopup(extension, win = window) {
  let group = getBrowserActionWidget(extension);

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    return win.document.getElementById("customizationui-widget-panel");
  }
  return win.PanelUI.overflowPanel;
}

var showBrowserAction = async function(extension, win = window) {
  let group = getBrowserActionWidget(extension);
  let widget = group.forWindow(win);

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    ok(!widget.overflowed, "Expect widget not to be overflowed");
  } else if (group.areaType == CustomizableUI.TYPE_MENU_PANEL) {
    await win.document.getElementById("nav-bar").overflowable.show();
  }
};

var clickBrowserAction = async function(extension, win = window) {
  await promiseAnimationFrame(win);
  await showBrowserAction(extension, win);

  let widget = getBrowserActionWidget(extension).forWindow(win);
  widget.node.click();
};

function closeBrowserAction(extension, win = window) {
  let group = getBrowserActionWidget(extension);

  let node = win.document.getElementById(group.viewId);
  CustomizableUI.hidePanelForNode(node);

  return Promise.resolve();
}

async function openContextMenuInPopup(extension, selector = "body") {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let browser = await awaitExtensionPanel(extension);
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "mousedown", button: 2}, browser);
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "contextmenu"}, browser);
  await popupShownPromise;
  return contentAreaContextMenu;
}

async function openContextMenuInSidebar(selector = "body") {
  let contentAreaContextMenu = SidebarUI.browser.contentDocument.getElementById("contentAreaContextMenu");
  let browser = SidebarUI.browser.contentDocument.getElementById("webext-panels-browser");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "mousedown", button: 2}, browser);
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "contextmenu"}, browser);
  await popupShownPromise;
  return contentAreaContextMenu;
}

async function openContextMenuInFrame(frameSelector) {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter([frameSelector, "body"], {type: "contextmenu"}, gBrowser.selectedBrowser);
  await popupShownPromise;
  return contentAreaContextMenu;
}

async function openContextMenu(selector = "#img1") {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "mousedown", button: 2}, gBrowser.selectedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "contextmenu"}, gBrowser.selectedBrowser);
  await popupShownPromise;
  return contentAreaContextMenu;
}

async function closeContextMenu(contextMenu) {
  let contentAreaContextMenu = contextMenu || document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  contentAreaContextMenu.hidePopup();
  await popupHiddenPromise;
}

async function openExtensionContextMenu(selector = "#img1") {
  let contextMenu = await openContextMenu(selector);
  let topLevelMenu = contextMenu.getElementsByAttribute("ext-type", "top-level-menu");

  // Return null if the extension only has one item and therefore no extension menu.
  if (topLevelMenu.length == 0) {
    return null;
  }

  let extensionMenu = topLevelMenu[0];
  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(extensionMenu, {});
  await popupShownPromise;
  return extensionMenu;
}

async function closeExtensionContextMenu(itemToSelect, modifiers = {}) {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  if (itemToSelect) {
    EventUtils.synthesizeMouseAtCenter(itemToSelect, modifiers);
  } else {
    contentAreaContextMenu.hidePopup();
  }
  await popupHiddenPromise;

  // Bug 1351638: parent menu fails to close intermittently, make sure it does.
  contentAreaContextMenu.hidePopup();
}

async function openToolsMenu(win = window) {
  const node = win.document.getElementById("tools-menu");
  const menu = win.document.getElementById("menu_ToolsPopup");
  const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  if (AppConstants.platform === "macosx") {
    // We can't open menubar items on OSX, so mocking instead.
    menu.dispatchEvent(new MouseEvent("popupshowing"));
    menu.dispatchEvent(new MouseEvent("popupshown"));
  } else {
    node.open = true;
  }
  await shown;
  return menu;
}

function closeToolsMenu(itemToSelect, win = window) {
  const menu = win.document.getElementById("menu_ToolsPopup");
  const hidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
  if (AppConstants.platform === "macosx") {
    // Mocking on OSX, see above.
    if (itemToSelect) {
      itemToSelect.doCommand();
    }
    menu.dispatchEvent(new MouseEvent("popuphiding"));
    menu.dispatchEvent(new MouseEvent("popuphidden"));
  } else if (itemToSelect) {
    EventUtils.synthesizeMouseAtCenter(itemToSelect, {}, win);
  } else {
    menu.hidePopup();
  }
  return hidden;
}

async function openChromeContextMenu(menuId, target, win = window) {
  const node = win.document.querySelector(target);
  const menu = win.document.getElementById(menuId);
  const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(node, {type: "contextmenu"}, win);
  await shown;
  return menu;
}

async function openSubmenu(submenuItem, win = window) {
  const submenu = submenuItem.firstChild;
  const shown = BrowserTestUtils.waitForEvent(submenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(submenuItem, {}, win);
  await shown;
  return submenu;
}

function closeChromeContextMenu(menuId, itemToSelect, win = window) {
  const menu = win.document.getElementById(menuId);
  const hidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
  if (itemToSelect) {
    EventUtils.synthesizeMouseAtCenter(itemToSelect, {}, win);
  } else {
    menu.hidePopup();
  }
  return hidden;
}

async function openActionContextMenu(extension, kind, win = window) {
  // See comment from clickPageAction below.
  SetPageProxyState("valid");
  await promiseAnimationFrame(win);
  let buttonID;
  let menuID;
  if (kind == "page") {
    buttonID = "#" + BrowserPageActions.urlbarButtonNodeIDForActionID(makeWidgetId(extension.id));
    menuID = "pageActionContextMenu";
  } else {
    buttonID = `#${makeWidgetId(extension.id)}-${kind}-action`;
    menuID = "toolbar-context-menu";
  }
  return openChromeContextMenu(menuID, buttonID, win);
}

function closeActionContextMenu(itemToSelect, kind, win = window) {
  let menuID = kind == "page" ? "pageActionContextMenu" : "toolbar-context-menu";
  return closeChromeContextMenu(menuID, itemToSelect, win);
}

function openTabContextMenu(win = window) {
  return openChromeContextMenu("tabContextMenu", ".tabbrowser-tab[selected]", win);
}

function closeTabContextMenu(itemToSelect, win = window) {
  return closeChromeContextMenu("tabContextMenu", itemToSelect, win);
}

function getPageActionPopup(extension, win = window) {
  let panelId = makeWidgetId(extension.id) + "-panel";
  return win.document.getElementById(panelId);
}

async function clickPageAction(extension, win = window) {
  // This would normally be set automatically on navigation, and cleared
  // when the user types a value into the URL bar, to show and hide page
  // identity info and icons such as page action buttons.
  //
  // Unfortunately, that doesn't happen automatically in browser chrome
  // tests.
  SetPageProxyState("valid");

  await promiseAnimationFrame(win);

  let pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(makeWidgetId(extension.id));

  let elem = win.document.getElementById(pageActionId);

  EventUtils.synthesizeMouseAtCenter(elem, {}, win);
  return new Promise(SimpleTest.executeSoon);
}

function closePageAction(extension, win = window) {
  let node = getPageActionPopup(extension, win);
  if (node) {
    return promisePopupShown(node).then(() => {
      node.hidePopup();
    });
  }

  return Promise.resolve();
}

function promisePrefChangeObserved(pref) {
  return new Promise((resolve, reject) =>
    Preferences.observe(pref, function prefObserver() {
      Preferences.ignore(pref, prefObserver);
      resolve();
    }));
}

function promiseWindowRestored(window) {
  return new Promise(resolve => window.addEventListener("SSWindowRestored", resolve, {once: true}));
}

function awaitEvent(eventName, id) {
  return new Promise(resolve => {
    let listener = (_eventName, ...args) => {
      let extension = args[0];
      if (_eventName === eventName &&
          extension.id == id) {
        Management.off(eventName, listener);
        resolve();
      }
    };

    Management.on(eventName, listener);
  });
}

function* BrowserWindowIterator() {
  let windowsEnum = Services.wm.getEnumerator("navigator:browser");
  while (windowsEnum.hasMoreElements()) {
    let currentWindow = windowsEnum.getNext();
    if (!currentWindow.closed) {
      yield currentWindow;
    }
  }
}

async function locationChange(tab, url, task) {
  let locationChanged = BrowserTestUtils.waitForLocationChange(gBrowser, url);
  await ContentTask.spawn(tab.linkedBrowser, url, task);
  return locationChanged;
}

function navigateTab(tab, url) {
  return locationChange(tab, url, (url) => { content.location.href = url; });
}

function historyPushState(tab, url) {
  return locationChange(tab, url, (url) => { content.history.pushState(null, null, url); });
}
