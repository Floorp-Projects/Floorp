// Invoke an invidual extension's "Find Updates" menu item
function checkOne(win, addon) {
  win.gViewController.doCommand("cmd_findItemUpdates", addon);
}

// Test "Find Updates" with both auto-update settings
async function test_find_updates() {
  info("Test 'Find Updates' with auto-update true");
  await interactiveUpdateTest(true, checkOne);
  info("Test 'Find Updates' with auto-update false");
  await interactiveUpdateTest(false, checkOne);
}

add_task(async function test_xul_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });

  await test_find_updates();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_html_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });

  await test_find_updates();

  await SpecialPowers.popPrefEnv();
});
