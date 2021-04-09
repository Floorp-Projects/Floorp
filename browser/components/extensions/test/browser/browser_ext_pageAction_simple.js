/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { PageActions } = ChromeUtils.import(
  "resource:///modules/PageActions.jsm"
);

const BASE =
  "http://example.com/browser/browser/components/extensions/test/browser/";

add_task(async function test_pageAction_basic() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {
        default_popup: "popup.html",
        unrecognized_property: "with-a-random-value",
      },
    },

    files: {
      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="popup.js"></script>
      </body></html>
      `,

      "popup.js": function() {
        browser.runtime.sendMessage("from-popup");
      },
    },

    background: function() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "from-popup", "correct message received");
        browser.test.sendMessage("popup");
      });
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        let tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("page-action-shown");
        });
      });
    },
  });

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: /Reading manifest: Warning processing page_action.unrecognized_property: An unexpected property was found/,
      },
    ]);
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);
  await extension.awaitMessage("page-action-shown");

  let elem = await getPageActionButton(extension);
  let parent = window.document.getElementById("page-action-buttons");
  is(
    elem && elem.parentNode,
    parent,
    `pageAction pinned to urlbar ${elem.parentNode.getAttribute("id")}`
  );

  clickPageAction(extension);

  await extension.awaitMessage("popup");

  await extension.unload();

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});

add_task(async function test_pageAction_pinned() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {
        default_popup: "popup.html",
        pinned: false,
      },
    },

    files: {
      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      </body></html>
      `,
    },

    background: function() {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        let tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("page-action-shown");
        });
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("page-action-shown");

  // There are plenty of tests for the main action button, we just verify
  // that we've properly set the pinned value.
  let action = PageActions.actionForID(makeWidgetId(extension.id));
  Assert.equal(
    action && action.pinnedToUrlbar,
    gProton,
    "Check pageAction pinning"
  );

  await extension.unload();
});

add_task(async function test_pageAction_icon_on_subframe_navigation() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {
        default_popup: "popup.html",
      },
    },

    files: {
      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      </body></html>
      `,
    },

    background: function() {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        let tabId = tabs[0].id;

        browser.pageAction.show(tabId).then(() => {
          browser.test.sendMessage("page-action-shown");
        });
      });
    },
  });

  await navigateTab(
    gBrowser.selectedTab,
    "data:text/html,<h1>Top Level Frame</h1>"
  );

  await extension.startup();
  await extension.awaitMessage("page-action-shown");

  const pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(
    makeWidgetId(extension.id)
  );

  await BrowserTestUtils.waitForCondition(() => {
    return document.getElementById(pageActionId);
  }, "pageAction is initially visible");

  info("Create a sub-frame");

  let subframeURL = `${BASE}#subframe-url-1`;
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [subframeURL],
    async url => {
      const iframe = this.content.document.createElement("iframe");
      iframe.setAttribute("id", "test-subframe");
      iframe.setAttribute("src", url);
      iframe.setAttribute("style", "height: 200px; width: 200px");

      // Await the initial url to be loaded in the subframe.
      await new Promise(resolve => {
        iframe.onload = resolve;
        this.content.document.body.appendChild(iframe);
      });
    }
  );

  await BrowserTestUtils.waitForCondition(() => {
    return document.getElementById(pageActionId);
  }, "pageAction should be visible when a subframe is created");

  info("Navigating the sub-frame");

  subframeURL = `${BASE}/file_dummy.html#subframe-url-2`;
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [subframeURL],
    async url => {
      const iframe = this.content.document.querySelector(
        "iframe#test-subframe"
      );

      // Await the subframe navigation.
      await new Promise(resolve => {
        iframe.onload = resolve;
        iframe.setAttribute("src", url);
      });
    }
  );

  info("Subframe location changed");

  await BrowserTestUtils.waitForCondition(() => {
    return document.getElementById(pageActionId);
  }, "pageAction should be visible after a subframe navigation");

  await extension.unload();
});
