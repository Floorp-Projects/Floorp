/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that toolbox remains open when opening and closing RDM.

const TEST_URL = "http://example.com/";

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

let checkServerConnectionCount = Task.async(function* (browser, expected) {
  let conns = yield getServerConnections(browser);
  is(conns.length || 0, expected, "Server connection count");
});

let checkToolbox = Task.async(function* (tab, location) {
  let target = TargetFactory.forTab(tab);
  ok(!!gDevTools.getToolbox(target), `Toolbox exists ${location}`);
});

add_task(function* () {
  let tab = yield addTab(TEST_URL);

  // Open toolbox outside RDM
  {
    // 0: No DevTools connections yet
    yield checkServerConnectionCount(tab.linkedBrowser, 0);
    let { toolbox } = yield openInspector();
    // 2: One for each tab (starting tab plus the one we opened).  Only one truly needed,
    //    but calling listTabs will create one for each tab.  `registerTestActor` calls
    //    this, triggering the extra tab's actor to be made.
    yield checkServerConnectionCount(tab.linkedBrowser, 2);
    yield checkToolbox(tab, "outside RDM");
    let { ui } = yield openRDM(tab);
    // 3: RDM UI uses an extra connection
    yield checkServerConnectionCount(ui.getViewportBrowser(), 3);
    yield checkToolbox(tab, "after opening RDM");
    yield closeRDM(tab);
    // 2: RDM UI closed, back to one for each tab
    yield checkServerConnectionCount(tab.linkedBrowser, 2);
    yield checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
    yield toolbox.destroy();
    // 0: All DevTools usage closed
    yield checkServerConnectionCount(tab.linkedBrowser, 0);
  }

  // Open toolbox inside RDM
  {
    // 0: No DevTools connections yet
    yield checkServerConnectionCount(tab.linkedBrowser, 0);
    let { ui } = yield openRDM(tab);
    // 1: RDM UI uses an extra connection
    yield checkServerConnectionCount(ui.getViewportBrowser(), 1);
    let { toolbox } = yield openInspector();
    // 3: One for each tab (starting tab plus the one we opened).  Only one truly needed,
    //    but calling listTabs will create one for each tab.  `registerTestActor` calls
    //    this, triggering the extra tab's actor to be made.
    yield checkServerConnectionCount(ui.getViewportBrowser(), 3);
    yield checkToolbox(tab, ui.getViewportBrowser(), "inside RDM");
    yield closeRDM(tab);
    // 2: RDM UI closed, back to one for each tab
    yield checkServerConnectionCount(tab.linkedBrowser, 2);
    yield checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
    yield toolbox.destroy();
    // 0: All DevTools usage closed
    yield checkServerConnectionCount(tab.linkedBrowser, 0);
  }

  yield removeTab(tab);
});
