/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let gTab1, gTab2, gTab3, gTab4;
let gPane1, gPane2;
let gNbox;

function test() {
  gNbox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);

  testTab1(function() {
    testTab2(function() {
      testTab3(function() {
        testTab4(function() {
          lastTest(function() {
            cleanup(function() {
              finish();
            });
          });
        });
      });
    });
  });
}

function testTab1(callback) {
  gTab1 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = gTab1;
    gNbox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification.");
    ok(!DebuggerUI.getDebugger(),
      "Shouldn't have a debugger pane for this tab yet.");

    info("Toggling a debugger (1).");

    gPane1 = DebuggerUI.toggleDebugger();
    ok(gPane1, "toggleDebugger() should return a pane.");
    is(gPane1.ownerTab, gTab1, "Incorrect tab owner.");

    is(DebuggerUI.getDebugger(), gPane1,
      "getDebugger() should return the same pane as toggleDebugger().");

    wait_for_connect_and_resume(function dbgLoaded() {
      info("First debugger has finished loading correctly.");
      executeSoon(function() {
        callback();
      });
    }, true);
  });
}

function testTab2(callback) {
  gTab2 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = gTab2;
    gNbox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification yet.");
    ok(DebuggerUI.getDebugger(),
      "Should already have a debugger pane for another tab.");

    gNbox.addEventListener("AlertActive", function active() {
      gNbox.removeEventListener("AlertActive", active, true);
      executeSoon(function() {
        ok(gPane2, "toggleDebugger() should always return a pane.");
        is(gPane2.ownerTab, gTab1, "Incorrect tab owner.");

        is(DebuggerUI.getDebugger(), gPane1,
          "getDebugger() should return the same pane as the first call to toggleDebugger().");
        is(DebuggerUI.getDebugger(), gPane2,
          "getDebugger() should return the same pane as the second call to toggleDebugger().");

        info("Second debugger has not loaded.");

        let notification = gNbox.getNotificationWithValue("debugger-tab-switch");
        ok(gNbox.currentNotification, "Should have a tab switch notification.");
        is(gNbox.currentNotification, notification, "Incorrect current notification.");

        info("Notification will be simply closed.");
        notification.close();

        executeSoon(function() {
          callback();
        });
      });
    }, true);

    info("Toggling a debugger (2).");

    gPane2 = DebuggerUI.toggleDebugger();
  });
}

function testTab3(callback) {
  gTab3 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = gTab3;
    gNbox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification.");
    ok(DebuggerUI.getDebugger(),
      "Should already have a debugger pane for another tab.");

    gNbox.addEventListener("AlertActive", function active() {
      gNbox.removeEventListener("AlertActive", active, true);
      executeSoon(function() {
        ok(gPane2, "toggleDebugger() should always return a pane.");
        is(gPane2.ownerTab, gTab1, "Incorrect tab owner.");

        is(DebuggerUI.getDebugger(), gPane1,
          "getDebugger() should return the same pane as the first call to toggleDebugger().");
        is(DebuggerUI.getDebugger(), gPane2,
          "getDebugger() should return the same pane as the second call to toggleDebugger().");

        info("Second debugger has not loaded.");

        let notification = gNbox.getNotificationWithValue("debugger-tab-switch");
        ok(gNbox.currentNotification, "Should have a tab switch notification.");
        is(gNbox.currentNotification, notification, "Incorrect current notification.");

        gBrowser.tabContainer.addEventListener("TabSelect", function tabSelect() {
          gBrowser.tabContainer.removeEventListener("TabSelect", tabSelect, true);
          executeSoon(function() {
            callback();
          });
        }, true);

        let buttonSwitch = notification.querySelectorAll("button")[0];
        buttonSwitch.focus();
        EventUtils.sendKey("SPACE");
        info("The switch button on the notification was pressed.");
      });
    }, true);

    info("Toggling a debugger (3).");

    gPane2 = DebuggerUI.toggleDebugger();
  });
}

function testTab4(callback) {
  is(gBrowser.selectedTab, gTab1,
    "Should've switched to the first debugged tab.");

  gTab4 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = gTab4;
    gNbox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification.");
    ok(DebuggerUI.getDebugger(),
      "Should already have a debugger pane for another tab.");

    gNbox.addEventListener("AlertActive", function active() {
      gNbox.removeEventListener("AlertActive", active, true);
      executeSoon(function() {
        ok(gPane2, "toggleDebugger() should always return a pane.");
        is(gPane2.ownerTab, gTab1, "Incorrect tab owner.");

        is(DebuggerUI.getDebugger(), gPane1,
          "getDebugger() should return the same pane as the first call to toggleDebugger().");
        is(DebuggerUI.getDebugger(), gPane2,
          "getDebugger() should return the same pane as the second call to toggleDebugger().");

        info("Second debugger has not loaded.");

        let notification = gNbox.getNotificationWithValue("debugger-tab-switch");
        ok(gNbox.currentNotification, "Should have a tab switch notification.");
        is(gNbox.currentNotification, notification, "Incorrect current notification.");

        let buttonOpen = notification.querySelectorAll("button")[1];
        buttonOpen.focus();
        EventUtils.sendKey("SPACE");
        info("The open button on the notification was pressed.");

        wait_for_connect_and_resume(function() {
          callback();
        });
      });
    }, true);

    info("Toggling a debugger (4).");

    gPane2 = DebuggerUI.toggleDebugger();
  });
}

function lastTest(callback) {
  isnot(gBrowser.selectedTab, gTab1,
    "Shouldn't have switched to the first debugged tab.");
  is(gBrowser.selectedTab, gTab4,
    "Should currently be in the fourth tab.");
  is(DebuggerUI.getDebugger().ownerTab, gTab4,
    "The debugger should be open for the fourth tab.");

  is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
    "Shouldn't have a tab switch notification.");

  info("Second debugger has loaded.");

  executeSoon(function() {
    callback();
  });
}

function cleanup(callback) {

  gPane1 = null;
  gPane2 = null;
  gNbox = null;

  closeDebuggerAndFinish(false, function() {
    removeTab(gTab1);
    removeTab(gTab2);
    removeTab(gTab3);
    removeTab(gTab4);
    gTab1 = null;
    gTab2 = null;
    gTab3 = null;
    gTab4 = null;

    callback();
  });
}
