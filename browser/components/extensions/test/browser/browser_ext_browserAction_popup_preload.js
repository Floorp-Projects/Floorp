/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

let scriptPage = url =>
  `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

add_task(async function testBrowserActionClickCanceled() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    gURLBar.textbox,
    { type: "mousemove" },
    window
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
        browser_style: true,
      },
      permissions: ["activeTab"],
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head></html>`,
    },
  });

  await extension.startup();

  const {
    Management: {
      global: { browserActionFor },
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

  let ext = WebExtensionPolicy.getByID(extension.id)?.extension;
  let browserAction = browserActionFor(ext);

  let widget = getBrowserActionWidget(extension).forWindow(window);

  // Test canceled click.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousemove" },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );

  isnot(browserAction.pendingPopup, null, "Have pending popup");
  is(
    browserAction.pendingPopup.window,
    window,
    "Have pending popup for the correct window"
  );

  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  is(browserAction.action.activeTabForPreload, tab, "Tab to revoke was saved");
  is(
    browserAction.tabManager.hasActiveTabPermission(tab),
    true,
    "Active tab was granted permission"
  );

  // We intentionally turn off this a11y check, because the following click
  // is send on the <body> to dismiss the pending popup using an alternative way
  // of the popup dismissal, where the other way like `Esc` key is available,
  // therefore this test can be ignored.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  EventUtils.synthesizeMouseAtCenter(
    document.documentElement,
    { type: "mousemove" },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    document.documentElement,
    { type: "mouseup", button: 0 },
    window
  );
  AccessibilityUtils.resetEnv();

  is(browserAction.pendingPopup, null, "Pending popup was cleared");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  is(
    browserAction.action.activeTabForPreload,
    null,
    "Tab to revoke was removed"
  );
  is(
    browserAction.tabManager.hasActiveTabPermission(tab),
    false,
    "Permission was revoked from tab"
  );

  // Test completed click.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousemove" },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );

  isnot(browserAction.pendingPopup, null, "Have pending popup");
  is(
    browserAction.pendingPopup.window,
    window,
    "Have pending popup for the correct window"
  );

  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // We need to do these tests during the mouseup event cycle, since the click
  // and command events will be dispatched immediately after mouseup, and void
  // the results.
  let mouseUpPromise = BrowserTestUtils.waitForEvent(
    widget.node,
    "mouseup",
    false,
    event => {
      isnot(browserAction.pendingPopup, null, "Pending popup was not cleared");
      isnot(
        browserAction.pendingPopupTimeout,
        null,
        "Have a pending popup timeout"
      );
      return true;
    }
  );

  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseup", button: 0 },
    window
  );

  await mouseUpPromise;

  is(browserAction.pendingPopup, null, "Pending popup was cleared");
  is(
    browserAction.pendingPopupTimeout,
    null,
    "Pending popup timeout was cleared"
  );

  await promisePopupShown(getBrowserActionPopup(extension));
  await closeBrowserAction(extension);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testBrowserActionDisabled() {
  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    gURLBar.textbox,
    { type: "mousemove" },
    window
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
        browser_style: true,
      },
    },

    background: async function () {
      await browser.browserAction.disable();
      browser.test.sendMessage("browserAction-disabled");
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"><script src="popup.js"></script></head></html>`,
      "popup.js"() {
        browser.test.fail("Should not get here");
      },
    },
  });

  await extension.startup();

  await extension.awaitMessage("browserAction-disabled");
  await promiseAnimationFrame();

  const {
    Management: {
      global: { browserActionFor },
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

  let ext = WebExtensionPolicy.getByID(extension.id)?.extension;
  let browserAction = browserActionFor(ext);

  let widget = getBrowserActionWidget(extension).forWindow(window);
  let button = widget.node.firstElementChild;

  is(button.getAttribute("disabled"), "true", "Button is disabled");
  is(browserAction.pendingPopup, null, "Have no pending popup prior to click");

  // Test canceled click.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousemove" },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // We intentionally turn off this a11y check, because the following click
  // is send on the <body> to dismiss the pending popup using an alternative way
  // of the popup dismissal, where the other way like `Esc` key is available,
  // therefore this test can be ignored.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  EventUtils.synthesizeMouseAtCenter(
    document.documentElement,
    { type: "mousemove" },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    document.documentElement,
    { type: "mouseup", button: 0 },
    window
  );
  AccessibilityUtils.resetEnv();

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // Test completed click.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousemove" },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // We need to do these tests during the mouseup event cycle, since the click
  // and command events will be dispatched immediately after mouseup, and void
  // the results.
  let mouseUpPromise = BrowserTestUtils.waitForEvent(
    widget.node,
    "mouseup",
    false,
    event => {
      is(browserAction.pendingPopup, null, "Have no pending popup");
      is(
        browserAction.pendingPopupTimeout,
        null,
        "Have no pending popup timeout"
      );
      return true;
    }
  );

  // We intentionally turn off a11y_checks, because the following click
  // is targeting a disabled control to confirm the click event won't come through.
  // It is not meant to be interactive and is not expected to be accessible:
  AccessibilityUtils.setEnv({
    mustBeEnabled: false,
  });
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseup", button: 0 },
    window
  );
  AccessibilityUtils.resetEnv();

  await mouseUpPromise;

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // Give the popup a chance to load and trigger a failure, if it was
  // erroneously opened.
  await new Promise(resolve => setTimeout(resolve, 250));

  await extension.unload();
});

add_task(async function testBrowserActionTabPopulation() {
  // Note: This test relates to https://bugzilla.mozilla.org/show_bug.cgi?id=1310019
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
        browser_style: true,
      },
      permissions: ["activeTab"],
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": function () {
        browser.tabs.query({ active: true, currentWindow: true }).then(tabs => {
          browser.test.assertEq(
            "mochitest index /",
            tabs[0].title,
            "Tab has the expected title on first click"
          );
          browser.test.sendMessage("tabTitle");
        });
      },
    },
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  BrowserTestUtils.startLoadingURIString(
    win.gBrowser.selectedBrowser,
    "http://example.com/"
  );
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    win.gURLBar.textbox,
    { type: "mousemove" },
    win
  );

  await extension.startup();

  let widget = getBrowserActionWidget(extension).forWindow(win);
  EventUtils.synthesizeMouseAtCenter(widget.node, { type: "mousemove" }, win);
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    win
  );

  await new Promise(resolve => setTimeout(resolve, 100));

  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseup", button: 0 },
    win
  );

  await extension.awaitMessage("tabTitle");

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testClosePopupDuringPreload() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
        browser_style: true,
      },
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": function () {
        browser.test.sendMessage("popup_loaded");
        window.close();
      },
    },
  });

  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    gURLBar.textbox,
    { type: "mousemove" },
    window
  );

  await extension.startup();

  const {
    Management: {
      global: { browserActionFor },
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

  let ext = WebExtensionPolicy.getByID(extension.id)?.extension;
  let browserAction = browserActionFor(ext);

  let widget = getBrowserActionWidget(extension).forWindow(window);

  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousemove" },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );

  isnot(browserAction.pendingPopup, null, "Have pending popup");
  is(
    browserAction.pendingPopup.window,
    window,
    "Have pending popup for the correct window"
  );

  await extension.awaitMessage("popup_loaded");
  try {
    await browserAction.pendingPopup.browserLoaded;
  } catch (e) {
    is(
      e.message,
      "Popup destroyed",
      "Popup content should have been destroyed"
    );
  }

  let promiseViewShowing = BrowserTestUtils.waitForEvent(
    document,
    "ViewShowing",
    false,
    ev => ev.target.id === browserAction.viewId
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseup", button: 0 },
    window
  );

  // The popup panel may become visible after the ViewShowing event. Wait a bit
  // to ensure that the popup is not shown when window.close() was used.
  await promiseViewShowing;
  await new Promise(resolve => setTimeout(resolve, 50));

  let panel = getBrowserActionPopup(extension);
  is(panel, null, "Popup panel should have been closed");

  await extension.unload();
});
