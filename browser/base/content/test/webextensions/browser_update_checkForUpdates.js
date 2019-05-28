// Invoke the "Check for Updates" menu item
function checkAll(win) {
  win.gViewController.doCommand("cmd_findAllUpdates");
  return new Promise(resolve => {
    let observer = {
      observe(subject, topic, data) {
        Services.obs.removeObserver(observer, "EM-update-check-finished");
        resolve();
      },
    };
    Services.obs.addObserver(observer, "EM-update-check-finished");
  });
}

// Test "Check for Updates" with both auto-update settings
async function test_check_for_updates() {
  info("Test 'Check for Updates' with auto-update true");
  await interactiveUpdateTest(true, checkAll);
  info("Test 'Check for Updates' with auto-update false");
  await interactiveUpdateTest(false, checkAll);
}

add_task(async function test_xul_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });

  await test_check_for_updates();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_html_aboutaddons() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });

  await test_check_for_updates();

  await SpecialPowers.popPrefEnv();
});
