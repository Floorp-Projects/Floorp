/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if page navigation ("close", "navigate", etc.) triggers an appropriate
 * action in the network monitor.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    testNavigate(() => testNavigateBack(() => testClose(() => finish())));

    function testNavigate(aCallback) {
      info("Navigating forward...");

      aMonitor.panelWin.once("NetMonitor:TargetWillNavigate", () => {
        is(aDebuggee.location, SIMPLE_URL,
          "Target started navigating to the correct location.");

        aMonitor.panelWin.once("NetMonitor:TargetNavigate", () => {
          is(aDebuggee.location, NAVIGATE_URL,
            "Target finished navigating to the correct location.");

          aCallback();
        });
      });

      aDebuggee.location = NAVIGATE_URL;
    }

    function testNavigateBack(aCallback) {
      info("Navigating backward...");

      aMonitor.panelWin.once("NetMonitor:TargetWillNavigate", () => {
        is(aDebuggee.location, NAVIGATE_URL,
          "Target started navigating back to the previous location.");

        aMonitor.panelWin.once("NetMonitor:TargetNavigate", () => {
          is(aDebuggee.location, SIMPLE_URL,
            "Target finished navigating back to the previous location.");

          aCallback();
        });
      });

      aDebuggee.location = SIMPLE_URL;
    }

    function testClose(aCallback) {
      info("Closing...");

      aMonitor.once("destroyed", () => {
        ok(!aMonitor._controller.client,
          "There shouldn't be a client available after destruction.");
        ok(!aMonitor._controller.tabClient,
          "There shouldn't be a tabClient available after destruction.");
        ok(!aMonitor._controller.webConsoleClient,
          "There shouldn't be a webConsoleClient available after destruction.");

        aCallback();
      });

      removeTab(aTab);
    }
  });
}
