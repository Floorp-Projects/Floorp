/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { GlobalManager } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm",
  null
);

function getBrowserAction(extension) {
  const {
    global: { browserActionFor },
  } = Management;

  let ext = GlobalManager.extensionMap.get(extension.id);
  return browserActionFor(ext);
}

async function assertViewCount(extension, count, waitForPromise) {
  let ext = GlobalManager.extensionMap.get(extension.id);

  if (waitForPromise) {
    await waitForPromise;
  }

  is(
    ext.views.size,
    count,
    "Should have the expected number of extension views"
  );
}

function promiseExtensionPageClosed(extension, viewType) {
  return new Promise(resolve => {
    const policy = WebExtensionPolicy.getByID(extension.id);

    const listener = (evtType, context) => {
      if (context.viewType === viewType) {
        policy.extension.off("extension-proxy-context-load", listener);

        context.callOnClose({
          close: resolve,
        });
      }
    };
    policy.extension.on("extension-proxy-context-load", listener);
  });
}

let scriptPage = url =>
  `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

async function testInArea(area) {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.browserAction.onClicked.addListener(() => {
        browser.test.sendMessage("browserAction-onClicked");
      });

      browser.test.onMessage.addListener(async msg => {
        if (msg.type === "setBrowserActionPopup") {
          let opts = { popup: msg.popup };
          if (msg.onCurrentWindowId) {
            let { id } = await browser.windows.getCurrent();
            opts = { ...opts, windowId: id };
          } else if (msg.onActiveTabId) {
            let [{ id }] = await browser.tabs.query({
              active: true,
              currentWindow: true,
            });
            opts = { ...opts, tabId: id };
          }
          await browser.browserAction.setPopup(opts);
          browser.test.sendMessage("setBrowserActionPopup:done");
        }
      });

      browser.test.sendMessage("background-page-ready");
    },
    manifest: {
      browser_action: {
        default_popup: "popup-a.html",
        browser_style: true,
      },
    },

    files: {
      "popup-a.html": scriptPage("popup-a.js"),
      "popup-a.js": function() {
        browser.test.onMessage.addListener(msg => {
          if (msg == "close-popup-using-window.close") {
            window.close();
          }
        });

        window.onload = () => {
          let color = window.getComputedStyle(document.body).color;
          browser.test.assertEq("rgb(34, 36, 38)", color);
          browser.test.sendMessage("from-popup", "popup-a");
        };
      },

      "data/popup-b.html": scriptPage("popup-b.js"),
      "data/popup-b.js": function() {
        window.onload = () => {
          browser.test.sendMessage("from-popup", "popup-b");
        };
      },

      "data/popup-c.html": scriptPage("popup-c.js"),
      "data/popup-c.js": function() {
        // Close the popup before the document is fully-loaded to make sure that
        // we handle this case sanely.
        browser.test.sendMessage("from-popup", "popup-c");
        window.close();
      },
    },
  });

  await Promise.all([
    extension.startup(),
    extension.awaitMessage("background-page-ready"),
  ]);

  let widget = getBrowserActionWidget(extension);

  // Move the browserAction widget to the area targeted by this test.
  CustomizableUI.addWidgetToArea(widget.id, area);

  async function setBrowserActionPopup(opts) {
    extension.sendMessage({ type: "setBrowserActionPopup", ...opts });
    await extension.awaitMessage("setBrowserActionPopup:done");
  }

  async function runTest({
    actionType,
    waitForPopupLoaded,
    expectPopup,
    expectOnClicked,
    closePopup,
  }) {
    const oncePopupPageClosed = promiseExtensionPageClosed(extension, "popup");
    const oncePopupLoaded = waitForPopupLoaded
      ? awaitExtensionPanel(extension)
      : undefined;

    if (actionType === "click") {
      clickBrowserAction(extension);
    } else if (actionType === "trigger") {
      getBrowserAction(extension).triggerAction(window);
    }

    if (expectPopup) {
      info(`Waiting for popup: ${expectPopup}`);
      is(
        await extension.awaitMessage("from-popup"),
        expectPopup,
        "expected popup opened"
      );
    } else if (expectOnClicked) {
      await extension.awaitMessage("browserAction-onClicked");
    }

    await oncePopupLoaded;

    if (closePopup) {
      info("Closing popup");
      await closeBrowserAction(extension);
      await assertViewCount(extension, 1, oncePopupPageClosed);
    }

    return { oncePopupPageClosed };
  }

  // Run the sequence of test cases.
  const tests = [
    async () => {
      info(`Click browser action, expect popup "a".`);

      await runTest({
        actionType: "click",
        expectPopup: "popup-a",
        closePopup: true,
      });
    },
    async () => {
      info(`Click browser action again, expect popup "a".`);

      await runTest({
        actionType: "click",
        expectPopup: "popup-a",
        waitForPopupLoaded: true,
        closePopup: true,
      });
    },
    async () => {
      info(`Call triggerAction, expect popup "a" again. Leave popup open.`);

      const { oncePopupPageClosed } = await runTest({
        actionType: "trigger",
        expectPopup: "popup-a",
        waitForPopupLoaded: true,
      });

      await assertViewCount(extension, 2);

      info(`Call triggerAction again. Expect remaining popup closed.`);
      getBrowserAction(extension).triggerAction(window);

      await assertViewCount(extension, 1, oncePopupPageClosed);
    },
    async () => {
      info(`Call triggerAction again. Expect popup "a" again.`);

      await runTest({
        actionType: "trigger",
        expectPopup: "popup-a",
        closePopup: true,
      });
    },
    async () => {
      info(`Set popup to "c" and click browser action. Expect popup "c".`);

      await setBrowserActionPopup({ popup: "data/popup-c.html" });

      const { oncePopupPageClosed } = await runTest({
        actionType: "click",
        expectPopup: "popup-c",
      });

      await assertViewCount(extension, 1, oncePopupPageClosed);
    },
    async () => {
      info(`Set popup to "b" and click browser action. Expect popup "b".`);

      await setBrowserActionPopup({ popup: "data/popup-b.html" });

      await runTest({
        actionType: "click",
        expectPopup: "popup-b",
        closePopup: true,
      });
    },
    async () => {
      info(`Click browser action again, expect popup "b".`);

      await runTest({
        actionType: "click",
        expectPopup: "popup-b",
        closePopup: true,
      });
    },
    async () => {
      info(`Clear popup URL. Click browser action. Expect click event.`);

      await setBrowserActionPopup({ popup: "" });

      await runTest({
        actionType: "click",
        expectOnClicked: true,
      });
    },
    async () => {
      info(`Click browser action again. Expect another click event.`);

      await runTest({
        actionType: "click",
        expectOnClicked: true,
      });
    },
    async () => {
      info(`Call triggerAction. Expect click event.`);

      await runTest({
        actionType: "trigger",
        expectOnClicked: true,
      });
    },
    async () => {
      info(
        `Set window-specific popup to "b" and click browser action. Expect popup "b".`
      );

      await setBrowserActionPopup({
        popup: "data/popup-b.html",
        onCurrentWindowId: true,
      });

      await runTest({
        actionType: "click",
        expectPopup: "popup-b",
        closePopup: true,
      });
    },
    async () => {
      info(
        `Set tab-specific popup to "a" and click browser action. Expect popup "a", and leave open.`
      );

      await setBrowserActionPopup({
        popup: "/popup-a.html",
        onActiveTabId: true,
      });

      const { oncePopupPageClosed } = await runTest({
        actionType: "click",
        expectPopup: "popup-a",
      });
      assertViewCount(extension, 2);

      info(`Tell popup "a" to call window.close(). Expect popup closed.`);
      extension.sendMessage("close-popup-using-window.close");

      await assertViewCount(extension, 1, oncePopupPageClosed);
    },
  ];

  for (let test of tests) {
    await test();
  }

  // Unload the extension and verify that the browserAction widget is gone.
  await extension.unload();

  let view = document.getElementById(widget.viewId);
  is(view, null, "browserAction view removed from document");
}

add_task(async function testBrowserActionInToolbar() {
  await testInArea(CustomizableUI.AREA_NAVBAR);
});

add_task(async function testBrowserActionInPanel() {
  await testInArea(getCustomizableUIPanelID());
});
