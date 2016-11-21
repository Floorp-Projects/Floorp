/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let scriptPage = url => `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

add_task(function* testBrowserActionClickCanceled() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
      "permissions": ["activeTab"],
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head></html>`,
    },
  });

  yield extension.startup();

  const {GlobalManager, Management: {global: {browserActionFor}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let ext = GlobalManager.extensionMap.get(extension.id);
  let browserAction = browserActionFor(ext);

  let widget = getBrowserActionWidget(extension).forWindow(window);
  let tab = window.gBrowser.selectedTab;

  // Test canceled click.
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mousedown", button: 0}, window);

  isnot(browserAction.pendingPopup, null, "Have pending popup");
  is(browserAction.pendingPopup.window, window, "Have pending popup for the correct window");

  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  is(browserAction.tabToRevokeDuringClearPopup, tab, "Tab to revoke was saved");
  is(browserAction.tabManager.hasActiveTabPermission(tab), true, "Active tab was granted permission");

  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mouseup", button: 0}, window);

  is(browserAction.pendingPopup, null, "Pending popup was cleared");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  is(browserAction.tabToRevokeDuringClearPopup, null, "Tab to revoke was removed");
  is(browserAction.tabManager.hasActiveTabPermission(tab), false, "Permission was revoked from tab");

  // Test completed click.
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mousedown", button: 0}, window);

  isnot(browserAction.pendingPopup, null, "Have pending popup");
  is(browserAction.pendingPopup.window, window, "Have pending popup for the correct window");

  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // We need to do these tests during the mouseup event cycle, since the click
  // and command events will be dispatched immediately after mouseup, and void
  // the results.
  let mouseUpPromise = BrowserTestUtils.waitForEvent(widget.node, "mouseup", false, event => {
    isnot(browserAction.pendingPopup, null, "Pending popup was not cleared");
    isnot(browserAction.pendingPopupTimeout, null, "Have a pending popup timeout");
    return true;
  });

  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mouseup", button: 0}, window);

  yield mouseUpPromise;

  is(browserAction.pendingPopup, null, "Pending popup was cleared");
  is(browserAction.pendingPopupTimeout, null, "Pending popup timeout was cleared");

  yield promisePopupShown(getBrowserActionPopup(extension));
  yield closeBrowserAction(extension);

  yield extension.unload();
});

add_task(function* testBrowserActionDisabled() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
    },

    background: async function() {
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

  yield extension.startup();

  yield extension.awaitMessage("browserAction-disabled");

  const {GlobalManager, Management: {global: {browserActionFor}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let ext = GlobalManager.extensionMap.get(extension.id);
  let browserAction = browserActionFor(ext);

  let widget = getBrowserActionWidget(extension).forWindow(window);

  is(widget.node.getAttribute("disabled"), "true", "Button is disabled");
  is(browserAction.pendingPopup, null, "Have no pending popup prior to click");

  // Test canceled click.
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mousedown", button: 0}, window);

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mouseup", button: 0}, window);

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");


  // Test completed click.
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mousedown", button: 0}, window);

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // We need to do these tests during the mouseup event cycle, since the click
  // and command events will be dispatched immediately after mouseup, and void
  // the results.
  let mouseUpPromise = BrowserTestUtils.waitForEvent(widget.node, "mouseup", false, event => {
    is(browserAction.pendingPopup, null, "Have no pending popup");
    is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");
    return true;
  });

  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mouseup", button: 0}, window);

  yield mouseUpPromise;

  is(browserAction.pendingPopup, null, "Have no pending popup");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  // Give the popup a chance to load and trigger a failure, if it was
  // erroneously opened.
  yield new Promise(resolve => setTimeout(resolve, 250));

  yield extension.unload();
});

add_task(function* testBrowserActionTabPopulation() {
  // Note: This test relates to https://bugzilla.mozilla.org/show_bug.cgi?id=1310019
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
      "permissions": ["activeTab"],
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": function() {
        browser.tabs.query({active: true, currentWindow: true}).then(tabs => {
          browser.test.assertEq("mochitest index /",
                                tabs[0].title,
                                "Tab has the expected title on first click");
          browser.test.sendMessage("tabTitle");
        });
      },
    },
  });

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, "http://example.com/");
  yield BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  yield extension.startup();

  let widget = getBrowserActionWidget(extension).forWindow(win);
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mousedown", button: 0}, win);

  yield extension.awaitMessage("tabTitle");

  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mouseup", button: 0}, win);

  yield extension.unload();
  yield BrowserTestUtils.closeWindow(win);
});
