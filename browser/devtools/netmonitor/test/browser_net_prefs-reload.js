/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the prefs that should survive across tool reloads work.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    // This test reopens the network monitor a bunch of times, for different
    // hosts (bottom, side, window). This seems to be slow on debug builds.
    requestLongerTimeout(3);

    let prefsToCheck = {
      networkDetailsWidth: {
        newValue: ~~(Math.random() * 200 + 100),
        validate: ($) => ~~$("#details-pane").getAttribute("width"),
        modifyFrontend: ($, aValue) => $("#details-pane").setAttribute("width", aValue)
      },
      networkDetailsHeight: {
        newValue: ~~(Math.random() * 300 + 100),
        validate: ($) => ~~$("#details-pane").getAttribute("height"),
        modifyFrontend: ($, aValue) => $("#details-pane").setAttribute("height", aValue)
      },
      /* add more prefs here... */
    };

    function storeFirstPrefValues() {
      info("Caching initial pref values.");

      for (let name in prefsToCheck) {
        let currentValue = aMonitor.panelWin.Prefs[name];
        prefsToCheck[name].firstValue = currentValue;
      }
    }

    function validateFirstPrefValues() {
      info("Validating current pref values to the UI elements.");

      for (let name in prefsToCheck) {
        let currentValue = aMonitor.panelWin.Prefs[name];
        let firstValue = prefsToCheck[name].firstValue;
        let validate = prefsToCheck[name].validate;

        is(currentValue, firstValue,
          "Pref " + name + " should be equal to first value: " + firstValue);
        is(currentValue, validate(aMonitor.panelWin.$),
          "Pref " + name + " should validate: " + currentValue);
      }
    }

    function modifyFrontend() {
      info("Modifying UI elements to the specified new values.");

      for (let name in prefsToCheck) {
        let currentValue = aMonitor.panelWin.Prefs[name];
        let firstValue = prefsToCheck[name].firstValue;
        let newValue = prefsToCheck[name].newValue;
        let validate = prefsToCheck[name].validate;
        let modifyFrontend = prefsToCheck[name].modifyFrontend;

        modifyFrontend(aMonitor.panelWin.$, newValue);
        info("Modified UI element affecting " + name + " to: " + newValue);

        is(currentValue, firstValue,
          "Pref " + name + " should still be equal to first value: " + firstValue);
        isnot(currentValue, newValue,
          "Pref " + name + " should't yet be equal to second value: " + newValue);
        is(newValue, validate(aMonitor.panelWin.$),
          "The UI element affecting " + name + " should validate: " + newValue);
      }
    }

    function validateNewPrefValues() {
      info("Invalidating old pref values to the modified UI elements.");

      for (let name in prefsToCheck) {
        let currentValue = aMonitor.panelWin.Prefs[name];
        let firstValue = prefsToCheck[name].firstValue;
        let newValue = prefsToCheck[name].newValue;
        let validate = prefsToCheck[name].validate;

        isnot(currentValue, firstValue,
          "Pref " + name + " should't be equal to first value: " + firstValue);
        is(currentValue, newValue,
          "Pref " + name + " should now be equal to second value: " + newValue);
        is(newValue, validate(aMonitor.panelWin.$),
          "The UI element affecting " + name + " should validate: " + newValue);
      }
    }

    function resetFrontend() {
      info("Resetting UI elements to the cached initial pref values.");

      for (let name in prefsToCheck) {
        let currentValue = aMonitor.panelWin.Prefs[name];
        let firstValue = prefsToCheck[name].firstValue;
        let newValue = prefsToCheck[name].newValue;
        let validate = prefsToCheck[name].validate;
        let modifyFrontend = prefsToCheck[name].modifyFrontend;

        modifyFrontend(aMonitor.panelWin.$, firstValue);
        info("Modified UI element affecting " + name + " to: " + firstValue);

        isnot(currentValue, firstValue,
          "Pref " + name + " should't yet be equal to first value: " + firstValue);
        is(currentValue, newValue,
          "Pref " + name + " should still be equal to second value: " + newValue);
        is(firstValue, validate(aMonitor.panelWin.$),
          "The UI element affecting " + name + " should validate: " + firstValue);
      }
    }

    function testBottom() {
      info("Testing prefs reload for a bottom host.");
      storeFirstPrefValues();

      // Validate and modify while toolbox is on the bottom.
      validateFirstPrefValues();
      modifyFrontend();

      return restartNetMonitor(aMonitor)
        .then(([,, aNewMonitor]) => {
          aMonitor = aNewMonitor;

          // Revalidate and reset frontend while toolbox is on the bottom.
          validateNewPrefValues();
          resetFrontend();

          return restartNetMonitor(aMonitor);
        })
        .then(([,, aNewMonitor]) => {
          aMonitor = aNewMonitor;

          // Revalidate.
          validateFirstPrefValues();
        });
    }

    function testSide() {
      info("Moving toolbox to the side...");

      return aMonitor._toolbox.switchHost(Toolbox.HostType.SIDE)
        .then(() => {
          info("Testing prefs reload for a side host.");
          storeFirstPrefValues();

          // Validate and modify frontend while toolbox is on the side.
          validateFirstPrefValues();
          modifyFrontend();

          return restartNetMonitor(aMonitor);
        })
        .then(([,, aNewMonitor]) => {
          aMonitor = aNewMonitor;

          // Revalidate and reset frontend while toolbox is on the side.
          validateNewPrefValues();
          resetFrontend();

          return restartNetMonitor(aMonitor);
        })
        .then(([,, aNewMonitor]) => {
          aMonitor = aNewMonitor;

          // Revalidate.
          validateFirstPrefValues();
        });
    }

    function testWindow() {
      info("Moving toolbox into a window...");

      return aMonitor._toolbox.switchHost(Toolbox.HostType.WINDOW)
        .then(() => {
          info("Testing prefs reload for a window host.");
          storeFirstPrefValues();

          // Validate and modify frontend while toolbox is in a window.
          validateFirstPrefValues();
          modifyFrontend();

          return restartNetMonitor(aMonitor);
        })
        .then(([,, aNewMonitor]) => {
          aMonitor = aNewMonitor;

          // Revalidate and reset frontend while toolbox is in a window.
          validateNewPrefValues();
          resetFrontend();

          return restartNetMonitor(aMonitor);
        })
        .then(([,, aNewMonitor]) => {
          aMonitor = aNewMonitor;

          // Revalidate.
          validateFirstPrefValues();
        });
    }

    function cleanupAndFinish() {
      info("Moving toolbox back to the bottom...");

      aMonitor._toolbox.switchHost(Toolbox.HostType.BOTTOM)
        .then(() => teardown(aMonitor))
        .then(finish);
    }

    testBottom()
      .then(testSide)
      .then(testWindow)
      .then(cleanupAndFinish);
  });
}
