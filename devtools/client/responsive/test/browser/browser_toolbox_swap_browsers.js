/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that toolbox remains open when opening and closing RDM.

const TEST_URL = "http://example.com/";

function getServerConnections(browser) {
  ok(browser.isRemoteBrowser, "Content browser is remote");
  return SpecialPowers.spawn(browser, [], async function () {
    const { require } = ChromeUtils.importESModule(
      "resource://devtools/shared/loader/Loader.sys.mjs"
    );
    const {
      DevToolsServer,
    } = require("resource://devtools/server/devtools-server.js");
    if (!DevToolsServer._connections) {
      return 0;
    }
    return Object.getOwnPropertyNames(DevToolsServer._connections);
  });
}

const checkServerConnectionCount = async function (browser, expected, msg) {
  const conns = await getServerConnections(browser);
  is(conns.length || 0, expected, "Server connection count: " + msg);
};

const checkToolbox = function (tab, location) {
  const toolbox = gDevTools.getToolboxForTab(tab);
  ok(!!toolbox, `Toolbox exists ${location}`);
};

addRDMTask(
  "",
  async function () {
    const tab = await addTab(TEST_URL);

    const tabsInDifferentProcesses =
      E10S_MULTI_ENABLED &&
      gBrowser.tabs[0].linkedBrowser.frameLoader.childID !=
        gBrowser.tabs[1].linkedBrowser.frameLoader.childID;

    info("Open toolbox outside RDM");
    {
      // 0: No DevTools connections yet
      await checkServerConnectionCount(
        tab.linkedBrowser,
        0,
        "0: No DevTools connections yet"
      );
      const { toolbox } = await openInspector();
      if (tabsInDifferentProcesses) {
        // 1: Two tabs open, but only one per content process
        await checkServerConnectionCount(
          tab.linkedBrowser,
          1,
          "1: Two tabs open, but only one per content process"
        );
      } else {
        // 2: One for each tab (starting tab plus the one we opened)
        await checkServerConnectionCount(
          tab.linkedBrowser,
          2,
          "2: One for each tab (starting tab plus the one we opened)"
        );
      }
      checkToolbox(tab, "outside RDM");
      const { ui } = await openRDM(tab);
      if (tabsInDifferentProcesses) {
        // 2: RDM UI adds an extra connection, 1 + 1 = 2
        await checkServerConnectionCount(
          ui.getViewportBrowser(),
          2,
          "2: RDM UI uses an extra connection"
        );
      } else {
        // 3: RDM UI adds an extra connection, 2 + 1 = 3
        await checkServerConnectionCount(
          ui.getViewportBrowser(),
          3,
          "3: RDM UI uses an extra connection"
        );
      }
      checkToolbox(tab, "after opening RDM");
      await closeRDM(tab);
      if (tabsInDifferentProcesses) {
        // 1: RDM UI closed, return to previous connection count
        await checkServerConnectionCount(
          tab.linkedBrowser,
          1,
          "1: RDM UI closed, return to previous connection count"
        );
      } else {
        // 2: RDM UI closed, return to previous connection count
        await checkServerConnectionCount(
          tab.linkedBrowser,
          2,
          "2: RDM UI closed, return to previous connection count"
        );
      }
      checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
      await toolbox.destroy();
      // 0: All DevTools usage closed
      await checkServerConnectionCount(
        tab.linkedBrowser,
        0,
        "0: All DevTools usage closed"
      );
    }

    info("Open toolbox inside RDM");
    {
      // 0: No DevTools connections yet
      await checkServerConnectionCount(
        tab.linkedBrowser,
        0,
        "0: No DevTools connections yet"
      );
      const { ui } = await openRDM(tab);
      // 1: RDM UI uses an extra connection
      await checkServerConnectionCount(
        ui.getViewportBrowser(),
        1,
        "1: RDM UI uses an extra connection"
      );
      const { toolbox } = await openInspector();
      if (tabsInDifferentProcesses) {
        // 2: Two tabs open, but only one per content process
        await checkServerConnectionCount(
          ui.getViewportBrowser(),
          2,
          "2: Two tabs open, but only one per content process"
        );
      } else {
        // 3: One for each tab (starting tab plus the one we opened)
        await checkServerConnectionCount(
          ui.getViewportBrowser(),
          3,
          "3: One for each tab (starting tab plus the one we opened)"
        );
      }
      checkToolbox(tab, ui.getViewportBrowser(), "inside RDM");
      await closeRDM(tab);
      if (tabsInDifferentProcesses) {
        // 1: RDM UI closed, one less connection
        await checkServerConnectionCount(
          tab.linkedBrowser,
          1,
          "1: RDM UI closed, one less connection"
        );
      } else {
        // 2: RDM UI closed, one less connection
        await checkServerConnectionCount(
          tab.linkedBrowser,
          2,
          "2: RDM UI closed, one less connection"
        );
      }
      checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
      await toolbox.destroy();
      // 0: All DevTools usage closed
      await checkServerConnectionCount(
        tab.linkedBrowser,
        0,
        "0: All DevTools usage closed"
      );
    }

    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);
