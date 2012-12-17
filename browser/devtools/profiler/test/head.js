/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let temp = {};
const PROFILER_ENABLED = "devtools.profiler.enabled";

Cu.import("resource:///modules/devtools/Target.jsm", temp);
let TargetFactory = temp.TargetFactory;

Cu.import("resource:///modules/devtools/gDevTools.jsm", temp);
let gDevTools = temp.gDevTools;

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
  let panel = gDevTools.getToolbox(target).getPanel("jsprofiler");
  panel.once("destroyed", callback);

  gDevTools.closeToolbox(target);
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
    Services.prefs.setBoolPref(PROFILER_ENABLED, false);
  });
}
