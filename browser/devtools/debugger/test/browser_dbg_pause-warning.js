/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gPane = null;
var gTab = null;
var gDebugger = null;
var gView = null;
var gToolbox = null;
var gTarget = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gView = gDebugger.DebuggerView;

    gTarget = TargetFactory.forTab(gBrowser.selectedTab);
    gToolbox = gDevTools.getToolbox(gTarget);

    testPause();
  });
}

function testPause() {
  let button = gDebugger.document.getElementById("resume");

  gDebugger.DebuggerController.activeThread.addOneTimeListener("paused", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      is(gDebugger.DebuggerController.activeThread.paused, true,
        "Debugger is paused.");

      ok(gTarget.isThreadPaused, "target.isThreadPaused has been updated");

      gToolbox.once("inspector-selected", testNotificationIsUp1);
      gToolbox.selectTool("inspector");
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

function testNotificationIsUp1() {
  let notificationBox = gToolbox.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue("inspector-script-paused");
  ok(notification, "Notification is present");
  gToolbox.once("jsdebugger-selected", testNotificationIsHidden);
  gToolbox.selectTool("jsdebugger");
}

function testNotificationIsHidden() {
  let notificationBox = gToolbox.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue("inspector-script-paused");
  ok(!notification, "Notification is hidden");
  gToolbox.once("inspector-selected", testNotificationIsUp2);
  gToolbox.selectTool("inspector");
}

function testNotificationIsUp2() {
  let notificationBox = gToolbox.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue("inspector-script-paused");
  ok(notification, "Notification is present");
  testResume();
}

function testResume() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      ok(!gTarget.isThreadPaused, "target.isThreadPaused has been updated");
      let notificationBox = gToolbox.getNotificationBox();
      let notification = notificationBox.getNotificationWithValue("inspector-script-paused");
      ok(!notification, "No notification once debugger resumed");

      closeDebuggerAndFinish();
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gView = null;
  gToolbox = null;
  gTarget = null;
});
