/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the markers and memory overviews render with the correct
 * theme on load, and rerenders when changed.
 */

const { setTheme } = require("devtools/client/shared/theme");

const LIGHT_BG = "#fcfcfc";
const DARK_BG = "#14171a";

setTheme("dark");
Services.prefs.setBoolPref(MEMORY_PREF, false);

requestLongerTimeout(2);

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, OverviewView, document: doc } = panel.panelWin;

  yield startRecording(panel);
  let markers = OverviewView.graphs.get("timeline");
  is(markers.backgroundColor, DARK_BG,
    "correct theme on load for markers.");
  yield stopRecording(panel);

  let refreshed = once(markers, "refresh");
  setTheme("light");
  yield refreshed;

  ok(true, "markers were rerendered after theme change.");
  is(markers.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for markers.");

  // reset back to dark
  refreshed = once(markers, "refresh");
  setTheme("dark");
  yield refreshed;

  info("Testing with memory overview");

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);
  let memory = OverviewView.graphs.get("memory");
  is(memory.backgroundColor, DARK_BG,
    "correct theme on load for memory.");
  yield stopRecording(panel);

  refreshed = Promise.all([
    once(markers, "refresh"),
    once(memory, "refresh"),
  ]);
  setTheme("light");
  yield refreshed;

  ok(true, "Both memory and markers were rerendered after theme change.");
  is(markers.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for markers.");
  is(memory.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for memory.");

  refreshed = Promise.all([
    once(markers, "refresh"),
    once(memory, "refresh"),
  ]);

  // Set theme back to light
  setTheme("light");
  yield refreshed;

  yield teardown(panel);
  finish();
}
