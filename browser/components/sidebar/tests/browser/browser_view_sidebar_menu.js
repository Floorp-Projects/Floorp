/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that pref controlled megalist sidebar menu item is hidden/shown
 */
add_task(async function test_megalist_menu() {
  const item = document.getElementById("menu_megalistSidebar");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.megalist.enabled", false]],
  });
  ok(item.hidden, "Megalist sidebar menu item hidden");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.megalist.enabled", true]],
  });
  ok(!item.hidden, "Megalist sidebar menu item shown");
});
