/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_devtools.js");

async function genericChecker() {
  const params = new URLSearchParams(window.location.search);
  const kind = params.get("kind");

  browser.test.onMessage.addListener(async (msg, ...args) => {
    if (msg == `${kind}-get-contexts-invalid-params`) {
      browser.test.assertThrows(
        () => browser.runtime.getContexts({ unknownParamName: true }),
        /Type error for parameter filter \(Unexpected property "unknownParamName"\)/,
        "Got the expected error on unexpected filter property"
      );
      browser.test.sendMessage(`${msg}:done`);
    } else if (msg == `${kind}-get-contexts`) {
      const filter = args[0];
      try {
        const result = await browser.runtime.getContexts(filter);
        browser.test.sendMessage(`${msg}:result`, result);
      } catch (err) {
        // In case of unexpected errors, log a failure and let the test
        // to continue to avoid it to only fail after timing out.
        browser.test.fail(`browser.runtime.getContexts call rejected: ${err}`);
        browser.test.sendMessage(`${msg}:result`, []);
      }
    } else if (msg == `${kind}-history-push-state`) {
      const pushStateURL = args[0];
      window.history.pushState({}, "", pushStateURL);
      browser.test.sendMessage(`${msg}:done`);
    } else if (msg == `${kind}-create-iframe`) {
      const iframeUrl = args[0];
      const iframe = document.createElement("iframe");
      iframe.src = iframeUrl;
      document.body.appendChild(iframe);
    } else if (msg == `${kind}-open-options-page`) {
      browser.runtime.openOptionsPage();
    }
  });

  if (kind === "devtools-page") {
    await browser.devtools.panels.create(
      "Test DevTool Panel",
      "fake-icon.png",
      "page.html?kind=devtools-panel"
    );
  }

  browser.test.log(`${kind} extension page loaded`);
  browser.test.sendMessage(`${kind}-loaded`);
}

async function triggerActionPopup(extension, win, callback) {
  // Window needs focus to open popups.
  await focusWindow(win);
  await clickBrowserAction(extension, win);
  let browser = await awaitExtensionPanel(extension, win);

  await callback();

  let { unloadPromise } = await promiseBrowserContentUnloaded(browser);
  closeBrowserAction(extension, win);
  await unloadPromise;
}

const byWindowId = (a, b) => a.windowId - b.windowId;
const byTabId = (a, b) => a.tabId - b.tabId;
const byFrameId = (a, b) => a.frameId - b.frameId;
const byContextType = (a, b) => a.contextType.localeCompare(b.contextType);

const assertValidContextId = contextId => {
  Assert.equal(
    typeof contextId,
    "string",
    "contextId should be set to a string"
  );
  Assert.notEqual(
    contextId.length,
    0,
    "contextId should be set to a non-zero length string"
  );
};

const assertGetContextsResult = (
  actual,
  expected,
  msg,
  { assertContextId = false } = {}
) => {
  const actualCopy = assertContextId ? actual : actual.map(it => ({ ...it }));
  if (!assertContextId) {
    actualCopy.forEach(it => delete it.contextId);
  }
  for (let [idx, expectedProps] of expected.entries()) {
    Assert.deepEqual(actualCopy[idx], expectedProps, msg);
  }
  Assert.equal(
    actualCopy.length,
    expected.length,
    "Got the expected number of extension contexts"
  );
};

add_task(async function test_runtime_getContexts() {
  const EXT_ID = "runtime-getContexts@mochitest";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    incognitoOverride: "spanning",
    manifest: {
      manifest_version: 3,
      browser_specific_settings: { gecko: { id: EXT_ID } },

      action: {
        default_popup: "page.html?kind=action",
        default_area: "navbar",
      },

      sidebar_action: {
        default_panel: "page.html?kind=sidebar",
      },

      options_ui: {
        page: "page.html?kind=options",
      },

      devtools_page: "page.html?kind=devtools-page",

      background: {
        page: "page.html?kind=background",
      },
    },

    files: {
      "page.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"></head>
      <body>
      <script src="page.js"></script>
      </body></html>
      `,

      "page.js": genericChecker,
    },
  });

  const {
    Management: {
      global: { tabTracker, windowTracker },
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

  let firstWin = window;
  let secondWin = await BrowserTestUtils.openNewBrowserWindow();
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await extension.startup();
  await extension.awaitMessage("background-loaded");

  // Expect 3 sidebars (2 non-private and 1 private windows).
  await extension.awaitMessage("sidebar-loaded");
  await extension.awaitMessage("sidebar-loaded");
  await extension.awaitMessage("sidebar-loaded");

  let firstWinId = windowTracker.getId(firstWin);
  let secondWinId = windowTracker.getId(secondWin);
  let privateWinId = windowTracker.getId(privateWin);

  const getGetContextsResults = async ({ filter, sortBy }) => {
    extension.sendMessage("background-get-contexts", filter);
    let results = await extension.awaitMessage(
      "background-get-contexts:result"
    );
    if (sortBy) {
      results.sort(sortBy);
    }
    return results;
  };

  const resolveExtPageUrl = urlPath =>
    WebExtensionPolicy.getByID(EXT_ID).extension.baseURI.resolve(urlPath);

  const documentOrigin = resolveExtPageUrl("/").slice(0, -1);

  const getExpectedExtensionContext = ({
    contextId,
    contextType,
    documentUrl,
    incognito = false,
    frameId = 0,
    tabId = -1,
    windowId = -1,
  }) => {
    let props = {
      contextType,
      documentOrigin,
      documentUrl,
      incognito,
      frameId,
      tabId,
      windowId,
    };
    if (contextId) {
      props.contextId = contextId;
    }
    return props;
  };

  let expected = [
    getExpectedExtensionContext({
      contextType: "BACKGROUND",
      documentUrl: resolveExtPageUrl("page.html?kind=background"),
    }),

    getExpectedExtensionContext({
      contextType: "SIDE_PANEL",
      documentUrl: resolveExtPageUrl("page.html?kind=sidebar"),
      windowId: firstWinId,
    }),

    getExpectedExtensionContext({
      contextType: "SIDE_PANEL",
      documentUrl: resolveExtPageUrl("page.html?kind=sidebar"),
      windowId: secondWinId,
    }),

    getExpectedExtensionContext({
      contextType: "SIDE_PANEL",
      documentUrl: resolveExtPageUrl("page.html?kind=sidebar"),
      windowId: privateWinId,
      incognito: true,
    }),
  ].sort(byWindowId);

  info("Test getContexts error on unsupported getContexts filter property");
  extension.sendMessage("background-get-contexts-invalid-params");
  await extension.awaitMessage("background-get-contexts-invalid-params:done");

  info("Test getContexts with a valid empty filter");
  let actual = await getGetContextsResults({ filter: {}, sortBy: byWindowId });

  assertGetContextsResult(
    actual,
    expected,
    "Got the expected results from runtime.getContexts (with an empty filter)"
  );

  for (const ctx of actual) {
    info(`Validate contextId for context ${ctx.contextType} ${ctx.contextId}`);
    assertValidContextId(ctx.contextId);
  }

  await BrowserTestUtils.withNewTab(
    {
      gBrowser: secondWin.gBrowser,
      url: resolveExtPageUrl("page.html?kind=tab"),
    },
    async browser => {
      info("Wait the extension page to be fully loaded in the new tab");
      await extension.awaitMessage("tab-loaded");

      const tabId = tabTracker.getBrowserData(browser).tabId;

      const expectedTabContext = getExpectedExtensionContext({
        contextType: "TAB",
        documentUrl: resolveExtPageUrl("page.html?kind=tab"),
        windowId: secondWinId,
        tabId,
        incognito: false,
      });

      info("Test getContexts with contextTypes TAB filter");
      let actual = await getGetContextsResults({
        filter: { contextTypes: ["TAB"] },
      });
      assertGetContextsResult(
        actual,
        [expectedTabContext],
        "Got the expected results from runtime.getContexts (with contextTypes TAB filter)"
      );
      assertValidContextId(actual[0].contextId);
      const initialTabContextId = actual[0].contextId;

      info("Test getContexts with contextTypes TabIds filter");
      actual = await getGetContextsResults({
        filter: { tabIds: [tabId] },
      });
      assertGetContextsResult(
        actual,
        [expectedTabContext],
        "Got the expected results from runtime.getContexts (with tabIds filter)"
      );

      info("Test getContexts with contextTypes WindowIds filter");
      actual = await getGetContextsResults({
        filter: { windowIds: [secondWinId] },
        sortBy: byTabId,
      });
      assertGetContextsResult(
        actual,
        [
          expectedTabContext,
          expected.find(it => it.windowId === secondWinId),
        ].sort(byTabId),
        "Got the expected results from runtime.getContexts (with windowIds filter)"
      );

      info("Test getContexts after navigating the tab");
      const newTabURL = resolveExtPageUrl("page.html?kind=tab&navigated=true");
      browser.loadURI(Services.io.newURI(newTabURL), {
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });
      await extension.awaitMessage("tab-loaded");

      actual = await getGetContextsResults({
        filter: {
          contextTypes: ["TAB"],
          windowIds: [secondWinId],
        },
      });
      Assert.equal(actual.length, 1, "Expect 1 tab extension context");
      Assert.equal(
        actual[0].documentUrl,
        newTabURL,
        "Expect documentUrl to match the new loaded url"
      );
      Assert.equal(actual[0].frameId, 0, "Got expected frameId");
      Assert.equal(
        actual[0].tabId,
        expectedTabContext.tabId,
        "Got expected tabId"
      );
      Assert.notEqual(
        actual[0].contextId,
        initialTabContextId,
        "Expect contextId to change on navigated tab"
      );
    }
  );

  await triggerActionPopup(extension, privateWin, async () => {
    info("Wait the extension page to be fully loaded in the action popup");
    await extension.awaitMessage("action-loaded");

    const expectedPopupContext = getExpectedExtensionContext({
      contextType: "POPUP",
      documentUrl: resolveExtPageUrl("page.html?kind=action"),
      windowId: privateWinId,
      tabId: -1,
      incognito: true,
    });

    info("Test getContexts with contextTypes POPUP filter");
    let actual = await getGetContextsResults({
      filter: {
        contextTypes: ["POPUP"],
      },
    });
    assertGetContextsResult(
      actual,
      [expectedPopupContext],
      "Got the expected results from runtime.getContexts (with contextTypes POPUP filter)"
    );

    info("Test getContexts with incognito true filter");
    actual = await getGetContextsResults({
      filter: { incognito: true },
      sortBy: byContextType,
    });
    assertGetContextsResult(
      actual.sort(byContextType),
      [expectedPopupContext, ...expected.filter(it => it.incognito)].sort(
        byContextType
      ),
      "Got the expected results from runtime.getContexts (with contextTypes incognito true filter)"
    );
  });

  info("Test getContexts with existing background iframes");
  extension.sendMessage(
    `background-create-iframe`,
    resolveExtPageUrl("page.html?kind=background-subframe")
  );
  await extension.awaitMessage(`background-subframe-loaded`);

  actual = await getGetContextsResults({
    filter: { contextTypes: ["BACKGROUND"] },
  });

  Assert.equal(
    actual.length,
    2,
    "Expect 2 background extension contexts to be found"
  );
  const bgTopFrame = actual.find(
    it => it.documentUrl === resolveExtPageUrl("page.html?kind=background")
  );
  const bgSubFrame = actual.find(
    it =>
      it.documentUrl === resolveExtPageUrl("page.html?kind=background-subframe")
  );

  assertValidContextId(bgTopFrame.contextId);
  assertValidContextId(bgSubFrame.contextId);
  Assert.notEqual(
    bgTopFrame.contextId,
    bgSubFrame.contextId,
    "Expect background top and sub frame to have different contextIds"
  );

  Assert.equal(
    bgTopFrame.frameId,
    0,
    "Expect background top frame to have frameId 0"
  );
  ok(
    typeof bgSubFrame.frameId === "number" && bgSubFrame.frameId > 0,
    "Expect background sub frame to have a non zero frameId"
  );
  Assert.equal(
    bgSubFrame.windowId,
    bgSubFrame.windowId,
    "Expect background top frame to have same windowId as the top frame"
  );
  Assert.equal(
    bgSubFrame.tabId,
    bgTopFrame.tabId,
    "Expect background top frame to have same tabId as the top frame"
  );

  info("Test getContexts with existing sidebars iframes");
  extension.sendMessage(
    `sidebar-create-iframe`,
    resolveExtPageUrl("page.html?kind=sidebar-subframe")
  );
  // Expect 3 sidebar subframe to be created.
  await extension.awaitMessage(`sidebar-subframe-loaded`);
  await extension.awaitMessage(`sidebar-subframe-loaded`);
  await extension.awaitMessage(`sidebar-subframe-loaded`);

  actual = await getGetContextsResults({
    filter: { contextTypes: ["SIDE_PANEL"], windowIds: [firstWinId] },
    sortBy: byFrameId,
  });
  Assert.equal(
    actual.length,
    2,
    "Expect 2 sidebar extension contexts to be found for the first window"
  );
  // NOTE: we have already asserted that the sidebar top level frame has frameId 0.
  Assert.greater(
    actual.find(
      it =>
        it.documentUrl == resolveExtPageUrl("page.html?kind=sidebar-subframe")
    )?.frameId,
    0,
    "Expect sidebar subframe to have the expected frameId"
  );
  // NOTE: we have already asserted that the top level frame have tabId -1.
  Assert.equal(
    actual[0].tabId,
    actual[1].tabId,
    "Expect iframe and top level sidebar frame to have the same tabId"
  );

  actual = await getGetContextsResults({
    filter: { contextTypes: ["SIDE_PANEL"], windowIds: [secondWinId] },
    sortBy: byFrameId,
  });
  Assert.equal(
    actual.length,
    2,
    "Expect 2 sidebar extension contexts to be found for the second window"
  );

  actual = await getGetContextsResults({
    filter: { contextTypes: ["SIDE_PANEL"], incognito: true },
  });
  Assert.equal(
    actual.length,
    2,
    "Expect 2 sidebar extension contexts to be found for private windows"
  );

  info("Test getContexts after background history push state");
  let pushStateURLPath = "/page.html?kind=background&pushedState=1";
  extension.sendMessage("background-history-push-state", pushStateURLPath);
  await extension.awaitMessage("background-history-push-state:done");

  actual = await getGetContextsResults({
    filter: { contextTypes: ["BACKGROUND"], frameIds: [0] },
  });
  Assert.equal(
    actual.length,
    1,
    "Expect 1 top level background context to be found"
  );
  Assert.equal(
    actual[0].contextId,
    bgTopFrame.contextId,
    "Expect top level background contextId to NOT be changed"
  );
  Assert.equal(
    actual[0].documentUrl,
    resolveExtPageUrl(pushStateURLPath),
    "Expect top level background documentUrl to change due to history.pushState"
  );

  await BrowserTestUtils.closeWindow(privateWin);
  await BrowserTestUtils.closeWindow(secondWin);

  info(
    "Test getContexts after opening an options page embedded in an about:addons tab"
  );
  await BrowserTestUtils.withNewTab("about:addons", async () => {
    extension.sendMessage("background-open-options-page");
    await extension.awaitMessage("options-loaded");
    const { selectedBrowser } = firstWin.gBrowser;
    Assert.equal(
      selectedBrowser.currentURI.spec,
      "about:addons",
      "Expect an about:addons tab to be current active tab"
    );
    let optionsTabId = tabTracker.getBrowserData(selectedBrowser).tabId;

    actual = await getGetContextsResults({
      filter: { windowIds: [firstWinId], tabIds: [optionsTabId] },
    });
    assertGetContextsResult(
      actual,
      [
        getExpectedExtensionContext({
          contextType: "TAB",
          documentUrl: resolveExtPageUrl("page.html?kind=options"),
          windowId: firstWinId,
          tabId: optionsTabId,
        }),
      ],
      "Got the expected results from runtime.getContexts for an options_page"
    );
  });

  info("Test getContexts with an extension devtools page and devtools panel");
  await BrowserTestUtils.withNewTab("https://example.com", async () => {
    const tab = gBrowser.selectedTab;
    const toolbox = await openToolboxForTab(tab);

    info("Wait for the devtools page to be loaded");
    await extension.awaitMessage("devtools-page-loaded");

    Assert.equal(
      toolbox.getAdditionalTools()?.length,
      1,
      "Expecte extension devtools panel to be registered"
    );
    let panelId = toolbox.getAdditionalTools()[0].id;
    await gDevTools.showToolboxForTab(tab, { toolId: panelId });

    info("Wait for the devtools panel to be loaded");
    await extension.awaitMessage("devtools-panel-loaded");

    actual = await getGetContextsResults({ filter: {} });
    // Expect the backgrond page and its subframe to still be returned.
    Assert.equal(
      actual.filter(ctx => ctx.contextType === "BACKGROUND").length,
      2,
      "Expect the existing 2 background context types"
    );
    // Expect the side_panel page and its subframe to still be returned.
    Assert.equal(
      actual.filter(ctx => ctx.contextType === "SIDE_PANEL").length,
      2,
      "Expect the existing 2 side_panel context types"
    );
    // Expect no other context to be listed in the getContexts results
    // (devtools page and panel are currently expected to not be
    // part of getContexts results).
    Assert.deepEqual(
      actual.filter(
        ctx => !["BACKGROUND", "SIDE_PANEL"].includes(ctx.contextType)
      ),
      [],
      "DevTools page and panel are not listed in getContexts results"
    );

    await closeToolboxForTab(gBrowser.selectedTab);
  });

  await extension.unload();
});
