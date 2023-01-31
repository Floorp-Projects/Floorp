/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;
add_task(setup_UITourTest);

// Tests assume there's at least 1 builtin theme with colorway id.
const { BuiltInThemes } = ChromeUtils.importESModule(
  "resource:///modules/BuiltInThemes.sys.mjs"
);
const COLORWAY_IDS = [...BuiltInThemes.builtInThemeMap.keys()].filter(
  id =>
    id.endsWith("-colorway@mozilla.org") && !BuiltInThemes.themeIsExpired(id)
);

add_UITour_task(async function test_getColorways() {
  const data = await getConfigurationPromise("colorway");

  ok(
    Array.isArray(data),
    "getConfiguration result should be an array of colorways"
  );
});

add_UITour_task(async function test_setColorway_unknown() {
  await gContentAPI.setConfiguration("colorway", "unknown");

  ok(
    (await AddonManager.getAddonByID("default-theme@mozilla.org")).isActive,
    "gContentAPI did not activate unknown colorway"
  );
});

add_UITour_task(async function test_setColorway() {
  const id = COLORWAY_IDS.at(0);
  if (!id) {
    info("No colorways to test");
    return;
  }

  await gContentAPI.setConfiguration("colorway", id);

  ok(
    (await AddonManager.getAddonByID(id)).isActive,
    `gContentAPI activated colorway ${id}`
  );
});

add_UITour_task(async function test_anotherColorway() {
  const id = COLORWAY_IDS.at(-1);
  if (!id) {
    info("No colorways to test");
    return;
  }

  await gContentAPI.setConfiguration("colorway", id);

  ok(
    (await AddonManager.getAddonByID(id)).isActive,
    `gContentAPI activated another colorway ${id}`
  );
});

add_UITour_task(async function test_resetColorway() {
  await gContentAPI.setConfiguration("colorway");

  ok(
    (await AddonManager.getAddonByID("default-theme@mozilla.org")).isActive,
    "gContentAPI reset colorway to original theme"
  );
});
