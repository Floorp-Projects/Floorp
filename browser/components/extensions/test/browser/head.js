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
 *          openExtensionContextMenu closeExtensionContextMenu
 *          openActionContextMenu openSubmenu closeActionContextMenu
 *          openTabContextMenu closeTabContextMenu
 *          imageBuffer getListStyleImage getPanelForNode
 *          awaitExtensionPanel awaitPopupResize
 *          promiseContentDimensions alterContent
 */

const {AppConstants} = Cu.import("resource://gre/modules/AppConstants.jsm", {});
const {CustomizableUI} = Cu.import("resource:///modules/CustomizableUI.jsm", {});

// We run tests under two different configurations, from browser.ini and
// browser-remote.ini. When running from browser-remote.ini, the tests are
// copied to the sub-directory "test-oop-extensions", which we detect here, and
// use to select our configuration.
if (gTestPath.includes("test-oop-extensions")) {
  SpecialPowers.pushPrefEnv({set: [
    ["dom.ipc.processCount.extension", 1],
    ["extensions.webextensions.remote", true],
  ]});
  // We don't want to reset this at the end of the test, so that we don't have
  // to spawn a new extension child process for each test unit.
  SpecialPowers.setIntPref("dom.ipc.keepProcessesAlive.extension", 1);
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

var focusWindow = Task.async(function* focusWindow(win) {
  let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  if (fm.activeWindow == win) {
    return;
  }

  let promise = new Promise(resolve => {
    win.addEventListener("focus", function listener() {
      win.removeEventListener("focus", listener, true);
      resolve();
    }, true);
  });

  win.focus();
  yield promise;
});

let img = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAAAABQABh6FO1AAAAABJRU5ErkJggg==";
var imageBuffer = Uint8Array.from(atob(img), byte => byte.charCodeAt(0)).buffer;

function getListStyleImage(button) {
  let style = button.ownerDocument.defaultView.getComputedStyle(button);

  let match = /^url\("(.*)"\)$/.exec(style.listStyleImage);

  return match && match[1];
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
  return ContentTask.spawn(browser, null, function* () {
    function copyProps(obj, props) {
      let res = {};
      for (let prop of props) {
        res[prop] = obj[prop];
      }
      return res;
    }

    return {
      window: copyProps(content,
        ["innerWidth", "innerHeight", "outerWidth", "outerHeight",
         "scrollX", "scrollY", "scrollMaxX", "scrollMaxY"]),
      body: copyProps(content.document.body,
        ["clientWidth", "clientHeight", "scrollWidth", "scrollHeight"]),
      root: copyProps(content.document.documentElement,
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
  if (content.document.readyState !== "complete") {
    return ContentTaskUtils.waitForEvent(this, "load", true).then(() => {});
  }
});

var awaitExtensionPanel = Task.async(function* (extension, win = window, awaitLoad = true) {
  let {originalTarget: browser} = yield BrowserTestUtils.waitForEvent(
    win.document, "WebExtPopupLoaded", true,
    event => event.detail.extension.id === extension.id);

  yield Promise.all([
    promisePopupShown(getPanelForNode(browser)),

    awaitLoad && awaitBrowserLoaded(browser, awaitLoad),
  ]);

  return browser;
});

function getBrowserActionWidget(extension) {
  return CustomizableUI.getWidget(makeWidgetId(extension.id) + "-browser-action");
}

function getBrowserActionPopup(extension, win = window) {
  let group = getBrowserActionWidget(extension);

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    return win.document.getElementById("customizationui-widget-panel");
  }
  return win.PanelUI.panel;
}

var showBrowserAction = Task.async(function* (extension, win = window) {
  let group = getBrowserActionWidget(extension);
  let widget = group.forWindow(win);

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    ok(!widget.overflowed, "Expect widget not to be overflowed");
  } else if (group.areaType == CustomizableUI.TYPE_MENU_PANEL) {
    yield win.PanelUI.show();
  }
});

var clickBrowserAction = Task.async(function* (extension, win = window) {
  yield showBrowserAction(extension, win);

  let widget = getBrowserActionWidget(extension).forWindow(win);

  EventUtils.synthesizeMouseAtCenter(widget.node, {}, win);
});

function closeBrowserAction(extension, win = window) {
  let group = getBrowserActionWidget(extension);

  let node = win.document.getElementById(group.viewId);
  CustomizableUI.hidePanelForNode(node);

  return Promise.resolve();
}

function* openContextMenu(selector = "#img1") {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  yield BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "contextmenu"}, gBrowser.selectedBrowser);
  yield popupShownPromise;
  return contentAreaContextMenu;
}

function* closeContextMenu() {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  contentAreaContextMenu.hidePopup();
  yield popupHiddenPromise;
}

function* openExtensionContextMenu(selector = "#img1") {
  let contextMenu = yield openContextMenu(selector);
  let topLevelMenu = contextMenu.getElementsByAttribute("ext-type", "top-level-menu");

  // Return null if the extension only has one item and therefore no extension menu.
  if (topLevelMenu.length == 0) {
    return null;
  }

  let extensionMenu = topLevelMenu[0].childNodes[0];
  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(extensionMenu, {});
  yield popupShownPromise;
  return extensionMenu;
}

function* closeExtensionContextMenu(itemToSelect) {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(itemToSelect, {});
  yield popupHiddenPromise;
}

function* openChromeContextMenu(menuId, target, win = window) {
  const node = win.document.querySelector(target);
  const menu = win.document.getElementById(menuId);
  const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(node, {type: "contextmenu"}, win);
  yield shown;
  return menu;
}

function* openSubmenu(submenuItem, win = window) {
  const submenu = submenuItem.firstChild;
  const shown = BrowserTestUtils.waitForEvent(submenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(submenuItem, {}, win);
  yield shown;
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

function openActionContextMenu(extension, kind, win = window) {
  // See comment from clickPageAction below.
  SetPageProxyState("valid");
  const id = `#${makeWidgetId(extension.id)}-${kind}-action`;
  return openChromeContextMenu("toolbar-context-menu", id, win);
}

function closeActionContextMenu(itemToSelect, win = window) {
  return closeChromeContextMenu("toolbar-context-menu", itemToSelect, win);
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

function clickPageAction(extension, win = window) {
  // This would normally be set automatically on navigation, and cleared
  // when the user types a value into the URL bar, to show and hide page
  // identity info and icons such as page action buttons.
  //
  // Unfortunately, that doesn't happen automatically in browser chrome
  // tests.
  /* globals SetPageProxyState */
  SetPageProxyState("valid");

  let pageActionId = makeWidgetId(extension.id) + "-page-action";
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
