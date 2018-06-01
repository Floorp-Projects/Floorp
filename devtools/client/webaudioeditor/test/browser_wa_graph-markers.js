/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the SVG marker styling is updated when devtools theme changes.
 */

const { setTheme } = require("devtools/client/shared/theme");

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, MARKER_STYLING } = panelWin;

  const currentTheme = Services.prefs.getCharPref("devtools.theme");

  ok(MARKER_STYLING.light, "Marker styling exists for light theme.");
  ok(MARKER_STYLING.dark, "Marker styling exists for dark theme.");

  const started = once(gFront, "start-context");

  const events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;

  is(getFill($("#arrowhead")), MARKER_STYLING[currentTheme],
    "marker initially matches theme.");

  // Switch to light
  setTheme("light");
  is(getFill($("#arrowhead")), MARKER_STYLING.light,
    "marker styling matches light theme on change.");

  // Switch to dark
  setTheme("dark");
  is(getFill($("#arrowhead")), MARKER_STYLING.dark,
    "marker styling matches dark theme on change.");

  // Switch to dark again
  setTheme("dark");
  is(getFill($("#arrowhead")), MARKER_STYLING.dark,
    "marker styling remains dark.");

  // Switch to back to light again
  setTheme("light");
  is(getFill($("#arrowhead")), MARKER_STYLING.light,
    "marker styling switches back to light once again.");

  await teardown(target);
});

/**
 * Returns a hex value found in styling for an element. So parses
 * <marker style="fill: #abcdef"> and returns "#abcdef"
 */
function getFill(el) {
  return el.getAttribute("style").match(/(#.*)$/)[1];
}
