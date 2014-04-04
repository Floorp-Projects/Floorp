/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that only panels that are relevant to the addon debugger
// display in the toolbox

const ADDON3_URL = EXAMPLE_URL + "addon3.xpi";

let gAddon, gClient, gThreadClient, gDebugger, gSources;
let PREFS = [
  "devtools.canvasdebugger.enabled",
  "devtools.shadereditor.enabled",
  "devtools.profiler.enabled",
  "devtools.netmonitor.enabled"
];
function test() {
  Task.spawn(function () {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init(() => true);
      DebuggerServer.addBrowserActors();
    }

    // Store and enable all optional dev tools panels
    let originalPrefs = PREFS.map(pref => {
      let original = Services.prefs.getBoolPref(pref);
      Services.prefs.setBoolPref(pref, true)
      return original;
    });

    gBrowser.selectedTab = gBrowser.addTab();
    let iframe = document.createElement("iframe");
    document.documentElement.appendChild(iframe);

    let transport = DebuggerServer.connectPipe();
    gClient = new DebuggerClient(transport);

    let connected = promise.defer();
    gClient.connect(connected.resolve);
    yield connected.promise;

    yield installAddon();
    let debuggerPanel = yield initAddonDebugger(gClient, ADDON3_URL, iframe);
    gDebugger = debuggerPanel.panelWin;
    gThreadClient = gDebugger.gThreadClient;
    gSources = gDebugger.DebuggerView.Sources;

    testPanels(iframe);
    yield uninstallAddon();
    yield closeConnection();
    yield debuggerPanel._toolbox.destroy();
    iframe.remove();

    PREFS.forEach((pref, i) => Services.prefs.setBoolPref(pref, originalPrefs[i]));

    finish();
  });
}

function installAddon () {
  return addAddon(ADDON3_URL).then(aAddon => {
    gAddon = aAddon;
  });
}

function testPanels(frame) {
  let tabs = frame.contentDocument.getElementById("toolbox-tabs").children;
  let expectedTabs = ["options", "jsdebugger"];

  is(tabs.length, 2, "displaying only 2 tabs in addon debugger");
  Array.forEach(tabs, (tab, i) => {
    let toolName = expectedTabs[i];
    is(tab.getAttribute("toolid"), toolName, "displaying " + toolName);
  });
}

function uninstallAddon() {
  return removeAddon(gAddon);
}

function closeConnection () {
  let deferred = promise.defer();
  gClient.close(deferred.resolve);
  return deferred.promise;
}

registerCleanupFunction(function() {
  gClient = null;
  gAddon = null;
  gThreadClient = null;
  gDebugger = null;
  gSources = null;
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});
