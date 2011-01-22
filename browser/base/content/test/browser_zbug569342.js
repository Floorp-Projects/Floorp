/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gTab = null;

function load(url, cb) {
  gTab = gBrowser.addTab(url);
  gBrowser.addEventListener("load", function (event) {
    gBrowser.removeEventListener("load", arguments.callee, true);
    // Trigger onLocationChange by switching tabs.
    gBrowser.selectedTab = gTab;
    cb();
  }, true);
}

function test() {
  waitForExplicitFinish();

  ok(gFindBar.hidden, "Find bar should not be visible");

  run_test_1();
}

function run_test_1() {
  load("about:config", function() {
    ok(gFindBar.hidden, "Find bar should not be visible");
    EventUtils.synthesizeKey("/", {}, gTab.linkedBrowser.contentWindow);
    ok(gFindBar.hidden, "Find bar should not be visible");
    EventUtils.synthesizeKey("f", { accelKey: true });
    ok(gFindBar.hidden, "Find bar should not be visible");
    ok(document.getElementById("cmd_find").getAttribute("disabled"),
       "Find command should be disabled");

    gBrowser.removeTab(gTab);
    run_test_2();
  });
}

function run_test_2() {
  load("about:addons", function() {
    ok(gFindBar.hidden, "Find bar should not be visible");
    EventUtils.synthesizeKey("/", {}, gTab.linkedBrowser.contentWindow);
    ok(gFindBar.hidden, "Find bar should not be visible");
    EventUtils.synthesizeKey("f", { accelKey: true });
    ok(gFindBar.hidden, "Find bar should not be visible");
    ok(document.getElementById("cmd_find").getAttribute("disabled"),
       "Find command should be disabled");

    gBrowser.removeTab(gTab);
    run_test_3();
  });
}

function run_test_3() {
  load("about:blank", function() {
    ok(!document.getElementById("cmd_find").getAttribute("disabled"),
       "Find command should not be disabled");

    gBrowser.removeTab(gTab);
    finish();
  });
}