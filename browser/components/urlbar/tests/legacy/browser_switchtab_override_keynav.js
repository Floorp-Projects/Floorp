/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URL = `${TEST_BASE_URL}dummy_page.html`;

add_task(async function test_switchtab_override_keynav() {
  info("Opening first tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Opening and selecting second tab");
  let secondTab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  registerCleanupFunction(() => {
    try {
      gBrowser.removeTab(tab);
      gBrowser.removeTab(secondTab);
    } catch (ex) {
      /* tabs may have already been closed in case of failure */
    }
    return PlacesUtils.history.clear();
  });

  gURLBar.focus();
  gURLBar.value = "dummy_pag";
  EventUtils.sendString("e");
  await promiseSearchComplete();

  info("Select second autocomplete popup entry");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  ok(/moz-action:switchtab/.test(gURLBar.value), "switch to tab entry found");

  info("Shift+left on switch-to-tab entry");

  EventUtils.synthesizeKey("KEY_Shift", { type: "keydown" });
  EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
  EventUtils.synthesizeKey("KEY_Shift", { type: "keyup" });

  ok(
    !/moz-action:switchtab/.test(gURLBar.inputField.value),
    "switch to tab should be hidden"
  );
});
