// Invoke an invidual extension's "Find Updates" menu item
function checkOne(win, addon) {
  win.gViewController.doCommand("cmd_findItemUpdates", addon);
}

// Test "Find Updates" with both auto-update settings
add_task(() => interactiveUpdateTest(true, checkOne));
add_task(() => interactiveUpdateTest(false, checkOne));
