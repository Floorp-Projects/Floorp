/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check regression when opening two tabs
 */

let { DebuggerServer } =
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } =
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { devtools } =
  Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

const TAB_URL_1 = "data:text/html;charset=utf-8,foo";
const TAB_URL_2 = "data:text/html;charset=utf-8,bar";

let gClient;
let gTab1, gTab2;
let gTabActor1, gTabActor2;

function test() {
  waitForExplicitFinish();

  if (!DebuggerServer.initialized) {
    DebuggerServer.init(() => true);
    DebuggerServer.addBrowserActors();
  }

  openTabs();
}

function openTabs() {
  // Open two tabs, select the second
  gTab1 = gBrowser.addTab(TAB_URL_1);
  gTab1.linkedBrowser.addEventListener("load", function onLoad1(evt) {
    gTab1.linkedBrowser.removeEventListener("load", onLoad1);

    gTab2 = gBrowser.selectedTab = gBrowser.addTab(TAB_URL_2);
    gTab2.linkedBrowser.addEventListener("load", function onLoad2(evt) {
      gTab2.linkedBrowser.removeEventListener("load", onLoad2);

      connect();
    }, true);
  }, true);
}

function connect() {
  // Connect to debugger server to fetch the two tab actors
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(() => {
    gClient.listTabs(response => {
      // Fetch the tab actors for each tab
      gTabActor1 = response.tabs.filter(a => a.url === TAB_URL_1)[0];
      gTabActor2 = response.tabs.filter(a => a.url === TAB_URL_2)[0];

      checkSelectedTabActor();
    });
  });
}

function checkSelectedTabActor() {
  // Send a naive request to the second tab actor
  // to check if it works
  gClient.request({ to: gTabActor2.consoleActor, type: "startListeners", listeners: [] }, aResponse => {
    ok("startedListeners" in aResponse, "Actor from the selected tab should respond to the request.");

    closeSecondTab();
  });
}

function closeSecondTab() {
  // Close the second tab, currently selected
  let container = gBrowser.tabContainer;
  container.addEventListener("TabClose", function onTabClose() {
    container.removeEventListener("TabClose", onTabClose);

    checkFirstTabActor();
  });
  gBrowser.removeTab(gTab2);
}

function checkFirstTabActor() {
  // then send a request to the first tab actor
  // to check if it still works
  gClient.request({ to: gTabActor1.consoleActor, type: "startListeners", listeners: [] }, aResponse => {
    ok("startedListeners" in aResponse, "Actor from the first tab should still respond.");

    cleanup();
  });
}

function cleanup() {
  let container = gBrowser.tabContainer;
  container.addEventListener("TabClose", function onTabClose() {
    container.removeEventListener("TabClose", onTabClose);

    gClient.close(finish);
  });
  gBrowser.removeTab(gTab1);
}
