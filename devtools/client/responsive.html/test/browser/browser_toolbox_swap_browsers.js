/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that toolbox remains open when opening and closing RDM.

const TEST_URL = "http://example.com/";

// Bug 1297575: Too slow for debug runs
requestLongerTimeout(2);

function getServerConnections(browser) {
  ok(browser.isRemoteBrowser, "Content browser is remote");
  return ContentTask.spawn(browser, {}, function* () {
    const Cu = Components.utils;
    const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
    const { DebuggerServer } = require("devtools/server/main");
    if (!DebuggerServer._connections) {
      return 0;
    }
    return Object.getOwnPropertyNames(DebuggerServer._connections);
  });
}

let checkServerConnectionCount = Task.async(function* (browser, expected, msg) {
  let conns = yield getServerConnections(browser);
  is(conns.length || 0, expected, "Server connection count: " + msg);
});

let checkToolbox = Task.async(function* (tab, location) {
  let target = TargetFactory.forTab(tab);
  ok(!!gDevTools.getToolbox(target), `Toolbox exists ${location}`);
});

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]]
  });
});

add_task(function* () {
  let tab = yield addTab(TEST_URL);

  let tabsInDifferentProcesses = E10S_MULTI_ENABLED &&
    (gBrowser.tabs[0].linkedBrowser.frameLoader.childID !=
     gBrowser.tabs[1].linkedBrowser.frameLoader.childID);

  info("Open toolbox outside RDM");
  {
    // 0: No DevTools connections yet
    yield checkServerConnectionCount(tab.linkedBrowser, 0,
      "0: No DevTools connections yet");
    let { toolbox } = yield openInspector();
    if (tabsInDifferentProcesses) {
      // 1: Two tabs open, but only one per content process
      yield checkServerConnectionCount(tab.linkedBrowser, 1,
        "1: Two tabs open, but only one per content process");
    } else {
      // 2: One for each tab (starting tab plus the one we opened)
      yield checkServerConnectionCount(tab.linkedBrowser, 2,
        "2: One for each tab (starting tab plus the one we opened)");
    }
    yield checkToolbox(tab, "outside RDM");
    let { ui } = yield openRDM(tab);
    if (tabsInDifferentProcesses) {
      // 2: RDM UI adds an extra connection, 1 + 1 = 2
      yield checkServerConnectionCount(ui.getViewportBrowser(), 2,
        "2: RDM UI uses an extra connection");
    } else {
      // 3: RDM UI adds an extra connection, 2 + 1 = 3
      yield checkServerConnectionCount(ui.getViewportBrowser(), 3,
        "3: RDM UI uses an extra connection");
    }
    yield checkToolbox(tab, "after opening RDM");
    yield closeRDM(tab);
    if (tabsInDifferentProcesses) {
      // 1: RDM UI closed, return to previous connection count
      yield checkServerConnectionCount(tab.linkedBrowser, 1,
        "1: RDM UI closed, return to previous connection count");
    } else {
      // 2: RDM UI closed, return to previous connection count
      yield checkServerConnectionCount(tab.linkedBrowser, 2,
        "2: RDM UI closed, return to previous connection count");
    }
    yield checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
    yield toolbox.destroy();
    // 0: All DevTools usage closed
    yield checkServerConnectionCount(tab.linkedBrowser, 0,
      "0: All DevTools usage closed");
  }

  info("Open toolbox inside RDM");
  {
    // 0: No DevTools connections yet
    yield checkServerConnectionCount(tab.linkedBrowser, 0,
      "0: No DevTools connections yet");
    let { ui } = yield openRDM(tab);
    // 1: RDM UI uses an extra connection
    yield checkServerConnectionCount(ui.getViewportBrowser(), 1,
      "1: RDM UI uses an extra connection");
    let { toolbox } = yield openInspector();
    if (tabsInDifferentProcesses) {
      // 2: Two tabs open, but only one per content process
      yield checkServerConnectionCount(ui.getViewportBrowser(), 2,
        "2: Two tabs open, but only one per content process");
    } else {
      // 3: One for each tab (starting tab plus the one we opened)
      yield checkServerConnectionCount(ui.getViewportBrowser(), 3,
        "3: One for each tab (starting tab plus the one we opened)");
    }
    yield checkToolbox(tab, ui.getViewportBrowser(), "inside RDM");
    yield closeRDM(tab);
    if (tabsInDifferentProcesses) {
      // 1: RDM UI closed, one less connection
      yield checkServerConnectionCount(tab.linkedBrowser, 1,
        "1: RDM UI closed, one less connection");
    } else {
      // 2: RDM UI closed, one less connection
      yield checkServerConnectionCount(tab.linkedBrowser, 2,
        "2: RDM UI closed, one less connection");
    }
    yield checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
    yield toolbox.destroy();
    // 0: All DevTools usage closed
    yield checkServerConnectionCount(tab.linkedBrowser, 0,
      "0: All DevTools usage closed");
  }

  yield removeTab(tab);
});
