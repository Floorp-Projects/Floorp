/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gTab = null;

function cleanUp() {
  gBrowser.removeTab(gTab);
  finish();
}

// function borrowed from browser_bug386835.js
function load(tab, url, cb) {
  tab.linkedBrowser.addEventListener("load", function (event) {
    event.currentTarget.removeEventListener("load", arguments.callee, true);
    cb();
  }, true);
  tab.linkedBrowser.loadURI(url);
}

function test() {
  waitForExplicitFinish();

  gTab = gBrowser.selectedTab = gBrowser.addTab();
  ok(gFindBar.hidden, "Find bar should not be visible");

  run_test_1();
}

function run_test_1() {
  load(gTab, "about:config", function() {
    ok(gFindBar.hidden, "Find bar should not be visible");
    EventUtils.synthesizeKey("/", {}, gTab.linkedBrowser.contentWindow);
    ok(gFindBar.hidden, "Find bar should not be visible");

    run_test_2();
  });
}

function run_test_2() {
  load(gTab, "about:addons", function() {
    ok(gFindBar.hidden, "Find bar should not be visible");
    EventUtils.synthesizeKey("/", {}, gTab.linkedBrowser.contentWindow);
    ok(gFindBar.hidden, "Find bar should not be visible");

    cleanUp();
  });
}
