/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
Cu.import("resource://services-sync/main.js");

let gTests = [ {
  desc: "Makes sure the progress bar appears if firstSync pref is set",
  setup: function () {
    Services.prefs.setCharPref("services.sync.firstSync", "newAccount");
  },
  run: function () {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let progressBar = doc.getElementById("uploadProgressBar");

    let win = doc.defaultView;
    isnot(win.getComputedStyle(progressBar).display, "none", "progress bar should be visible");
    executeSoon(runNextTest);
  }
},

{
  desc: "Makes sure the progress bar is hidden if firstSync pref is not set",
  setup: function () {
    Services.prefs.clearUserPref("services.sync.firstSync");
    is(Services.prefs.getPrefType("services.sync.firstSync"),
       Ci.nsIPrefBranch.PREF_INVALID, "pref DNE" );
  },
  run: function () {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let progressBar = doc.getElementById("uploadProgressBar");

    let win = doc.defaultView;
    is(win.getComputedStyle(progressBar).display, "none",
       "progress bar should not be visible");
    executeSoon(runNextTest);
  }
},
{
  desc: "Makes sure the observer updates are reflected in the progress bar",
  setup: function () {
  },
  run: function () {
     let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
     let progressBar = doc.getElementById("uploadProgressBar");

     Services.obs.notifyObservers(null, "weave:engine:sync:finish", null);
     Services.obs.notifyObservers(null, "weave:engine:sync:error", null);

     let received = progressBar.getAttribute("value");

     is(received, 2, "progress bar received correct notifications");
     executeSoon(runNextTest);
  }
},
{
  desc: "Close button should close tab",
  setup: function (){
  },
  run: function () {
    function onTabClosed() {
      ok(true, "received TabClose notification");
      gBrowser.tabContainer.removeEventListener("TabClose", onTabClosed, false);
      executeSoon(runNextTest);
    }
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let button = doc.getElementById('closeButton');
    let window = doc.defaultView;
    gBrowser.tabContainer.addEventListener("TabClose", onTabClosed, false);
    EventUtils.sendMouseEvent({type: "click"}, button, window);
  }
},
];

function test () {
  waitForExplicitFinish();
  executeSoon(runNextTest);
}

function runNextTest()
{
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }

  if (gTests.length) {
    let test = gTests.shift();
    info(test.desc);
    test.setup();
    let tab = gBrowser.selectedTab = gBrowser.addTab("about:sync-progress");
    tab.linkedBrowser.addEventListener("load", function (event) {
      tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      // Some part of the page is populated on load, so enqueue on it.
      executeSoon(test.run);
    }, true);
  }
  else {
    finish();
  }
}

