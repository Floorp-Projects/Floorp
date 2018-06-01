/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that toggling meta option prefs change visibility of other options.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_EXPERIMENTAL_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");

add_task(async function() {
  Services.prefs.setBoolPref(UI_EXPERIMENTAL_PREF, false);

  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { $ } = panel.panelWin;
  const $body = $(".theme-body");
  const $menu = $("#performance-options-menupopup");

  ok(!$body.classList.contains("experimental-enabled"),
    "The body node does not have `experimental-enabled` on start.");
  ok(!$menu.classList.contains("experimental-enabled"),
    "The menu popup does not have `experimental-enabled` on start.");

  Services.prefs.setBoolPref(UI_EXPERIMENTAL_PREF, true);

  ok($body.classList.contains("experimental-enabled"),
    "The body node has `experimental-enabled` after toggle.");
  ok($menu.classList.contains("experimental-enabled"),
    "The menu popup has `experimental-enabled` after toggle.");

  await teardownToolboxAndRemoveTab(panel);
});
