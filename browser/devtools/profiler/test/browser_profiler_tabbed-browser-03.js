/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler displays and organizes the recording data in tabs,
 * spawning a new tab from a graph's selection when required.
 */

const WAIT_TIME = 1000; // ms

let gPrevShowPlatformData =
  Services.prefs.getBoolPref("devtools.profiler.ui.show-platform-data");

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, $$, EVENTS, Prefs, ProfileView } = panel.panelWin;

  // Make sure platform data is displayed, so there's samples available.
  Prefs.showPlatformData = true;

  yield startRecording(panel);
  busyWait(WAIT_TIME); // allow the profiler module to sample more cpu activity
  yield stopRecording(panel, { waitForDisplay: true });

  let categoriesGraph = ProfileView._getCategoriesGraph();
  ok(categoriesGraph,
    "There's a categories graph available on the profile view.");

  ok(!categoriesGraph.hasSelection(),
    "The categories graph shouldn't have a selection available yet.");
  ok($("#profile-newtab-button").hidden,
    "The new tab button on the profile view tab bar should be hidden.");

  dragStart(categoriesGraph, 10);
  dragStop(categoriesGraph, 50);

  ok(categoriesGraph.hasSelection(),
    "The categories graph should have a selection available now.");
  ok(!$("#profile-newtab-button").hidden,
    "The new tab button on the profile view tab bar should be visible.");

  let tabSpawned = panel.panelWin.once(EVENTS.TAB_SPAWNED_FROM_SELECTION);
  EventUtils.sendMouseEvent({ type: "click" }, $("#profile-newtab-button"));
  yield tabSpawned;

  is(ProfileView.tabCount, 2,
    "There should be two tabs visible now.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should still be displayed in the profile view.");
  is($("#profile-content").selectedIndex, 1,
    "The second tab is now selected.");

  let firstTabTitle = $$("#profile-content .tab-title-label")[0].getAttribute("value");
  let secondTabTitle = $$("#profile-content .tab-title-label")[1].getAttribute("value");
  info("The first tab's title is: " + firstTabTitle);
  info("The second tab's title is: " + secondTabTitle);

  isnot(firstTabTitle, secondTabTitle,
    "The first and second tab titles are different.");
  ok(firstTabTitle.match(/\d+ ms . \d+ ms/),
    "The recording's first tab title is correct.");
  ok(secondTabTitle.match(/[\d\.,]+ ms . [\d\.,]+ ms/),
    "The recording's second tab title is correct.");

  yield teardown(panel);
  finish();
});

registerCleanupFunction(() => {
  Services.prefs.setBoolPref(
    "devtools.profiler.ui.show-platform-data", gPrevShowPlatformData);
});

// EventUtils just doesn't work!

function dragStart(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseDown({ testX: x, testY: y });
}

function dragStop(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseUp({ testX: x, testY: y });
}
