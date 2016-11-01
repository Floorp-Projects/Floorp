/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function getBrowserAction(extension) {
  const {GlobalManager, Management: {global: {browserActionFor}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let ext = GlobalManager.extensionMap.get(extension.id);
  return browserActionFor(ext);
}

function* testInArea(area) {
  let scriptPage = url => `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "background": {
        "page": "data/background.html",
      },
      "browser_action": {
        "default_popup": "popup-a.html",
        "browser_style": true,
      },
    },

    files: {
      "popup-a.html": scriptPage("popup-a.js"),
      "popup-a.js": function() {
        window.onload = () => {
          let color = window.getComputedStyle(document.body).color;
          browser.test.assertEq("rgb(34, 36, 38)", color);
          browser.runtime.sendMessage("from-popup-a");
        };
        browser.runtime.onMessage.addListener(msg => {
          if (msg == "close-popup-using-window.close") {
            window.close();
          }
        });
      },

      "data/popup-b.html": scriptPage("popup-b.js"),
      "data/popup-b.js": function() {
        window.onload = () => {
          browser.runtime.sendMessage("from-popup-b");
        };
      },

      "data/popup-c.html": scriptPage("popup-c.js"),
      "data/popup-c.js": function() {
        // Close the popup before the document is fully-loaded to make sure that
        // we handle this case sanely.
        browser.runtime.sendMessage("from-popup-c");
        window.close();
      },

      "data/background.html": scriptPage("background.js"),

      "data/background.js": function() {
        let sendClick;
        let tests = [
          () => {
            browser.test.log(`Click browser action, expect popup "a".`);
            sendClick({expectEvent: false, expectPopup: "a"});
          },
          () => {
            browser.test.log(`Click browser action again, expect popup "a".`);
            sendClick({expectEvent: false, expectPopup: "a"});
          },
          () => {
            browser.test.log(`Call triggerAction, expect popup "a" again. Leave popup open.`);
            sendClick({expectEvent: false, expectPopup: "a", closePopup: false}, "trigger-action");
          },
          () => {
            browser.test.log(`Call triggerAction again. Expect remaining popup closed.`);
            sendClick({expectEvent: false, expectPopup: null}, "trigger-action");
            browser.test.sendMessage("next-test", {waitUntilClosed: true});
          },
          () => {
            browser.test.log(`Call triggerAction again. Expect popup "a" again.`);
            sendClick({expectEvent: false, expectPopup: "a"}, "trigger-action");
          },
          () => {
            browser.test.log(`Set popup to "c" and click browser action. Expect popup "c".`);
            browser.browserAction.setPopup({popup: "popup-c.html"});
            sendClick({expectEvent: false, expectPopup: "c", closePopup: false});
          },
          () => {
            browser.test.log(`Set popup to "b" and click browser action. Expect popup "b".`);
            browser.browserAction.setPopup({popup: "popup-b.html"});
            sendClick({expectEvent: false, expectPopup: "b"});
          },
          () => {
            browser.test.log(`Click browser action again, expect popup "b".`);
            sendClick({expectEvent: false, expectPopup: "b"});
          },
          () => {
            browser.test.log(`Clear popup URL. Click browser action. Expect click event.`);
            browser.browserAction.setPopup({popup: ""});
            sendClick({expectEvent: true, expectPopup: null});
          },
          () => {
            browser.test.log(`Click browser action again. Expect another click event.`);
            sendClick({expectEvent: true, expectPopup: null});
          },
          () => {
            browser.test.log(`Call triggerAction. Expect click event.`);
            sendClick({expectEvent: true, expectPopup: null}, "trigger-action");
          },
          () => {
            browser.test.log(`Set popup to "a" and click browser action. Expect popup "a", and leave open.`);
            browser.browserAction.setPopup({popup: "/popup-a.html"});
            sendClick({expectEvent: false, expectPopup: "a", closePopup: false});
          },
          () => {
            browser.test.log(`Tell popup "a" to call window.close(). Expect popup closed.`);
            browser.test.sendMessage("next-test", {closePopupUsingWindow: true});
          },
        ];

        let expect = {};
        sendClick = ({expectEvent, expectPopup, runNextTest, waitUntilClosed, closePopup}, message = "send-click") => {
          if (closePopup == undefined) {
            closePopup = true;
          }

          expect = {event: expectEvent, popup: expectPopup, runNextTest, waitUntilClosed, closePopup};
          browser.test.sendMessage(message);
        };

        browser.runtime.onMessage.addListener(msg => {
          if (msg == "close-popup-using-window.close") {
            return;
          } else if (expect.popup) {
            browser.test.assertEq(msg, `from-popup-${expect.popup}`,
                                  "expected popup opened");
          } else {
            browser.test.fail(`unexpected popup: ${msg}`);
          }

          expect.popup = null;
          browser.test.sendMessage("next-test", expect);
        });

        browser.browserAction.onClicked.addListener(() => {
          if (expect.event) {
            browser.test.succeed("expected click event received");
          } else {
            browser.test.fail("unexpected click event");
          }

          expect.event = false;
          browser.test.sendMessage("next-test", expect);
        });

        browser.test.onMessage.addListener((msg) => {
          if (msg == "close-popup-using-window.close") {
            browser.runtime.sendMessage("close-popup-using-window.close");
            return;
          }

          if (msg != "next-test") {
            browser.test.fail("Expecting 'next-test' message");
          }

          if (tests.length) {
            let test = tests.shift();
            test();
          } else {
            browser.test.notifyPass("browseraction-tests-done");
          }
        });

        browser.test.sendMessage("next-test");
      },
    },
  });

  extension.onMessage("send-click", () => {
    clickBrowserAction(extension);
  });

  extension.onMessage("trigger-action", () => {
    getBrowserAction(extension).triggerAction(window);
  });

  let widget;
  extension.onMessage("next-test", Task.async(function* (expecting = {}) {
    if (!widget) {
      widget = getBrowserActionWidget(extension);
      CustomizableUI.addWidgetToArea(widget.id, area);
    }
    if (expecting.waitUntilClosed) {
      let panel = getBrowserActionPopup(extension);
      if (panel && panel.state != "closed") {
        yield promisePopupHidden(panel);
      }
    } else if (expecting.closePopupUsingWindow) {
      let panel = getBrowserActionPopup(extension);
      ok(panel, "Expect panel to exist");
      yield promisePopupShown(panel);

      extension.sendMessage("close-popup-using-window.close");

      yield promisePopupHidden(panel);
      ok(true, "Panel is closed");
    } else if (expecting.closePopup) {
      yield closeBrowserAction(extension);
    }

    extension.sendMessage("next-test");
  }));

  yield Promise.all([extension.startup(), extension.awaitFinish("browseraction-tests-done")]);

  yield extension.unload();

  let view = document.getElementById(widget.viewId);
  is(view, null, "browserAction view removed from document");
}

add_task(function* testBrowserActionInToolbar() {
  yield testInArea(CustomizableUI.AREA_NAVBAR);
});

add_task(function* testBrowserActionInPanel() {
  yield testInArea(CustomizableUI.AREA_PANEL);
});

add_task(function* testBrowserActionClickCanceled() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "popup.html",
        "browser_style": true,
      },
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

  // Test canceled click.
  EventUtils.synthesizeMouseAtCenter(widget.node, {type: "mousedown", button: 0}, window);

  isnot(browserAction.pendingPopup, null, "Have pending popup");
  is(browserAction.pendingPopup.window, window, "Have pending popup for the correct window");

  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

  EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mouseup", button: 0}, window);

  is(browserAction.pendingPopup, null, "Pending popup was cleared");
  is(browserAction.pendingPopupTimeout, null, "Have no pending popup timeout");

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

    background() {
      browser.browserAction.disable();
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"><script src="popup.js"></script></head></html>`,
      "popup.js"() {
        browser.test.fail("Should not get here");
      },
    },
  });

  yield extension.startup();

  const {GlobalManager, Management: {global: {browserActionFor}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let ext = GlobalManager.extensionMap.get(extension.id);
  let browserAction = browserActionFor(ext);

  let widget = getBrowserActionWidget(extension).forWindow(window);

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
