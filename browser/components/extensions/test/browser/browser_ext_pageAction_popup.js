/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

PromiseTestUtils.whitelistRejectionsGlobally(/packaging errors/);

const { GlobalManager } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm",
  null
);

function assertViewCount(extension, count) {
  let ext = GlobalManager.extensionMap.get(extension.id);
  is(
    ext.views.size,
    count,
    "Should have the expected number of extension views"
  );
}

add_task(async function testPageActionPopup() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  let scriptPage = url =>
    `<html><head><meta charset="utf-8"><script src="${url}"></script></head></html>`;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        page: "data/background.html",
      },
      page_action: {
        default_popup: "popup-a.html",
      },
    },

    files: {
      "popup-a.html": scriptPage("popup-a.js"),
      "popup-a.js": function() {
        window.onload = () => {
          let background = window.getComputedStyle(document.body)
            .backgroundColor;
          browser.test.assertEq("rgba(0, 0, 0, 0)", background);
          browser.runtime.sendMessage("from-popup-a");
        };
        browser.runtime.onMessage.addListener(msg => {
          if (msg == "close-popup") {
            window.close();
          }
        });
      },

      "data/popup-b.html": scriptPage("popup-b.js"),
      "data/popup-b.js": function() {
        browser.runtime.sendMessage("from-popup-b");
      },

      "data/background.html": scriptPage("background.js"),

      "data/background.js": async function() {
        let tabId;

        let sendClick;
        let tests = [
          () => {
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
          () => {
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
          () => {
            browser.pageAction.setPopup({ tabId, popup: "popup-b.html" });
            sendClick({ expectEvent: false, expectPopup: "b" });
          },
          () => {
            sendClick({ expectEvent: false, expectPopup: "b" });
          },
          () => {
            sendClick({
              expectEvent: true,
              expectPopup: "b",
              middleClick: true,
            });
          },
          () => {
            browser.pageAction.setPopup({ tabId, popup: "" });
            sendClick({ expectEvent: true, expectPopup: null });
          },
          () => {
            sendClick({ expectEvent: true, expectPopup: null });
          },
          () => {
            browser.pageAction.setPopup({ tabId, popup: "/popup-a.html" });
            sendClick({
              expectEvent: false,
              expectPopup: "a",
              runNextTest: true,
            });
          },
          () => {
            browser.test.sendMessage("next-test", { expectClosed: true });
          },
          () => {
            sendClick({
              expectEvent: false,
              expectPopup: "a",
              runNextTest: true,
            });
          },
          () => {
            browser.test.sendMessage("next-test", { closeOnTabSwitch: true });
          },
        ];

        let expect = {};
        sendClick = ({
          expectEvent,
          expectPopup,
          runNextTest,
          middleClick,
        }) => {
          expect = { event: expectEvent, popup: expectPopup, runNextTest };

          browser.test.sendMessage("send-click", middleClick ? 1 : 0);
        };

        browser.runtime.onMessage.addListener(msg => {
          if (msg == "close-popup") {
            return;
          } else if (expect.popup) {
            browser.test.assertEq(
              msg,
              `from-popup-${expect.popup}`,
              "expected popup opened"
            );
          } else {
            browser.test.fail(`unexpected popup: ${msg}`);
          }

          expect.popup = null;
          if (expect.runNextTest) {
            expect.runNextTest = false;
            tests.shift()();
          } else {
            browser.test.sendMessage("next-test");
          }
        });

        browser.pageAction.onClicked.addListener((tab, info) => {
          if (expect.event) {
            browser.test.succeed("expected click event received");
          } else {
            browser.test.fail("unexpected click event");
          }
          expect.event = false;

          if (info.button == 1) {
            browser.pageAction.openPopup();
            return;
          }

          browser.test.sendMessage("next-test");
        });

        browser.test.onMessage.addListener(msg => {
          if (msg == "close-popup") {
            browser.runtime.sendMessage("close-popup");
            return;
          }

          if (msg != "next-test") {
            browser.test.fail("Expecting 'next-test' message");
          }

          if (expect.event) {
            browser.test.fail(
              "Expecting click event before next test but none occurred"
            );
          }

          if (expect.popup) {
            browser.test.fail(
              "Expecting popup before next test but none were shown"
            );
          }

          if (tests.length) {
            let test = tests.shift();
            test();
          } else {
            browser.test.notifyPass("pageaction-tests-done");
          }
        });

        let [tab] = await browser.tabs.query({
          active: true,
          currentWindow: true,
        });
        tabId = tab.id;

        await browser.pageAction.show(tabId);
        browser.test.sendMessage("next-test");
      },
    },
  });

  extension.onMessage("send-click", button => {
    clickPageAction(extension, window, { button });
  });

  let pageActionId, panelId;
  extension.onMessage("next-test", async function(expecting = {}) {
    pageActionId = `${makeWidgetId(extension.id)}-page-action`;
    panelId = `${makeWidgetId(extension.id)}-panel`;
    let panel = document.getElementById(panelId);
    if (expecting.expectClosed) {
      ok(panel, "Expect panel to exist");
      await promisePopupShown(panel);

      extension.sendMessage("close-popup");

      await promisePopupHidden(panel);
      ok(true, `Panel is closed`);
    } else if (expecting.closeOnTabSwitch) {
      ok(panel, "Expect panel to exist");
      await promisePopupShown(panel);

      let oldTab = gBrowser.selectedTab;
      ok(
        oldTab != gBrowser.tabs[0],
        "Should have an inactive tab to switch to"
      );

      let hiddenPromise = promisePopupHidden(panel);

      gBrowser.selectedTab = gBrowser.tabs[0];
      await hiddenPromise;
      info("Panel closed");

      gBrowser.selectedTab = oldTab;
    } else if (panel) {
      await promisePopupShown(panel);
      panel.hidePopup();
    }

    assertViewCount(extension, 1);

    if (panel) {
      panel = document.getElementById(panelId);
      is(panel, null, "panel successfully removed from document after hiding");
    }

    extension.sendMessage("next-test");
  });

  await extension.startup();
  await extension.awaitFinish("pageaction-tests-done");

  await extension.unload();

  let node = document.getElementById(pageActionId);
  is(node, null, "pageAction image removed from document");

  let panel = document.getElementById(panelId);
  is(panel, null, "pageAction panel removed from document");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function testPageActionSecurity() {
  const URL = "chrome://browser/content/browser.xhtml";

  let apis = ["browser_action", "page_action"];

  for (let api of apis) {
    info(`TEST ${api} icon url: ${URL}`);

    let messages = [/Access to restricted URI denied/];

    let waitForConsole = new Promise(resolve => {
      // Not necessary in browser-chrome tests, but monitorConsole gripes
      // if we don't call it.
      SimpleTest.waitForExplicitFinish();

      SimpleTest.monitorConsole(resolve, messages);
    });

    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        [api]: { default_popup: URL },
      },
    });

    await Assert.rejects(
      extension.startup(),
      /startup failed/,
      "Manifest rejected"
    );

    SimpleTest.endMonitorConsole();
    await waitForConsole;
  }
});

add_task(forceGC);
