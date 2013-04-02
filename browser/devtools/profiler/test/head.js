/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let temp = {};
const PROFILER_ENABLED = "devtools.profiler.enabled";
const REMOTE_ENABLED = "devtools.debugger.remote-enabled";

Cu.import("resource:///modules/devtools/Target.jsm", temp);
let TargetFactory = temp.TargetFactory;

Cu.import("resource:///modules/devtools/gDevTools.jsm", temp);
let gDevTools = temp.gDevTools;

Cu.import("resource://gre/modules/devtools/dbg-server.jsm", temp);
let DebuggerServer = temp.DebuggerServer;

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

registerCleanupFunction(function () {
  helpers = null;
  Services.prefs.clearUserPref(PROFILER_ENABLED);
  Services.prefs.clearUserPref(REMOTE_ENABLED);
  DebuggerServer.destroy();
});

function getProfileInternals(uid) {
  let profile = (uid != null) ? gPanel.profiles.get(uid) : gPanel.activeProfile;
  let win = profile.iframe.contentWindow;
  let doc = win.document;

  return [win, doc];
}

function loadTab(url, callback) {
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  content.location.assign(url);

  let browser = gBrowser.getBrowserForTab(tab);
  if (browser.contentDocument.readyState === "complete") {
    callback(tab, browser);
    return;
  }

  let onLoad = function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    callback(tab, browser);
  };

  browser.addEventListener("load", onLoad, true);
}

function openProfiler(tab, callback) {
  let target = TargetFactory.forTab(tab);
  gDevTools.showToolbox(target, "jsprofiler").then(callback);
}

function closeProfiler(tab, callback) {
  let target = TargetFactory.forTab(tab);
  let toolbox = gDevTools.getToolbox(target);
  toolbox.destroy().then(callback);
}

function setUp(url, callback=function(){}) {
  Services.prefs.setBoolPref(PROFILER_ENABLED, true);

  loadTab(url, function onTabLoad(tab, browser) {
    openProfiler(tab, function onProfilerOpen() {
      let target = TargetFactory.forTab(tab);
      let panel = gDevTools.getToolbox(target).getPanel("jsprofiler");
      callback(tab, browser, panel);
    });
  });
}

function tearDown(tab, callback=function(){}) {
  closeProfiler(tab, function onProfilerClose() {
    callback();

    while (gBrowser.tabs.length > 1) {
      gBrowser.removeCurrentTab();
    }

    finish();
  });
}
