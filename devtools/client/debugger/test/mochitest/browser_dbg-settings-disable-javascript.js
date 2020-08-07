/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

requestLongerTimeout(2);

// Tests that using the Settings menu to enable and disable JavaScript
// updates the pref properly
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const menuItemClassName = ".debugger-settings-menu-item-disable-javascript";

  info("Waiting for source to load");
  await waitForSource(dbg, "simple1.js");

  info("Clicking the disable javascript button in the settings menu");
  await toggleDebbuggerSettingsMenuItem(dbg, { className: menuItemClassName, isChecked: false });

  info("Waiting for reload triggered by disabling javascript");
  await waitForSourceCount(dbg, 0);

  info("Clicking the disable javascript button in the settings menu to reenable JavaScript");
  await toggleDebbuggerSettingsMenuItem(dbg, { className: menuItemClassName, isChecked: true });
  is(Services.prefs.getBoolPref("javascript.enabled"), true, "JavaScript is enabled");

  info("Reloading page to ensure there are sources");
  await reload(dbg);
  await waitForSource(dbg, "simple1.js");
});
