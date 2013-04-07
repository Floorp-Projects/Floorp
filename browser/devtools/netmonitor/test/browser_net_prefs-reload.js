/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the prefs that should survive across tool reloads work.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let prefsToCheck = {
      networkDetailsWidth: {
        newValue: ~~(Math.random() * 200 + 100),
        validate: () =>
          ~~aMonitor._view._detailsPane.getAttribute("width"),
        modifyFrontend: (aValue) =>
          aMonitor._view._detailsPane.setAttribute("width", aValue)
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
        is(currentValue, validate(),
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

        modifyFrontend(newValue);
        info("Modified UI element affecting " + name + " to: " + newValue);

        is(currentValue, firstValue,
          "Pref " + name + " should still be equal to first value: " + firstValue);
        isnot(currentValue, newValue,
          "Pref " + name + " should't yet be equal to second value: " + newValue);
        is(newValue, validate(),
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
        let modifyFrontend = prefsToCheck[name].modifyFrontend;

        isnot(currentValue, firstValue,
          "Pref " + name + " should't be equal to first value: " + firstValue);
        is(currentValue, newValue,
          "Pref " + name + " should now be equal to second value: " + newValue);
        is(newValue, validate(),
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

        modifyFrontend(firstValue);
        info("Modified UI element affecting " + name + " to: " + firstValue);

        isnot(currentValue, firstValue,
          "Pref " + name + " should't yet be equal to first value: " + firstValue);
        is(currentValue, newValue,
          "Pref " + name + " should still be equal to second value: " + newValue);
        is(firstValue, validate(),
          "The UI element affecting " + name + " should validate: " + firstValue);
      }
    }

    storeFirstPrefValues();

    // Validate and modify.
    validateFirstPrefValues();
    modifyFrontend();
    restartNetMonitor(aMonitor).then(([,, aNewMonitor]) => {
      aMonitor = aNewMonitor;

      // Revalidate and reset.
      validateNewPrefValues();
      resetFrontend();
      restartNetMonitor(aMonitor).then(([,, aNewMonitor]) => {
        aMonitor = aNewMonitor;

        // Revalidate and finish.
        validateFirstPrefValues();
        teardown(aMonitor).then(finish);
      });
    });
  });
}
