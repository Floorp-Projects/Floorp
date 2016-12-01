/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the prefs that should survive across tool reloads work.
 */

add_task(function* () {
  let Actions = require("devtools/client/netmonitor/actions/index");
  let { getActiveFilters } = require("devtools/client/netmonitor/selectors/index");

  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  // This test reopens the network monitor a bunch of times, for different
  // hosts (bottom, side, window). This seems to be slow on debug builds.
  requestLongerTimeout(3);

  // Use these getters instead of caching instances inside the panel win,
  // since the tool is reopened a bunch of times during this test
  // and the instances will differ.
  let getStore = () => monitor.panelWin.gStore;
  let getState = () => getStore().getState();

  let prefsToCheck = {
    filters: {
      // A custom new value to be used for the verified preference.
      newValue: ["html", "css"],
      // Getter used to retrieve the current value from the frontend, in order
      // to verify that the pref was applied properly.
      validateValue: ($) => getActiveFilters(getState()),
      // Predicate used to modify the frontend when setting the new pref value,
      // before trying to validate the changes.
      modifyFrontend: ($, value) => value.forEach(e =>
        getStore().dispatch(Actions.toggleFilterType(e)))
    },
    networkDetailsWidth: {
      newValue: ~~(Math.random() * 200 + 100),
      validateValue: ($) => ~~$("#details-pane").getAttribute("width"),
      modifyFrontend: ($, value) => $("#details-pane").setAttribute("width", value)
    },
    networkDetailsHeight: {
      newValue: ~~(Math.random() * 300 + 100),
      validateValue: ($) => ~~$("#details-pane").getAttribute("height"),
      modifyFrontend: ($, value) => $("#details-pane").setAttribute("height", value)
    }
    /* add more prefs here... */
  };

  yield testBottom();
  yield testSide();
  yield testWindow();

  info("Moving toolbox back to the bottom...");
  yield monitor._toolbox.switchHost(Toolbox.HostType.BOTTOM);
  return teardown(monitor);

  function storeFirstPrefValues() {
    info("Caching initial pref values.");

    for (let name in prefsToCheck) {
      let currentValue = monitor.panelWin.Prefs[name];
      prefsToCheck[name].firstValue = currentValue;
    }
  }

  function validateFirstPrefValues() {
    info("Validating current pref values to the UI elements.");

    for (let name in prefsToCheck) {
      let currentValue = monitor.panelWin.Prefs[name];
      let firstValue = prefsToCheck[name].firstValue;
      let validateValue = prefsToCheck[name].validateValue;

      is(currentValue.toSource(), firstValue.toSource(),
        "Pref " + name + " should be equal to first value: " + firstValue);
      is(currentValue.toSource(), validateValue(monitor.panelWin.$).toSource(),
        "Pref " + name + " should validate: " + currentValue);
    }
  }

  function modifyFrontend() {
    info("Modifying UI elements to the specified new values.");

    for (let name in prefsToCheck) {
      let currentValue = monitor.panelWin.Prefs[name];
      let firstValue = prefsToCheck[name].firstValue;
      let newValue = prefsToCheck[name].newValue;
      let validateValue = prefsToCheck[name].validateValue;
      let modFrontend = prefsToCheck[name].modifyFrontend;

      modFrontend(monitor.panelWin.$, newValue);
      info("Modified UI element affecting " + name + " to: " + newValue);

      is(currentValue.toSource(), firstValue.toSource(),
        "Pref " + name + " should still be equal to first value: " + firstValue);
      isnot(currentValue.toSource(), newValue.toSource(),
        "Pref " + name + " should't yet be equal to second value: " + newValue);
      is(newValue.toSource(), validateValue(monitor.panelWin.$).toSource(),
        "The UI element affecting " + name + " should validate: " + newValue);
    }
  }

  function validateNewPrefValues() {
    info("Invalidating old pref values to the modified UI elements.");

    for (let name in prefsToCheck) {
      let currentValue = monitor.panelWin.Prefs[name];
      let firstValue = prefsToCheck[name].firstValue;
      let newValue = prefsToCheck[name].newValue;
      let validateValue = prefsToCheck[name].validateValue;

      isnot(currentValue.toSource(), firstValue.toSource(),
        "Pref " + name + " should't be equal to first value: " + firstValue);
      is(currentValue.toSource(), newValue.toSource(),
        "Pref " + name + " should now be equal to second value: " + newValue);
      is(newValue.toSource(), validateValue(monitor.panelWin.$).toSource(),
        "The UI element affecting " + name + " should validate: " + newValue);
    }
  }

  function resetFrontend() {
    info("Resetting UI elements to the cached initial pref values.");

    for (let name in prefsToCheck) {
      let currentValue = monitor.panelWin.Prefs[name];
      let firstValue = prefsToCheck[name].firstValue;
      let newValue = prefsToCheck[name].newValue;
      let validateValue = prefsToCheck[name].validateValue;
      let modFrontend = prefsToCheck[name].modifyFrontend;

      modFrontend(monitor.panelWin.$, firstValue);
      info("Modified UI element affecting " + name + " to: " + firstValue);

      isnot(currentValue.toSource(), firstValue.toSource(),
        "Pref " + name + " should't yet be equal to first value: " + firstValue);
      is(currentValue.toSource(), newValue.toSource(),
        "Pref " + name + " should still be equal to second value: " + newValue);
      is(firstValue.toSource(), validateValue(monitor.panelWin.$).toSource(),
        "The UI element affecting " + name + " should validate: " + firstValue);
    }
  }

  function* testBottom() {
    info("Testing prefs reload for a bottom host.");
    storeFirstPrefValues();

    // Validate and modify while toolbox is on the bottom.
    validateFirstPrefValues();
    modifyFrontend();

    let newMonitor = yield restartNetMonitor(monitor);
    monitor = newMonitor.monitor;

    // Revalidate and reset frontend while toolbox is on the bottom.
    validateNewPrefValues();
    resetFrontend();

    newMonitor = yield restartNetMonitor(monitor);
    monitor = newMonitor.monitor;

    // Revalidate.
    validateFirstPrefValues();
  }

  function* testSide() {
    info("Moving toolbox to the side...");

    yield monitor._toolbox.switchHost(Toolbox.HostType.SIDE);
    info("Testing prefs reload for a side host.");
    storeFirstPrefValues();

    // Validate and modify frontend while toolbox is on the side.
    validateFirstPrefValues();
    modifyFrontend();

    let newMonitor = yield restartNetMonitor(monitor);
    monitor = newMonitor.monitor;

    // Revalidate and reset frontend while toolbox is on the side.
    validateNewPrefValues();
    resetFrontend();

    newMonitor = yield restartNetMonitor(monitor);
    monitor = newMonitor.monitor;

    // Revalidate.
    validateFirstPrefValues();
  }

  function* testWindow() {
    info("Moving toolbox into a window...");

    yield monitor._toolbox.switchHost(Toolbox.HostType.WINDOW);
    info("Testing prefs reload for a window host.");
    storeFirstPrefValues();

    // Validate and modify frontend while toolbox is in a window.
    validateFirstPrefValues();
    modifyFrontend();

    let newMonitor = yield restartNetMonitor(monitor);
    monitor = newMonitor.monitor;

    // Revalidate and reset frontend while toolbox is in a window.
    validateNewPrefValues();
    resetFrontend();

    newMonitor = yield restartNetMonitor(monitor);
    monitor = newMonitor.monitor;

    // Revalidate.
    validateFirstPrefValues();
  }
});
