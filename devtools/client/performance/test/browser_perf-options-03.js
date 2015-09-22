/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that toggling meta option prefs change visibility of other options.
 */

Services.prefs.setBoolPref(EXPERIMENTAL_PREF, false);

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, PerformanceController } = panel.panelWin;

  let $body = $(".theme-body");
  let $menu = $("#performance-options-menupopup");

  ok(!$body.classList.contains("experimental-enabled"), "body does not have `experimental-enabled` on start");
  ok(!$menu.classList.contains("experimental-enabled"), "menu does not have `experimental-enabled` on start");

  Services.prefs.setBoolPref(EXPERIMENTAL_PREF, true);

  ok($body.classList.contains("experimental-enabled"), "body has `experimental-enabled` after toggle");
  ok($menu.classList.contains("experimental-enabled"), "menu has `experimental-enabled` after toggle");

  yield teardown(panel);
  finish();
}
