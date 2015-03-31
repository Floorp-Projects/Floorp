/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the markers and memory overviews render with the correct
 * theme on load, and rerenders when changed.
 */

const LIGHT_BG = "#fcfcfc";
const DARK_BG = "#14171a";

setTheme("dark");
Services.prefs.setBoolPref(MEMORY_PREF, false);

function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, OverviewView, document: doc } = panel.panelWin;

  yield startRecording(panel);
  is(OverviewView.markersOverview.backgroundColor, DARK_BG,
    "correct theme on load for markers.");
  yield stopRecording(panel);

  let refreshed = once(OverviewView.markersOverview, "refresh");
  setTheme("light");
  yield refreshed;

  ok(true, "markers were rerendered after theme change.");
  is(OverviewView.markersOverview.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for markers.");

  // reset back to dark
  refreshed = once(OverviewView.markersOverview, "refresh");
  setTheme("dark");
  yield refreshed;

  info("Testing with memory overview");

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);
  is(OverviewView.memoryOverview.backgroundColor, DARK_BG,
    "correct theme on load for memory.");
  yield stopRecording(panel);

  refreshed = Promise.all([
    once(OverviewView.markersOverview, "refresh"),
    once(OverviewView.memoryOverview, "refresh"),
  ]);
  setTheme("light");
  yield refreshed;

  ok(true, "Both memory and markers were rerendered after theme change.");
  is(OverviewView.markersOverview.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for markers.");
  is(OverviewView.memoryOverview.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for memory.");

  refreshed = Promise.all([
    once(OverviewView.markersOverview, "refresh"),
    once(OverviewView.memoryOverview, "refresh"),
  ]);

  // Set theme back to light
  setTheme("light");
  yield refreshed;

  yield teardown(panel);
  finish();
}

/**
 * Mimics selecting the theme selector in the toolbox;
 * sets the preference and emits an event on gDevTools to trigger
 * the themeing.
 */
function setTheme (newTheme) {
  let oldTheme = Services.prefs.getCharPref("devtools.theme");
  info("Setting `devtools.theme` to \"" + newTheme + "\"");
  Services.prefs.setCharPref("devtools.theme", newTheme);
  gDevTools.emit("pref-changed", {
    pref: "devtools.theme",
    newValue: newTheme,
    oldValue: oldTheme
  });
}
