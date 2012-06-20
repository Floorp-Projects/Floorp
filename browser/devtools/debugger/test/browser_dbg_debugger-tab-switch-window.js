/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let gInitialTab, gTab1, gTab2, gTab3, gTab4;
let gInitialWindow, gSecondWindow;
let gPane1, gPane2;
let gNbox;

/**
 * Tests that a debugger instance can't be opened in multiple windows at once,
 * and that on such an attempt a notification is shown, which can either switch
 * to the old debugger instance, or close the old instance to open a new one.
 */

function test() {
  gInitialWindow = window;
  gInitialTab = window.gBrowser.selectedTab;
  gNbox = gInitialWindow.gBrowser.getNotificationBox(gInitialWindow.gBrowser.selectedBrowser);

  testTab1_initialWindow(function() {
    testTab2_secondWindow(function() {
      testTab3_secondWindow(function() {
        testTab4_secondWindow(function() {
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

function testTab1_initialWindow(callback) {
  gTab1 = addTab(TAB1_URL, function() {
    gInitialWindow.gBrowser.selectedTab = gTab1;
    gNbox = gInitialWindow.gBrowser.getNotificationBox(gInitialWindow.gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification.");
    ok(!gInitialWindow.DebuggerUI.getDebugger(),
      "Shouldn't have a debugger pane for this tab yet.");

    info("Toggling a debugger (1).");

    gPane1 = gInitialWindow.DebuggerUI.toggleDebugger();
    ok(gPane1, "toggleDebugger() should return a pane.");
    is(gPane1.ownerTab, gTab1, "Incorrect tab owner.");

    is(gInitialWindow.DebuggerUI.getDebugger(), gPane1,
      "getDebugger() should return the same pane as toggleDebugger().");

    wait_for_connect_and_resume(function dbgLoaded() {
      info("First debugger has finished loading correctly.");
      executeSoon(function() {
        callback();
      });
    }, gInitialWindow);
  }, gInitialWindow);
}

function testTab2_secondWindow(callback) {
  gSecondWindow = addWindow();

  gTab2 = addTab(TAB1_URL, function() {
    gSecondWindow.gBrowser.selectedTab = gTab2;
    gNbox = gSecondWindow.gBrowser.getNotificationBox(gSecondWindow.gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification yet.");
    ok(gSecondWindow.DebuggerUI.findDebugger(),
      "Should already have a debugger pane for another tab.");

    gNbox.addEventListener("AlertActive", function active() {
      gNbox.removeEventListener("AlertActive", active, true);
      executeSoon(function() {
        ok(gPane2, "toggleDebugger() should always return a pane.");
        is(gPane2.ownerTab, gTab1, "Incorrect tab owner.");

        is(gSecondWindow.DebuggerUI.findDebugger(), gPane1,
          "findDebugger() should return the same pane as the first call to toggleDebugger().");
        is(gSecondWindow.DebuggerUI.findDebugger(), gPane2,
          "findDebugger() should return the same pane as the second call to toggleDebugger().");

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

    gPane2 = gSecondWindow.DebuggerUI.toggleDebugger();
  }, gSecondWindow);
}

function testTab3_secondWindow(callback) {
  gTab3 = addTab(TAB1_URL, function() {
    gSecondWindow.gBrowser.selectedTab = gTab3;
    gNbox = gSecondWindow.gBrowser.getNotificationBox(gSecondWindow.gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification.");
    ok(gSecondWindow.DebuggerUI.findDebugger(),
      "Should already have a debugger pane for another tab.");

    gNbox.addEventListener("AlertActive", function active() {
      gNbox.removeEventListener("AlertActive", active, true);
      executeSoon(function() {
        ok(gPane2, "toggleDebugger() should always return a pane.");
        is(gPane2.ownerTab, gTab1, "Incorrect tab owner.");

        is(gSecondWindow.DebuggerUI.findDebugger(), gPane1,
          "findDebugger() should return the same pane as the first call to toggleDebugger().");
        is(gSecondWindow.DebuggerUI.findDebugger(), gPane2,
          "findDebugger() should return the same pane as the second call to toggleDebugger().");

        info("Second debugger has not loaded.");

        let notification = gNbox.getNotificationWithValue("debugger-tab-switch");
        ok(gNbox.currentNotification, "Should have a tab switch notification.");
        is(gNbox.currentNotification, notification, "Incorrect current notification.");

        gInitialWindow.gBrowser.selectedTab = gInitialTab;
        gInitialWindow.gBrowser.tabContainer.addEventListener("TabSelect", function tabSelect() {
          gInitialWindow.gBrowser.tabContainer.removeEventListener("TabSelect", tabSelect, true);
          executeSoon(function() {
            callback();
          });
        }, true);

        let buttonSwitch = notification.querySelectorAll("button")[0];
        buttonSwitch.focus();
        EventUtils.sendKey("SPACE", gSecondWindow);
        info("The switch button on the notification was pressed.");
      });
    }, true);

    info("Toggling a debugger (3).");

    gPane2 = gSecondWindow.DebuggerUI.toggleDebugger();
  }, gSecondWindow);
}

function testTab4_secondWindow(callback) {
  is(gInitialWindow.gBrowser.selectedTab, gTab1,
    "Should've switched to the first debugged tab.");

  gTab4 = addTab(TAB1_URL, function() {
    gSecondWindow.gBrowser.selectedTab = gTab4;
    gNbox = gSecondWindow.gBrowser.getNotificationBox(gSecondWindow.gBrowser.selectedBrowser);

    is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
      "Shouldn't have a tab switch notification.");
    ok(gSecondWindow.DebuggerUI.findDebugger(),
      "Should already have a debugger pane for another tab.");

    gNbox.addEventListener("AlertActive", function active() {
      gNbox.removeEventListener("AlertActive", active, true);
      executeSoon(function() {
        ok(gPane2, "toggleDebugger() should always return a pane.");
        is(gPane2.ownerTab, gTab1, "Incorrect tab owner.");

        is(gSecondWindow.DebuggerUI.findDebugger(), gPane1,
          "findDebugger() should return the same pane as the first call to toggleDebugger().");
        is(gSecondWindow.DebuggerUI.findDebugger(), gPane2,
          "findDebugger() should return the same pane as the second call to toggleDebugger().");

        info("Second debugger has not loaded.");

        let notification = gNbox.getNotificationWithValue("debugger-tab-switch");
        ok(gNbox.currentNotification, "Should have a tab switch notification.");
        is(gNbox.currentNotification, notification, "Incorrect current notification.");

        let buttonOpen = notification.querySelectorAll("button")[1];
        buttonOpen.focus();
        EventUtils.sendKey("SPACE", gSecondWindow);
        info("The open button on the notification was pressed.");

        wait_for_connect_and_resume(function() {
          callback();
        }, gSecondWindow);
      });
    }, true);

    info("Toggling a debugger (4).");

    gPane2 = gSecondWindow.DebuggerUI.toggleDebugger();
  }, gSecondWindow);
}

function lastTest(callback) {
  is(gInitialWindow.gBrowser.selectedTab, gTab1,
    "The initial window should continue having selected the first debugged tab.");
  is(gSecondWindow.gBrowser.selectedTab, gTab4,
    "Should currently be in the fourth tab.");
  is(gSecondWindow.DebuggerUI.findDebugger().ownerTab, gTab4,
    "The debugger should be open for the fourth tab.");

  is(gNbox.getNotificationWithValue("debugger-tab-switch"), null,
    "Shouldn't have a tab switch notification.");

  info("Second debugger has loaded.");

  executeSoon(function() {
    callback();
  });
}

function cleanup(callback)
{
  gPane1 = null;
  gPane2 = null;
  gNbox = null;

  closeDebuggerAndFinish(false, function() {
    removeTab(gTab1, gInitialWindow);
    removeTab(gTab2, gSecondWindow);
    removeTab(gTab3, gSecondWindow);
    removeTab(gTab4, gSecondWindow);
    gSecondWindow.close();
    gTab1 = null;
    gTab2 = null;
    gTab3 = null;
    gTab4 = null;
    gInitialWindow = null;
    gSecondWindow = null;

    callback();
  }, gSecondWindow);
}
