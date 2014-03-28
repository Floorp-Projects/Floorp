/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure the add-on actor can see loaded JS Modules from an add-on

const ADDON4_URL = EXAMPLE_URL + "addon4.xpi";

let gAddon, gClient, gThreadClient, gDebugger, gSources, gTitle;

function onMessage(event) {
  try {
    let json = JSON.parse(event.data);
    switch (json.name) {
      case "toolbox-title":
        gTitle = json.data.value;
        break;
    }
  } catch(e) { Cu.reportError(e); }
}

function test() {
  Task.spawn(function () {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init(() => true);
      DebuggerServer.addBrowserActors();
    }

    gBrowser.selectedTab = gBrowser.addTab();
    let iframe = document.createElement("iframe");
    document.documentElement.appendChild(iframe);

    window.addEventListener("message", onMessage);

    let transport = DebuggerServer.connectPipe();
    gClient = new DebuggerClient(transport);

    let connected = promise.defer();
    gClient.connect(connected.resolve);
    yield connected.promise;

    yield installAddon();
    let debuggerPanel = yield initAddonDebugger(gClient, ADDON4_URL, iframe);
    gDebugger = debuggerPanel.panelWin;
    gThreadClient = gDebugger.gThreadClient;
    gSources = gDebugger.DebuggerView.Sources;

    yield testSources(false);

    Cu.import("resource://browser_dbg_addon4/test2.jsm", {});

    yield testSources(true);

    Cu.unload("resource://browser_dbg_addon4/test2.jsm");

    yield uninstallAddon();
    yield closeConnection();
    yield debuggerPanel._toolbox.destroy();
    iframe.remove();
    window.removeEventListener("message", onMessage);
    finish();
  });
}

function installAddon () {
  return addAddon(ADDON4_URL).then(aAddon => {
    gAddon = aAddon;
  });
}

function testSources(expectSecondModule) {
  let deferred = promise.defer();
  let foundAddonModule = false;
  let foundAddonModule2 = false;
  let foundAddonBootstrap = false;

  gThreadClient.getSources(({sources}) => {
    ok(sources.length, "retrieved sources");

    for (let source of sources) {
      let url = source.url.split(" -> ").pop();
      let { label, group } = gSources.getItemByValue(source.url).attachment;

      if (url.indexOf("resource://browser_dbg_addon4/test.jsm") === 0) {
        is(label, "test.jsm", "correct label for addon code");
        is(group, "resource://browser_dbg_addon4", "addon module is in its own group");
        foundAddonModule = true;
      } else if (url.indexOf("resource://browser_dbg_addon4/test2.jsm") === 0) {
        is(label, "test2.jsm", "correct label for addon code");
        is(group, "resource://browser_dbg_addon4", "addon module is in its own group");
        foundAddonModule2 = true;
      } else if (url.endsWith("/browser_dbg_addon4@tests.mozilla.org.xpi!/bootstrap.js")) {
        is(label, "bootstrap.js", "correct label for bootstrap code");
        is(group, "jar:", "addon bootstrap script is in its own group");
        foundAddonBootstrap = true;
      } else {
        ok(false, "Saw an unexpected source: " + url);
      }
    }

    ok(foundAddonModule, "found JS module for the addon in the list");
    is(foundAddonModule2, expectSecondModule, "saw the second addon module");
    ok(foundAddonBootstrap, "found bootstrap script for the addon in the list");

    is(gTitle, "Debugger - Test add-on with JS Modules", "Saw the right toolbox title.");

    let groups = gDebugger.document.querySelectorAll(".side-menu-widget-group-title .name");
    is(groups[0].value, "jar:", "Add-on bootstrap should be the first group");
    is(groups[1].value, "resource://browser_dbg_addon4", "Add-on code should be the second group");
    is(groups.length, 2, "Should be only two groups.");

    deferred.resolve();
  });

  return deferred.promise;
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
  while (gBrowser.tabs.length > 1)
    gBrowser.removeCurrentTab();
});
