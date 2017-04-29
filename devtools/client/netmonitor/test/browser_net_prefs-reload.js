/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the prefs that should survive across tool reloads work.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  let { getRequestFilterTypes } = monitor.panelWin
    .windowRequire("devtools/client/netmonitor/src/selectors/index");
  let Actions = monitor.panelWin
    .windowRequire("devtools/client/netmonitor/src/actions/index");
  info("Starting test... ");

  // This test reopens the network monitor a bunch of times, for different
  // hosts (bottom, side, window). This seems to be slow on debug builds.
  requestLongerTimeout(3);

  // Use these getters instead of caching instances inside the panel win,
  // since the tool is reopened a bunch of times during this test
  // and the instances will differ.
  let getDoc = () => monitor.panelWin.document;
  let getPrefs = () => monitor.panelWin
    .windowRequire("devtools/client/netmonitor/src/utils/prefs").Prefs;
  let getStore = () => monitor.panelWin.store;
  let getState = () => getStore().getState();

  let prefsToCheck = {
    filters: {
      // A custom new value to be used for the verified preference.
      newValue: ["html", "css"],
      // Getter used to retrieve the current value from the frontend, in order
      // to verify that the pref was applied properly.
      validateValue: () => getRequestFilterTypes(getState())
        .filter(([type, check]) => check)
        .map(([type, check]) => type),
      // Predicate used to modify the frontend when setting the new pref value,
      // before trying to validate the changes.
      modifyFrontend: (value) => value.forEach(e =>
        getStore().dispatch(Actions.toggleRequestFilterType(e)))
    },
    networkDetailsWidth: {
      newValue: ~~(Math.random() * 200 + 100),
      validateValue: () =>
        getDoc().querySelector(".monitor-panel .split-box .controlled").clientWidth,
      modifyFrontend: function (value) {
        getDoc().querySelector(".monitor-panel .split-box .controlled")
                .style.width = `${value}px`;
      }
    },
    networkDetailsHeight: {
      newValue: ~~(Math.random() * 300 + 100),
      validateValue: () =>
        getDoc().querySelector(".monitor-panel .split-box .controlled").clientHeight,
      modifyFrontend: function (value) {
        getDoc().querySelector(".monitor-panel .split-box .controlled")
                .style.height = `${value}px`;
      }
    }
    /* add more prefs here... */
  };

  yield testBottom();
  yield testSide();
  yield testWindow();

  info("Moving toolbox back to the bottom...");
  yield monitor.toolbox.switchHost("bottom");
  return teardown(monitor);

  function storeFirstPrefValues() {
    info("Caching initial pref values.");

    for (let name in prefsToCheck) {
      let currentValue = getPrefs()[name];
      prefsToCheck[name].firstValue = currentValue;
    }
  }

  function validateFirstPrefValues(isVerticalSplitter) {
    info("Validating current pref values to the UI elements.");

    for (let name in prefsToCheck) {
      if ((isVerticalSplitter && name === "networkDetailsHeight") ||
          (!isVerticalSplitter && name === "networkDetailsWidth")) {
        continue;
      }

      let currentValue = getPrefs()[name];
      let firstValue = prefsToCheck[name].firstValue;
      let validateValue = prefsToCheck[name].validateValue;

      is(firstValue.toSource(), currentValue.toSource(),
        "Pref " + name + " should be equal to first value: " + currentValue);
      is(validateValue().toSource(), currentValue.toSource(),
        "Pref " + name + " should validate: " + currentValue);
    }
  }

  function modifyFrontend(isVerticalSplitter) {
    info("Modifying UI elements to the specified new values.");

    for (let name in prefsToCheck) {
      if ((isVerticalSplitter && name === "networkDetailsHeight") ||
          (!isVerticalSplitter && name === "networkDetailsWidth")) {
        continue;
      }

      let currentValue = getPrefs()[name];
      let firstValue = prefsToCheck[name].firstValue;
      let newValue = prefsToCheck[name].newValue;
      let validateValue = prefsToCheck[name].validateValue;
      let modFrontend = prefsToCheck[name].modifyFrontend;

      modFrontend(newValue);
      info("Modified UI element affecting " + name + " to: " + newValue);

      is(firstValue.toSource(), currentValue.toSource(),
        "Pref " + name + " should still be equal to first value: " + currentValue);
      isnot(newValue.toSource(), currentValue.toSource(),
        "Pref " + name + " should't yet be equal to second value: " + currentValue);
      is(validateValue().toSource(), newValue.toSource(),
        "The UI element affecting " + name + " should validate: " + newValue);
    }
  }

  function validateNewPrefValues(isVerticalSplitter) {
    info("Invalidating old pref values to the modified UI elements.");

    for (let name in prefsToCheck) {
      if ((isVerticalSplitter && name === "networkDetailsHeight") ||
          (!isVerticalSplitter && name === "networkDetailsWidth")) {
        continue;
      }

      let currentValue = getPrefs()[name];
      let firstValue = prefsToCheck[name].firstValue;
      let newValue = prefsToCheck[name].newValue;
      let validateValue = prefsToCheck[name].validateValue;

      isnot(firstValue.toSource(), currentValue.toSource(),
        "Pref " + name + " should't be equal to first value: " + currentValue);
      is(newValue.toSource(), currentValue.toSource(),
        "Pref " + name + " should now be equal to second value: " + currentValue);
      is(validateValue().toSource(), newValue.toSource(),
        "The UI element affecting " + name + " should validate: " + newValue);
    }
  }

  function resetFrontend(isVerticalSplitter) {
    info("Resetting UI elements to the cached initial pref values.");

    for (let name in prefsToCheck) {
      if ((isVerticalSplitter && name === "networkDetailsHeight") ||
          (!isVerticalSplitter && name === "networkDetailsWidth")) {
        continue;
      }

      let currentValue = getPrefs()[name];
      let firstValue = prefsToCheck[name].firstValue;
      let newValue = prefsToCheck[name].newValue;
      let validateValue = prefsToCheck[name].validateValue;
      let modFrontend = prefsToCheck[name].modifyFrontend;

      modFrontend(firstValue);
      info("Modified UI element affecting " + name + " to: " + firstValue);

      isnot(firstValue.toSource(), currentValue.toSource(),
        "Pref " + name + " should't yet be equal to first value: " + currentValue);
      is(newValue.toSource(), currentValue.toSource(),
        "Pref " + name + " should still be equal to second value: " + currentValue);
      is(validateValue().toSource(), firstValue.toSource(),
        "The UI element affecting " + name + " should validate: " + firstValue);
    }
  }

  function* restartNetMonitorAndSetupEnv() {
    let newMonitor = yield restartNetMonitor(monitor);
    monitor = newMonitor.monitor;

    let networkEvent = waitForNetworkEvents(monitor, 1);
    newMonitor.tab.linkedBrowser.reload();
    yield networkEvent;

    let wait = waitForDOM(getDoc(), ".network-details-panel");
    EventUtils.sendMouseEvent({ type: "click" },
      getDoc().querySelector(".network-details-panel-toggle"));
    yield wait;
  }

  function* testBottom() {
    yield restartNetMonitorAndSetupEnv();

    info("Testing prefs reload for a bottom host.");
    storeFirstPrefValues();

    // Validate and modify while toolbox is on the bottom.
    validateFirstPrefValues(true);
    modifyFrontend(true);

    yield restartNetMonitorAndSetupEnv();

    // Revalidate and reset frontend while toolbox is on the bottom.
    validateNewPrefValues(true);
    resetFrontend(true);

    yield restartNetMonitorAndSetupEnv();

    // Revalidate.
    validateFirstPrefValues(true);
  }

  function* testSide() {
    yield restartNetMonitorAndSetupEnv();

    info("Moving toolbox to the side...");

    yield monitor.toolbox.switchHost("side");
    info("Testing prefs reload for a side host.");
    storeFirstPrefValues();

    // Validate and modify frontend while toolbox is on the side.
    validateFirstPrefValues(false);
    modifyFrontend(false);

    yield restartNetMonitorAndSetupEnv();

    // Revalidate and reset frontend while toolbox is on the side.
    validateNewPrefValues(false);
    resetFrontend(false);

    yield restartNetMonitorAndSetupEnv();

    // Revalidate.
    validateFirstPrefValues(false);
  }

  function* testWindow() {
    yield restartNetMonitorAndSetupEnv();

    info("Moving toolbox into a window...");

    yield monitor.toolbox.switchHost("window");
    info("Testing prefs reload for a window host.");
    storeFirstPrefValues();

    // Validate and modify frontend while toolbox is in a window.
    validateFirstPrefValues(true);
    modifyFrontend(true);

    yield restartNetMonitorAndSetupEnv();

    // Revalidate and reset frontend while toolbox is in a window.
    validateNewPrefValues(true);
    resetFrontend(true);

    yield restartNetMonitorAndSetupEnv();

    // Revalidate.
    validateFirstPrefValues(true);
  }
});
