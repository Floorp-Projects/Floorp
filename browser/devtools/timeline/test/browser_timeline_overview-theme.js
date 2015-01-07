/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the markers and memory overviews render with the correct
 * theme on load, and rerenders when changed.
 */

const LIGHT_BG = "#fcfcfc";
const DARK_BG = "#14171a";

add_task(function*() {
  let { target, panel } = yield initTimelinePanel("about:blank");
  let { $, EVENTS, TimelineView, TimelineController } = panel.panelWin;

  $("#memory-checkbox").checked = true;

  setTheme("dark");

  yield TimelineController.updateMemoryRecording();
  is(TimelineView.markersOverview.backgroundColor, DARK_BG,
    "correct theme on load for markers.");
  is(TimelineView.memoryOverview.backgroundColor, DARK_BG,
    "correct theme on load for memory.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  let refreshed = Promise.all([
    once(TimelineView.markersOverview, "refresh"),
    once(TimelineView.memoryOverview, "refresh"),
  ]);

  setTheme("light");
  yield refreshed;

  ok(true, "Both memory and markers were rerendered after theme change.");
  is(TimelineView.markersOverview.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for markers.");
  is(TimelineView.memoryOverview.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for memory.");

  refreshed = Promise.all([
    once(TimelineView.markersOverview, "refresh"),
    once(TimelineView.memoryOverview, "refresh"),
  ]);

  setTheme("dark");
  yield refreshed;

  ok(true, "Both memory and markers were rerendered after theme change.");
  is(TimelineView.markersOverview.backgroundColor, DARK_BG,
    "correct theme on after toggle for markers once more.");
  is(TimelineView.memoryOverview.backgroundColor, DARK_BG,
    "correct theme on after toggle for memory once more.");

  refreshed = Promise.all([
    once(TimelineView.markersOverview, "refresh"),
    once(TimelineView.memoryOverview, "refresh"),
  ]);

  // Set theme back to light
  setTheme("light");
  yield refreshed;
});

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
