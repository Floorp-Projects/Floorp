/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

requestLongerTimeout(2);

async function toggleJavaScript(dbg, shouldBeCheckedAtStart) {
  const menuItemClassName = ".debugger-settings-menu-item-disable-javascript";

  const menuButton = findElementWithSelector(dbg, ".debugger-settings-menu-button");
  menuButton.click();
  await waitForTime(200);

  // Wait for the menu to show before trying to click the item
  const { parent } = dbg.panel.panelWin;
  const { document } = parent;

  const menuItem = document.querySelector(menuItemClassName);
  is(
    !!menuItem.getAttribute("aria-checked"), 
    shouldBeCheckedAtStart,
    "Item is checked before clicking"
  );
  menuItem.click();
}

// Tests that using the Settings menu to enable and disable JavaScript
// updates the pref properly
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");

  info("Waiting for source to load");
  await waitForSource(dbg, "simple1.js");

  info("Clicking the disable javascript button in the settings menu");
  await toggleJavaScript(dbg, false);
  is(Services.prefs.getBoolPref("javascript.enabled"), false, "JavaScript is disabled");

  info("Reloading page to ensure there are no sources");
  await reload(dbg);
  await waitForSourceCount(dbg, 0);

  info("Clicking the disable javascript button in the settings menu to reenable JavaScript");
  await toggleJavaScript(dbg, true);
  is(Services.prefs.getBoolPref("javascript.enabled"), true, "JavaScript is enabled");

  info("Reloading page to ensure there are sources");
  await reload(dbg);
  await waitForSource(dbg, "simple1.js");
});
