/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the markers and memory overviews render with the correct
 * theme on load, and rerenders when changed.
 */

const { setTheme } = require("devtools/client/shared/theme");

const LIGHT_BG = "white";
const DARK_BG = "#14171a";

setTheme("dark");
Services.prefs.setBoolPref(MEMORY_PREF, false);

requestLongerTimeout(2);

async function spawnTest() {
  let { panel } = await initPerformance(SIMPLE_URL);
  let { EVENTS, $, OverviewView, document: doc } = panel.panelWin;

  await startRecording(panel);
  let markers = OverviewView.graphs.get("timeline");
  is(markers.backgroundColor, DARK_BG,
    "correct theme on load for markers.");
  await stopRecording(panel);

  let refreshed = once(markers, "refresh");
  setTheme("light");
  await refreshed;

  ok(true, "markers were rerendered after theme change.");
  is(markers.backgroundColor, LIGHT_BG,
    "correct theme on after toggle for markers.");

  // reset back to dark
  refreshed = once(markers, "refresh");
  setTheme("dark");
  await refreshed;

  info("Testing with memory overview");

  Services.prefs.setBoolPref(MEMORY_PREF, true);

  await startRecording(panel);
  let memory = OverviewView.graphs.get("memory");
  is(memory.backgroundColor, DARK_BG,
    "correct theme on load for memory.");
  await stopRecording(panel);

  refreshed = Promise.all([
    once(markers, "refresh"),
    once(memory, "refresh"),
  ]);
  setTheme("light");
  await refreshed;

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
  await refreshed;

  await teardown(panel);
  finish();
}
/* eslint-enable */
