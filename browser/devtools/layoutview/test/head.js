/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Task.jsm");

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const promise = devtools.require("sdk/core/promise");
let TargetFactory = devtools.TargetFactory;

Services.prefs.setBoolPref("devtools.inspector.sidebarOpen", true);
Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
  gDevTools.testing = false;
});

// All tests are async in general
waitForExplicitFinish();

function loadTab(url) {
  let deferred = promise.defer();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(function() {
      deferred.resolve(content);
    }, content);
  }, true);

  content.location = url;

  return deferred.promise;
}

function selectNode(aNode) {
  info("selecting node");
  let onSelect = inspector.once("layoutview-updated");
  inspector.selection.setNode(aNode, "test");
  return onSelect.then(() => {
    let view = inspector.sidebar.getWindowForTab("layoutview");
    ok(!!view.layoutview, "LayoutView document is alive.");

    return view;
  });
}

function waitForUpdate() {
  return inspector.once("layoutview-updated");
}

function asyncTest(testfunc) {
  return Task.async(function*() {
    let initialTab = gBrowser.selectedTab;

    yield testfunc();

    // Remove all tabs except for the initial tab. This is basically
    // gBrowser.removeAllTabsBut without the animation
    let tabs = gBrowser.visibleTabs;
    gBrowser.selectedTab = initialTab;
    for (let i = tabs.length - 1; i >= 0; i--) {
      if (tabs[i] != initialTab)
        gBrowser.removeTab(tabs[i]);
    }

    finish();
  });
}

var TESTS = [];

function addTest(message, func) {
  TESTS.push([message, Task.async(func)])
}

var runTests = Task.async(function*() {
  for (let [message, test] of TESTS) {
    info(message);
    yield test();
  }
});
